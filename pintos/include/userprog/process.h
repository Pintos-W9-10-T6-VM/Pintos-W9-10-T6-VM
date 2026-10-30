#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "filesys/off_t.h"
/* [CJG] userprog에서 load_info의 ofs를 몰라서 문제 날까봐 */
#include "threads/thread.h"

/* [SHW] 코드 로드시 lazy_load_segment에 넘겨줄 파일 정보*/
/* [CJG] vm.c에서도 load_info를 활용하기 위해 process.h로 옮김. */
struct load_info {
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
};

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);
void process_terminate(int status) NO_RETURN; // [YYJ]
int process_add_file(
    struct file *
        file); /* 성공하면 파일 소유권을 FD 목록이 인수하고 실패하면 호출자가 유지한다. */
struct file *process_get_file(
    int fd); /* 소유권을 넘기지 않고 현재 프로세스의 파일을 조회한다. */
void process_close_file(
    int fd); /* 현재 프로세스의 해당 FD를 제거하고 파일과 기록을 해제한다. */
void *set_load_info(struct file *file, off_t ofs, size_t page_read_bytes,
                    size_t page_zero_bytes);

#endif /* userprog/process.h */
