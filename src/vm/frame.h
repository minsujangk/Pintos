#include <stddef.h>
#include <list.h>


struct list frameTable;
struct list recentFrameTable;

struct FrameEntry
{
    struct list_elem orderElem;
    struct list_elem elem;
    void *pages;
    size_t pageCnt;
};

struct lock acquireLock;

void initFrameTable(void);
void putFrameEntry(void *pages, size_t page_cnt);
void deleteFrameEntry(void *frame);
void useFrameEntry(struct FrameEntry *fe_p);
bool evictLRU(int pageCnt);