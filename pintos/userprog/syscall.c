#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "hash.h"
#include "stdbool.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "lib/stdio.h"        /* STDOUT_FILENO */
#include "lib/kernel/stdio.h" /* putbuf */
#include "threads/vaddr.h"    /* is_user_vaddr, PGSIZE */
#include "threads/mmu.h"      /* pml4_get_page */
#include "userprog/process.h" /* process_terminate */

#include "threads/palloc.h"  /* 커널 명령어 페이지 할당과 해제를 사용한다. */
#include "filesys/filesys.h" /* filesys_open 선언을 사용한다. */
#include "filesys/file.h"    /* 파일 읽기·쓰기·닫기 함수를 사용한다. */
#include "devices/input.h"   /* 표준 입력에서 input_getc를 사용한다. */
#include <debug.h>           /* NOT_REACHED를 명시적으로 가져온다. */
#include "threads/init.h" /* [YJH] power_off()로 시스템 전원을 끄기 위해 사용한다. */
#include "vm/vm.h"
#include <stdint.h> /* INT32_MAX를 사용한다. */

struct lock
    filesys_lock; /* [JE/YYJ] 파일 로딩과 파일 시스템 호출이 함께 사용할 락이다. */

void syscall_entry(void);
void syscall_handler(struct intr_frame *);

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

