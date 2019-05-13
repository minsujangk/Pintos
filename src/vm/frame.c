#include "frame.h"
#include "page.h"
#include <stdio.h>
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

bool isFrameTableInitiating = false;

void *acquireFrameEntry(void);
struct FrameEntry *fetchFrameEntry(void *frame);
bool frameEntryOrder(struct list_elem *e_a, struct list_elem *e_b);

void initFrameTable(void)
{
    lock_init(&acquireLock);
    list_init(&frameTable);
    list_init(&recentFrameTable);
}

void *acquireFrameEntry(void)
{
    printf("acquire %s\n", thread_name());
    lock_acquire(&acquireLock);
    isFrameTableInitiating = true;
    // init here because first~
    void *fe_p = malloc(sizeof(struct FrameEntry));
    // if any putFrameEntry occurs here, omit it.

    isFrameTableInitiating = false;
    lock_release(&acquireLock);

    return fe_p;
}

void putFrameEntry(void *pages, size_t page_cnt)
{
    if (isFrameTableInitiating)
        return;
    printf("pushing FrameEntry %p, %d\n", pages, page_cnt);
    struct FrameEntry *entry_p = (struct FrameEntry *)acquireFrameEntry();
    entry_p->pages = pages;
    entry_p->pageCnt = page_cnt;
    list_insert_ordered(&frameTable, &entry_p->orderElem, frameEntryOrder, NULL);
    list_push_back(&recentFrameTable, &entry_p->elem);
}

bool frameEntryOrder(struct list_elem *e_a, struct list_elem *e_b)
{
    struct FrameEntry *t_a = list_entry(e_a, struct FrameEntry, elem);
    struct FrameEntry *t_b = list_entry(e_b, struct FrameEntry, elem);
    return t_a->pages < t_b->pages;
}

void deleteFrameEntry(void *frame)
{
    printf("deleting entry %p\n", frame);
    // 프로세스 등 죽을 때 사용.
    struct FrameEntry *entry_p = fetchFrameEntry(frame);
    if (entry_p != NULL)
    {
        list_remove(&entry_p->elem);
        list_remove(&entry_p->orderElem);
        free(entry_p);
    }
}

void useFrameEntry(struct FrameEntry *fe_p)
{
    // 꺼내서 맨 뒤에 넣기.
}

bool evictLRU(int pageCnt)
{
    // unaccessed 먼저.
    int consecCnt = 0;
    struct FrameEntry *last_entry_p;
    struct list_elem *last_e = NULL;
    struct list_elem *e;
    for (e = list_begin(&frameTable); e != list_end(&frameTable); e = list_next(e))
    {
        struct FrameEntry *entry_p = list_entry(e, struct FrameEntry, elem);
        if (!isRecentlyUsed(entry_p->pages))
        {
            if (last_e == NULL && consecCnt == 0)
            {
                last_e = e;
                last_entry_p = entry_p;
            }
            consecCnt = (entry_p->pages - last_entry_p->pages) / PGSIZE + entry_p->pageCnt;
        }
        else
        {
            last_e = NULL;
            consecCnt = 0;
        }
        if (consecCnt >= pageCnt)
        {
            printf("eviction unaccessed found %p to %p@%d\n", last_entry_p->pages, entry_p->pages, entry_p->pageCnt);
            struct list_elem *handle;
            for (handle = last_e; handle != list_next(e); handle = list_next(handle))
            {
                struct FrameEntry *entry_p = list_entry(handle, struct FrameEntry, elem);
                printf("eviction unaccessed %p %d\n", entry_p->pages, entry_p->pageCnt);
                dropPageMultiple(entry_p->pages, entry_p->pageCnt);
            }
            return true;
        }
    }

    // unaccessed로 해결 안 되면 걍 막 빼
    last_entry_p = NULL;
    last_e = NULL;
    consecCnt = 0;
    for (e = list_begin(&frameTable); e != list_end(&frameTable); e = list_next(e))
    {
        struct FrameEntry *entry_p = list_entry(e, struct FrameEntry, elem);
        if (last_e == NULL && consecCnt == 0)
        {
            last_e = e;
            last_entry_p = entry_p;
        }
        consecCnt = (entry_p->pages - last_entry_p->pages) / PGSIZE + entry_p->pageCnt;

        if (consecCnt >= pageCnt)
        {
            printf("eviction any found %p to %p@%d\n", last_entry_p->pages, entry_p->pages, entry_p->pageCnt);
            struct list_elem *handle;
            for (handle = last_e; handle != list_next(e); handle = list_next(handle))
            {
                struct FrameEntry *entry_p = list_entry(handle, struct FrameEntry, elem);
                printf("eviction any %p %d\n", entry_p->pages, entry_p->pageCnt);
                dropPageMultiple(entry_p->pages, entry_p->pageCnt);
            }
            return true;
        }
    }

    return false;
}

struct FrameEntry *fetchFrameEntry(void *frame)
{
    struct list_elem *e;
    for (e = list_begin(&recentFrameTable); e != list_end(&recentFrameTable); e = list_next(e))
    {
        struct FrameEntry *entry_p = list_entry(e, struct FrameEntry, elem);
        if (entry_p->pages == frame)
        {
            return entry_p;
        }
    }
    return NULL;
}