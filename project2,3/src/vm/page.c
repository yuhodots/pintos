/* Header file */
#include "vm/page.h"
#include <stdlib.h>

/* spt_entry의 값 설정, success값 리턴 */
void spt_entry_set (struct list *spt, int type, uint8_t *upage, struct file *file, 
                    off_t ofs, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {

    struct spt_entry * spte;
    spte = malloc (sizeof (struct spt_entry));
    if (spte == NULL){
        free(spte);
        return false;
    }
    memset (spte, 0, sizeof (struct spt_entry));
    list_push_back (spt, &spte->spt_elem);

    spte->type = type;
    spte->upage = upage;
    spte->file = file;
    spte->ofs = ofs;
    spte->read_bytes = read_bytes;
    spte->zero_bytes = zero_bytes;
    spte->writable = writable;
}

/* spt_entry가 가리키는 ft_entry 내 frame_addr 수정 */
void spt_entry_update (struct spt_entry *spte, uint8_t *kpage){
    spte->ft_entry->frame_addr = kpage;
}

/* upage를 가리키는 spt_entry를 리턴 */
struct spt_entry * spt_entry_get (struct list * spt, uint8_t *upage){
    struct spt_entry * spte;
    struct list_elem * e;
    for (e = list_begin (spt); e != list_end (spt); e = list_next (e)){
        spte = list_entry (e, struct spt_entry, spt_elem);
        if (spte->upage == upage)
            break;
    }
    return spte;
}

/* 디버깅을 위한 함수 */
void print_spt_kpage (struct list *spt){
    printf("[print_spt_kpage]");
    struct list_elem * e;
    struct spt_entry * print;
    for (e = list_begin (spt); e != list_end (spt); e = list_next (e)){
        print = list_entry (e, struct spt_entry, spt_elem);
        printf("%X | ",print->ft_entry->frame_addr);
    }
    printf("\n\n");
}