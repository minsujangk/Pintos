#include "frame.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include <debug.h>
#include <stdio.h>

void _ffree(struct frame_entry *entry_p);

void finit(void)
{
    list_init(&frame_table);
    lock_init(&f_lock);
    lock_init(&evict_lock);
}

void *falloc(enum palloc_flags f, struct spt_entry *spte_p)
{
    void *frame = palloc_get_page(f);
    // printf("fallocing %p\n", frame);

    while (frame == NULL)
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
    entry_p->spt_entry = spte_p;

    // printf("fallocing complete %p\n", frame);
    lock_acquire(&f_lock);
    list_push_back(&frame_table, &entry_p->elem);
    lock_release(&f_lock);
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
    // printf("starting evction\n");
    struct frame_entry *entry_to_evict = NULL;
    int max_count = -1;
    struct list_elem *e;
    bool is_dirty;

    // lock_acquire(&evict_lock);
    lock_acquire(&f_lock);
    while (entry_to_evict == NULL)
    {
        for (e = list_begin(&frame_table); e != list_end(&frame_table);
             e = list_next(e))
        {
            struct frame_entry *entry_p = list_entry(e, struct frame_entry, elem);
            bool is_last_dirty = false;
            // printf("searchign %p\n", entry_p->frame);
            struct spt_entry *spte_p = entry_p->spt_entry;
            if (pagedir_is_accessed(spte_p->thread->pagedir, spte_p->upage))
            {
                pagedir_set_accessed(spte_p->thread->pagedir, spte_p->upage, false);
            }
            else
            {
                entry_p->unused_cnt++;
            }

            if (pagedir_is_dirty(spte_p->thread->pagedir, spte_p->upage))
            {
                is_last_dirty = true;
            }
            else
            {
                entry_p->unused_cnt++;
            }
            if (spte_p->pinning)
                continue;

            if (entry_p->unused_cnt > max_count)
            {
                is_dirty = is_last_dirty;
                entry_to_evict = entry_p;
                max_count = entry_p->unused_cnt;
            }
            entry_p->unused_cnt = 0;
        }
    }
    // printf("eviction %p: %p\n", entry_to_evict, entry_to_evict->spt_entry->upage);
    // entry_to_evict->spt_entry->pinning = true;
    entry_to_evict->spt_entry->type = IN_SWAP;
    write_back(entry_to_evict->spt_entry, entry_to_evict->frame, is_dirty);
    list_remove(&entry_to_evict->elem);
    lock_release(&f_lock);
    _ffree(entry_to_evict);
    // entry_to_evict->spt_entry->pinning = false;
    // printf("eviction complete\n");
    // lock_release(&evict_lock);
    return true;
}

void ffree_thread(struct thread *t)
{
    lock_acquire(&f_lock);
    struct list_elem *e;
    struct list_elem *e_next;
    for (e = list_begin(&frame_table); e != list_end(&frame_table);)
    {
        struct frame_entry *entry_p = list_entry(e, struct frame_entry, elem);
        e = list_next(e);
        if (entry_p->t == t)
        {
            list_remove(&entry_p->elem);
            _ffree(entry_p);
        }
    }
    lock_release(&f_lock);
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
            lock_acquire(&f_lock);
            list_remove(&entry_p->elem);
            lock_release(&f_lock);
            _ffree(entry_p);
            return;
        }
    }
}

void _ffree(struct frame_entry *entry_p)
{
    // printf("freeing %p\n", entry_p->frame);
    palloc_free_page(entry_p->frame);
    free(entry_p);
    // printf("free complete %p\n", entry_p->frame);
}