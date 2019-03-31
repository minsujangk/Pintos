#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/kernel/stdio.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"

static void syscall_handler (struct intr_frame *);
bool is_valid_pointer (void *esp, int max_dist);
bool create (void *esp);

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
    case SYS_HALT:
      halt();
    case SYS_EXIT:
      exit(arg_addr);
    break;
    case SYS_CREATE:
      f->eax = create(arg_addr);
    break;
    case SYS_OPEN:
      f->eax = open(arg_addr);
    break;
    case SYS_WRITE:
      f->eax = write(arg_addr);
    break; 
    
  }
}

void halt () {
  power_off ();
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

//(const char *file, unsigned initial_size)
bool
create (void *esp) {
  esp = esp+12; // temporary

  if (!is_valid_pointer(esp, 8)) exit(-1);
  // hex_dump(esp, esp, 100, 1);
  char *file_name = (char*) *(int*)(esp);
  // hex_dump(file_name, file_name, 32, 1);
  unsigned initial_size = *(unsigned*) (esp + 4);

  if (!is_valid_pointer(file_name, 0))
    exit(-1);
  // printf;("create is: %s\n", file_name);

  return filesys_create(file_name, initial_size);
}

// (const char *file)
int open (void *esp) {
  if (!is_valid_pointer(esp, 4)) exit(-1);

  char *file_name = (char*) *(int*)esp;
  if (!is_valid_pointer(file_name, 0))
    exit(-1);

  struct file *f = filesys_open(file_name);
  if (f==NULL) return -1;
  
  int old_max_fd;
  if (list_empty(&thread_current()->fd_list))
    old_max_fd = 2;
  else
    old_max_fd = list_entry(list_front(&thread_current()->fd_list), struct fd_file, elem)->fd;

  // printf("old max_fd is %d\n", old_max_fd);
  struct fd_file *ff = malloc(sizeof(struct fd_file));
  ff->fd = old_max_fd + 1;
  ff->file_ptr = f;
  list_push_front(&thread_current()->fd_list, &ff->elem);

  // printf("file p=%p, fd=%d\n",f, ff->fd);
  return ff->fd;
}

// (int fd, void *buffer, unsigned size)
int write (void *esp) {
  esp = esp + 16; // temporary

  if (!is_valid_pointer(esp, 12)) exit(-1);

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

bool is_valid_pointer (void *esp, int max_length) {
  bool success = true;
  // check both boundaries
  if (!is_user_vaddr(esp) ||!is_user_vaddr((void *)(esp + max_length-1)) )
    return false;
  
  if (pagedir_get_page(thread_current()->pagedir, esp) == NULL)
    return false;

  return success;
}