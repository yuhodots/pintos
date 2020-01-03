#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/page.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

// static bool is_load_success;

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
   // FILENAME으로 부터 로드된 user program을 구동하는 새 스레드 시작
   // new thread는 process_execute()가 리턴되기전에 스케줄되며,
   // new process의 스레드 id를 리턴하거나, thread가 생성되지 않았다면 TID_ERROR를 리턴
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
     // FILE_NAME을 카피해서 사용
     // 다만 caller와 load()사이에 race 존재
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
    // FILE_NAME을 execute하는 new thread 생성
  /* File name parsing 실행 */
  char *save_ptr;
  strtok_r(file_name, " ", &save_ptr);
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
  // user process를 로드하고 시작하는 thread function
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  /* Parsing file_name */
  /* 진짜 file_name만 올바르게 추출 후 real_file_name에 저장 */
  char *save_ptr;
  char *token;
  char *real_file_name;

  /* argc 값 찾아내기 */
  int argc = 1;
  char * temp_ptr = file_name;

  while (temp_ptr != NULL) {
    temp_ptr = strchr(temp_ptr, ' ');
    if (temp_ptr != NULL) {
      temp_ptr++;
      argc++;
    }
  }
  token = strtok_r(file_name, " ", &save_ptr); // " " 발견하기 전까지 추출, save_ptr에 위치저장
  real_file_name = token;

  /* Initialize interrupt frame and load executable. */
  // 인터럽트 프레임을 초기화하고 실행파일을 로드
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (real_file_name, &if_.eip, &if_.esp);

  /* Argument stacking */
  /* argument를 stack의 적절한 위치에 쌓기*/
  if (success){

    /* Declaration of variables */
    void **temp_esp = &if_.esp;       // esp (stack pointer)
    int len_sum = 0;                  // for calculating word_align
    int len, len_word_align;          // string length & word_align

    char ** argv = malloc(sizeof(char *) * argc);
    uint32_t ** arg_addr = malloc(sizeof(uint32_t *) * argc);

    argv[0] = real_file_name;
    int arg_idx = 1;
    for (token = strtok_r (NULL, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
      argv[arg_idx] = token;
      arg_idx++;
    }

    /* Stacking argument */
    int i;                                    // initial declaration for loop is allowed only C99 mode
    int max_idx = argc - 1;                   // max index number of the argv[] 
    for (i = max_idx; i > 0; i--){
      len = strlen(argv[i]);
      *temp_esp = *temp_esp - (len + 1);
      strlcpy(*temp_esp, argv[i], len + 1);
      arg_addr[i] = (uint32_t *)*temp_esp;    // uint32_t: unsigned 32bit (8자리 hex 주소)
      len_sum += len + 1;
    }

    /* Stack argv[0][...] */
    /* about file name */
    len = strlen(argv[0]);
    *temp_esp = *temp_esp - (len + 1);
    strlcpy(*temp_esp, argv[0], len + 1);
    arg_addr[0] = (uint32_t *)*temp_esp;
    len_sum += len + 1;

    /* Stack word-align */
    len_word_align = 4 - (len_sum % 4);
    *temp_esp = *temp_esp - len_word_align;

    /* Stack NULL address */
    *temp_esp = *temp_esp - 4;
    **(uint32_t **)temp_esp = 0;

    /* Stack argv[i] */
    /* about arguments */
    for (i = max_idx; i > 0; i--){
      *temp_esp = *temp_esp - 4;
      **(uint32_t **)temp_esp = arg_addr[i];
    }

    /* Stack argv[0] */
    /* about file name */
    *temp_esp = *temp_esp - 4;
    **(uint32_t **)temp_esp = arg_addr[0];
      
    /* Stack argv */
    *temp_esp = *temp_esp - 4;
    **(uint32_t **)temp_esp = *temp_esp + 4;

    /* Stack argc */
    *temp_esp = *temp_esp - 4;
    **(int **)temp_esp = argc;

    /* Stack return address */
    *temp_esp = *temp_esp - 4;
    **(uint32_t **)temp_esp = 0;  // void type으로 넣는 방법을 모르겠어서 8자리 hex로 넣음

    /* For Debugging: hex_dump  */
    // hex_dump(*temp_esp, *temp_esp, 100, 1);
  }

  /* If load failed, quit. */
  palloc_free_page (real_file_name);
  if (!success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  /* Project2-2 */
  /* 인터럽트에 의해 child가 종료되는 경우는 아직 구현하지 못함 */
  struct thread * parent = thread_current();
  struct thread * child;
  bool is_valid_TID = false;

  struct list_elem * e;
  /* child_list에서 자식 thread 하나씩 로드해서 child_tid와 비교 */
  for (e = list_begin (&(parent->child_list)); e != list_end (&(parent->child_list)); e = list_next (e)){
    child = list_entry (e, struct thread, celem);
    if (child->tid == child_tid){
      is_valid_TID = true;
      break;
    }
  }

  /* 비정상적인 TID인 경우 */
  if(!is_valid_TID){   
    return -1;
  }

  /* 정상적인 TID인 경우 */
  sema_down(&child->wait_sema);
  int status = parent->exit_status;
  return status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  
  /* Project3 */
  int maximum;
  int minimum = 1;
  while(minimum < cur->fd_max){
    maximum = (cur->fd_max)-1;
    file_close (cur->fd_table[maximum]);
    cur->fd_max = cur->fd_max - 1;
  }

  /* Project2-2 */
  sema_up(&cur->wait_sema);
  list_remove(&(cur->celem));
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
  // FILE_NAME에서 현재 스레드로 ELF 실행 파일을 로드
  // 실행 파일의 진입점을 *EIP에 저장
  // 초기 스택 포인터를 *ESP에 저장
  // 성공하면 true, 그렇지 않으면 false를 리턴
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  // 실행파일 open
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  // 프로그램의 헤더 read
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  // esp에 스택 셋업
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */


/* 주소 UPAGE에서 FILE의 오프셋 OFS에서 시작하는 세그먼트를 로드합니다. 
  총합하여 다음과 같이 가상 메모리의 READ_BYTES + ZERO_BYTES 바이트가 초기화 됩니다.

  - UPAGE의 READ_BYTES 바이트는 오프셋 OFS에서 시작하는 FILE에서 읽어야 합니다. (?)
  - UPAGE + READ_BYTES의 ZERO_BYTES 바이트는 0으로 설정되어야 합니다. (?)

  이 기능에 의해 초기화된 페이지의 WRITABLE이 true인 경우 사용자 프로세스에 
  의해 쓰기 가능해야 하며, 그렇지 않으면 읽기 전용이어야 합니다.
  
  성공하면 true를 반환하고, 메모리 할당 오류 또는 디스크 읽기 오류가 발생하면 false를 반환하십시오.*/

static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Project3 */
      int type = 1;
      struct thread *t = thread_current ();
      enum palloc_flags flags = PAL_USER;
      struct list * spt = &t->spt;
      spt_entry_set(spt, type, upage, file, ofs, page_read_bytes, page_zero_bytes, writable);
      struct spt_entry* spte = spt_entry_get(spt, upage);

      ofs += page_read_bytes;
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  struct ft_entry * fte;
  bool success = false;
  enum palloc_flags flags = PAL_USER | PAL_ZERO;
  uint8_t *upage = ((uint8_t *) PHYS_BASE) - PGSIZE;
  struct thread *t = thread_current ();

  fte = frame_alloc(flags, t, NULL);
  if (fte != NULL) 
  {
    success = install_page (upage, fte->frame_addr, true);
    if (!success)
    {
      pagedir_clear_page(fte->page_owner->pagedir, fte->spte->upage);
      frame_free (fte);
      return false;
    }
    *esp = PHYS_BASE;
    struct list * spt = &t->spt;
    int type = 3;
    spt_entry_set(spt, type, upage, NULL, NULL, NULL, NULL, true);
    struct spt_entry* spte = spt_entry_get(spt, upage);
    fte->page_owner = t;
    fte->spte = spte;
    spte->ft_entry = fte;
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
