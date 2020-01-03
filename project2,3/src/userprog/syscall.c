#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <string.h>
#include <devices/input.h>
#include "lib/user/syscall.h"

#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

#include "userprog/pagedir.h"
#include "threads/vaddr.h"
#include "filesys/off_t.h"
#include "vm/page.h"

#define   ARG_SIZE  4

struct file
{
  struct inode *inode;
  off_t pos;
  bool deny_write;      
};

static void syscall_handler (struct intr_frame *);

/* system call을 처리하는 함수들 */
void sys_halt();
void sys_exit(int status);
pid_t sys_exec(const char * cmd_line);
int sys_wait (pid_t pid);
bool sys_create(char *file, unsigned init_size);
int sys_open(char *file);
void sys_close(int fd);
int sys_write(int fd, const void * buffer, unsigned size);
void sys_seek (int fd, unsigned position);
unsigned sys_tell (int fd); 
bool sys_remove (const char * file);
int sys_filesize (int fd);
int sys_read (int fd, void *buffer, unsigned size);

/* Project 3 */
mapid_t sys_mmap(int fd, void *addr);   // addr은 Virtual Memory상의 주소
void sys_munmap(mapid_t mapid);

/* 유효한 syscall인지 확인하는 함수 */
bool is_valid_access(void * ptr);

