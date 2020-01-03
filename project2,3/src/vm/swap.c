#include "vm/swap.h"
#include "lib/kernel/bitmap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

/* 한 페이지를 저장하는 데 필요한 SECTOR의 개수 */
#define SECTORS_PER_PAGE ( PGSIZE / BLOCK_SECTOR_SIZE )

/* swapping에 필요한 데이터 구조들 */
struct block * swap_block;      // swapping에 사용되는 disk block (영역)
struct bitmap * swap_table;     // swap slot이 사용되고 있는 지에 대한 정보를 가짐
struct lock swap_lock;          // 동기화를 위한 lock

/* Swapping에 관련된 데이터 구조들을 세팅함.
   Lock initialize
   Swapping에 사용하는 block을 가져오고 만약 가져오지 못하면 return
   Swap slot의 개수 size의 bitmap을 생성 (swap table)
   Swap table의 모든 bit를 SLOT_FREE(true)로 설정 */
void swap_init () {

    lock_init(&swap_lock);

    swap_block = block_get_role(BLOCK_SWAP);
    if (swap_block == NULL) {
        printf("\n[ERROR] there is no space for swapping\n\n");
        return;
    }

    /* swap slot의 개수 (bitmap size) */
    size_t num_swap_slot = block_size(swap_block) / SECTORS_PER_PAGE;
    swap_table = bitmap_create(num_swap_slot);
    if (!swap_table) {
        printf("\n[ERROR] Creating swap table is failed !!\n\n");
        return;
    }

    bitmap_set_all(swap_table, SLOT_FREE);
}

/* memory에서 swap disk로 page를 넘김 (memory가 disk에 write)
   Frame의 address를 받고, swap slot의 인덱스를 return 함.
   
   swap_table에서 SWAP_FREE인 슬롯을 찾고, 해당 슬롯을 TAKEN 처리 한다.
   만일 BITMAP_ERROR인 경우, FREE인 슬롯이 없으므로 ERROR임을 return함
   
   Free한 slot을 찾은 경우, block_write를 통해 block에 page 데이터를 write함.
   이 때, 한 페이지는 SECTORS_PER_PAGE 만큼의 sector로 쪼개지기 때문에, (Sector는 block을 구성하는 단위)
   for loop를 이용하여, 각각의 sector 모두 write될 수 있도록 한다. */
size_t swap_out (void * frame_addr) {

    lock_acquire(&swap_lock);

    size_t free_slot_idx = bitmap_scan_and_flip(swap_table, 0, 1, SLOT_FREE);
    if (free_slot_idx == BITMAP_ERROR) {
        printf("\n[Error] There is no free swap slot !!\n\n");
        return BITMAP_ERROR;
    }

    int sec_idx;        /* 페이지 내에서의 sector index */
    for (sec_idx = 0; sec_idx < SECTORS_PER_PAGE; sec_idx++) {
        block_write (
            swap_block,                                        
            free_slot_idx * SECTORS_PER_PAGE + sec_idx,              
		    (uint8_t *) frame_addr + (sec_idx * BLOCK_SECTOR_SIZE)
        );
    }

    lock_release(&swap_lock);

    return free_slot_idx;
}

/* swap disk에서 memory로 page를 넘겨줌 (memory가 read함)
   swap slot의 인덱스와 allocate 할 frame 주소를 받아서, 성공 여부를 return함.

   잘못된 slot 인덱스인지를 테스트하고,
   통과되면 block_read를 통해 메모리에 블록에 있는 섹터의 데이터를 load한다.
   block_write와 마찬가지로 for loop를 통해 해당 페이지의 모든 섹터를 read한다. */
bool swap_in (size_t slot_idx, void * frame_addr) {

    lock_acquire(&swap_lock);

    if (slot_idx > bitmap_size(swap_table) ||               // slot_idx가 slot 개수보다 큰 경우
        bitmap_test(swap_table, slot_idx) != SLOT_TAKEN )   // swap page가 없는 slot인 경우
    {
        lock_release(&swap_lock);
        return false;
    }

    int sec_idx;
    for (sec_idx = 0; sec_idx < SECTORS_PER_PAGE; sec_idx++) {
        block_read (
            swap_block,                                        
            slot_idx * SECTORS_PER_PAGE + sec_idx,              
		    (uint8_t *) frame_addr + (sec_idx * BLOCK_SECTOR_SIZE)
        );
    }

    bitmap_flip(swap_table, slot_idx);

    lock_release(&swap_lock);
    return true;
}


