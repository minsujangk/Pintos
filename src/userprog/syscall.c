#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/kernel/stdio.h"
#include "lib/stdio.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  int syscall_num =  *(int*) f->esp;
  void *arg_addr = f->esp + 4;
  printf ("system call! %d\n", syscall_num);

  switch(syscall_num) {
    case SYS_EXIT:
      exit(*(int*) arg_addr);
    break;
    case SYS_WRITE:
      f->eax = write(arg_addr);
    break; 
    
  }
}

// (int fd, void *buffer, unsigned size)
int write (void *esp) {
  esp = esp + 16; // temporary

  // hex_dump(esp, esp, 32, 1);
  int fd = *(int*) esp;
  void *buffer = *(void**) (esp + 4);
  int size = *(unsigned*) (esp + 8);
  // printf("write started %d, %p, %d\n", fd, buffer, size);

  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
}

void exit (int status) {
  printf ("%s: exit(%d)\n", thread_name(), status);
  thread_exit();
}