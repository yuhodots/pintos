#include "devices/timer.h"
#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */   // -> 왜 process라 적었을까 ?
static struct list all_list;  

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */
int load_avg = 0;               /* System-wide load_avg value (initialization 0 when BOOT) */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

bool compare_priority_of_threads(struct list_elem *a, struct list_elem *b, void *aux UNUSED);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();                       // 실행 중인 thread, 아래에 정의 됨.
  init_thread (initial_thread, "main", PRI_DEFAULT);        // main 이라는 이름을 가진 block 상태의 thread로 초기화. 우선 순위는 default(31), init_thread는 아래에 정의 됨
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();                    // next_tid 값을 받게 됨, allocate_tid는 아래에 정의 됨.
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;                            // semaphore: synch.h에 정의 됨. 개념 공부하기
  sema_init (&idle_started, 0);                             // sema_init: synch.c에 정의 됨. 0 value를 가진 idle_started semaphore 초기화
  thread_create ("idle", PRI_MIN, idle, &idle_started);     // idle thread 생성. 0의 우선순위

  /* Start preemptive thread scheduling. */
  intr_enable ();                                           // Enables interrupts and returns the previous interrupt status. : interrupt.c 에 정의됨

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);                                // sema_down: synch.c에 정의 됨. sema의 값이 1 줄어들고, 이것저것 작업이 있음.
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void)                          // devices/timer.c 의 timer_interrupt에 의해 실행 됨.
{
  struct thread *t = thread_current ();

  /* Implemetation every time interrupt */
  if(t->status == THREAD_RUNNING && t != idle_thread){
    t->recent_cpu += 1;
  }

  /* Implemetation every 1 second */
  if (timer_ticks()%TIMER_FREQ == 0){
    update_load_avg();
    thread_foreach(&update_recent_cpu_sec, 0);  // thread_foreach는 인터럽트가 꺼진상태에서 실행되어야 함
  }

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE){        // TIME_SLICE는 위에서 4로 정의 됨. (# of timer ticks to give each thread.)
    intr_yield_on_return ();                // interrupt.c 에 정의 됨. Should we yield on interrupt return? -> true
  }                
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  t->recent_cpu = 0;
  t->nice = 0;

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);

  /* [1] Declaration */
  struct thread *cur = thread_current ();
  struct thread *pri_max = list_entry (list_begin (&ready_list), struct thread, elem);

  /* [2] Yield check */
  if(pri_max->priority > cur->priority)
    thread_yield();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;       // 현재 thread의 상태를 block 시키고,
  schedule ();                                      // schedule() 을 실행시킴, 아래에서 2번 째 위치에 있음.
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;                      // intr_level은 INTR_OFF, INTR_ON (0,1)값 만을 가진 enum이다. -> off는 disable, on은 enable

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  //list_push_back (&ready_list, &t->elem);
  list_insert_ordered(&ready_list, &t->elem, compare_priority_of_threads, NULL);
  t->status = THREAD_READY;                      // thread를 READY 상태로
  intr_set_level (old_level);                    // interrupt를 disable 시킴.
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);       // list에서 제거하고
  thread_current ()->status = THREAD_DYING;       // thread 상태도 DYING
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();           // 현재의 thread
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread)                           // idle thread가 아니면,
    //list_push_back (&ready_list, &cur->elem);       // ready_list에 현재 thread의 elem을 넣음
    list_insert_ordered(&ready_list, &cur->elem, compare_priority_of_threads, NULL);
  cur->status = THREAD_READY;                       // 현재 thread를 READY 상태로
  schedule ();
  intr_set_level (old_level);                       // 원래의 intr status로 다시 set
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;                                            // list_elem은 lib/kernel/list.h에 정의 됨.

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);     // all_list는 모든 thread가 들어가 있는 리스트 (위에 정의 됨)
       e = list_next (e))                                         // 모든 thread를 반복문으로 실행하겠다는 뜻
    {
      struct thread *t = list_entry (e, struct thread, allelem);  // list_entry가 list.h에 정의되어 있긴 한데, 뭔 뜻인지 잘 모르겟...
      func (t, aux);                                              // 모든 thread에 대해 func을 실행한다 ?
    }
}

