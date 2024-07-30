#include "sim.h"
#include "coremap.h"
#include "list.h"
#include "malloc369.h"

int threshold;
list_head* a1;
list_head* am;
extern struct frame* coremap;

//helper
bool is_in_queue(list_entry *entry, list_head *queue)
{
    list_entry *pos;
    list_for_each(pos, queue) {
        if (pos == entry) {
            return true;
        }
    }
    return false;
}

int list_length(const list_head *list) {
    int length = 0;
    struct list_entry *pos;
    
    list_for_each(pos, list) {
        length++;
    }
    return length;
}

/* Page to evict is chosen using the simplified 2Q algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int s2q_evict(void)
{
    list_entry * target;
    if (list_length(a1) > threshold){
        target = list_last_entry(a1);
        list_del(target);
    } else if (!(am->head.next == &am->head)){
        target = list_last_entry(am);
        list_del(target);
    }
    struct frame *fr = container_of(target, struct frame, framelist_entry);
    return fr - coremap;
}

/* This function is called on each access to a page to update any information
 * needed by the simplified 2Q algorithm.
 * Input: The page table entry and full virtual address (not just VPN)
 * for the page that is being accessed.
 */
void s2q_ref(int frame, vaddr_t vaddr)
{
    (void) vaddr;
    list_entry *entry = &(coremap[frame].framelist_entry);

    if (is_in_queue(entry, a1)){
        list_del(entry);
        list_add_head(am, entry);
    } else if (is_in_queue(entry, am)){
        list_del(entry);
        list_add_head(am, entry); //head: recently used.
    } else {
        list_add_head(a1, entry);
    }
}

/* Initialize any data structures needed for this replacement algorithm. */
void s2q_init(void)
{
    threshold = memsize / 10;
    a1 = (list_head*) malloc369(sizeof(list_head));
    list_init(a1);
    am = (list_head*) malloc369(sizeof(list_head));
    list_init(am);
}

/* Cleanup any data structures created in s2q_init(). */
void s2q_cleanup(void)
{
    while(!(a1->head.next == &a1->head)){
        list_entry* node = list_first_entry(a1);
        list_del(node);
    }
    while(!(am->head.next == &am->head)){
        list_entry* node = list_first_entry(am);
        list_del(node);
    }
    free369(a1);
    free369(am);
}
