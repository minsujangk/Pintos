#include "devices/disk.h"
#include "threads/malloc.h"
#include <list.h>
#include <bitmap.h>

#define CACHE_LIMIT_SECTORS 64

struct list buffer_cache_list;
void *buffer_pool;
struct bitmap *buffer_bitmap;

struct buffer_cache
{
    struct list_elem elem;
    disk_sector_t sector_idx;
    int pool_idx;

    int access_cnt;
    bool is_dirty;
};

void buffer_init(void);
void *buffer_fetch(disk_sector_t sector_idx);
void *buffer_insert(disk_sector_t sector_idx, bool is_access);
void *buffer_load_file(disk_sector_t sector_idx);
void buffer_remove(struct buffer_cache *c);