/* load_avg를 update하는 함수 */
int 
update_load_avg (void)                                  
{
  /* [1] Declaration */
  struct thread *cur = thread_current();                // 현재 실행중인 thread
  int fp_load_avg;                                      // 17.14 Fixed-point load_avg 값을 담을 변수
  static int f = (1<<14);                               // 17.14 Fixed-point의 f값: 2^14
  int ready_threads;                                    // READY 또는 RUNNING state의 Thread 개수

  /* [2] Calculate ready_threads */
  if (cur == idle_thread)                               // 의문점: list 타입은 어디서 호출되는거지?
    ready_threads = (int)(list_size(&ready_list));      // 현재 실행중인 thread가 idle인 경우의 ready_thread 값
  else
    ready_threads = (int)(list_size(&ready_list)) + 1;  // 현재 실행중인 thread가 idle이 아닌 경우의 ready_thread 값

  /* [3] Calculate load_avg */                          // 17.14 Fixed-point load_avg 값 계산
  int x_a = ((int64_t)(59*f))*f/(60*f);
  int y_a = load_avg*f;
  int x_b = ((int64_t)(1*f))*f/(60*f);
  int y_b = ready_threads*f;
  fp_load_avg = ((int64_t)x_a)*y_a/f + ((int64_t)x_b)*y_b/f;
  if (fp_load_avg >= 0)                                 // 17.14 Fixed-point load_avg 값 가장 가까운 정수로 반올림
    load_avg = (fp_load_avg + f/2)/f;
  else
    load_avg = (fp_load_avg - f/2)/f;
  
  return load_avg;
}

/* sec마다 모든 thread의 recent_cpu를 update하는 함수 */
void
update_recent_cpu_sec (struct thread *t, void *aux UNUSED)                            
{
  /* [1] Declaration */
  static int f = (1<<14);
  int fp_recent_cpu = (t->recent_cpu)*f;
  int fp_nice = (t->nice)*f;

  /* [2] Calculate fp_recent_cpu */
  int x = (2*load_avg)*f;
  int y = (2*load_avg+1)*f;
  x = ((int64_t)x)*f/y;
  fp_recent_cpu = ((int64_t)x)*fp_recent_cpu/f + fp_nice;

  /* [3] Update recent_cpu */
  if (fp_recent_cpu >= 0) 
    t->recent_cpu = (fp_recent_cpu + f/2)/f;
  else
    t->recent_cpu = (fp_recent_cpu - f/2)/f;
}

/* Idle thread.  Executes when no other thread is ready to run.     // READY 상태인 thread가 없을 때, 실행되는 thread

   The idle thread is initially put on the ready list by            // idle thread는 thread_start()에 의해 ready list에 들어감.
   thread_start().  It will be scheduled once initially, at which   // 처음에 initialize할 때, 한 번만 schedule되고, 
   point it initializes idle_thread, "up"s the semaphore passed     // 지나는 semaphore를 'up' 시켜서 
   to it to enable thread_start() to continue, and immediately      // thread_start()가 continue 할 수 있게 한 뒤, block된다.
   blocks.  After that, the idle thread never appears in the        // 이후로는, idle thread는 ready list에 나오지 않는다.
   ready list.  It is returned by next_thread_to_run() as a         // ready list가 비어있는 케이스에는 next_thread_to_run()
   special case when the ready list is empty. */                    // 에 의해 반환된다.
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();                        // idle_thread는 위에 전역으로 struct thread로 선언되어 있음.
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;   // t가 NULL이 아니고, t의 magic 값이 create나 init에 의해 유효한 값을 갖어야 함.
}

/* Does basic initialization of T as a blocked thread named
   NAME. 
   넘겨준 이름과 우선순위를 갖고, block 상태의 thread를 만들고, stack 할당, magin 값 설정을 해준 뒤,
   all_list에 넣는다. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should     // 왜 run queue ?
   return a thread from the run queue, unless the run queue is      // run queue라는게 있나 ?
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */ 
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_back (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page    // thread를 switch함
   tables, and, if the previous thread is dying, destroying it.     // prev가 죽으면 destroy함

   At this function's invocation, we just switched from thread      
   PREV, the new thread is already running, and interrupts are      
   still disabled.  This function is normally invoked by            // schedule 함수에서 실행 됨.
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);     // interrupt가 On 이면 error 발생

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);          // 죽은 경우 free 시켜 줌.
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);    // switch.S에 있음. 인자로 들어간 두 thread를 바꾸어 줌.
  thread_schedule_tail (prev);            // cur을 RUNNING status로 만들고, ticks를 0으로 초기화
}

/* Returns a tid to use for a new thread. */
static tid_t                      // tid_t는 thread id의 뜻을 가진 int 자료형을 의미
allocate_tid (void) 
{
  static tid_t next_tid = 1;      /* static 변수는 처음 call 될 때만 선언되는 문장이 유효하고, 이후부터는 선언문에 정의되는 값은 무시된다.
                                     따라서 next_tid 값은 allocate_tid가 실행될 때마다 1로 초기화 되는 것이 아니라, 1씩 증가한 값이 된다. */ 
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

bool compare_priority_of_threads(struct list_elem *a, struct list_elem *b, void *aux UNUSED) {
  struct thread * a_thread = list_entry(a, struct thread, elem);
  struct thread * b_thread = list_entry(b, struct thread, elem);

  if (a_thread->priority > b_thread->priority) {
    return false;
  } else {
    return true;
  }
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
