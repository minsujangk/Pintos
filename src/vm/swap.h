#include <stdint.h>
#include <bitmap.h>
#include "devices/disk.h"

struct swap_entry
{
    disk_sector_t swap_idx;
};

struct disk *swap_disk;
struct lock swap_lock;
struct bitmap *swap_bitmap;

void swap_init(void);
struct swap_entry *save_swap(void *upage);
void load_swap(void *upage, struct swap_entry *entry_p);
