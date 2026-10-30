/* file.c: Implementation of memory backed file object (mmaped object). */
/* [한국어 번역] file.c: 메모리에 매핑된 파일 객체(mmap 객체)의 구현. */

#include "threads/vaddr.h"
#include "vm/vm.h"

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
/* [한국어 번역] 이 구조체를 수정하지 말 것. */
static const struct page_operations file_ops = {
    .swap_in = file_backed_swap_in,
    .swap_out = file_backed_swap_out,
    .destroy = file_backed_destroy,
    .type = VM_FILE,
};

/* The initializer of file vm */
/* [한국어 번역] 파일 기반 가상 메모리의 초기화 함수. */
void vm_file_init(void)
{
}

/* Initialize the file backed page */
/* [한국어 번역] 파일 기반 페이지를 초기화한다. */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva)
{
    /* Set up the handler */
    /* [한국어 번역] 핸들러를 설정한다. */
    page->operations = &file_ops;

    struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
/* [한국어 번역] 파일에서 내용을 읽어 페이지를 스왑 인한다. */
static bool file_backed_swap_in(struct page *page, void *kva)
{
    struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
/* [한국어 번역] 페이지 내용을 파일에 다시 기록하여 스왑 아웃한다. */
static bool file_backed_swap_out(struct page *page)
{
    struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
/* [한국어 번역] 파일 기반 페이지를 파괴한다. PAGE 자체는 호출자가 해제한다. */
static void file_backed_destroy(struct page *page)
{
    struct file_page *file_page UNUSED = &page->file;
}

static bool is_valid_addr_and_length(void *addr, size_t length, off_t offset)
{
    void *end = addr + length;

    if (addr == NULL || pg_ofs(addr) != 0 || length <= 0 ||
        pg_ofs(offset) != 0) {
        return false;
    }

    if (end < addr) {
        return false;
    } /* [CJG] Overflow 방지. */

    if (!is_user_vaddr((void *)(end - 1))) {
        return false;
    }

    return true;
}

/* Do the mmap */
/* [한국어 번역] mmap을 수행한다. */
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset)
{
    if (!is_valid_addr_and_length(addr, length, offset)) {
        return NULL;
    }

    while (read_bytes > 0 || zero_bytes > 0)
    
    return addr;
}

/* Do the munmap */
/* [한국어 번역] munmap을 수행한다. */
void do_munmap(void *addr)
{
}
