#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/kernel/stdio.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);
bool is_valid_pointer (void *esp, int max_dist);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // syscall num stack is invalid.
  if (!is_valid_pointer(f->esp, 0)) {
    exit(-1);
    return;
  }
  int syscall_num =  *(int*) f->esp;
  void *arg_addr = f->esp + 4;
  // printf ("system call! %d\n", syscall_num);

  switch(syscall_num) {
    case SYS_EXIT:
      exit(arg_addr);
    break;
    case SYS_WRITE:
      f->eax = write(arg_addr);
    break; 
    
  }
}

// (int fd, void *buffer, unsigned size)
int write (void *esp) {
  esp = esp + 16; // temporary

  if (!is_valid_pointer(esp, 8))
    return -1;

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

void exit (void* esp) {
  int status;
  if (!is_valid_pointer(esp, 0))
    status = -1;
  else status = *(int*) esp;
  char *name;
  char *process_name = strtok_r(thread_current()->command_line, " ", &name);
  printf ("%s: exit(%d)\n", process_name, status);
  thread_exit();
}

bool is_valid_pointer (void *esp, int max_dist) {
  bool success = true;
  // check both boundaries
  if (!is_user_vaddr(esp) ||!is_user_vaddr((void *)(esp + max_dist + 3)) )
    return false;
  
  if (pagedir_get_page(thread_current()->pagedir, esp) == NULL)
    return false;

  return success;
}