bool check_args_vaild_onearg(void *ptr1);
bool check_args_vaild_twoarg(void *ptr1, void *ptr2);
bool check_args_vaild_threearg(void *ptr1, void *ptr2, void *ptr3);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* system call handler */
static void
syscall_handler (struct intr_frame *f) {
  
  void *stack_ptr = f->esp;
  if(!is_valid_access(stack_ptr))
    sys_exit(-1);

  int syscall_num = (int)*(uint32_t*)(stack_ptr);

  /* switch 문을 이용해, syscall_num 값에 따라 알맞는 system call 함수를 실행 */
  switch (syscall_num) {
  case SYS_HALT:
    sys_halt();
    break;
  
  case SYS_EXIT:
  {
    if(!check_args_vaild_onearg(stack_ptr + ARG_SIZE * 1))
      sys_exit(-1);

    int status = (int)*(uint32_t *)(stack_ptr + ARG_SIZE * 1);
    sys_exit(status);
    break;
  }

  case SYS_EXEC:      
  {
    if(!check_args_vaild_onearg(stack_ptr + ARG_SIZE * 1))
      sys_exit(-1);

    uint32_t *cmd_addr = (uint32_t *)(stack_ptr + ARG_SIZE * 1);
    if (!is_valid_access(*cmd_addr))
      sys_exit(-1);
      
    const char *cmd_line = *cmd_addr;
    
    f->eax = sys_exec(cmd_line);
    break;
  }

  case SYS_WAIT:      
  {
    if(!check_args_vaild_onearg(stack_ptr + ARG_SIZE * 1))
      sys_exit(-1);

    pid_t pid = (pid_t)*(uint32_t *)(stack_ptr + ARG_SIZE * 1);
    f->eax = sys_wait(pid);
    break;
  }

  case SYS_CREATE:   
  {
    if(!check_args_vaild_twoarg(stack_ptr + ARG_SIZE * 4,
                                stack_ptr + ARG_SIZE * 5))
      sys_exit(-1);

    uint32_t * file_addr = (uint32_t *)(stack_ptr + ARG_SIZE * 4);
    if (!is_valid_access(*file_addr))
      sys_exit(-1);

    char * file = *file_addr;
    unsigned init_size = (unsigned)*(uint32_t *)(stack_ptr + ARG_SIZE * 5);
    if (file == NULL)
      sys_exit(-1);

    f->eax = sys_create(file, init_size);
    break;
  }

  case SYS_REMOVE:   
  {
    if(!check_args_vaild_onearg(stack_ptr + ARG_SIZE * 1))
      sys_exit(-1);
    
    uint32_t * file_addr = (uint32_t *)(stack_ptr + ARG_SIZE * 1);
    if (!is_valid_access(*file_addr))
      sys_exit(-1);
      
    const char * file = *file_addr;
    if (file == NULL)
      sys_exit(-1);
    f->eax = sys_remove(file);
    break;
  }

  case SYS_OPEN:   
  {
    if(!check_args_vaild_onearg(stack_ptr + ARG_SIZE * 1))
      sys_exit(-1);

    uint32_t * file_addr = (uint32_t *)(stack_ptr + ARG_SIZE * 1);
    if (!is_valid_access(*file_addr))
      sys_exit(-1);
      
    const char * file = *file_addr;
    if (file == NULL)
      sys_exit(-1);
    f->eax = sys_open(file);
    break;
  }

  case SYS_FILESIZE:  
  {
    if(!check_args_vaild_onearg(stack_ptr + ARG_SIZE * 1))
      sys_exit(-1);

    int fd = (int)*(uint32_t *)(stack_ptr + ARG_SIZE * 1);
    f->eax = sys_filesize(fd);
    break;
  }

  case SYS_READ:      
  {
    if(!check_args_vaild_threearg(stack_ptr + ARG_SIZE * 5,
                                  stack_ptr + ARG_SIZE * 6,
                                  stack_ptr + ARG_SIZE * 7))
      sys_exit(-1);

    uint32_t * buf_addr = (uint32_t *)(stack_ptr + ARG_SIZE * 6);
    if (!is_valid_access(*buf_addr))
      sys_exit(-1);

    int fd = (int)*(uint32_t *)(stack_ptr + ARG_SIZE * 5);
    void * buffer = (void *)*buf_addr;
    unsigned size = (unsigned)*(uint32_t *)(stack_ptr + ARG_SIZE * 7);    

    f->eax = sys_read(fd, buffer, size);
    break;
  }

  case SYS_WRITE:     
  {
    if(!check_args_vaild_threearg(stack_ptr + ARG_SIZE * 5,
                                  stack_ptr + ARG_SIZE * 6,
                                  stack_ptr + ARG_SIZE * 7))
      sys_exit(-1);

    uint32_t *buffer_ptr = (uint32_t *)(stack_ptr + ARG_SIZE * 6);
    if(!is_valid_access(*buffer_ptr)) {
      sys_exit(-1);
    }

    int fd = (int)*(uint32_t *)(stack_ptr + ARG_SIZE * 5);
    void * buffer = (void *)*buffer_ptr;
    unsigned size = (unsigned)*(uint32_t *)(stack_ptr + ARG_SIZE * 7);

    f->eax = sys_write(fd, buffer, size);
    break;
  }

  case SYS_CLOSE:  
  {
    if(!check_args_vaild_onearg(stack_ptr + ARG_SIZE * 1))
      sys_exit(-1);

    int fd = (int)*(uint32_t *)(stack_ptr + ARG_SIZE * 1);

    sys_close(fd);
    break;
  }

  case SYS_MMAP:       
  {
    if(!check_args_vaild_twoarg(stack_ptr + ARG_SIZE * 4,
                                stack_ptr + ARG_SIZE * 5))
      sys_exit(-1);

    uint32_t *addr_ptr = (uint32_t *)(stack_ptr + ARG_SIZE * 5);

    int fd = (int)*(uint32_t *)(stack_ptr + ARG_SIZE * 4);
    void * addr = (void *)*addr_ptr;

    f->eax = sys_mmap(fd, addr);
    break;
  }

  case SYS_MUNMAP:    
  {
    if(!check_args_vaild_onearg(stack_ptr + ARG_SIZE * 1))
      sys_exit(-1);

    mapid_t mapid = (mapid_t)*(uint32_t *)(stack_ptr + ARG_SIZE * 1);

    sys_munmap(mapid);
    break;
  }

  default:
    break;
  }
}


void sys_halt() {
  shutdown_power_off();
}

void sys_exit(int status) {
  printf("%s: exit(%d)\n", thread_name(), status);

  struct thread *t = thread_current();

  int i;
  for(i = 2; i < 128; i++){
    if(t->fd_table[i] != NULL){
      sys_close(i);
    }
  }
  t->parent_thread->exit_status = status;
  thread_exit();
}

