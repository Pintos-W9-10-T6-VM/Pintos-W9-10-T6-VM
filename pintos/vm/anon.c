/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */
/* [한국어 번역] anon.c: 디스크 이미지와 관련 없는 페이지(익명 페이지)의 구현. */

#include "stdbool.h"
#include "threads/palloc.h"
#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/malloc.h"

/* DO NOT MODIFY BELOW LINE */
/* [한국어 번역] 아래 줄을 수정하지 말 것. */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
/* [한국어 번역] 이 구조체를 수정하지 말 것. */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

/* Initialize the data for anonymous pages */
/* [한국어 번역] 익명 페이지에 사용할 데이터를 초기화한다. */
void vm_anon_init(void)
{
    /* TODO: Set up the swap_disk. */
    /* [한국어 번역] TODO: swap_disk를 설정한다. */
    swap_disk = NULL;
}

/* Initialize the file mapping */
/* [한국어 번역] 파일 매핑을 초기화한다. */
bool anon_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* Set up the handler */
    /* [한국어 번역] 핸들러를 설정한다. */

    /* [SHW] anon타입이 아니면 실패*/
    if (VM_TYPE(type) != VM_ANON) {
        return false;
    }

    page->operations = &anon_ops;

    struct anon_page *anon_page = &page->anon;

    return true;
}

/* Swap in the page by read contents from the swap disk. */
/* [한국어 번역] 스왑 디스크에서 내용을 읽어 페이지를 스왑 인한다. */
static bool anon_swap_in(struct page *page, void *kva)
{
    struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
/* [한국어 번역] 페이지 내용을 스왑 디스크에 기록하여 스왑 아웃한다. */
static bool anon_swap_out(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
/* [한국어 번역] 익명 페이지를 파괴한다. PAGE 자체는 호출자가 해제한다. */
static void anon_destroy(struct page *page)
{
    struct anon_page *anon_page = &page->anon;
    if (page->frame != NULL) {
        palloc_free_page(page->frame->kva);
        free(page->frame);
    }
}
