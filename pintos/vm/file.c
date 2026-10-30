/* file.c: Implementation of memory backed file object (mmaped object). */
/* [한국어 번역] file.c: 메모리에 매핑된 파일 객체(mmap 객체)의 구현. */

#include "round.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "stddef.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/vm.h"
#include "userprog/syscall.h"
#include "userprog/process.h"

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

static bool is_page_range_unused(struct supplemental_page_table *spt,
                                 void *addr, size_t length)
{
    /* [CJG] Check whether any page already registed in the range where we'll allocate. */
    for (void *va = addr; va < addr + length; va += PGSIZE) {
        if (spt_find_page(spt, va) == NULL) {
            return NULL;
        }
    }
    return true;
}

static struct file *register_memory_mapped_file(struct file *file)
{
    lock_acquire(&filesys_lock);
    struct file *memory_mapped_file = file_reopen(file);
    lock_release(&filesys_lock);
    if (memory_mapped_file == NULL) {
        return NULL;
    }
    return memory_mapped_file;
}

// When _find() failed, it means the `page adress` is already utilized by code, stack, or an earler mmap.
// -> It's natural in this `do_mmap() context` always to send a NULL, because the purpose of do_mmap() itself is "to allocate a page at the user adress."
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset)
{
    if (!is_valid_addr_and_length(addr, length, offset)) {
        return NULL;
    }

    struct supplemental_page_table *spt = &thread_current()->spt;
    if (!is_page_range_unused(spt, addr, length)) {
        return NULL;
    }

    struct file *memory_mapped_file = register_memory_mapped_file(file);
    if (!memory_mapped_file) {
        return NULL;
    }

    off_t bytes_after_offset = file_length(memory_mapped_file) - offset;
    size_t read_bytes = (length < (size_t)bytes_after_offset)
                            ? length
                            : (size_t)bytes_after_offset;
    size_t zero_bytes = ROUND_UP(length, PGSIZE) - read_bytes;

    void *va = addr;
    while (read_bytes > 0 || zero_bytes > 0) {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        struct load_info *load_info = set_load_info(
            memory_mapped_file, offset, page_read_bytes, page_zero_bytes);

        if (!vm_alloc_page_with_initializer(VM_FILE, va, writable,
                                            lazy_load_file, load_info)) {
            free(load_info);
            return NULL;
        }

        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        va += PGSIZE;
        offset += page_read_bytes;
    }
    return addr;
}

/* Do the munmap */
/* [한국어 번역] munmap을 수행한다. */
void do_munmap(void *addr)
{
}
