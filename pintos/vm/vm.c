/* vm.c: Generic interface for virtual memory objects. */

#include "debug.h"
#include "hash.h"
#include "threads/malloc.h"
#include "vm/vm.h"
#include "threads/mmu.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/inspect.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
    vm_anon_init();
    vm_file_init();
#ifdef EFILESYS /* For project 4 */
    pagecache_init();
#endif
    register_inspect_intr();
    /* DO NOT MODIFY UPPER LINES. */
    /* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type page_get_type(struct page *page)
{
    int ty = VM_TYPE(page->operations->type);
    switch (ty) {
    case VM_UNINIT:
        return VM_TYPE(page->uninit.type);
    default:
        return ty;
    }
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

static bool less(const struct hash_elem *elem1, const struct hash_elem *elem2,
                 void *aux UNUSED)
{
    const struct page *page1 = hash_entry(elem1, struct page, spt_elem);
    const struct page *page2 = hash_entry(elem2, struct page, spt_elem);

    return page1->va < page2->va;
}

static uint64_t hashing(const struct hash_elem *elem, void *aux UNUSED)
{
    const struct page *p = hash_entry(elem, struct page, spt_elem);
    uint64_t page_num = pg_no(p->va);
    /* [CJG] Hashing pg_no(va), not va: because offsets of page refer to start-adress of page - are always 0.
     * hash_bytes() uses the low bits, so 'raw va' puts consecutive pages in one bucket. */

    return hash_bytes(&page_num, sizeof(p->va));
}

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
                                    bool writable, vm_initializer *init,
                                    void *aux)
{

    ASSERT(VM_TYPE(type) != VM_UNINIT)

    struct supplemental_page_table *spt = &thread_current()->spt;

    /* Check wheter the upage is already occupied or not. */
    if (spt_find_page(spt, upage) == NULL) {
        /* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */

        /* TODO: Insert the page into the spt. */
    }
err:
    return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *spt, void *va)
{
    struct page tmp;
    struct hash_elem *elem;

    tmp.va = pg_round_down(va);
    elem = hash_find(&spt->pages, &tmp.spt_elem);

    /* [CJG] If the provided SPT has page holding (va), or not. */
    return (elem == NULL) ? NULL : hash_entry(elem, struct page, spt_elem);
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page)
{
    bool result = hash_insert(&spt->pages, &page->spt_elem) == NULL;
    /* [CJG] Check whether page(hash) insertion into the provided spt success or not*/
    return result;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
    vm_dealloc_page(page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim(void)
{
    struct frame *victim = NULL;
    /* TODO: The policy for eviction is up to you. */

    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void)
{
    struct frame *victim UNUSED = vm_get_victim();
    /* TODO: swap out the victim and return the evicted frame. */

    return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame(void)
{
    struct frame *frame = malloc(sizeof *frame);
    if (frame == NULL) {
        PANIC("Memory allocation failed.");
    } /*  [CJG] Malloc() failed. */

    frame->kva = palloc_get_page(PAL_USER);
    frame->page = NULL;

    if (frame->kva == NULL) {
        PANIC("todo");
    }

    /* [CJG] Swap out() would be placed in this area, soon. */
    /* [CJG] Swap out() 함수관련 내용이 PANIC("todo")부분 대신 들어갈 거라고 gitbook이 말해줍니다. */

    return frame;
}

/* Growing the stack. */
static void vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
static bool vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
                         bool user UNUSED, bool write UNUSED,
                         bool not_present UNUSED)
{
    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
    struct page *page = NULL;

    // if (!is_writable(thread_current()->pml4)) {
    //     thread_current()->exit_status = -1;
    //     process_exit();
    // }
    /* TODO: Validate the fault */
    /* TODO: Your code goes here */

    return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
    destroy(page);
    free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va)
{
    struct page *page = spt_find_page(&thread_current()->spt, va);
    if (page == NULL) {
        return false;
    }

    /* [CJG]  Find the appropirate page for user's va. */
    /* [CJG] 유저의 va를 넣을 페이지 할당 요청. */

    return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page(struct page *page)
{
    struct frame *frame = vm_get_frame();

    /* Set links for each other. */
    frame->page = page;
    page->frame = frame;

    if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva, PTE_W)) {
        palloc_free_page(frame->kva);
        free(frame);
        page->frame = NULL;

        frame->page = NULL;
        return false;
    }

    /* [CJG] 'PTE_W' refers to flags. It means write-flag-bit is turned on. Must be considered all pages now have a write authority. */
    /* [CJG] PTE_W라고 쓴 이유는 page->va의 권한 플래그(특히 쓰기 권한)을 활성화하기 위함입니다. 다만, 지금은 '모든 페이지가 쓰기 가능한 상태'입니다.*/

    return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
bool supplemental_page_table_init(struct supplemental_page_table *spt)
{
    bool result = hash_init(&spt->pages, hashing, less, NULL);
    /* [CJG] False case derived from malloc() at pages->buckets.*/

    /* [CJG] Changed the original void type to `bool`, because of its false case. For example, __do_fork() needs this function's result(or any signal), in light of calling `goto error`. */

    if (!result) {
        return false;
    }
    return result;
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
                                  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
    /* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}
