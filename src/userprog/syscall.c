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
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);
bool is_valid_pointer (void *esp, int max_dist);
bool create (void *esp);
bool remove (void *esp);
unsigned tell (void *esp);

bool isdebug2 = false;

void
syscall_init (void) 
{
  lock_init(&fs_lock);
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
    case SYS_EXEC:
      f->eax = exec(arg_addr);
    break;
    case SYS_WAIT:
      f->eax = wait(arg_addr);
    break;
    case SYS_CREATE:
      f->eax = create(arg_addr);
    break;
    case SYS_REMOVE:
      f->eax = remove(arg_addr);
    break;
    case SYS_OPEN:
      f->eax = open(arg_addr);
    break;
    case SYS_FILESIZE:
      f->eax = filesize(arg_addr);
    break;
    case SYS_READ:
      f->eax = read(arg_addr);
    break; 
    case SYS_WRITE:
      f->eax = write(arg_addr);
    break; 
    case SYS_SEEK:
      seek(arg_addr);
    break;
    case SYS_TELL:
      f->eax = tell(arg_addr);
    break;
    case SYS_CLOSE:
      close(arg_addr);
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

  exit_impl(status);
}

void exit_impl (int status) {

  _close_all_fd();

  struct list_elem *e, *next;
  for (e=list_begin(&parent_child_list); e!=list_end(&parent_child_list); e=next) {
    next=list_next(e);
    struct child_status *cstat = list_entry(e, struct child_status, elem);
    if (cstat->parent_pid == thread_tid()) {
      list_remove(&cstat->elem);
      free(cstat);
    }
    // update child status to later use in wait
    else if (cstat->child_pid == thread_tid()) {
      cstat->exit_status = status;
    }
  }

  char *name;
  char *process_name = strtok_r(thread_current()->command_line, " ", &name);
  printf ("%s: exit(%d)\n", process_name, status);
  thread_exit();
}

tid_t exec (void *esp) {
  char *cmd_line = (char*) *(int*) esp;

  if(!is_valid_pointer(cmd_line, 0)) exit(-1);

  lock_acquire(&fs_lock);
  tid_t pid = process_execute(cmd_line);
  lock_release(&fs_lock);
  // printf("pid %d: %s, syscall::exec for %d\n", thread_tid(), cmd_line, pid);
  struct child_status *cstat = malloc(sizeof(struct child_status));
  // struct child_status *cstat = thread_get_child_status(pid);
  // struct child_status *cstat = palloc_get_page(0);
  cstat->parent_pid = thread_tid();
  cstat->child_pid = pid;
  cstat->exit_status = 1000;
  sema_init(&cstat->sema_start, 0);
  
  list_push_back(&parent_child_list, &cstat->elem);
// printf("pid %d: exec %s waiting to start of %d\n", thread_tid(),  cmd_line, pid);
  sema_down(&cstat->sema_start); // wait for start process complete
  
// printf("pid %d: start complete of %d\n", thread_tid(), pid);
  pid = cstat->child_pid;
  
// printf("pid %d: cstat of %d\n", thread_tid(), pid);
  return pid;
}

int wait (void *esp) {
  int pid = *(int*) esp;
  if (isdebug2) printf("pid %d: syscall::wait for %d\n", thread_tid(), pid);
  struct list_elem *e, *next;
  for (e=list_begin(&parent_child_list); e!=list_end(&parent_child_list); e=next) {
    next=list_next(e);
    struct child_status *cstat = list_entry(e, struct child_status, elem);
    if (cstat->child_pid == pid) {
      // if waiting found
      // if (cstat->exit_status == -1) return -1;
      if (cstat->exit_status != 1000) { 
        list_remove(&cstat->elem);
        return cstat->exit_status;
      }
      int result = process_wait(cstat->child_pid);
      list_remove(&cstat->elem);
      free(cstat);
      return result;
    }
  }
  return -1;
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

  lock_acquire(&fs_lock);
  bool returnVal = filesys_create(file_name, initial_size);
  lock_release(&fs_lock);
  // printf("create is: %s, %d, %d\n", file_name, initial_size, returnVal);
  return returnVal;
}

