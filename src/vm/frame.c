#include "frame.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <debug.h>

void _ffree(struct frame_entry *entry_p);

void finit(void)
{
    list_init(&frame_table);
    lock_init(&f_lock);
}

void *falloc(enum palloc_flags f)
{
    void *frame = palloc_get_page(f);
    // printf("fallocing %p\n", frame);

    if (frame == NULL)
    {
        bool evictSuccess = evict();
        if (!evictSuccess)
            PANIC("eviction fail");

        // try again
        frame = palloc_get_page(f);
    }

    struct frame_entry *entry_p = malloc(sizeof(struct frame_entry));
    entry_p->frame = frame;
    entry_p->t = thread_current();

    lock_acquire(&f_lock);
    list_push_back(&frame_table, &entry_p->elem);
    lock_release(&f_lock);
    // printf("fallocing complete\n");
    return frame;
}

struct frame_entry *ffetch(void *frame)
{
    struct list_elem *e;
    for (e = list_begin(&frame_table); e != list_end(&frame_table);
         e = list_next(e))
    {
        struct frame_entry *entry_p = list_entry(e, struct frame_entry, elem);
        if (entry_p->frame == frame)
            return entry_p;
    }
    return NULL;
}

bool evict(void)
{
}

void ffree_thread(struct thread *t)
{
    struct list_elem *e;
    struct list_elem *e_next;
    for (e = list_begin(&frame_table); e != list_end(&frame_table);)
    {
        struct frame_entry *entry_p = list_entry(e, struct frame_entry, elem);
        e = list_next(e);
        if (entry_p->t == t)
        {
            _ffree(entry_p);
        }
    }
}

void ffree(void *frame)
{
    struct list_elem *e;
    struct list_elem *e_next;
    for (e = list_begin(&frame_table); e != list_end(&frame_table);)
    {
        struct frame_entry *entry_p = list_entry(e, struct frame_entry, elem);
        e = list_next(e);
        if (entry_p->frame == frame)
        {
            _ffree(entry_p);
            return;
        }
    }
}

void _ffree(struct frame_entry *entry_p)
{
    lock_acquire(&f_lock);
    list_remove(&entry_p->elem);
    lock_release(&f_lock);
    free(entry_p);
    palloc_free_page(entry_p->frame);
}