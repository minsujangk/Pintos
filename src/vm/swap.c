
#include "swap.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

struct disk *swapDisk;

void initSwap(void)
{
    swapDisk = disk_get(1, 1);
    swapBitmap = bitmap_create(disk_size(swapDisk));
}

struct SwapEntry *acquireSwapEntry(int swapCnt)
{
    struct SwapEntry *entry_p = (struct SwapEntry *)malloc(sizeof(struct SwapEntry));
    entry_p->swapCnt = swapCnt;
    entry_p->secList = malloc(DISK_SECTOR_SIZE * swapCnt);
    return entry_p;
}

struct SwapEntry *saveSwap(void *upage, int pageCnt)
{
    int swapCnt = pageCnt * (PGSIZE / DISK_SECTOR_SIZE);
    struct SwapEntry *entry_p = acquireSwapEntry(swapCnt);
    int i = 0;
    for (; i < swapCnt; i++)
    {
        disk_sector_t swap_idx = bitmap_scan_and_flip(swapBitmap, 0, 1, false);
        if (swap_idx == BITMAP_ERROR)
        {
            // swap 되돌리는 기능 추가.
            return NULL;
        }
        printf("write swap %d %s\n", swap_idx, thread_name());
        disk_write(swapDisk, swap_idx, upage + i * DISK_SECTOR_SIZE);
        entry_p->secList[i] = swap_idx;
    }
    return entry_p;
}

void loadSwap(void *upage, struct SwapEntry *entry)
{
    int i = 0;
    for (; i < entry->swapCnt; i++)
    {
        disk_sector_t swap_idx = entry->secList[i];
        disk_read(swapDisk, swap_idx, (upage + i * DISK_SECTOR_SIZE));
        bitmap_flip(swapBitmap, swap_idx);
    }
}
