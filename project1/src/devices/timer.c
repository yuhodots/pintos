#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "devices/pit.h"
#include "threads/interrupt.h"
#include "threads/synch.h"
#include "threads/thread.h"
  
/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* timer_sleep() 에 의해, sleep 상태에 있는 thread의 리스트.
   sleep 상태의 thread는 sleep_tick 만큼의 시간 이후에 READY 상태되가 되는 BLOCK 상태의 thread이다. */
static struct list sleep_list;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);
static void real_time_delay (int64_t num, int32_t denom);

/* sleep_list에 thread를 넣을 때 사용 되는 less 함수.
   thread의 sleep_ticks 값이 작은 thread가 앞으로 가게 한다. */
bool compare_sleep_ticks_of_threads(struct list_elem *a, struct list_elem *b, void *aux UNUSED);

/* sleep_list의 가장 앞의 thread를 반환하는 함수. */
struct thread * get_front_thread_of_sleep_list();

/* Sets up the timer to interrupt TIMER_FREQ times per second,
   and registers the corresponding interrupt. */
void
timer_init (void) 
{
  pit_configure_channel (0, 2, TIMER_FREQ);
  intr_register_ext (0x20, timer_interrupt, "8254 Timer");
  list_init(&sleep_list);                                   // sleep_list를 initialize 해준다.
}

