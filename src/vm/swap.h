#include "devices/disk.h"
#include <bitmap.h>

struct bitmap *swapBitmap;

void initSwap(void);

struct SwapEntry
{
    int swapCnt;
    disk_sector_t *secList;
};

struct SwapEntry *saveSwap(void *upage, int pageCnt);

void loadSwap(void *upage, struct SwapEntry *entry);