// bool remove (const char *file)
bool remove (void *esp) {
  char *file_name = (char*) *(int*)(esp);

  if (!is_valid_pointer(file_name, 0))
    exit(-1);

  lock_acquire(&fs_lock);
  bool returnVal = filesys_remove(file_name);
  lock_release(&fs_lock);
  return returnVal;
}

// (const char *file)
int open (void *esp) {
  if (!is_valid_pointer(esp, 4)) exit(-1);

  char *file_name = (char*) *(int*)esp;
  if (!is_valid_pointer(file_name, 0))
    exit(-1);

  lock_acquire(&fs_lock);
  struct file *f = filesys_open(file_name);
  if (f == NULL)
  {
    lock_release(&fs_lock);
    return -1;
  }

  int old_max_fd;
  if (list_empty(&thread_current()->fd_list))
    old_max_fd = 2;
  else
    old_max_fd = list_entry(list_front(&thread_current()->fd_list), struct fd_file, elem)->fd;

  // printf("old max_fd is %d\n", old_max_fd);
  struct fd_file *ff = malloc(sizeof(struct fd_file));
  ff->fd = old_max_fd + 1;
  ff->file_ptr = f;
  strlcpy(ff->file_name, file_name, strlen(file_name) + 1);
  list_push_front(&thread_current()->fd_list, &ff->elem);
  lock_release(&fs_lock);

  // printf("file p=%p, fd=%d\n",f, ff->fd);
  return ff->fd;
}

int filesize (void *esp) {
  int fd = *(int*) esp;

  struct list *fd_list = &thread_current()->fd_list;
  struct list_elem *e;
  lock_acquire(&fs_lock);
  for (e = list_begin(fd_list); e != list_end(fd_list); e = list_next(e))
  {
    struct fd_file *ff = list_entry(e, struct fd_file, elem);
    if (ff->fd == fd)
    {
      int returnVal = file_length(ff->file_ptr);
      lock_release(&fs_lock);
      return returnVal;
    }
  }
  lock_release(&fs_lock);
  return -1;
}

// read (int fd, void *buffer, unsigned size)
int read (void *esp) {
  esp = esp + 16;
  
  // hex_dump(esp, esp, 32, 1);
  if (!is_valid_pointer(esp, 12)) exit(-1);

  int fd = *(int*) esp;
  void *buffer = *(void**) (esp + 4);
  unsigned size = *(unsigned*) (esp + 8);

  // printf("reading %d, %p, %d\n", fd, buffer, size);

  // hex_dump(buffer, buffer, 32, 1);
  // if (!is_valid_pointer(buffer, size)) {
  //   printf("read exit here %p, %d\n", buffer, size);
  //   exit(-1);}

  lock_acquire(&fs_lock);
  struct file *file = NULL;
  struct list *fd_list = &thread_current()->fd_list;
  struct list_elem *e;
  for (e=list_begin(fd_list); e!=list_end(fd_list); e=list_next(e)) {
    struct fd_file *ff = list_entry(e, struct fd_file, elem);
    if (ff->fd == fd) {
      file = ff->file_ptr;
      break;
    }
  }

  if (file == NULL)
  {
    lock_release(&fs_lock);
    return -1;
  }
  void *upage = pg_round_down(buffer);
  for (int i = 0; i < size; i += PGSIZE)
  {
    struct spt_entry *entry_p = fetch_spt_entry(upage + i);
    entry_p->pinning = true;
    handle_page_fault(upage + i, NULL);
  }
  size = file_read(file, buffer, size);
  for (int i = 0; i < size; i += PGSIZE)
  {
    struct spt_entry *entry_p = fetch_spt_entry(upage + i);
    entry_p->pinning = false;
  }
  lock_release(&fs_lock);
  return size;
}

