#include "cache.h"

int buffer_evict(void);
void *buffer_pool_addr(int pool_idx);
bool buffer_less_func(struct list_elem *a, struct list_elem *b);

void buffer_init(void)
{
    list_init(&buffer_cache_list);
    buffer_bitmap = bitmap_create(CACHE_LIMIT_SECTORS);
    buffer_pool = malloc(DISK_SECTOR_SIZE * CACHE_LIMIT_SECTORS);
}

void *buffer_fetch(disk_sector_t sector_idx)
{
    struct list_elem *e;
    for (e = list_begin(&buffer_cache_list); e != list_end(&buffer_cache_list);
         e = list_next(e))
    {
        struct buffer_cache *c = list_entry(e, struct buffer_cache, elem);
        if (c->sector_idx == sector_idx)
        {
            c->access_cnt += 1;
            return buffer_pool_addr(c->pool_idx);
        }
    }
    return NULL;
}

void *buffer_insert(disk_sector_t sector_idx, bool is_access)
{
    printf("buffer inserting %d\n", sector_idx);
    int pool_idx = bitmap_scan_and_flip(buffer_bitmap, 0, 1, false);
    if (pool_idx == BITMAP_ERROR)
        {
            pool_idx = buffer_evict();
        }

    struct buffer_cache *c = (struct buffer_cache *)malloc(sizeof(struct buffer_cache));
    c->pool_idx = pool_idx;
    c->sector_idx = sector_idx;
    if (is_access)
        c->access_cnt += 1;
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

    struct buffer_cache *c = list_entry(list_pop_back(&buffer_cache_list), struct buffer_cache, elem);
    int pool_idx = c->pool_idx;
    free(c);
    
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