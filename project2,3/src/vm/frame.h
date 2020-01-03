#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/thread.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "vm/page.h"

/* FTE(Frame Table Entry) 구조체 */
struct ft_entry
{
    void * frame_addr;              // physical memory의 frame 주소 -> 혹시 palloc 된 상태라면, &로 값을 알아내나 ?
    struct thread * page_owner;     // frame과 연결된 page의 소유자 thread
    struct spt_entry * spte;        // FTE와 연결된 spt_entry

    struct list_elem elem;          // ft_list의 element로 사용
};

/* Frame Table 관련 함수 */
void ft_init (void);                                                                                    // Frame Table 초기화 (생성)
struct ft_entry * frame_alloc (enum palloc_flags flags, struct thread * t, struct spt_entry * spte);    // Frame 할당
void frame_free (struct ft_entry * free_fte);                                                           // Frame 해제
void frame_evic (void);                                                                                 // Frame Eviction

/* Functions for Debugging */
void print_ft (void);

#endif
