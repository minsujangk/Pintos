#include "threads/thread.h"

enum LoadMode
{
    LOAD_FILE,
    LOAD_SWAP,
    LOAD_ZERO
};

struct SupPageTableEntry
{
    struct list_elem elem;

    tid_t processId;
    uint32_t *pagedir;
    void *upage;
    void *frame;
    struct SwapEntry *swapEntry;
    char *file;
    int fileOffset;
    enum LoadMode mode;
};

struct list supPageTable;

void initSupPageTable(void);
void addSupPageTableEntry(tid_t processId, void *upage, void *frame);
void setFileSupPageTableEntry(void *upage, int file_offset, char *file_name);

bool dropPage(void *frame);
bool dropPageMultiple(void *frame, int pageCnt);

void *handlePageFault(void *fault_addr);
void freePages(struct thread *t);
void writeBack(void *frame, int pageCnt);
bool isRecentlyUsed(void *frame);