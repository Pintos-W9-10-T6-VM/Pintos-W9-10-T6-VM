#ifndef USERPROG_SYSCALL_H /* 헤더가 이미 포함되었는지 확인한다. */
#define USERPROG_SYSCALL_H /* 중복 포함을 막는 표시를 정의한다. */
#include "threads/synch.h" /* struct lock의 정의를 가져온다. */
extern struct lock filesys_lock; /* [JE/YYJ] 실제 객체는 syscall.c에 하나만 있다. */
void syscall_init (void); /* 기존 시스템 콜 초기화 함수의 선언을 유지한다. */
#endif /* userprog/syscall.h  USERPROG_SYSCALL_H 보호 구간을 끝낸다. */


