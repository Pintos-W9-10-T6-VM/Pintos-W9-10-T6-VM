#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);
void process_terminate(int status) NO_RETURN;   // [YYJ]
int process_add_file (struct file *file); /* 성공하면 파일 소유권을 FD 목록이 인수하고 실패하면 호출자가 유지한다. */
struct file *process_get_file (int fd); /* 소유권을 넘기지 않고 현재 프로세스의 파일을 조회한다. */
void process_close_file (int fd); /* 현재 프로세스의 해당 FD를 제거하고 파일과 기록을 해제한다. */

#endif /* userprog/process.h */
