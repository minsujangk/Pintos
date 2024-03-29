#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif


/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;
static struct list sleep_list; // a list for struct thread_sleep_info

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */

/* This is 2016 spring cs330 skeleton code */

void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&sleep_list);
  list_init (&parent_child_list);
  list_init (&thread_all);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();

  thread_load_avg_fp = 0;

  // if (thread_mlfqs) {
  //   struct list temp_queue[64];
  //   int i = 0;
  //   for (i=0; i<64; i++) {
  //     list_init(&(temp_queue[i]));
  //   }
  //   mlfqs_queue = temp_queue;
  // }
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  // sema_down(&t->process_lock);

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;

  list_push_back(&thread_all, &t->elem_all);

  /* Add to run queue. */
  thread_unblock (t);

  if (priority > thread_get_priority())
    thread_yield();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  // if (thread_mlfqs)
  //   list_push_back(&mlfqs_queue[t->priority], &t->elem);
  // else
  if (thread_mlfqs)
    thread_nice_refresh_priority(t);
  list_insert_ordered(&ready_list, &t->elem, thread_compare_priority, NULL);
  // list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

void thread_check_priority (int priority) {
  if (thread_current () == idle_thread) return;
  if (priority > thread_get_priority ()) {
    thread_yield ();
  }
}

void thread_sleep(int64_t target_ticks)
{
  enum intr_level old_level;
  old_level = intr_disable ();

  struct thread_sleep_info info;
  info.t = thread_current();
  info.target_ticks = target_ticks;
  list_insert_ordered(&sleep_list, &info.elem, thread_compare_sleep, NULL);
  // list_push_back(&sleep_list, &info.elem);
  thread_block();
  
  intr_set_level (old_level);
}

bool thread_compare_sleep(struct list_elem *e_a, struct list_elem *e_b) {
  struct thread_sleep_info *t_a = list_entry(e_a, struct thread_sleep_info, elem);
  struct thread_sleep_info *t_b = list_entry(e_b, struct thread_sleep_info, elem);
  return t_a->target_ticks < t_b->target_ticks;
}

void thread_wake(struct thread_sleep_info *info)
{
  thread_unblock(info->t);
  list_remove(&info->elem);
}

