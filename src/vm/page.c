#include "page.h"
#include "frame.h"
#include "swap.h"
#include <stdio.h>
#include "userprog/pagedir.h"
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

void remove_spt_entry(struct thread *t)
{
    struct list_elem *e;
    for (e = list_begin(&t->spage_table); e != list_end(&t->spage_table);)
    {
        struct spt_entry *entry_p = list_entry(e, struct spt_entry, elem);
        e = list_next(e);
        if (entry_p->thread == t)
            list_remove(&entry_p->elem);
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
    uint8_t *addr = pg_round_down(upage);
    // printf("handle pf %p\n", addr);
    struct spt_entry *entry_p = fetch_spt_entry(addr);

    if (entry_p->type == IN_FILE)
    {
        load_spte_file(entry_p);
    }
    else if (entry_p->type == IN_SWAP)
    {
        load_spte_swap(entry_p);
    }
    else if (entry_p->type == IN_MMAP)
    {
        //mmap
    }
    else if (entry_p == NULL)
    {
        grow_stack(addr - PGSIZE);
    }

    return true;
}
void load_spte_swap(struct spt_entry *entry_p)
{
    /* Get a page of memory. */
    uint8_t *kpage = falloc(PAL_USER);

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
    uint8_t *kpage = falloc(PAL_USER);

    /* Load this page. */
    if (file_read_at(entry_p->file, kpage, entry_p->read_bytes, entry_p->offset) !=
        (int)entry_p->read_bytes)
    {
        ffree(kpage);
        return;
    }
    memset(kpage + entry_p->read_bytes, 0, entry_p->zero_bytes);

    /* Add the page to the process's address space. */
    if (!install_page(entry_p->upage, kpage,true))
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
    uint8_t *kpage = falloc(PAL_USER | PAL_ZERO);
    struct thread *t = thread_current();

    struct spt_entry *entry_p = malloc(sizeof(struct spt_entry));
    entry_p->upage = upage;
    entry_p->type = STACK;
    entry_p->thread = t;

    lock_acquire(&t->spt_lock);
    list_push_back(&t->spage_table, &entry_p->elem);
    lock_release(&t->spt_lock);

    if (!install_page(upage, kpage, true))
    {
        printf("stack fail\n");
    }
}