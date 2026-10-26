/* vm.c: Generic interface for virtual memory objects. */
/* [한국어 번역] vm.c: 가상 메모리 객체를 위한 공통 인터페이스. */

#include "stdbool.h"
#include "stddef.h"
#include "threads/malloc.h"
#include "vm/vm.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "vm/anon.h"
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
        uninit_new(page, upage, init, type, aux, anon_initializer);
        page->writable = writable;

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
struct page *spt_find_page(struct supplemental_page_table *spt UNUSED,
                           void *va UNUSED)
{
    struct page *page = NULL;
    /* TODO: Fill this function. Find struct page that corresponds to va from the given supplemental page table. If fail, return NULL.*/
    /* [한국어 번역]
     * TODO: 이 함수를 구현한다. 주어진 보조 페이지 테이블에서 va에 해당하는
     * struct page를 찾는다. 찾지 못하면 NULL을 반환한다.
     */

    return page;
}

/* Insert PAGE into spt with validation. */
/* [한국어 번역] 유효성을 검사하여 PAGE를 spt에 삽입한다. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
                     struct page *page UNUSED)
{
    int succ = false;
    /* TODO: Fill this function. Insert struct page into the given supplemental page table. 
    This function should checks that the virtual address does not exist in the given supplemental page table.*/
    /* [한국어 번역]
     * TODO: 이 함수를 구현한다. 주어진 보조 페이지 테이블에 struct page를 삽입한다.
     * 해당 가상 주소가 테이블에 이미 존재하지 않는지 확인해야 한다.
     */

    return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
    vm_dealloc_page(page);
    return true;
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
    struct frame *frame = NULL;
    /* TODO: Fill this function. */
    /* [한국어 번역] TODO: 이 함수를 구현한다. */

    ASSERT(frame != NULL);
    ASSERT(frame->page == NULL);
    return frame;
}

/* Growing the stack. */
/* [한국어 번역] 스택을 확장한다. */
static void vm_stack_growth(void *addr UNUSED)
{
}

/* Handle the fault on write_protected page */
/* [한국어 번역] 쓰기 보호된 페이지에서 발생한 폴트를 처리한다. */
static bool vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
/* [한국어 번역] 성공하면 true를 반환한다. */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
                         bool user UNUSED, bool write UNUSED,
                         bool not_present UNUSED)
{
    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
    struct page *page = NULL;
    /* TODO: Validate the fault */
    /* [한국어 번역] TODO: 폴트의 유효성을 검사한다. */
    /* TODO: Your code goes here */
    /* [한국어 번역] TODO: 여기에 코드를 작성한다. */

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
bool vm_claim_page(void *va UNUSED)
{
    struct page *page = NULL;
    /* TODO: Fill this function */
    /* [한국어 번역] TODO: 이 함수를 구현한다. */

    return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
/* [한국어 번역] PAGE의 실제 메모리를 확보하고 MMU를 설정한다. */
static bool vm_do_claim_page(struct page *page)
{
    struct frame *frame = vm_get_frame();

    /* Set links */
    /* [한국어 번역] 서로를 가리키는 연결을 설정한다. */
    frame->page = page;
    page->frame = frame;

    /* TODO: Insert page table entry to map page's VA to frame's PA. */
    /* [한국어 번역]
     * TODO: 페이지의 가상 주소(VA)를 프레임의 물리 주소(PA)에 매핑하는
     * 페이지 테이블 항목을 삽입한다.
     */

    return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
/* [한국어 번역] 새 보조 페이지 테이블을 초기화한다. */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
    /*
    Initializes the supplemental page table. You may choose the data structure to use for the supplemental page table. 
    The function is called when a new process starts (in initd of userprog/process.c) and when a process is being forked 
    (in __do_fork of userprog/process.c).
    */
    /* [한국어 번역]
     * 보조 페이지 테이블을 초기화한다. 사용할 자료구조는 자유롭게 선택할 수 있다.
     * 새 프로세스가 시작될 때(userprog/process.c의 initd)와
     * 프로세스를 fork할 때(userprog/process.c의 __do_fork) 호출된다.
     */
}

/* Copy supplemental page table from src to dst */
/* [한국어 번역] 보조 페이지 테이블을 src에서 dst로 복사한다. */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
                                  struct supplemental_page_table *src UNUSED)
{
}

/* Free the resource hold by the supplemental page table */
/* [한국어 번역] 보조 페이지 테이블이 보유한 자원을 해제한다. */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
    /* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
    /* [한국어 번역]
     * TODO: 스레드가 보유한 보조 페이지 테이블 전체를 파괴하고,
     * TODO: 수정된 모든 내용을 저장 장치에 다시 기록한다.
     */
}
