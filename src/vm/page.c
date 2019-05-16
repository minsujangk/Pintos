#include "frame.h"
#include "swap.h"
#include <stdio.h>
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

static bool install_page(void *upage, void *kpage, bool writable);

void add_spt_entry_file(struct file *file, off_t ofs, uint8_t *upage,
                        uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    // file_seek(file, ofs);
    while (read_bytes > 0 || zero_bytes > 0)
    {
        /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct thread *t = thread_current();

        struct spt_entry *entry_p = malloc(sizeof(struct spt_entry));
        entry_p->file = file;
        entry_p->offset = ofs;
        entry_p->upage = upage;
        entry_p->read_bytes = page_read_bytes;
        entry_p->zero_bytes = page_zero_bytes;
        entry_p->thread = t;
        entry_p->type = IN_FILE;
        entry_p->pinning = false;
        entry_p->writeable = writable;

        // printf("add entry %p: %d\n", upage, writable);

        lock_acquire(&t->spt_lock);
        list_push_back(&t->spage_table, &entry_p->elem);
        lock_release(&t->spt_lock);

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += PGSIZE;
    }
    return true;
}

bool add_spt_entry_mmap(struct file *file, off_t ofs, uint8_t *upage,
                        uint32_t read_bytes, uint32_t zero_bytes, bool writable,
                        int mapid)
{
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    // file_seek(file, ofs);
    while (read_bytes > 0 || zero_bytes > 0)
    {
        /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct thread *t = thread_current();

        // mmap-overlap
        struct spt_entry *old_entry = fetch_spt_entry(upage);
        if (old_entry != NULL)
        {
            remove_mmap_spt_entry(mapid);
            return false;
        }

        struct spt_entry *entry_p = malloc(sizeof(struct spt_entry));
        entry_p->file = file;
        entry_p->offset = ofs;
        entry_p->upage = upage;
        entry_p->read_bytes = page_read_bytes;
        entry_p->zero_bytes = page_zero_bytes;
        entry_p->thread = t;
        entry_p->type = IN_MMAP;
        entry_p->pinning = false;
        entry_p->mapid = mapid;
        entry_p->writeable = writable;

        lock_acquire(&t->spt_lock);
        list_push_back(&t->spage_table, &entry_p->elem);
        lock_release(&t->spt_lock);

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += PGSIZE;
    }
    return true;
}

void remove_mmap_spt_entry(int mapid)
{
    struct thread *t = thread_current();
    struct list_elem *e;
    for (e = list_begin(&t->spage_table); e != list_end(&t->spage_table);)
    {
        struct spt_entry *entry_p = list_entry(e, struct spt_entry, elem);
        e = list_next(e);
        if (entry_p->type == IN_MMAP && entry_p->mapid == mapid)
        {
            if (pagedir_is_dirty(t->pagedir, entry_p->upage))
                write_back(entry_p, NULL, true);
            else
                pagedir_clear_page(t->pagedir, entry_p->upage);
            ffree(pagedir_get_page(t->pagedir, entry_p->upage));
            list_remove(&entry_p->elem);

            free(entry_p);
        }
    }
}

void remove_spt_entry(struct thread *t)
{
    ffree_thread(t);
    struct list_elem *e;
    for (e = list_begin(&t->spage_table); e != list_end(&t->spage_table);)
    {
        struct spt_entry *entry_p = list_entry(e, struct spt_entry, elem);
        e = list_next(e);
        // ffree(pagedir_get_page(t->pagedir, entry_p->upage));
        pagedir_clear_page(t->pagedir, entry_p->upage);

        free(entry_p);
    }
}

struct spt_entry *fetch_spt_entry(void *upage)
{
    struct thread *t = thread_current();
    struct list_elem *e;
    for (e = list_begin(&t->spage_table); e != list_end(&t->spage_table); e = list_next(e))
    {
        struct spt_entry *entry_p = list_entry(e, struct spt_entry, elem);
        if (entry_p->upage == upage)
            return entry_p;
    }
    return NULL;
}