pid_t sys_exec(const char * cmd_line) {
  return process_execute(cmd_line);
}

bool sys_create(char *file, unsigned init_size) {
  bool success;
  success = filesys_create(file, init_size);
  return success;
}

int sys_wait (pid_t pid){
  int exit_status;
  exit_status = process_wait(pid);
  return exit_status;
}

int sys_open(char *file) {
  struct file * file_ptr;
  file_ptr = filesys_open(file);
  if(file_ptr != NULL){
    struct thread *t = thread_current();
    int fd = t->fd_max;
    if (strcmp(thread_current()->name, file) == 0) {
      file_deny_write(file_ptr);
    }
    t->fd_table[fd] = file_ptr;
    t->fd_max ++;
    return fd;
  }
  else{
    return -1;
  }
}

void sys_close(int fd) {
  struct thread *t = thread_current();

  if(fd > 1 && fd < 128 && t->fd_table[fd] != NULL){
    file_close(t->fd_table[fd]);
    t->fd_table[fd] = NULL;
  }
  else{
    sys_exit(-1);
  }
}

int sys_write(int fd, const void * buffer, unsigned size) {
  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  /* write size bytes from buffer to the file opened with fd, return size */
  struct thread *t = thread_current();
  int bytes_written;
  if (t->fd_table[fd] == NULL) {
    sys_exit(-1);
  }
  if (t->fd_table[fd]->deny_write) {
    file_deny_write(t->fd_table[fd]);
  }
  bytes_written = file_write(t->fd_table[fd], buffer, size);
  return bytes_written;
}

bool sys_remove (const char * file) {
  bool success;
  success = filesys_remove(file);
  return success;
}

int sys_filesize (int fd) {
  struct thread *t = thread_current();
  int file_size;
  if(fd > 1 && fd < 128 && t->fd_table[fd] != NULL){
    file_size = file_length (t->fd_table[fd]);
  }
  else {
    sys_exit(-1);
  }
  return file_size;
}

/* read 시스템 콜 함수.
   'fd'에 해당하는 파일의 pos 위치부터 'size' 만큼 읽고,
   읽은 바이트의 크기를 return합니다. (size가 파일 크기보다 큰 경우, 실제 읽은 양만 반환함)
   읽을 수 없는 경우에는 -1을 리턴합니다.
   fd 값이 0인 경우, input_getc()를 이용합니다. */
int sys_read (int fd, void *buffer, unsigned size) {
  
  /* fd가 0인 경우 = stdin인 경우
     loop를 이용해, 키보드로 입력받은 값을 buffer에 넣음으로써 읽는다. */
  if (fd == 0) {
    int i;
    for (i = 0; i < size; i++) {
      *((char *)buffer) = input_getc();
      buffer += ARG_SIZE;
    }
    return size;
  }

  /* fd_table의 인덱스로 벗어난 값 이면 return -1을 한다. */
  if(fd < 2 || fd > 127 ){
    return -1;
  }

  /* 현재 thread에서 fd_table을 가져와서 file을 선택하고 */
  struct thread *t = thread_current();
  struct file *selected_file = t->fd_table[fd];

  /* 해당 fd에 파일이 없는 경우, -1을 리턴 */
  if (selected_file == NULL) {
    return -1;
  }

  /* 파일이 있는 경우, file_read를 통해 읽고, 읽은 byte 크기를 리턴한다. */
  int bites_read;
  bites_read = file_read(selected_file, buffer, size);

  return bites_read;
}

