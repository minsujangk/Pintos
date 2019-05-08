#include <stddef.h>
#include <list.h>


struct list frameTable;

struct FrameEntry
{
    struct list_elem elem;
    void *pages;
    size_t pageCnt;
};

struct lock acquireLock;

void initFrameTable();
void putFrameEntry(void *pages, size_t page_cnt);
void deleteFrameEntry();
void useFrameEntry();
void evict();