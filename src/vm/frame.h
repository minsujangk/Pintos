#include <stddef.h>
#include <list.h>
#include "threads/palloc.h"

struct list frame_table;

struct lock *f_lock;

struct frame_entry
{
    struct list_elem elem;
    struct thread *t;
    void *frame;
    int unused_cnt;
};

struct lock fe_Lock;

void finit(void);
void *falloc(enum palloc_flags f);
struct frame_entry *ffetch(void *frame);
bool evict(void);
void ffree_thread(struct thread *t);
void ffree(void *frame);