#include <stdlib.h>

#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"

/* Frame Table 관련 데이터 구조 */
static struct lock ft_lock;     // Frame Table Lock. allocation 과정에서 생길 수 있는 문제를 예방 
static struct list ft_list;     // Frame Table. struct list 자료구조로 구현.

/* 리팩토링 함수 */
void create_frame_add_ftlist (void * frame_addr, struct thread * t, struct spt_entry * spte, struct ft_entry * fte);

/* frame table을 초기화하는 함수.
   struct list의 자료구조를 갖기 때문에, list_init()으로 초기화 하는 것이 필요하다.

   threads/thread.c에서 thread_init()에서 main thread를 init한 뒤, 
   이후의 process들은 Frame table을 이용해 physical memory에 load 될 수 있도록 
   해당 함수 마지막 부분에 frame table 초기화가 이루어지도록 한다. */
void ft_init(void) {
    list_init(&ft_list);
    lock_init(&ft_lock);
}

/* Physical memeory에 프로세스의 페이지를 allocation 하는 함수.

   Physical memory에 free한 frame이 있는 경우, 인자로 받는 프로세스의 페이지의 데이터를 allocation하고, 
   새로운 FTE에 해당 페이지 정보를 넣어준 뒤 ft_list에 추가한다.
   
   Physical memory에 free한 frame이 없는 경우, frame_evic()을 실행해 vimtim frame을 swap out하고,
   palloc_get_page()를 통해 allocation한 뒤, 새로운 FTE를 frame_table에 추가한다.
   이 때 while loop를 이용하는 것은, frame_evic 실행 과정에서 frame_free()를 호출하여 frame을 헤제하는데,
   frame이 헤제된 이후 부터 다시 alloc함수로 돌아와 palloc_get을 실행할 동안 다른 프로세스가 frame_alloc()을
   호출하여, 동기화 문제가 생길 수 있기 때문이다.
   
   (free frame이 있는 지는 palloc_get_page()했을 때, 주소를 return하면 free한 frame이 있어서 성공한거고,
   NULL 을 return하면, free한 frame이 없어서 실패한 것이다.) 
   
   allocation이 완료되면, 해당 frame의 주소를 return 한다. */
struct ft_entry * frame_alloc (enum palloc_flags flags, struct thread * t, struct spt_entry * spte) {

    void * frame_addr = palloc_get_page(flags);
    struct ft_entry * new_fte = malloc (sizeof (struct ft_entry));
    if (frame_addr != NULL) {
        create_frame_add_ftlist(frame_addr, t, spte, new_fte);
    }

    else {
        while(frame_addr == NULL) {
            frame_evic();
            frame_addr = palloc_get_page(flags);
            if ( frame_addr != NULL){
                create_frame_add_ftlist(frame_addr, t, spte, new_fte);
            }
        }
    }

    return new_fte;
}

/* FTE를 받아서 해당 frame을 해제시키고, FTE 구조체도 free한다.

   palloc_free_page()를 통해 FTE의 frame을 먼저 해제 시키고,
   list_remove()를 통해 FTE를 ft_list에서 delete 한다.
   마지막으로, palloc_free_page() 함수를 통해 FTE가 가리키는 FTE 구조체도 해제한다. */
void frame_free (struct ft_entry * free_fte) {

    if (pg_ofs(free_fte->frame_addr) != 0) {
        sys_exit(-1);
    }

    pagedir_clear_page(free_fte->page_owner->pagedir, free_fte->spte->upage);
    palloc_free_page(free_fte->frame_addr);

    lock_acquire(&ft_lock);
    list_remove(&free_fte->elem);
    lock_release(&ft_lock);

    free(free_fte);
}

/* Eviction 알고리즘을 통해 victim frame을 찾아서 swap 한 뒤, 해당 FTE를 반환하는 함수.
   
   (Eviction 알고리즘) -> Victim frame을 찾는 알고리즘

   Victim frame의 데이터를 swap out 시킨 후, frame을 가리키는 FTE와 연결된 thread의 
   SPTE를 찾아가 해당 SPTE를 invaild로 설정함. 이후 frame_free 함수를 통해 FTE가 가리키는 
   frame을 해제 시키고 FTE를 ft_list에서 delete한다. */
void frame_evic (void) {

    /* Eviction Algorithm */
    struct ft_entry * victim_frame = malloc (sizeof (struct ft_entry));
    struct list_elem * elem = list_front(&ft_list);
    
    victim_frame = list_entry(elem, struct ft_entry, elem);
    size_t swap_idx;

    /* Swap out */
    bool frame_is_dirty = pagedir_is_dirty(victim_frame->page_owner->pagedir, victim_frame->spte->upage);

    if (victim_frame->spte->type == 1) {
        if (frame_is_dirty) {
            swap_idx = swap_out(victim_frame->frame_addr);
            victim_frame->spte->swap_idx = swap_idx;
            victim_frame->spte->type = 3;
        }
    }

    else {
        swap_idx = swap_out(victim_frame->frame_addr);
        victim_frame->spte->swap_idx = swap_idx;
    }

    frame_free(victim_frame);
}

/* 새로운 FTE를 생성해서 인자로 받은 데이터를 member 값으로 설정하고,
   해당 FTE를 ft_list에 추가하는 함수 */
void create_frame_add_ftlist (void * frame_addr, struct thread * t, struct spt_entry * spte, struct ft_entry * fte) {
    fte->frame_addr = frame_addr;
    fte->page_owner = t;
    fte->spte = spte;
    lock_acquire(&ft_lock);
    list_push_back(&ft_list, &fte->elem);
    lock_release(&ft_lock);
}

/* Functions for Debugging */

/* ft_list의 정보를 출력하는 함수.
   ft_list에 있는 fte의 frame_addr을 출력하고, 마지막으로 총 entry의 개수를 출력한다. */
void print_ft () {
    struct list_elem * iter;
    int count = 0;

    printf("\n[debug] ft_list\n");

    for (iter = list_begin(&ft_list); iter != list_end(&ft_list);
         iter = list_next(iter))
        {
            struct ft_entry * fte = malloc(sizeof (struct ft_entry));
            fte = list_entry(iter, struct ft_entry, elem);

            printf("%p ", fte->frame_addr);
            count++;
        }
    
    printf("\n총 fte의 개수: %d\n\n", count);
}