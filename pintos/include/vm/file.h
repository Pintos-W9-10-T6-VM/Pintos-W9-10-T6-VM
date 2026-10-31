#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"
#include <stdint.h>

struct page;
enum vm_type;

struct file_page {
    uint32_t file_read_bytes;
    off_t offset; // 원래 메모리 주소로부터 얼마나 지금 파일 페이지의 메모리 주소가 떨어져 있는 지 - 사실상 첫 번째 빼고는 모두 페이지 단위
    struct file *file;
    int page_cnt; // 몇 개의 페이지가 mmap()으로 할당 되냐. unmap() 할 때 for 문 조건식
};

void vm_file_init(void);
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset);
void do_munmap(void *va);
#endif
