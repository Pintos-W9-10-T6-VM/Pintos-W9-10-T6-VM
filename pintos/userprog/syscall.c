#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "lib/stdio.h"           /* STDOUT_FILENO */
#include "lib/kernel/stdio.h"    /* putbuf */
#include "threads/vaddr.h"       /* is_user_vaddr, PGSIZE */
#include "threads/mmu.h"         /* pml4_get_page */
#include "userprog/process.h"    /* process_terminate */
#include "threads/palloc.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/*
 * TODO [JE]: user_buffer_readable
 *
 * 역할
 * - 커널이 사용자 버퍼의 지정 범위를 읽어도 되는지 검사한다.
 *
 * 작성 파일
 * - pintos/userprog/syscall.c, static 함수.
 *
 * 입력 / 출력
 * - buffer: 사용자 버퍼 시작 주소.
 * - size: 커널이 읽을 바이트 수.
 * - 반환값: 전체 범위가 유효하면 true, 아니면 false.
 *
 * 로직 / 분기
 * 1. size == 0이면 메모리를 읽지 않고 true를 반환한다.
 * 2. 양수 길이에서 NULL이면 false를 반환한다.
 * 3. 마지막 주소 계산의 정수 오버플로를 검사한다.
 * 4. 전체 범위가 사용자 주소 영역에 있는지 검사한다.
 * 5. 범위에 걸친 모든 페이지의 사용자 접근 가능한 매핑을 확인한다.
 * - 시작 주소만 검사하고 끝내면 안 된다.
 *
 * 호출
 * - syscall.c의 sys_write():
 *   양수 길이의 stdout 버퍼를 putbuf()에 넘기기 전.
 *
 * 기존 함수 / 자료 사용
 * - threads/mmu.c: pml4_get_page(), 페이지 테이블 조회 관련 함수.
 * - include/threads/mmu.h: 페이지 테이블 관련 선언.
 * - include/threads/vaddr.h: is_user_vaddr(), 페이지 주소 관련 도구.
 * - threads/thread.c: thread_current().
 *
 * 주의
 * - pml4_get_page() 호출 전에 사용자 주소인지 확인한다.
 * - 이 함수는 검사만 수행하고 프로세스를 종료하지 않는다.
 *
 * 소유권
 * - 메모리 할당이나 해제를 하지 않는다.
 *
 * 검증
 * - 별도 사용자 진단 프로그램으로 정상 버퍼, 길이 0,
 *   페이지 경계를 지나는 버퍼, 미매핑 페이지 포함 범위를 확인한다.
 * - args-* 통과만으로 이 검사가 완성됐다고 판단하지 않는다.
 */
static bool 
user_buffer_readable(const void *buffer, size_t size){
	// TODO [YJH 연습]: 직접 구현하기. 위의 설계 주석 "로직 / 분기" 1~5번을 따른다.
	
	// (a) size가 0이면 true, buffer가 NULL이면 false
	if (size == 0)
		return true;
	if (buffer == NULL)
		return false;

	// (b) 시작 주소가 사용자 영역인가
	if (!(is_user_vaddr(buffer)))
		return false;

	// (c) 끝 주소(시작 + size - 1)를 계산하고, 넘침이 없는지, 끝도 사용자 영역인지
	uintptr_t start = (uintptr_t)buffer; // 번지를 숫자로 바꾼다
	uintptr_t end = start + size - 1;				 // 마지막 바이트의 번지 (위 공식)

	if (end < start) // 넘쳐서 끝이 시작보다 작아졌나?
		return false;

	if (!is_user_vaddr((void *)end)) // 끝도 사용자 영역이어야 한다
		return false;

	// (d) 시작~끝에 걸친 모든 페이지가 실제로 연결돼 있는가 */
	uint64_t *pml4 = thread_current()->pml4;	// 이 프로세스의 연결표
	uintptr_t page = start / PGSIZE * PGSIZE;	// 시작 번지가 속한 페이지의, 시작 번지

	for (; page <= end; page += PGSIZE) {		// 페이지를 하나씩 확인
		if (pml4_get_page (pml4, (void *) page) == NULL)	// 연결이 없으면
			return false;
	}

	return true;	// 전부 연결돼 있다
}

/*
 * TODO [JE]: sys_write
 *
 * 역할
 * - 첫 단계에서는 표준 출력(fd == 1)으로 지정한 바이트 수를 출력한다.
 *
 * 작성 파일
 * - pintos/userprog/syscall.c, static 함수.
 *
 * 입력 / 출력
 * - fd: 출력 대상 식별자.
 * - buffer: 사용자 버퍼.
 * - size: 출력할 바이트 수.
 * - 반환값: 출력한 바이트 수. 현재 미지원 fd는 -1.
 *
 * 로직 / 분기
 * 1. fd가 표준 출력이 아니면 현재 단계에서는 -1을 반환한다.
 * 2. size == 0이면 버퍼에 접근하지 않고 0을 반환한다.
 * 3. user_buffer_readable()로 전체 범위를 검사한다.
 * 4. 유효하지 않으면 process_terminate(-1)을 호출한다.
 * 5. putbuf(buffer, size)로 출력하고 출력한 길이를 반환한다.
 * - buffer를 문자열로 취급해서 strlen()을 호출하지 않는다.
 *
 * 호출
 * - syscall.c의 syscall_handler(), SYS_WRITE 분기.
 * - 인자는 f->R.rdi, f->R.rsi, f->R.rdx에서 전달한다.
 * - 반환값은 syscall_handler()가 f->R.rax에 기록한다.
 *
 * 기존 함수 / 자료 사용
 * - lib/kernel/console.c: putbuf().
 * - include/lib/kernel/stdio.h: putbuf() 선언.
 * - include/lib/stdio.h: STDOUT_FILENO.
 * - process.c: 새 process_terminate().
 *
 * 소유권
 * - 사용자 버퍼를 해제하지 않는다.
 *
 * 검증
 * - args-* 테스트에서 사용자 출력이 나오는지 확인한다.
 * - 실제 printf() 호출 진단도 별도로 실행한다.
 * - 일반 파일 write 테스트는 FD 기능 구현 후 검증한다.
 */
static int 
sys_write(int fd, const void *buffer, unsigned size){
	if (fd != STDOUT_FILENO)
		return -1;
	if (size == 0)
		return 0;
	if (!user_buffer_readable (buffer, size))
		process_terminate (-1);

	putbuf (buffer, size);
	return size;
}

/* The main system call interface */
void
syscall_handler(struct intr_frame *f) {
    switch (f->R.rax) {
    case SYS_WRITE:
        f->R.rax = sys_write((int) f->R.rdi,
                            (const void *) f->R.rsi,
                            (unsigned) f->R.rdx);
        break;

    case SYS_EXIT:
        /* [YYJ] 사용자가 전달한 종료 상태를 기록하고 종료한다. */
        process_terminate((int) f->R.rdi);
        break;

	case SYS_WAIT:
		/* 사용자 프로그램의 wait(pid) 요청.
		 * 인자: rdi = 기다릴 자식의 번호(pid)
		 * 반환: 자식의 종료값. 기다릴 수 없는 경우 -1 */

		// rdi에서 자식 번호를 꺼내 process_wait에 넘기고,
		// 돌아온 종료값을 rax에 넣어 사용자 프로그램의 wait()가 받게 한다
		f->R.rax = process_wait((tid_t)f->R.rdi);
		break;

	default:
        thread_exit();
    }
}