/* file.c: Implementation of memory backed file object (mmaped object). */
/* [한국어 번역] file.c: 메모리에 매핑된 파일 객체(mmap 객체)의 구현. */

#include "vm/vm.h"
#include "vm/file.h"
#include "hash.h"
#include "stddef.h"
#include <debug.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "threads/mmu.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

/* [YYJ] 요것은 검증을 위한 것 */
#ifndef VM_MUNMAP_DEBUG
#define VM_MUNMAP_DEBUG 1
#endif

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

static struct page *find_map(struct supplemental_page_table *spt, void *addr);
static void unmap_pages(struct supplemental_page_table *spt, void *addr,
                        size_t page_cnt);
static void unmap_page(struct supplemental_page_table *spt, struct page *page);
static void *next_page(void *addr);
static void trace_unmap(struct supplemental_page_table *spt, void *addr,
                        size_t page_cnt);

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

/* Do the mmap */
/* [한국어 번역] mmap을 수행한다. */
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset)
{
}
/* Do the munmap */
/* [한국어 번역] munmap을 수행한다. */
void do_munmap(void *addr)
{
    struct supplemental_page_table *spt = &thread_current()->spt;
    struct page *first_page = find_map(spt, addr);
    if (first_page == NULL) {
        return;
    }

    size_t page_cnt = first_page->mmap_how_many_page;

    unmap_pages(spt, addr, page_cnt);
    trace_unmap(spt, addr, page_cnt);
}

static struct page *find_map(struct supplemental_page_table *spt, void *addr)
{
    if (addr == NULL || !is_user_vaddr(addr) || pg_ofs(addr) != 0) {
        return NULL;
    }

    struct page *page = spt_find_page(spt, addr);

    if (page == NULL || page_get_type(page) != VM_FILE ||
        page->mmap_how_many_page == 0) {
        return NULL;
    }

    return page;
}

static void unmap_pages(struct supplemental_page_table *spt, void *addr,
                        size_t page_cnt)
{
    for (size_t i = 0; i < page_cnt; i++) {
        struct page *page = spt_find_page(spt, addr);

        if (page != NULL) {
            unmap_page(spt, page);
        }

        addr = next_page(addr);
    }
}

static void unmap_page(struct supplemental_page_table *spt, struct page *page)
{
    struct hash_elem *removed = hash_delete(&spt->pages, &page->spt_elem);

    if (removed != NULL) {
        vm_dealloc_page(page);
    }
}

static void *next_page(void *addr)
{
    return (uint8_t *)addr + PGSIZE;
}

static void trace_unmap(struct supplemental_page_table *spt UNUSED,
                        void *addr UNUSED, size_t page_cnt UNUSED)
{
#if VM_MUNMAP_DEBUG
    printf("[munmap] start=%p pages=%zu\n", addr, page_cnt);
    for (size_t i = 0; i < page_cnt; i++) {
        bool in_spt = spt_find_page(spt, addr) != NULL;
        bool mapped = pml4_get_page(thread_current()->pml4, addr) != NULL;

        printf("[munmap] page=%p spt=%s pml4=%s\n", addr,
               in_spt ? "present" : "removed", mapped ? "present" : "cleared");
        addr = next_page(addr);
    }
#endif
}
/*
printf로 디버깅 및 검증하기

확인할 때만 상단 값을 바꿉니다.

#define VM_MUNMAP_DEBUG 1

컨테이너의 저장소 최상위에서 실행합니다.

make -C pintos/vm

rm -f pintos/vm/build/tests/vm/mmap-unmap.{output,errors,result}

make -C pintos/vm/build tests/vm/mmap-unmap.output

grep '\[munmap\]' pintos/vm/build/tests/vm/mmap-unmap.output

기존 mmap-unmap 테스트는 0x10000000부터 두 페이지를 매핑합니다. 정상적인 정리 후에는 다음 의미의 출력이 나와야 합니다.

[munmap] start=0x10000000 pages=2
[munmap] page=0x10000000 spt=removed pml4=cleared
[munmap] page=0x10001000 spt=removed pml4=cleared

주소의 출력 자릿수는 다를 수 있습니다.

| 출력 | 판단 |
|---|---|
| 출력 없음 | 시작 페이지 검색 실패 또는 페이지 수 미설정 등을 확인 |
| spt=present | SPT 등록이 남아 있음 |
| spt=removed pml4=present | 관리 정보만 제거되고 사용자 매핑은 남아 있음 |
| spt=removed pml4=cleared | 해당 페이지의 등록·매핑 제거 확인 |


*/