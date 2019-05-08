#include "vm/frame.h"
#include "threads/malloc.h"
#include "threads/synch.h"

bool isFrameTableInitiating = false;

void *acquireFrameEntry();

void initFrameTable()
{
    lock_init(&acquireLock);
    list_init(&frameTable);
}

void *acquireFrameEntry()
{
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
    struct FrameEntry *entry_p = (struct FrameEntry *)acquireFrameEntry();
    entry_p->pages = pages;
    entry_p->pageCnt = page_cnt;
    list_push_front(&frameTable, entry_p);
}

void deleteFrameEntry()
{
    // 프로세스 등 죽을 때 사용.
}

void useFrameEntry(struct FrameEntry* fe_p)
{
    // 꺼내서 맨 뒤에 넣기.
}

void evictLRU() {
    // LRU 기준 제일 뒤에 있는 거
}