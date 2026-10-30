/* vm.c: Generic interface for virtual memory objects. */
/* [한국어 번역] vm.c: 가상 메모리 객체를 위한 공통 인터페이스. */

#include <string.h>

#include "stdbool.h"
#include "stddef.h"
#include "threads/malloc.h"
#include "vm/vm.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "vm/anon.h"
#include "vm/file.h"
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
#include "vm/uninit.h"

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
/* [한국어 번역] 각 하위 시스템의 초기화 코드를 호출하여 가상 메모리 하위 시스템을 초기화한다. */
void vm_init(void)
{
    vm_anon_init();
    vm_file_init();
#ifdef EFILESYS /* For project 4 */
                /* [한국어 번역] 프로젝트 4용. */
    pagecache_init();
#endif
    register_inspect_intr();
    /* DO NOT MODIFY UPPER LINES. */
    /* [한국어 번역] 위의 줄들을 수정하지 말 것. */
    /* TODO: Your code goes here. */
    /* [한국어 번역] TODO: 여기에 코드를 작성한다. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
/* [한국어 번역]
 * 페이지의 타입을 얻는다. 초기화 이후 페이지의 타입을 알고 싶을 때 유용하다.
 * 이 함수는 이미 완전히 구현되어 있다.
 */
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
/* [한국어 번역] 보조 함수들. */
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

static void destructor(struct hash_elem *elem, void *aux UNUSED)
{
    struct page *page = hash_entry(elem, struct page, spt_elem);
    free(page->frame);
    destroy(page); // [CJG] destroy according to page type.
    free(page);
}

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
/* [한국어 번역]
 * 초기화 함수를 가진 대기 상태의 페이지 객체를 생성한다.
 * 페이지를 직접 생성하지 말고 이 함수나 vm_alloc_page를 통해 생성한다.
 */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
                                    bool writable, vm_initializer *init,
                                    void *aux)
{

    ASSERT(VM_TYPE(type) != VM_UNINIT)

    struct supplemental_page_table *spt = &thread_current()->spt;

    /* Check wheter the upage is already occupied or not. */
    /* [한국어 번역] upage가 이미 사용 중인지 확인한다. */
    if (spt_find_page(spt, upage) == NULL) {
        /* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
        /* [한국어 번역]
         * TODO: 페이지를 생성하고 VM 타입에 맞는 초기화 함수를 선택한 다음,
         * TODO: uninit_new를 호출하여 "uninit" 페이지 구조체를 생성한다.
         * TODO: 필드 수정은 uninit_new를 호출한 뒤에 해야 한다.
         */

        struct page *page = malloc(sizeof(struct page));
        if (page == NULL) {
            goto err;
        }

        /* [SHW] 현재 initializer를 페이지 타입에 따라 initializer를 바꿔줘야한다. */
        bool (*page_initializer)(struct page *page, enum vm_type type,
                                 void *kva);
        switch (VM_TYPE(type)) {
        case VM_ANON:
            page_initializer = anon_initializer;
            break;

        case VM_FILE:
            page_initializer = file_backed_initializer;
            break;

        default:
            free(page);
            goto err;
        }
        uninit_new(page, upage, init, type, aux, page_initializer);
        page->writable = writable;
        page->is_stack =
            type & VM_MARKER_0; /* [SHW] VM_MARKER_0으로 스택인지 판단*/

        /* TODO: Insert the page into the spt. */
        /* [한국어 번역] TODO: 페이지를 spt에 삽입한다. */
        if (!spt_insert_page(&thread_current()->spt, page)) {
            free(page);
            goto err;
        }
        return true;
    }
err:
    return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* [한국어 번역] spt에서 VA에 해당하는 페이지를 찾아 반환한다. 오류가 발생하면 NULL을 반환한다. */
struct page *spt_find_page(struct supplemental_page_table *spt, void *va)
{
    struct page tmp;
    struct hash_elem *elem;

    tmp.va = pg_round_down(va);
    elem = hash_find(&spt->pages, &tmp.spt_elem);

    /* [CJG] If the provided SPT has page holding (va), or not. */
    return (elem != NULL) ? hash_entry(elem, struct page, spt_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
/* [한국어 번역] 유효성을 검사하여 PAGE를 spt에 삽입한다. */
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page)
{
    bool result = (hash_insert(&spt->pages, &page->spt_elem) == NULL);
    /* [CJG] Check whether page(hash) insertion into the provided spt success or not*/
    return result;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
    /* [SHW] spt에서 해당 페이지 제거 및 PTE 제거*/
    hash_delete(&spt->pages, &page->spt_elem);
    pml4_clear_page(thread_current()->pml4, page->va);

    vm_dealloc_page(page);
}

/* Get the struct frame, that will be evicted. */
/* [한국어 번역] 퇴거할 프레임의 struct frame을 얻는다. */
static struct frame *vm_get_victim(void)
{
    struct frame *victim = NULL;
    /* TODO: The policy for eviction is up to you. */
    /* [한국어 번역] TODO: 퇴거 정책은 구현자에게 맡긴다. */

    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
/* [한국어 번역]
 * 페이지 하나를 퇴거시키고 해당 프레임을 반환한다.
 * 오류가 발생하면 NULL을 반환한다.
 */
static struct frame *vm_evict_frame(void)
{
    struct frame *victim UNUSED = vm_get_victim();
    /* TODO: swap out the victim and return the evicted frame. */
    /* [한국어 번역] TODO: 선택된 페이지를 스왑 아웃하고 퇴거된 프레임을 반환한다. */

    return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
/* [한국어 번역]
 * palloc()으로 프레임을 할당받는다. 사용 가능한 페이지가 없으면
 * 페이지를 퇴거시켜 그 프레임을 반환한다. 이 함수는 항상 유효한 주소를 반환한다.
 * 즉, 사용자 풀 메모리가 가득 차면 프레임을 확보하기 위해 페이지를 퇴거시킨다.
 */
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
/* [한국어 번역] 스택을 확장한다. */
static bool vm_stack_growth(void *addr)
{
    addr = pg_round_down(addr); /* [SHW] PGSIZE단위로 내림 처리 */

    vm_alloc_page(VM_ANON | VM_MARKER_0, addr, true);
    return vm_claim_page(addr);
}

/* Handle the fault on write_protected page */
/* [한국어 번역] 쓰기 보호된 페이지에서 발생한 폴트를 처리한다. */
static bool vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
/* [한국어 번역] 성공하면 true를 반환한다. */
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user,
                         bool write UNUSED, bool not_present)
{
    if (!not_present) {
        return false;
    } /* [CJG] P비트(메모리 매핑)가 있는데도 fault가 난 경우 = PTE_U | PTE_W 위반. */

    /* [CJG](userprog) not_present = (f->error_code & PF_P) == 0; */

    struct supplemental_page_table *spt = &thread_current()->spt;
    struct page *page = spt_find_page(spt, addr);

    /* [CJG] For now, just consider the first case, lazy-loaded page. */

    /* [SHW] 이 페이지 폴트가 스택 성장인지 판단 */
    if (page == NULL) {
        uint64_t rsp;
        if (user) {
            rsp = f->rsp;
        } else {
            rsp = thread_current()->syscall_prev_rsp;
        }
        /* [SHW] 스택 성장 조건이 맞으면 스택 성장 */
        if ((uint64_t)addr < USER_STACK &&
            (uint64_t)addr >= USER_STACK - (1 << 20) &&
            (uint64_t)addr >= rsp - 8) {

            return vm_stack_growth(addr);
        }
        /* [SHW] 스택 성장 조건이 아니면 잘못된 페이지 폴트 */
        else {
            return false;
        }
    }

    return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
/* [한국어 번역]
 * 페이지를 해제한다.
 * 이 함수를 수정하지 말 것.
 */
void vm_dealloc_page(struct page *page)
{
    destroy(page);
    free(page);
}

/* Claim the page that allocate on VA. */
/* [한국어 번역] VA에 할당된 페이지의 실제 메모리를 확보한다. */
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
/* [한국어 번역] PAGE의 실제 메모리를 확보하고 MMU를 설정한다. */
static bool vm_do_claim_page(struct page *page)
{
    struct frame *frame = vm_get_frame();

    /* Set links for each other. */
    frame->page = page;
    page->frame = frame;

    if (!pml4_set_page(thread_current()->pml4, page->va, frame->kva,
                       page->writable)) {
        page->frame = NULL;
        palloc_free_page(frame->kva);
        free(frame);

        return false;
    }

    /* [CJG] 'PTE_W' refers to flags. It means write-flag-bit is turned on. Must be considered all pages now have a write authority. */
    /* [CJG] PTE_W라고 쓴 이유는 page->va의 권한 플래그(특히 쓰기 권한)을 활성화하기 위함입니다. 다만, 지금은 '모든 페이지가 쓰기 가능한 상태'입니다.*/
    /* [SHW] PTE_W -> page->writable로 수정, page별 쓰기 권한을 저장*/

    return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
/* [한국어 번역] 새 보조 페이지 테이블을 초기화한다. */
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

static bool uninit_page_copy(struct page *src_page)
{
    struct uninit_page *uninit = &src_page->uninit;

    struct load_info *aux = malloc(sizeof *aux);
    if (aux == NULL) {
        return false;
    }
    memcpy(aux, uninit->aux, sizeof *aux);

    if (!vm_alloc_page_with_initializer(uninit->type, src_page->va,
                                        src_page->writable, uninit->init,
                                        aux)) {
        free(aux);
        return false;
    }
    return vm_claim_page(src_page->va);
}

static bool anon_page_copy(struct page *src_page)
{
    if (!vm_alloc_page(VM_ANON, src_page->va, src_page->writable)) {
        return false;
    }
    if (!vm_claim_page(src_page->va)) {
        return false;
    }

    struct page *dst_page = spt_find_page(&thread_current()->spt, src_page->va);
    memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
    return true;
}

/* Copy supplemental page table from src to dst */
/* [한국어 번역] 보조 페이지 테이블을 src에서 dst로 복사한다. */
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
                                  struct supplemental_page_table *src)
{
    struct hash_iterator i;
    bool copied;

    hash_first(&i, &src->pages);
    /* [CJG]  src->pages->bucket의 list_head = i->elem. */
    while (hash_next(&i)) {
        struct page *src_page = hash_entry(hash_cur(&i), struct page, spt_elem);
        enum vm_type type = VM_TYPE(src_page->operations->type);

        switch (type) {
        case VM_UNINIT:
            copied = uninit_page_copy(src_page);
            break;
        case VM_ANON:
            copied = anon_page_copy(src_page);
            break;
        default:
            copied = false;
            break;
        }

        if (!copied) {
            return false;
        }
    }
    return true;
}

void supplemental_page_table_kill(struct supplemental_page_table *spt)
{
    /* [CJG] _init() 실패(buckets == NULL) 뒤 종료 경로(_terminate(-1))에서 다시 불림. */
    if (spt->pages.buckets == NULL) {
        return;
    }

    hash_destroy(&spt->pages, destructor);
    /* [CJG] spt는 thread 안의 값이라 계속 그 안에 남아있음. 다음 _kill()이 해시(pages.buckets) 없음으로 읽도록 비우기. */
    spt->pages.buckets = NULL;
}
