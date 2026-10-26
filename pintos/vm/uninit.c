/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */
/* [한국어 번역]
 * uninit.c: 초기화되지 않은 페이지의 구현.
 * 모든 페이지는 uninit 페이지로 생성된다. 첫 페이지 폴트가 발생하면
 * 핸들러 호출 체인이 uninit_initialize(page->operations.swap_in)를 호출한다.
 * uninit_initialize는 페이지 객체를 초기화하여 특정 타입의 페이지 객체
 * (anon, file, page_cache)로 전환하고, vm_alloc_page_with_initializer에
 * 전달된 초기화 콜백을 호출한다.
 */

#include "vm/vm.h"
#include "vm/uninit.h"

/* [SHW] free 사용을 위해서 malloc.h include*/
#include "threads/malloc.h"

static bool uninit_initialize(struct page *page, void *kva);
static void uninit_destroy(struct page *page);

/* DO NOT MODIFY this struct */
/* [한국어 번역] 이 구조체를 수정하지 말 것. */
static const struct page_operations uninit_ops = {
    .swap_in = uninit_initialize,
    .swap_out = NULL,
    .destroy = uninit_destroy,
    .type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
/* [한국어 번역] 이 함수를 수정하지 말 것. */
void uninit_new(struct page *page, void *va, vm_initializer *init,
                enum vm_type type, void *aux,
                bool (*initializer)(struct page *, enum vm_type, void *))
{
    ASSERT(page != NULL);

    *page = (struct page){.operations = &uninit_ops,
                          .va = va,
                          .frame = NULL, /* no frame for now */
                          /* [한국어 번역] 현재는 프레임이 없다. */
                          .uninit = (struct uninit_page){
                              .init = init,
                              .type = type,
                              .aux = aux,
                              .page_initializer = initializer,
                          }};
}

/* Initalize the page on first fault */
/* [한국어 번역] 첫 폴트가 발생했을 때 페이지를 초기화한다. */
static bool uninit_initialize(struct page *page, void *kva)
{
    struct uninit_page *uninit = &page->uninit;

    /* Fetch first, page_initialize may overwrite the values */
    /* [한국어 번역] page_initialize가 값을 덮어쓸 수 있으므로 먼저 가져온다. */
    vm_initializer *init = uninit->init;
    void *aux = uninit->aux;

    /* TODO: You may need to fix this function. */
    /* [한국어 번역] TODO: 이 함수를 수정해야 할 수도 있다. */
    return uninit->page_initializer(page, uninit->type, kva) &&
           (init ? init(page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
/* [한국어 번역]
 * uninit_page가 보유한 자원을 해제한다. 대부분은 다른 페이지 객체로 전환되지만,
 * 실행 중 한 번도 참조되지 않아 프로세스 종료 시에도 uninit 상태인 페이지가
 * 남아 있을 수 있다. PAGE 자체는 호출자가 해제한다.
 */
static void uninit_destroy(struct page *page)
{
    struct uninit_page *uninit = &page->uninit;
    /* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
    /* [한국어 번역]
     * TODO: 이 함수를 구현한다.
     * TODO: 처리할 작업이 없다면 바로 반환한다.
     */

    /* [SHW] 페이지 폴트가 일어나지 않고 destroy된다면 aux에 받았던 load_info가 해제되지 않아서 해제해준다.*/
    free(uninit->aux);
}
