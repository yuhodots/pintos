#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#define   ARG_SIZE  4

static void syscall_handler (struct intr_frame *);

void sys_halt();
void sys_exit(int status);
void sys_create(char *file, unsigned init_size);
void sys_open(char *file);
void sys_close(int fd);
void sys_write(int fd, void * buffer, unsigned size);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) {
  
  void *stack_ptr = f->esp;
  int syscall_num = *(int *)(stack_ptr);
  
  switch (syscall_num) {
  case SYS_HALT:
    sys_halt();
    break;
  
  case SYS_EXIT:
  {
    int status = *(int *)(stack_ptr + ARG_SIZE);
    sys_exit(status);
    break;
  }

  case SYS_CREATE:
  {
    char * file = (char *)(stack_ptr + ARG_SIZE);
    unsigned init_size = *(unsigned *)(stack_ptr + ARG_SIZE * 2);
    sys_create(file, init_size);
    break;
  }

  case SYS_OPEN:
  {
    char * file = (char *)(stack_ptr + ARG_SIZE);
    sys_open(file);
    break;
  }

  case SYS_CLOSE:
  {
    int fd = *(int *)(stack_ptr + ARG_SIZE);
    sys_close(fd);
    break;
  }

  case SYS_WRITE:
  {
    int fd = *(int *)(stack_ptr + ARG_SIZE);
    void * buffer = stack_ptr + ARG_SIZE * 2;
    unsigned size = *(unsigned *)(stack_ptr + ARG_SIZE *3);
    sys_write(fd, buffer, size);
    break;
  }

  default:
    printf("default\n");
    break;
  }
  
  printf ("system call!\n");
  thread_exit ();
}


void sys_halt() {
  printf("halt\n");
}

void sys_exit(int status) {
  printf("exit, status: %d\n", status);
}

void sys_create(char *file, unsigned init_size) {
  printf("create, file: %s, init_size: %d\n", file, init_size);
}

void sys_open(char *file) {
  printf("open, file: %s\n", file);
}

void sys_close(int fd) {
  printf("close, fd: %d\n", fd);
}

void sys_write(int fd, void * buffer, unsigned size) {
  printf("write, fd: %d, buffer: %p, size: %u\n", fd, buffer, size);
}