void syscall_init(void)
{
    lock_init(
        &filesys_lock); /* [JE/YYJ] 공통 락을 초기화한다. 락 획득은 아니다. */

    write_msr(MSR_STAR,
              ((uint64_t)SEL_UCSEG - 0x10) << 48 | ((uint64_t)SEL_KCSEG) << 32);
    write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
    write_msr(MSR_SYSCALL_MASK,
              FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* [YYJ] 사용자 범위 전체의 매핑과 사용자 접근 권한을 읽기 전에 검사한다. */
static bool user_buffer_readable(const void *buffer, size_t size)
{                    /* 문자열 한 바이트 또는 write 전체 버퍼에 사용한다. */
    if (size == 0) { /* 실제 메모리 접근이 없는 요청인지 확인한다. */
        return true; /* 빈 범위에는 주소 역참조가 필요 없다. */
    }
    if (buffer == NULL ||
        !is_user_vaddr(buffer)) { /* NULL과 커널 영역 시작 주소를 거부한다. */
        return false;             /* 유효하지 않은 시작 주소다. */
    }
    uintptr_t start = (uintptr_t)buffer; /* 주소 계산에 쓸 정수로 변환한다. */
    if (size - 1 >
        UINTPTR_MAX -
            start) {  /* 마지막 주소 계산이 정수 범위를 넘는지 검사한다. */
        return false; /* 주소가 낮은 값으로 감기는 요청을 거부한다. */
    }
    uintptr_t end =
        start + size - 1; /* 범위에 포함된 마지막 바이트 주소를 구한다. */
    if (!is_user_vaddr(
            (void *)end)) { /* 범위 끝이 커널 영역에 닿는지 검사한다. */
        return false;       /* 일부만 사용자 영역인 요청을 거부한다. */
    }
    uint64_t *pml4 =
        thread_current()->pml4; /* 현재 사용자 주소 공간을 얻는다. */
    if (pml4 == NULL) {         /* 유효한 사용자 주소 공간이 있는지 확인한다. */
        return false;           /* 조회할 페이지 테이블이 없으면 실패한다. */
    }

    /* [SHW] SPT에 해당 페이지가 있는지 확인 및 프레임 할당*/
    for (uintptr_t upage = (uintptr_t)pg_round_down(buffer); upage <= end;
         upage += PGSIZE) { /* 걸친 모든 페이지를 검사한다. */

        struct page *page;
        if ((page = spt_find_page(&thread_current()->spt, (void *)upage))) {
            if (page->frame != NULL) {
                continue;
            }
            if (!vm_claim_page((void *)upage)) {
                return false;
            }
        } else {
            if (!stack_growth((void *)upage,
                              thread_current()->syscall_prev_rsp)) {
                return false;
            }
        }
    }
    return true; /* 전체 범위를 읽을 수 있다. */
}

/* [JE/YYJ] 사용자 문자열을 검사하며 커널 페이지로 복사한다. 페이지 소유권은 호출자에게 넘긴다. */
static char *user_string_copy(const char *user_string)
{ /* 사용자 주소를 검증 전에 문자열 함수에 넘기지 않는다. */
    char *copy = palloc_get_page(
        0); /* 사용자 주소 공간 교체 이후에도 유지할 커널 페이지를 받는다. */
    if (copy == NULL) { /* 페이지 할당에 실패했는지 확인한다. */
        return NULL; /* 호출자가 fork 실패 또는 exec 종료 정책을 적용하게 한다. */
    }
    uintptr_t address = (uintptr_t)
        user_string; /* 사용자 포인터를 주소 계산용 정수로 보관한다. */
    for (size_t i = 0; i < PGSIZE;
         i++) { /* NUL을 포함해 한 페이지까지만 저장한다. */
        if (!user_buffer_readable(
                (const void *)address,
                1)) { /* 실제 읽을 한 바이트의 사용자 영역·매핑을 먼저 확인한다. */
            palloc_free_page(
                copy); /* 검증 실패 시 이미 받은 임시 페이지를 반납한다. */
            process_terminate(
                -1); /* 잘못된 사용자 포인터로 커널이 죽지 않도록 요청 프로세스만 종료한다. */
        } /* 현재 바이트의 주소 검증을 끝낸다. */
        copy[i] =
            *(const char *)
                address; /* 검증한 사용자 바이트를 커널 복사본에 기록한다. */
        if (copy[i] == '\0') { /* 문자열 끝 NUL까지 복사했는지 확인한다. */
            return copy;       /* 복사 페이지를 호출자에게 넘긴다. */
        }
        address++; /* 다음 바이트 주소로 이동한다. 검증된 주소는 KERN_BASE 미만이라 정수 최댓값에 닿지 않는다. */
    } /* 한 페이지 범위의 복사 시도를 끝낸다. */
    palloc_free_page(
        copy); /* 길이 제한 안에 NUL이 없으면 불완전한 복사본을 반납한다. */
    process_terminate(
        -1); /* 허용 길이를 초과한 명령어·이름을 비정상 종료로 처리한다. */
}

/* [YYJ] 커널이 사용자 버퍼에 결과를 쓸 수 있는지 전체 페이지의 쓰기 권한까지 검사한다. */
static bool user_buffer_writable(void *buffer, size_t size)
{ /* read의 목적지에 사용하는 검사다. */
    if (!user_buffer_readable(
            buffer,
            size)) {  /* 먼저 범위·매핑·사용자 접근 가능 여부를 검사한다. */
        return false; /* 매핑이 없으면 PTE를 읽지 않고 실패한다. */
    }
    if (size == 0) { /* 접근할 바이트가 없는지 확인한다. */
        return true; /* 길이 0이면 쓰기 권한을 검사할 페이지도 없다. */
    }
    uintptr_t end =
        (uintptr_t)buffer + size -
        1; /* 앞선 범위 검사로 오버플로가 없음을 확인한 마지막 주소다. */
    for (uintptr_t page = (uintptr_t)pg_round_down(buffer); page <= end;
         page += PGSIZE) { /* 걸친 모든 페이지를 검사한다. */
        uint64_t *pte = pml4e_walk(
            thread_current()->pml4, page,
            0); /* 페이지 테이블을 새로 만들지 않고 PTE를 조회한다. */
        if (!is_writable(
                pte)) {   /* 이미 매핑을 검증한 PTE의 쓰기 권한을 검사한다. */
            return false; /* 읽기 전용 메모리를 read 목적지로 허용하지 않는다. */
        }
    } /* 쓰기 권한 검사를 끝낸다. */
    return true; /* 전체 범위에 쓸 수 있다. */
}

/* [YYJ] 사용자 파일 이름을 검증·복사한 뒤 일반 FD를 만든다. */
static int sys_open(const char *user_name)
{ /* 문자열을 다룰 때 파일 시스템 락을 먼저 잡지 않는다. */
    char *name = user_string_copy(
        user_name);     /* 커널에서 사용할 안전한 이름 페이지를 만든다. */
    if (name == NULL) { /* 페이지 할당 실패를 확인한다. */
        return -1;      /* 아직 열린 파일이 없으므로 실패만 반환한다. */
    }
    lock_acquire(
        &filesys_lock); /* filesys_open과 FD 등록·실패 정리를 함께 보호한다. */
    struct file *file =
        filesys_open(name); /* 기존 파일을 연다. 없으면 NULL이다. */
    int fd = -1;            /* 성공하기 전까지 반환값은 실패다. */
    if (file != NULL) {     /* 파일 열기에 성공했는지 확인한다. */
        fd = process_add_file(
            file); /* 성공하면 파일 소유권을 현재 프로세스의 FD 기록에 넘긴다. */
        if (fd == -1) { /* FD 기록 생성 실패를 검사한다. */
            file_close(
                file); /* 소유권을 넘기지 못한 열린 파일을 즉시 닫는다. */
        }
    } /* 파일 열기 후 처리를 끝낸다. */
    lock_release(
        &filesys_lock); /* 사용자 코드로 복귀하기 전에 락을 반납한다. */
    palloc_free_page(
        name); /* 파일 시스템이 이름을 저장할 필요는 없으므로 복사 페이지를 반납한다. */
    return fd; /* FD 또는 -1을 반환한다. */
}

/* [YYJ] 표준 입력 또는 일반 파일을 사용자 목적지 버퍼로 읽는다. */
static int sys_read(int fd, void *buffer, unsigned size)
{ /* 반환은 읽은 바이트 수 또는 -1이다. */
    if (!user_buffer_writable(
            buffer,
            size)) { /* 파일 시스템 락을 잡기 전에 전체 목적지를 검증한다. */
        process_terminate(
            -1); /* 잘못된 주소이면 요청한 프로세스만 정리하고 종료한다. */
    }
    if (fd == 0) { /* 예약된 표준 입력을 별도로 처리한다. */
        for (unsigned i = 0; i < size;
             i++) { /* 요청 바이트 수만큼 입력을 받는다. */
            ((uint8_t *)buffer)[i] =
                input_getc(); /* 검증한 사용자 버퍼에 한 바이트씩 기록한다. */
        }
        return size; /* 표준 입력에서 채운 바이트 수를 반환한다. */
    } /* 표준 입력 처리를 끝낸다. */
    lock_acquire(&filesys_lock);              /* 파일 조회와 read를 보호한다. */
    struct file *file = process_get_file(fd); /* stdout·잘못된 FD는 NULL이다. */
    int result =
        file == NULL
            ? -1
            : file_read(file, buffer,
                        size); /* 파일이 있을 때만 기존 파일 읽기를 호출한다. */
    lock_release(&filesys_lock); /* 읽기 성공·실패와 무관하게 락을 반납한다. */
    return result;               /* 실제 읽은 바이트 수 또는 실패를 반환한다. */
}

/* [YYJ] stdout 동작을 유지하며 일반 파일 쓰기도 같은 FD 구조에 연결한다. */
static int sys_write(int fd, const void *buffer, unsigned size)
{ /* 기존 sys_write 정의 전체를 교체한다. */
    if (!user_buffer_readable(
            buffer, size)) { /* 락 획득 전에 원본 버퍼 전체를 검증한다. */
        process_terminate(
            -1); /* 잘못된 사용자 메모리는 프로세스 종료로 처리한다. */
    }
    if (fd == STDOUT_FILENO) { /* 표준 출력은 파일 객체 없이 처리한다. */
        if (size != 0) {       /* 빈 버퍼에는 접근하지 않도록 한다. */
            putbuf(
                buffer,
                size); /* 여러 프로세스의 출력이 섞이지 않도록 기존 콘솔 함수를 사용한다. */
        }
        return size; /* 출력한 바이트 수를 반환한다. */
    } /* 표준 출력 처리를 끝낸다. */
    lock_acquire(&filesys_lock); /* 일반 파일 쓰기를 공통 락으로 보호한다. */
    struct file *file = process_get_file(fd); /* stdin·잘못된 FD는 NULL이다. */
    int result =
        file == NULL
            ? -1
            : file_write(
                  file, buffer,
                  size); /* 실행 파일 쓰기 금지는 file 객체가 처리한다. */
    lock_release(
        &filesys_lock); /* 모든 일반 파일 쓰기 경로에서 락을 반납한다. */
    return result;      /* 실제 쓴 바이트 수 또는 실패를 반환한다. */
}

/* [YYJ] 표준 입출력 닫기 extra는 제외하고 일반 FD만 닫는다. */
static void sys_close(int fd)
{                                /* 닫은 뒤 반환할 상태값은 없다. */
    lock_acquire(&filesys_lock); /* file_close를 공통 락으로 보호한다. */
    process_close_file(
        fd); /* 내 목록에 있으면 닫고 없으면 아무 작업도 하지 않는다. */
    lock_release(&filesys_lock); /* 사용자에게 복귀하기 전에 락을 반납한다. */
}

/* [YYJ] 현재 프로세스가 연 파일의 전체 크기를 바이트 단위로 반환한다. */
static int sys_filesize(int fd)
{
    int result = -1; /* 유효하지 않은 FD에 사용할 기본 반환값이다. */

    lock_acquire(
        &filesys_lock); /* FD 조회와 파일 크기 조회를 공통 락으로 보호한다. */

    struct file *file = process_get_file(
        fd); /* 현재 프로세스에서 fd에 해당하는 파일을 찾는다. */

    if (file != NULL) { /* 파일을 찾았을 때만 file_length()를 호출한다. */
        result = file_length(
            file); /* 현재 읽기 위치와 무관한 전체 파일 크기를 얻는다. */
    } /* 파일이 없으면 기본값 -1을 유지한다. */

    lock_release(&filesys_lock); /* 성공과 실패 모두 락을 반납한다. */

    return result; /* 파일 크기 또는 -1을 호출한 핸들러에 반환한다. */
}

/* [YYJ] 열린 파일의 다음 읽기·쓰기 위치를 변경한다. */
static void sys_seek(int fd, unsigned position)
{
    if (position >
        INT32_MAX) { /* 현재 off_t로 표현할 수 없는 위치를 검사한다. */
        return; /* 음수로 변환되어 file_seek()의 ASSERT가 실패하는 것을 막는다. */
    } /* 위치 범위 검사를 끝낸다. */

    lock_acquire(&filesys_lock); /* FD 조회와 파일 위치 변경을 보호한다. */

    struct file *file =
        process_get_file(fd); /* 현재 프로세스에서 열린 파일을 찾는다. */

    if (file != NULL) { /* 유효한 파일에만 위치 변경을 적용한다. */
        file_seek(file,
                  (off_t)position); /* 파일 시작 기준의 절대 위치를 설정한다. */
    } /* 없는 FD는 아무 작업 없이 처리한다. */

    lock_release(&filesys_lock); /* 모든 경로에서 락을 반납한다. */
}

/* [YYJ] 열린 파일의 현재 읽기·쓰기 위치를 반환한다. */
static unsigned sys_tell(int fd)
{
    unsigned result = (unsigned)-1; /* 유효하지 않은 FD의 오류 반환값이다. */

    lock_acquire(&filesys_lock); /* FD 조회와 파일 위치 조회를 보호한다. */

    struct file *file =
        process_get_file(fd); /* 현재 프로세스에서 열린 파일을 찾는다. */

    if (file != NULL) { /* 유효한 파일에서만 위치를 조회한다. */
        result = (unsigned)file_tell(
            file); /* 파일 시작 기준의 현재 위치를 얻는다. */
    } /* 없는 FD이면 오류 반환값을 유지한다. */

    lock_release(&filesys_lock); /* 성공과 실패 모두 락을 반납한다. */

    return result; /* 현재 위치 또는 unsigned 형식의 오류값을 반환한다. */
}
//[JE]
static bool sys_create(const char *user_name, unsigned initial_size)
{

    //이름 복사 → 락 안에서 filesys_create 호출 → 락 해제 → 페이지 해제 순서
    char *name = user_string_copy(user_name);
    if (name == NULL) {
        return false;
    }

    //락 획득
    lock_acquire(&filesys_lock);

    //filesys_create 호출 → 결과를 변수에 저장
    bool success = filesys_create(name, initial_size);
    //락 해제
    lock_release(&filesys_lock);
    palloc_free_page(name);

    //결과 반환
    return success;
}

//[JE]
static bool sys_remove(const char *user_name)
{
    char *name = user_string_copy(user_name);
    if (name == NULL) {
        return false;
    }

    //락 획득
    lock_acquire(&filesys_lock);

    //filesys_remove 호출 → 결과를 변수에 저장
    bool success = filesys_remove(name);
    //락 해제
    lock_release(&filesys_lock);
    palloc_free_page(name);

    //결과 반환
    return success;
}

/* The main system call interface */
/* [JE/YYJ] 사용자 요청 번호를 해석하고 프로세스 관리 함수에 연결한다. */
void syscall_handler(struct intr_frame *f)
{
    thread_current()->syscall_prev_rsp =
        f->rsp; /* [SHW] 시스템 콜 이전에 사용자 스택의 rsp 저장*/
    /* f는 사용자 레지스터와 복귀 위치를 담는다. */
    switch (f->R.rax) { /* 진입 시 RAX에 저장된 시스템 콜 번호로 분기한다. */
    case SYS_WRITE:     /* 기존 표준 출력 시스템 콜을 유지한다. */
        f->R.rax = sys_write(
            (int)f->R.rdi, (const void *)f->R.rsi,
            (unsigned)
                f->R.rdx); /* fd·버퍼·길이를 전달하고 반환값을 기록한다. */
        break;             /* 다른 시스템 콜 분기로 이어지지 않게 한다. */
    case SYS_EXIT:         /* 사용자가 명시적으로 종료를 요청한 경우다. */
        process_terminate(
            (int)f->R
                .rdi); /* 부호 있는 int 종료값을 기록하고 종료한다. 반환하지 않는다. */
    case SYS_OPEN: /* 일반 파일 열기를 요청한 경우다. */
        f->R.rax = sys_open(
            (const char *)f->R.rdi); /* 파일 이름을 검증하고 FD를 반환한다. */
        break;                       /* open 분기를 끝낸다. */
    case SYS_READ: /* 표준 입력 또는 일반 파일 읽기를 요청한 경우다. */
        f->R.rax = sys_read((int)f->R.rdi, (void *)f->R.rsi,
                            (unsigned)f->R.rdx); /* 쓰기 권한 검사 후 읽는다. */
        break;                                   /* read 분기를 끝낸다. */
    case SYS_CLOSE:               /* 일반 파일 닫기를 요청한 경우다. */
        sys_close((int)f->R.rdi); /* 현재 프로세스가 보유한 FD를 닫는다. */
        break;                    /* close 분기를 끝낸다. */
    case SYS_FORK: { /* 이 분기의 지역 변수들이 다른 case와 섞이지 않도록 블록을 연다. */
        char *name = user_string_copy(
            (const char *)f->R
                .rdi); /* 사용자 자식 이름을 안전한 커널 페이지에 복사한다. */
        if (name == NULL) { /* 이름 복사용 페이지 할당 실패를 처리한다. */
            f->R.rax =
                TID_ERROR; /* 자식을 만들지 않고 부모에게 실패를 반환한다. */
            break;         /* fork 분기를 끝낸다. */
        } /* 이름 페이지 할당 실패 처리를 끝낸다. */
        tid_t tid = process_fork(
            name, f); /* 사용자 프레임을 전달하고 자식 복제 결과를 기다린다. */
        palloc_free_page(
            name); /* thread_create가 이름을 복사했으므로 임시 페이지는 더 필요 없다. */
        f->R.rax = tid; /* 부모에게 자식 PID 또는 TID_ERROR를 반환한다. */
        break;          /* fork 분기를 끝낸다. */
    } /* fork의 지역 변수 범위를 끝낸다. */
    case SYS_EXEC: { /* exec 명령어를 보관할 지역 변수 범위를 연다. */
        char *cmdline = user_string_copy(
            (const char *)f->R
                .rdi); /* 이전 사용자 주소 공간을 없애기 전에 명령어를 복사한다. */
        if (cmdline == NULL) { /* 커널 명령어 페이지 할당 실패를 확인한다. */
            process_terminate(
                -1); /* exec 준비 실패 시 현재 프로세스를 종료한다. */
        }
        if (process_exec(cmdline) <
            0) { /* 페이지 소유권을 넘긴다. 성공 시 돌아오지 않고 실패 시 내부에서 페이지를 정리한다. */
            process_terminate(
                -1); /* 실행 실패는 기존 프로그램으로 복귀하지 않고 -1로 종료한다. */
        }
        NOT_REACHED(); /* 기존 process_exec 계약상 성공·실패 모두 여기로 정상 복귀하지 않는다. */
    } /* exec의 지역 변수 범위를 끝낸다. */
    case SYS_WAIT: /* 자식의 종료 결과를 요청한 경우다. */
        f->R.rax = process_wait(
            (tid_t)f->R
                .rdi); /* 직접 자식을 한 번만 기다리는 기존 함수를 호출한다. */
        break;         /* 기다린 결과를 가지고 사용자 코드로 복귀한다. */
    case SYS_CREATE:
        f->R.rax = sys_create((const char *)f->R.rdi, (unsigned)f->R.rsi);
        break;
    case SYS_REMOVE:
        f->R.rax = sys_remove((const char *)f->R.rdi);
        break;
    case SYS_FILESIZE:
        f->R.rax = sys_filesize((int)f->R.rdi);
        break;
    case SYS_SEEK:
        sys_seek((int)f->R.rdi, (unsigned)f->R.rsi);
        break;
    case SYS_TELL:
        f->R.rax = sys_tell((int)f->R.rdi);
        break;
    case SYS_HALT: /* [YJH] 현재 프로세스가 아니라 시스템 전체 종료를 요청한 경우다. */
        power_off(); /* [YJH] process_terminate가 아니라 전원을 끈다. 종료 메시지 없이 반환하지 않는다. */
    default:         /* 아직 지원하지 않는 번호를 요청한 경우다. */
                     // thread_exit();
        process_terminate(
            -1); /* 미지원 호출은 현재 프로세스의 비정상 종료로 처리한다. */
    }
}