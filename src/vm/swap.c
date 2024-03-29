#include "swap.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#define BLOCK_PER_PAGE (PGSIZE / DISK_SECTOR_SIZE)

void swap_init(void)
{
    swap_disk = disk_get(1, 1);
    lock_init(&swap_lock);
    swap_bitmap = bitmap_create(disk_size(swap_disk) / BLOCK_PER_PAGE);
}

struct swap_entry *save_swap(void *upage)
{
    // printf("saving swap %p\n", upage);
    struct swap_entry *entry_p = malloc(sizeof(struct swap_entry));
    lock_acquire(&swap_lock);
    entry_p->swap_idx = BLOCK_PER_PAGE * bitmap_scan_and_flip(swap_bitmap, 0, 1, false);

    int i;
    for (i = 0; i < BLOCK_PER_PAGE; i++)
    {
        disk_write(swap_disk, entry_p->swap_idx + i, upage + i * DISK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
    return entry_p;
}

void load_swap(void *kpage, struct swap_entry *entry_p)
{
    int i;
    for (i = 0; i < BLOCK_PER_PAGE; i++)
    {
        disk_read(swap_disk, entry_p->swap_idx + i, kpage + i * DISK_SECTOR_SIZE);
    }
    lock_acquire(&swap_lock);
    bitmap_flip(swap_bitmap, entry_p->swap_idx / BLOCK_PER_PAGE);
    lock_release(&swap_lock);

    free(entry_p);
}