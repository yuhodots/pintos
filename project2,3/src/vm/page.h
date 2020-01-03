#ifndef VM_PAGE
#define VM_PAGE

/* Header file */
#include "lib/kernel/list.h"
#include "threads/thread.h"
#include "vm/frame.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"

/* Supplemental Page Table Entry 구조체 */
struct spt_entry{

    int type;                   // BIN:1, FILE(mmf):2, ANON(swap):3
    uint8_t *upage;             // user page address
    struct ft_entry *ft_entry;  // frame table entry

    /* load_segment */
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    /* swap */
    size_t swap_idx;            // swap table (bitmap) index

    /* list elem */
    struct list_elem spt_elem;
    struct list_elem mapped_file_elem;
};

/* mmap: file Entry 구조체 */
struct mapped_file{
    int mapping_id;
    struct list spt_entry_list;
    struct file * file;
    struct list_elem elem;
};

/* Supplemental Page Table Entry 메소드 */
void spt_entry_set (struct list *spt, int type, uint8_t *upage, struct file *file,
                    off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
struct spt_entry * spt_entry_get (struct list *spt, uint8_t *upage);
void spt_entry_update (struct spt_entry *spte, uint8_t *kpage);
void print_spt_kpage (struct list *spt);

#endif