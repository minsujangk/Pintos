#include "cache.h"
#include <debug.h>
#include "filesys/filesys.h"

int buffer_evict(void);
void *buffer_pool_addr(int pool_idx);
bool buffer_less_func(struct list_elem *a, struct list_elem *b);

bool cache_debug = false;

void buffer_init(void)
{
    list_init(&buffer_cache_list);
    buffer_bitmap = bitmap_create(CACHE_LIMIT_SECTORS);
    buffer_pool = malloc(DISK_SECTOR_SIZE * CACHE_LIMIT_SECTORS);
    lock_init(&filesys_cache_lock);
}

void *buffer_fetch_or_insert(disk_sector_t sector_idx, bool is_dirty)
{
    if (cache_debug)
        printf("여기는? %d\n", sector_idx);
    lock_acquire(&filesys_cache_lock);
    void *buf_addr = buffer_fetch(sector_idx, is_dirty);
    if (buf_addr == NULL)
    {
        buf_addr = buffer_insert(sector_idx, true, is_dirty);
        disk_read(filesys_disk, sector_idx, buf_addr);
    }
    
    // hex_dump(0, buf_addr, 64, true);

    lock_release(&filesys_cache_lock);
    return buf_addr;
}

void *buffer_fetch(disk_sector_t sector_idx, bool is_dirty)
{
    struct list_elem *e;
    for (e = list_begin(&buffer_cache_list); e != list_end(&buffer_cache_list);
         e = list_next(e))
    {
        struct buffer_cache *c = list_entry(e, struct buffer_cache, elem);
        if (c->sector_idx == sector_idx)
        {
            c->access_cnt += 1;
            c->is_accessed = true;
            c->is_dirty |= is_dirty;
            return buffer_pool_addr(c->pool_idx);
        }
    }
    return NULL;
}

void *buffer_insert(disk_sector_t sector_idx, bool is_access, bool is_dirty)
{
    int pool_idx = bitmap_scan_and_flip(buffer_bitmap, 0, 1, false);
    if (pool_idx == BITMAP_ERROR)
    {
        pool_idx = buffer_evict();
    }
    // printf("buffer inserting sector=%d, pool=%d\n", sector_idx, pool_idx);

    struct buffer_cache *c = (struct buffer_cache *)malloc(sizeof(struct buffer_cache));
    c->pool_idx = pool_idx;
    c->sector_idx = sector_idx;
    if (is_access)
    {
        c->access_cnt += 1;
        c->is_accessed = true;
    }
    c->is_dirty |= is_dirty;
    list_push_back(&buffer_cache_list, &c->elem);

    return buffer_pool_addr(pool_idx);
}

void buffer_remove(struct buffer_cache *c)
{
    bitmap_flip(buffer_pool, c->pool_idx);
    list_remove(&c->elem);
}

int buffer_evict(void)
{
    list_sort(&buffer_cache_list, buffer_less_func, NULL);
    struct buffer_cache *c_to_evict = NULL;

    struct buffer_cache *c;
    struct list_elem *e;
    while (c_to_evict == NULL)
    {
        for (e = list_begin(&buffer_cache_list); e != list_end(&buffer_cache_list);
             e = list_next(e))
        {
            c = list_entry(e, struct buffer_cache, elem);
            if (c->access_cnt > 1)
                continue;
            if (c->is_accessed)
                c->is_accessed = false;
            else
            {
                c_to_evict = c;
                if (c_to_evict->is_dirty)
                {
                    // write back
                    disk_write(filesys_disk, c->sector_idx, buffer_pool_addr(c->pool_idx));
                }
                break;
            }
        }
    }
    // struct buffer_cache *c = list_entry(list_pop_back(&buffer_cache_list), struct buffer_cache, elem);
    int pool_idx = c_to_evict->pool_idx;
    list_remove(&c_to_evict->elem);
    // printf("evict sector=%d pool=%d\n", c_to_evict->sector_idx , pool_idx);
    free(c_to_evict);
    // struct buffer_cache *c = list_entry(list_pop_back(&buffer_cache_list), struct buffer_cache, elem);
    // int pool_idx = c->pool_idx;
    // printf("evict sector=%d, pool=%d\n", c->sector_idx, pool_idx);
    // free(c);

    return pool_idx;
}

void *buffer_pool_addr(int pool_idx)
{
    return buffer_pool + (DISK_SECTOR_SIZE * pool_idx);
}

bool buffer_less_func(struct list_elem *a, struct list_elem *b)
{
    struct buffer_cache *c_a = list_entry(a, struct buffer_cache, elem);
    struct buffer_cache *c_b = list_entry(b, struct buffer_cache, elem);
    return c_a->access_cnt > c_b->access_cnt;
}