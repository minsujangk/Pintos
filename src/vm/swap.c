#include "swap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#define BLOCK_PER_PAGE PGSIZE / DISK_SECTOR_SIZE

void swap_init(void)
{
    swap_disk = disk_get(1, 1);
    lock_init(&swap_lock);
    swap_bitmap = bitmap_create(disk_size(swap_disk) / BLOCK_PER_PAGE);
}

struct swap_entry *save_swap(void *upage)
{
    struct swap_entry *entry_p = malloc(sizeof(struct swap_entry));
    lock_acquire(&swap_lock);
    entry_p->swap_idx = 4 * bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
    lock_release(&swap_lock);

    for (int i = 0; i < BLOCK_PER_PAGE; i++)
    {
        disk_write(swap_disk, entry_p->swap_idx + i, upage + i * DISK_SECTOR_SIZE);
    }
    return entry_p;
}

void load_swap(void *kpage, struct swap_entry *entry_p)
{
    for (int i = 0; i < BLOCK_PER_PAGE; i++)
    {
        disk_read(swap_disk, entry_p->swap_idx + i, kpage + i * DISK_SECTOR_SIZE);
    }
    lock_acquire(&swap_lock);
    bitmap_flip(swap_bitmap, entry_p->swap_idx / 4);
    lock_release(&swap_lock);

    free(entry_p);
}