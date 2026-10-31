#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "vm/vm.h"
#include <stdint.h>

struct page;
enum vm_type;

struct file_page {
    /* [SHW] 파일에 대한 정보*/
    struct file *file;
    off_t offset;
    uint32_t file_read_bytes;

    /* [SHW] 파일이 차지하는 페이지의 개수*/
    int page_cnt;
};

void vm_file_init(void);
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable, struct file *file,
              off_t offset);
void do_munmap(void *va);
#endif