bool handle_page_fault(void *upage, void *esp)
{
    void *addr = pg_round_down(upage);
    struct spt_entry *entry_p = fetch_spt_entry(addr);
    // printf("handle pf %p, %p, %d, %p\n", upage, esp, addr > esp-400*PGSIZE, entry_p);
    if (entry_p == NULL)
    {
        // printf("handle pf1\n");
        if (upage >= esp - 32)
            grow_stack(addr);
        else
            return false;
    }
    else if (entry_p->type == IN_FILE)
    {
        // printf("handle pf2\n");
        entry_p->pinning = true;
        load_spte_file(entry_p);
        entry_p->pinning = false;
    }
    else if (entry_p->type == IN_SWAP)
    {
        // printf("handle pf3\n");
        entry_p->pinning = true;
        load_spte_swap(entry_p);
        entry_p->pinning = false;
    }
    else if (entry_p->type == IN_MMAP)
    {
        // printf("handle pf4\n");
        //mmap
        entry_p->pinning = true;
        load_spte_file(entry_p);
        entry_p->pinning = false;
    }
    else if (entry_p != NULL)
    {
        // printf("handle pf5\n");
        entry_p->pinning = true;
        load_spte_zero(entry_p);
        entry_p->pinning = false;
    }

    return true;
}

void load_spte_zero(struct spt_entry *entry_p)
{
    /* Get a page of memory. */
    uint8_t *kpage = falloc(PAL_USER, entry_p);

    memset(kpage, 0, PGSIZE);

    if (!install_page(entry_p->upage, kpage, true))
    {
        ffree(kpage);
        return;
    }
}

void load_spte_swap(struct spt_entry *entry_p)
{
    /* Get a page of memory. */
    uint8_t *kpage = falloc(PAL_USER, entry_p);

    load_swap(kpage, entry_p->swap);
    entry_p->swap = NULL;

    if (!install_page(entry_p->upage, kpage, true))
    {
        ffree(kpage);
        return;
    }
}

void load_spte_file(struct spt_entry *entry_p)
{
    /* Get a page of memory. */
    uint8_t *kpage = falloc(PAL_USER, entry_p);

    // lock_acquire(&fs_lock);
    /* Load this page. */
    if (file_read_at(entry_p->file, kpage, entry_p->read_bytes, entry_p->offset) !=
        (int)entry_p->read_bytes)
    {
        // lock_release(&fs_lock);
        ffree(kpage);
        return;
    }

    // lock_release(&fs_lock);
    memset(kpage + entry_p->read_bytes, 0, entry_p->zero_bytes);

    /* Add the page to the process's address space. */
    if (!install_page(entry_p->upage, kpage, entry_p->writeable))
    {
        ffree(kpage);
        return;
    }
}
static bool
install_page(void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
     address, then map our page there. */
    return (pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable));
}

void grow_stack(void *upage)
{
    // printf("grow stack %p\n", upage);
    struct spt_entry *entry_p = malloc(sizeof(struct spt_entry));

    uint8_t *kpage = falloc(PAL_USER | PAL_ZERO, entry_p);
    struct thread *t = thread_current();

    entry_p->upage = upage;
    entry_p->type = STACK;
    entry_p->thread = t;
    entry_p->writeable = true;

    lock_acquire(&t->spt_lock);
    list_push_back(&t->spage_table, &entry_p->elem);
    lock_release(&t->spt_lock);

    if (!install_page(upage, kpage, true))
    {
        //printf("stack fail\n");
    }
}

void write_back(struct spt_entry *entry_p, void *kpage, bool is_dirty)
{
    if (entry_p->type == IN_FILE && !is_dirty)
    {
    }
    else if (entry_p->type == IN_MMAP)
    {
        //mmap
        file_write_at(entry_p->file, entry_p->upage,
                      PGSIZE, entry_p->offset);
    }
    else
    {
        entry_p->type = IN_SWAP;
        entry_p->swap = save_swap(kpage);
        // printf("write back to swap %p: %d\n", entry_p->upage, entry_p->swap->swap_idx);
    }
    pagedir_clear_page(entry_p->thread->pagedir, entry_p->upage);
}