#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "fixed_pointer.h"
#include "synch.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

struct child_status {
  int parent_pid;
  int child_pid;
  int exit_status;
  struct semaphore sema_start;
  struct list_elem elem;
};
/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    struct list holding_locks;
    struct lock *waiting_lock;
    int orig_priority;

    int nice;
    fp recent_cpu_fp;

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    char executable_name[128];
    char command_line[128];
    struct semaphore process_lock;
    struct list fd_list;
    struct list_elem elem_all;
    /**
     * 구현 방향에 대하여.
     * 1. 따로 struct child_status를 둘 때.
     * 1-1. 이걸 담을 list를 thread 내부에 child_list로 둘 경우 child가 죽었을 때 (thread_exit())
     *      thread 객체를 free하지 않고 관리해야 되는데 (wait에서 쓰기 위해)
     *      비효율적일 것 같아서 패스.
     * 1-2. 이걸 global 변수. parent_child_list에 담아 사용하는 방식.
     *      내 입장에선 가장 편해 보였음.
     * 2. thread 내부에 exit_status, child_thread_list 등을 둬서 관리. 
     *    1-1과 같은 이유로 패스.
     */

    struct list spage_table;
    struct lock spt_lock; 
    struct file *exe_file;
    struct list mm_list;
    void *sys_esp;

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);


struct thread_sleep_info {
  struct thread* t;
  int64_t target_ticks;
  struct list_elem elem;
};

void thread_sleep(int64_t target_ticks);
void thread_wake(struct thread_sleep_info *info);
void thread_check_sleepers();
void thread_check_priority (int priority) ;
bool thread_compare_priority(struct list_elem *e_a, struct list_elem *e_b) ;
int thread_locks_max_priority (struct thread *t);
bool thread_compare_sleep(struct list_elem *e_a, struct list_elem *e_b);

struct list *mlfqs_queue;
fp thread_load_avg_fp;

/* Project 2 */
struct semaphore* thread_get_process_lock(tid_t tid);
bool thread_is_executables (char *file_name);

struct fd_file {
  int fd;
  struct file *file_ptr;
  struct list_elem elem;
  char file_name[128];
};

struct mm_item {
  int mapid;
  struct file *file_ptr;
  char file_name[128];  
  struct list_elem elem;
};

struct list thread_all;
struct list parent_child_list;


#endif /* threads/thread.h */