mapid_t sys_mmap (int fd, void *addr) {

  /* 사용하는 변수들 선언 */
  struct mapped_file *mapped_file;
  struct thread *t = thread_current();
  struct file * temp_file;
  int counter;
  int page_read_bytes;
  int type;
  off_t ofs = 0;
  struct list * spt = &t->spt;
  struct spt_entry* spte;

  /* 예외 처리 */
  if (!t->fd_table[fd])   // 파일이 0byte인 경우 (파일 구조체가 존재하지 않는 경우)
    return -1;
  if (pg_ofs(addr) != 0)  // addr이 page-aligned 되어있지 않은 경우
    return -1;
  if (addr == 0)          // addr이 0인 경우
    return -1;
  if (fd == 0 || fd == 1) // fd가 0이나 1인 경우
    return -1;

  /* mapped_file 구조체 초기화 */
  mapped_file = malloc (sizeof (struct mapped_file));
  memset (mapped_file, 0, sizeof(struct mapped_file));
  list_init(&mapped_file->spt_entry_list);
  list_push_back(&t->mapped_list, &mapped_file->elem);

  /* mapped_file 구조체 정보쌓기 */ 
  mapped_file->mapping_id = t->mapping_max;
  t->mapping_max = t->mapping_max + 1;
  mapped_file->file = t->fd_table[fd];
  temp_file = file_reopen(mapped_file->file);
  mapped_file->file = temp_file;

  /* 파일의 크기만큼 spt_entry 생성 */
  for(counter = file_length(temp_file); counter > 0; counter = counter - PGSIZE){

    /* memory overlaping 방지 */
    struct spt_entry * spte;
    struct list_elem * e;
    struct list * spt = &t->spt;
    for (e = list_begin (spt); e != list_end (spt); e = list_next (e)){
        spte = list_entry (e, struct spt_entry, spt_elem);
        if (spte->upage == addr)
            return -1;
    }

    /* Main part */
    page_read_bytes = counter < PGSIZE ? counter : PGSIZE;
    spt_entry_set(spt, 2, addr, temp_file, ofs, page_read_bytes, 0, true);
    spte = spt_entry_get(spt, addr);
    list_push_back (&mapped_file->spt_entry_list, &spte->mapped_file_elem);

    addr += PGSIZE;
    ofs += PGSIZE;
  }

  return mapped_file->mapping_id;
}


void sys_munmap(mapid_t mapid) {

  /* 사용하는 변수들 선언 */
  struct mapped_file *mapped_file;
  struct thread *t = thread_current();
  struct list_elem *e;
  bool success = false;
  struct spt_entry *spte;
  bool is_dirty;

  /* mapid에 해당하는 mapped file 검색 */
  for (e = list_begin (&t->mapped_list); e != list_end (&t->mapped_list); e = list_next (e)){
    mapped_file = list_entry (e, struct mapped_file, elem);
    if (mapped_file->mapping_id == mapid){
      success = true;
      break;
    }
  }

  /* 검색 실패 */
  if(!success)
    return;

  /* 검색 성공시 매핑 해제 */
  for (e = list_begin (&mapped_file->spt_entry_list); e != list_end (&mapped_file->spt_entry_list); ){
    spte = list_entry (e, struct spt_entry, mapped_file_elem);
    is_dirty = pagedir_is_dirty(t->pagedir, spte->upage);

    if (is_dirty) {
      file_write_at(spte->file, spte->upage, spte->read_bytes, spte->ofs);
      pagedir_clear_page(t->pagedir, spte->upage);
      frame_free(spte->ft_entry);
    }

    e = list_remove(e);

    list_remove(&spte->spt_elem);
    free(spte);
  }
  list_remove(&mapped_file->elem);
  free(mapped_file);
}

/* 1. PHYS_BASE(0xc0000000) 보다 아래 있어야 함. (user address 상에 존재)
   2. pagedir 에 access 되어야 함. (매핑되어 있음) */
bool is_valid_access(void * ptr) {
  return is_user_vaddr(ptr) && pagedir_is_accessed(thread_current()->pagedir, ptr);
}

bool check_args_vaild_onearg(void *ptr1) {
  return is_valid_access(ptr1);
}

bool check_args_vaild_twoarg(void *ptr1, void *ptr2){
  return is_valid_access(ptr2) && is_valid_access(ptr1);
}

bool check_args_vaild_threearg(void *ptr1, void *ptr2, void *ptr3){
  return is_valid_access(ptr3) && is_valid_access(ptr2) && is_valid_access(ptr1);
}