void thread_check_sleepers()
{
  struct list_elem *e, *next;
  for (e = list_begin(&sleep_list); e != list_end(&sleep_list); e = next)
  {
    next = list_next(e);
    struct thread_sleep_info *info = list_entry(e, struct thread_sleep_info, elem);
    if (timer_ticks() >= info->target_ticks)
    {
      thread_wake(info);
    } else {
      break;
    }
  }
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Just set our status to dying and schedule another process.
     We will be destroyed during the call to schedule_tail(). */
  intr_disable ();
  sema_up(&thread_current()->process_lock);
  list_remove(&thread_current()->elem_all);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *curr = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (curr != idle_thread) 
    // list_push_back (&ready_list, &curr->elem);
    
    // if(thread_mlfqs)
    //   list_push_back(&mlfqs_queue[curr->priority], &curr->elem);
    // else
      list_insert_ordered(&ready_list, &curr->elem, thread_compare_priority, NULL);
  curr->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

struct semaphore*
thread_get_process_lock(tid_t tid){
  struct list_elem *e;

  for (e=list_begin(&thread_all); e!=list_end(&thread_all); e=list_next(e)) {
    struct thread *t = list_entry(e, struct thread, elem_all);
    if (t->tid == tid) return &t->process_lock;
  }

  // maybe the thread had been already finished;;
  return NULL;
}

int
thread_ready_max_priority () {
  return list_entry(list_front(&ready_list), struct thread, elem)->priority;
}

int
thread_locks_max_priority (struct thread *t) {
  struct list_elem *e;
  int max_priority = -1;
  for (e=list_begin(&t->holding_locks); e!=list_end(&t->holding_locks); e=list_next(e)) {
    struct lock *lock = list_entry(e, struct lock, elem);
    if(list_empty(&lock->semaphore.waiters)) continue;
    int lock_max_priority = list_entry(list_front(&lock->semaphore.waiters), struct thread, elem)->priority;
    if (max_priority < lock_max_priority) {
      max_priority = lock_max_priority;
    }
  }
  return max_priority;
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  if (thread_mlfqs) return;

  int locks_max_priority = thread_locks_max_priority(thread_current());
  if (locks_max_priority < new_priority)
    thread_current ()->priority = new_priority;
  thread_current ()->orig_priority = new_priority;
  if (!list_empty(&ready_list))
    thread_check_priority(thread_ready_max_priority ());
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

bool thread_compare_priority(struct list_elem *e_a, struct list_elem *e_b) {
  struct thread *t_a = list_entry(e_a, struct thread, elem);
  struct thread *t_b = list_entry(e_b, struct thread, elem);
  return t_a->priority > t_b->priority;
}

int thread_ready_count() {
  int count = list_size(&ready_list);
  if (thread_current() != idle_thread) count++;
  return count;
}

void
thread_refresh_mlfqs(int64_t ticks) {
  if (thread_current() != idle_thread)
    thread_current()->recent_cpu_fp = fadd_int(thread_current()->recent_cpu_fp, 1);
  if (ticks%100 == 0) {
    thread_all_refresh_recent_cpu();
    thread_refresh_load_avg();
  }
  if (ticks%4 == 0) {
    thread_nice_all_refresh_priority();
  }
}

struct thread *thread_mlfqs_pop() {
  int i=0;
  for (i=63; i>=0; i--) {
    struct list *queue = &mlfqs_queue[i];
    if(!list_empty(queue)) {
      return list_entry(list_pop_front(queue), struct thread, elem);
    }
  }
  return NULL;
}

void
thread_refresh_load_avg() {
  // printf("load avg was=%d / ", fp_to_int_nearest(thread_load_avg_fp));
  // printf("avg was=%d, multiplied avg=%d / ", fp_to_int_nearest(thread_load_avg_fp), fp_to_int_nearest(fmul(thread_load_avg_fp, fdiv_int(int_to_fp(59), 60))));
  // printf("thread_count=%d, ready count=%d / ", thread_ready_count(), fp_to_int_nearest(fmul_int(fdiv_int(int_to_fp(1), 60), thread_ready_count())));
  thread_load_avg_fp = fadd(fmul(thread_load_avg_fp, fdiv_int(int_to_fp(59), 60)), fmul_int(fdiv_int(int_to_fp(1), 60), thread_ready_count()));

  // printf("load avg is now=%d \n", fp_to_int_nearest(thread_load_avg_fp));
}

void
thread_all_refresh_recent_cpu() {
  struct list_elem *e;
  for (e=list_begin(&ready_list); e!=list_end(&ready_list); e=list_next(e)) {
    thread_refresh_recent_cpu(list_entry(e, struct thread, elem));
  }
  
  for (e=list_begin(&sleep_list); e!=list_end(&sleep_list); e=list_next(e)) {
    thread_refresh_recent_cpu(list_entry(e, struct thread_sleep_info, elem)->t);
  }
  if (thread_current() != idle)
    thread_refresh_recent_cpu(thread_current());
}

void
thread_refresh_recent_cpu(struct thread *t) {
  fp decay_fp = fdiv(fmul_int(thread_load_avg_fp, 2), fadd_int(fmul_int(thread_load_avg_fp, 2), 1));
  fp load_twice = fmul_int(thread_load_avg_fp, 2);
  // printf("%s: prev recent cpu=%d; load twice=%d / ", t->name, fp_to_int_nearest(t->recent_cpu_fp), fp_to_int_nearest(load_twice));
  // fp recent_cpu_off = fdiv(fmul(t->recent_cpu_fp, load_twice), fadd_int(load_twice, 1));
  fp recent_cpu_off = fmul(t->recent_cpu_fp, fdiv(load_twice, fadd_int(load_twice, 1)));
  // printf("middle value=%d, %d /", fp_to_int_nearest(fmul(t->recent_cpu_fp, load_twice)), fp_to_int_nearest(load_twice));
  t->recent_cpu_fp = fadd_int(recent_cpu_off, t->nice);
  // printf("%d, recent_cpu=%d\n", 
  //   fp_to_int_nearest(recent_cpu_off * 100),
  //   fp_to_int_nearest(t->recent_cpu_fp));
}

void
thread_nice_all_refresh_priority() {
  struct list_elem *e;
  
  for (e=list_begin(&ready_list); e!=list_end(&ready_list); e=list_next(e)) {
    thread_nice_refresh_priority(list_entry(e, struct thread, elem));
  }

  if (thread_current() != idle_thread)
    thread_nice_refresh_priority(thread_current());

  list_sort(&ready_list, thread_compare_priority, NULL);


  if (!list_empty(&ready_list) && thread_get_priority() < thread_ready_max_priority())
    if (intr_context()) intr_yield_on_return();
    else thread_yield();
}

void
thread_nice_refresh_priority(struct thread *t) {
  t->priority = PRI_MAX - fp_to_int_nearest(fdiv_int(t->recent_cpu_fp, 4)) - (t->nice*2);

  // adjust range
  if (t->priority < PRI_MIN) t->priority = PRI_MIN;
  else if (t->priority > PRI_MAX) t->priority = PRI_MAX;

  // printf("%s: p=%d, n=%d\n", t->name, t->priority, fp_to_int_nearest(fdiv_int(t->recent_cpu_fp, 4)));
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) 
{
  /* Not yet implemented. */
  thread_current()->nice = nice;

  thread_refresh_recent_cpu(thread_current());
  thread_nice_refresh_priority(thread_current());
  
  list_sort(&ready_list, thread_compare_priority, NULL);
  
  // if (!list_empty(&ready_list) && thread_get_priority() < thread_ready_max_priority())
  //   thread_yield();
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  /* Not yet implemented. */
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  /* Not yet implemented. */
  return fp_to_int_nearest(fmul_int(thread_load_avg_fp, 100));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  /* Not yet implemented. */
  return fp_to_int_nearest(fmul_int(thread_current()->recent_cpu_fp, 100));
}

bool thread_is_executables (char *file_name) {
  struct list_elem *e;

  for (e=list_begin(&thread_all); e!=list_end(&thread_all); e=list_next(e)) {
    struct thread *t = list_entry(e, struct thread, elem_all);
    if (strcmp(t->executable_name, file_name) == 0) return true;
  }

  return false;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);
                    
  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Since `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  strlcpy (t->command_line, name, strlen(name)+1);
  t->stack = (uint8_t *) t + PGSIZE;
  if (!thread_mlfqs) {
    t->priority = priority;
    t->orig_priority = priority;
  } else {
    t->priority = 0;
  }
  t->waiting_lock = NULL;
  t->magic = THREAD_MAGIC;
  sema_init(&t->process_lock, 0);
  list_init(&t->fd_list);
  list_init(&t->mm_list);


  t->nice = 0;
  t->recent_cpu_fp = 0;

  list_init(&t->holding_locks);
  list_init(&t->spage_table);
  lock_init(&t->spt_lock);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  // if (!thread_mlfqs) {
    if (list_empty (&ready_list))
      return idle_thread;
    else {
      struct thread *next_thread = list_entry(list_pop_front(&ready_list), struct thread, elem);
      return next_thread;
    }
  // } else {
  //   struct thread *next_thread = thread_mlfqs_pop();
  //   if (next_thread == NULL) return idle_thread;
  //   else return next_thread;
  // }
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev) 
{
  struct thread *curr = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  curr->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != curr);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   
   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void) 
{
  struct thread *curr = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (curr->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (curr != next)
    prev = switch_threads (curr, next);
  schedule_tail (prev); 

  // printf("thread sch %s\n", thread_name(), thread_ready_count());
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
