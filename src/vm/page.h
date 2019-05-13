#include <list.h>
#include "filesys/file.h"

enum spte_type
{
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
    void *upage;
    off_t offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;

    struct swap_entry *swap;
};

void add_spt_entry_file(struct file *file, off_t ofs, uint8_t *upage,
                        uint32_t read_bytes, uint32_t zero_bytes, bool writable);
void remove_spt_entry(struct thread *t);
struct spt_entry *fetch_spt_entry(void *upage);
bool handle_page_fault(void *upage, void *esp);
void load_spte_swap(struct spt_entry *entry_p);
void load_spte_file(struct spt_entry *entry_p);
void grow_stack(void *upage);