// (int fd, void *buffer, unsigned size)
int write (void *esp) {
  esp = esp + 16; // temporary

  if (!is_valid_pointer(esp, 12)) exit(-1);

  // hex_dump(esp, esp, 32, 1);
  int fd = *(int*) esp;
  void *buffer = *(void**) (esp + 4);
  unsigned size = *(unsigned*) (esp + 8);
  // printf("write started %d, %p, %d\n", fd, buffer, size);

  // if (!is_valid_pointer(buffer, size)) exit(-1);

  if (fd == 1) {
    putbuf(buffer, size);
    return size;
  }
  
  lock_acquire(&fs_lock);
  struct file *file = NULL;
  struct fd_file *ff_pick;
  struct list *fd_list = &thread_current()->fd_list;
  struct list_elem *e;
  for (e=list_begin(fd_list); e!=list_end(fd_list); e=list_next(e)) {
    struct fd_file *ff = list_entry(e, struct fd_file, elem);
    if (ff->fd == fd) {
      file = ff->file_ptr;
      ff_pick = ff;
      break;
    }
  }


  if (file == NULL) return -1;
  void *upage = pg_round_down(buffer);
  for (int i = 0; i < size; i += PGSIZE)
  {
    struct spt_entry *entry_p = fetch_spt_entry(upage + i);
    entry_p->pinning = true;
    handle_page_fault(upage + i, NULL);
  }
  if (thread_is_executables(ff_pick->file_name))
  {
    file_deny_write(ff_pick->file_ptr);
  }
  else
    file_allow_write(ff_pick->file_ptr);
  size = file_write(ff_pick->file_ptr, buffer, size);
  for (int i = 0; i < size; i += PGSIZE)
  {
    struct spt_entry *entry_p = fetch_spt_entry(upage + i);
    entry_p->pinning = false;
  }

  lock_release(&fs_lock);
  return size;
}

// void seek (int fd, unsigned position)
void seek (void *esp) {
  int fd = *(int*) (esp + 12);
  unsigned position = *(unsigned*) (esp + 16);

  // printf("sick %d %d\n", fd, position);
  
  lock_acquire(&fs_lock);
  struct list *fd_list = &thread_current()->fd_list;
  struct list_elem *e;
  for (e=list_begin(fd_list); e!=list_end(fd_list); e=list_next(e)) {
    struct fd_file *ff = list_entry(e, struct fd_file, elem);
    if (ff->fd == fd) {
      file_seek (ff->file_ptr, position);
      break;
    }
  }
  lock_release(&fs_lock);
}

// unsigned tell (int fd)
unsigned tell (void *esp) {
  int fd = *(int*) esp;

  lock_acquire(&fs_lock);
  struct list *fd_list = &thread_current()->fd_list;
  struct list_elem *e;
  for (e=list_begin(fd_list); e!=list_end(fd_list); e=list_next(e)) {
    struct fd_file *ff = list_entry(e, struct fd_file, elem);
    if (ff->fd == fd) {
      lock_release(&fs_lock);
      return file_tell(ff->file_ptr);
    }
  }
  lock_release(&fs_lock);
  return -1;
}

void close (void *esp) {
  int fd = *(int*) esp;

  struct list *fd_list = &thread_current()->fd_list;
  struct list_elem *e, *next;
  lock_acquire(&fs_lock);
  for (e=list_begin(fd_list); e!=list_end(fd_list); e=next) {
    struct fd_file *ff = list_entry(e, struct fd_file, elem);
    next = list_next(e);
    if (ff->fd == fd) {
      // printf("closing %d, %p\n\n", fd, ff->file_ptr);
      file_close(ff->file_ptr);
      list_remove(&ff->elem);
      free(ff);
      break;
    }
  }
  lock_release(&fs_lock);
}

void _close_all_fd (void) {
  struct list *fd_list = &thread_current()->fd_list;
  struct list_elem *e, *next;
  lock_acquire(&fs_lock);
  for (e=list_begin(fd_list); e!=list_end(fd_list); e=next) {
    struct fd_file *ff = list_entry(e, struct fd_file, elem);
    next = list_next(e);
    file_close(ff->file_ptr);
    list_remove(&ff->elem);
    free(ff);
  }
  lock_release(&fs_lock);
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