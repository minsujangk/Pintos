#include "page.h"
#include "swap.h"
#include <list.h>
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include <string.h>

struct SupPageTableEntry *fetchEntry(void *upage);

void initSupPageTable(void)
{
    list_init(&supPageTable);
}

void addSupPageTableEntry(tid_t processId, void *upage, void *frame)
{
    printf("adding p%d %p %p\n", processId, upage, frame);
    struct SupPageTableEntry *entry_p =
        (struct SupPageTableEntry *)malloc(sizeof(struct SupPageTableEntry));
    entry_p->pagedir = thread_current()->pagedir;
    entry_p->upage = upage;
    entry_p->frame = frame;
    entry_p->processId = processId;
    entry_p->mode = LOAD_SWAP;

    // struct list *supPageTable = thread_get_sup_pagetable(processId);
    list_push_front(&supPageTable, &entry_p->elem);
}

void setFileSupPageTableEntry(void *upage, int file_offset, char *file_name)
{
    printf("file set %p, %d, %s\n", upage, file_offset, file_name);
    struct SupPageTableEntry *entry_p = fetchEntry(upage);
    if (entry_p != NULL)
    {
        entry_p->mode = LOAD_FILE;
        entry_p->file = (char *)malloc(strlen(file_name) + 1);
        entry_p->fileOffset = file_offset;
        strlcpy(entry_p->file, file_name, strlen(file_name) + 1);
    }
}

bool dropPage(void *frame)
{
    return dropPageMultiple(frame, 1);
}

bool dropPageMultiple(void *frame, int pageCnt)
{
    printf("dropping %p, %d\n", frame, pageCnt);
    writeBack(frame, pageCnt);

    palloc_free_multiple(frame, pageCnt);
    return false;
}

void writeBack(void *frame, int pageCnt)
{
    printf("start write back %p %d %s\n", frame, pageCnt, thread_name());
    struct SupPageTableEntry *entry_p = NULL;
    struct list_elem *e;
    for (e = list_begin(&supPageTable); e != list_end(&supPageTable); e = list_next(e))
    {
        entry_p = list_entry(e, struct SupPageTableEntry, elem);
        if (entry_p->frame == frame)
            break;
    }
    if (entry_p == NULL)
        return;
    if (pagedir_is_dirty(entry_p->pagedir, entry_p->upage))
    {
        // write back
        if (entry_p->mode == LOAD_FILE)
        {
            // load file segment
            struct file *file = filesys_open(entry_p->file);
            file_write_at(file, entry_p->upage, pageCnt * PGSIZE, entry_p->fileOffset);
            printf("writing back %p: @file %d\n", entry_p->upage, entry_p->fileOffset);

            file_close(file);
        }
    }
    if (entry_p->mode == LOAD_SWAP)
    {
        struct SwapEntry *swapEntry = saveSwap(entry_p->upage, pageCnt);
        entry_p->swapEntry = swapEntry;
        entry_p->mode = LOAD_SWAP;
        printf("writing back %p: @swap %d\n", entry_p->upage, swapEntry->secList[0]);
    }
    entry_p->frame = NULL;

    pagedir_clear_page(entry_p->pagedir, entry_p->upage);
    printf("finished writing back %p %d %s\n", frame, pageCnt, thread_name());
}

void *handlePageFault(void *fault_addr)
{
    struct SupPageTableEntry *entry_p = fetchEntry(fault_addr);
    if (entry_p == NULL)
    {
        printf("page fault null\n");
        // is an error
        return NULL;
    }
    void *return_addr;

    if (entry_p->mode == LOAD_FILE)
    {
        // load file segment
        struct file *file = filesys_open(entry_p->file);
        off_t filesize = file_length(file);
        return_addr = palloc_get_multiple(PAL_USER | PAL_ZERO, 1);
        if (return_addr != NULL)
            file_read_at(file, return_addr, PGSIZE, entry_p->fileOffset);
    }
    else
    {
        return_addr = palloc_get_multiple(PAL_USER | PAL_ZERO, entry_p->swapEntry->swapCnt / 4);
        if (entry_p->mode == LOAD_SWAP)
        {
            loadSwap(return_addr, entry_p->swapEntry);
        }
    }
    entry_p->frame = return_addr;
    return return_addr;
}

struct SupPageTableEntry *fetchEntry(void *upage)
{
    // struct list *supPageTable = thread_get_sup_pagetable(thread_tid());

    struct list_elem *e;
    for (e = list_begin(&supPageTable); e != list_end(&supPageTable); e = list_next(e))
    {
        struct SupPageTableEntry *entry_p = list_entry(e, struct SupPageTableEntry, elem);
        if (entry_p->upage == upage)
            return entry_p;
    }

    return NULL;
}

bool isRecentlyUsed(void *frame)
{
    struct SupPageTableEntry *entry_p = NULL;
    struct list_elem *e;
    for (e = list_begin(&supPageTable); e != list_end(&supPageTable); e = list_next(e))
    {
        entry_p = list_entry(e, struct SupPageTableEntry, elem);
        if (entry_p->frame == frame)
            break;
    }
    if (entry_p == NULL)
        return false;

    return pagedir_is_accessed(entry_p->pagedir, entry_p->upage) ||
           pagedir_is_dirty(entry_p->pagedir, entry_p->upage);
}