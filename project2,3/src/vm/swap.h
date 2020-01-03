#ifndef VM_SWAP
#define VM_SWAP

#include <stdbool.h>
#include <stddef.h>

/* Swap Table에서 slot에 페이지의 여부 */
#define SLOT_FREE       true
#define SLOT_TAKEN      false

void swap_init (void);
size_t swap_out (void * frame_addr);
bool swap_in (size_t slot_idx, void * frame_addr);

#endif