#include <list.h>
#include "threads/thread.h"
#include "filesys/file.h"

enum spte_type
{
    NONE,
    IN_FILE,
    IN_SWAP,
    IN_MMAP,
    STACK
};

struct spt_entry
{
    struct list_elem elem;
    enum spte_type type;
    struct thread *thread;
    struct file *file;
    int mapid;
    void *upage;
    off_t offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writeable;

    struct swap_entry *swap;
    bool pinning;
};

void add_spt_entry_file(struct file *file, off_t ofs, uint8_t *upage,
                        uint32_t read_bytes, uint32_t zero_bytes, bool writable);
bool add_spt_entry_mmap(struct file *file, off_t ofs, uint8_t *upage,
                        uint32_t read_bytes, uint32_t zero_bytes, bool writable,
                        int mapid);
void remove_spt_entry(struct thread *t);
void remove_mmap_spt_entry(int mapid);
struct spt_entry *fetch_spt_entry(void *upage);
bool handle_page_fault(void *upage, void *esp);
void load_spte_zero(struct spt_entry *entry_p);
void load_spte_swap(struct spt_entry *entry_p);
void load_spte_file(struct spt_entry *entry_p);
void grow_stack(void *upage);
void write_back(struct spt_entry *entry_p, void *kpage, bool is_dirty);