/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) 
{
  unsigned high_bit, test_bit;

  ASSERT (intr_get_level () == INTR_ON);
  printf ("Calibrating timer...  ");

  /* Approximate loops_per_tick as the largest power-of-two
     still less than one timer tick. */
  loops_per_tick = 1u << 10;
  while (!too_many_loops (loops_per_tick << 1)) 
    {
      loops_per_tick <<= 1;
      ASSERT (loops_per_tick != 0);
    }

  /* Refine the next 8 bits of loops_per_tick. */
  high_bit = loops_per_tick;
  for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
    if (!too_many_loops (high_bit | test_bit))
      loops_per_tick |= test_bit;

  printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t                                         // int64_t는 64비트 크기의 signed int형 (-2^63 ~ 2^63 - 1)
timer_ticks (void) 
{
  enum intr_level old_level = intr_disable ();
  int64_t t = ticks;                            // ticks = Number of timer ticks since OS booted. (위에 정의된 전역 변수)
  intr_set_level (old_level);
  return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks(). */
int64_t
timer_elapsed (int64_t then) 
{
  return timer_ticks () - then;
}

/* Sleeps for approximately TICKS timer ticks.  Interrupts must
   be turned on. */
void
timer_sleep (int64_t ticks) 
{
  int64_t start = timer_ticks ();

  ASSERT (intr_get_level () == INTR_ON);

  /* 1. Current thread의 sleep_ticks에 ticks 값을 넣는다.
     2. Current thread를 sleep_list에 넣는다. 이 때, list_insert_ordered 함수를 이용하여
        thread의 sleep_ticks 값이 낮은 elem이 가장 앞이 되도록 정렬된 순서로 넣는다.
     3. Current thread를 block 상태로 만든다. */
  
  /* 1 */
  struct thread *cur = thread_current();
  cur->sleep_ticks = ticks;    

  /* 2 */
  list_insert_ordered(&sleep_list, &cur->elem, compare_sleep_ticks_of_threads, NULL);

  /* 3) thread block을 실행하기 위해서는 먼저 intrrupt를 off시켜야 한다. */
  enum intr_level old_level = intr_disable ();
  thread_block();
  intr_set_level (old_level);
}

/* 2번에 사용된 정렬 기준 (less) 함수. list_elem a, b를 비교한다. 
   list_entry를 통해 list_elem과 연결된 thread를 불러내어,
   thread의 sleep_ticks 값을 비교해 a가 b보다 크면 false를, 작거나 같으면 true를 return 한다. */
bool compare_sleep_ticks_of_threads(struct list_elem *a, struct list_elem *b, void *aux UNUSED) {
  struct thread * a_thread = list_entry(a, struct thread, elem);
  struct thread * b_thread = list_entry(b, struct thread, elem);

  if (a_thread->sleep_ticks > b_thread->sleep_ticks) {
    return false;
  } else {
    return true;
  }
}

/* Sleeps for approximately MS milliseconds.  Interrupts must be
   turned on. */
void
timer_msleep (int64_t ms) 
{
  real_time_sleep (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts must be
   turned on. */
void
timer_usleep (int64_t us) 
{
  real_time_sleep (us, 1000 * 1000);
}

/* Sleeps for approximately NS nanoseconds.  Interrupts must be
   turned on. */
void
timer_nsleep (int64_t ns) 
{
  real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Busy-waits for approximately MS milliseconds.  Interrupts need
   not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_msleep()
   instead if interrupts are enabled. */
void
timer_mdelay (int64_t ms) 
{
  real_time_delay (ms, 1000);
}

/* Sleeps for approximately US microseconds.  Interrupts need not
   be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_usleep()
   instead if interrupts are enabled. */
void
timer_udelay (int64_t us) 
{
  real_time_delay (us, 1000 * 1000);
}

/* Sleeps execution for approximately NS nanoseconds.  Interrupts
   need not be turned on.

   Busy waiting wastes CPU cycles, and busy waiting with
   interrupts off for the interval between timer ticks or longer
   will cause timer ticks to be lost.  Thus, use timer_nsleep()
   instead if interrupts are enabled.*/
void
timer_ndelay (int64_t ns) 
{
  real_time_delay (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) 
{
  printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}

/* Timer interrupt handler. */
static void
timer_interrupt (struct intr_frame *args UNUSED)
{
  /* 1. sleep_list가 empty가 아닌 경우,
        1) sleep_list에 있는 모든 thread의 sleep_ticks를 1 감소
        2) 가장 앞 (sleep_ticks가 가장 작은) thread의 sleep_ticks 값이 0이면,
           sleep_list의 가장 앞 thread를 pop하고,
           그 thread를 unblock() 시킨다.
           (추가: 0이 되는 thread가 여러 개일 때의 처리를 위해, 
            while문을 통해 1-2를 반복한다.)

     2. 모든 경우에 대하여, ticks를 1 증가 시키고,
        thread_tick()을 실행한다. */

  /* 1 */
  bool is_sleep_list_empty = list_empty(&sleep_list);
  if(!is_sleep_list_empty) {
    
    /* 1-1 */
    struct list_elem *iter;
    for (iter = list_begin(&sleep_list); iter != list_end(&sleep_list);
         iter = list_next(iter))
      {
        struct thread *t = list_entry(iter, struct thread, elem);
        t->sleep_ticks = t->sleep_ticks - 1;
      }
    
    /* 1-2 */
    struct thread *f_thread = get_front_thread_of_sleep_list();

    while (f_thread->sleep_ticks <= 0) {
      list_pop_front(&sleep_list);
      thread_unblock(f_thread);
      f_thread = get_front_thread_of_sleep_list();
    }
  }

  /* 2 */
  ticks++;
  thread_tick ();
}

/* sleep_list의 가장 앞(sleep_ticks값이 작은)의 thread 반환 */
struct thread *
get_front_thread_of_sleep_list() {
  struct list_elem *first_elem = list_begin(&sleep_list);
  struct thread *t = list_entry(first_elem, struct thread, elem);
  return t;
}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) 
{
  /* Wait for a timer tick. */
  int64_t start = ticks;
  while (ticks == start)
    barrier ();

  /* Run LOOPS loops. */
  start = ticks;
  busy_wait (loops);

  /* If the tick count changed, we iterated too long. */
  barrier ();
  return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) 
{
  while (loops-- > 0)
    barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) 
{
  /* Convert NUM/DENOM seconds into timer ticks, rounding down.
          
        (NUM / DENOM) s          
     ---------------------- = NUM * TIMER_FREQ / DENOM ticks. 
     1 s / TIMER_FREQ ticks
  */
  int64_t ticks = num * TIMER_FREQ / denom;

  ASSERT (intr_get_level () == INTR_ON);
  if (ticks > 0)
    {
      /* We're waiting for at least one full timer tick.  Use
         timer_sleep() because it will yield the CPU to other
         processes. */                
      timer_sleep (ticks); 
    }
  else 
    {
      /* Otherwise, use a busy-wait loop for more accurate
         sub-tick timing. */
      real_time_delay (num, denom); 
    }
}

/* Busy-wait for approximately NUM/DENOM seconds. */
static void
real_time_delay (int64_t num, int32_t denom)
{
  /* Scale the numerator and denominator down by 1000 to avoid
     the possibility of overflow. */
  ASSERT (denom % 1000 == 0);
  busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000)); 
}
