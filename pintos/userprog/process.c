#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "filesys/off_t.h"
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#include "threads/malloc.h" /* [YYJ] */
#include "threads/synch.h"  /* [YYJ] */
#include "userprog/syscall.h" /* [YYJ] syscall.c에 정의한 공통 파일 시스템 락 선언을 가져온다. */

#ifdef VM
#include "vm/vm.h"
#endif

/* [YYJ] 자식 스레드가 해제된 뒤에도 종료 상태를 보관한다. */
struct child_status {
    tid_t tid;               // 어느 자식의 기록인지
    int exit_status;         // 자식의 종료 결과
    struct semaphore exited; // 자식 종료를 부모에게 알리는 세마포어
    struct lock ref_lock;    // 참조 수를 변경할 때 사용할 락
    unsigned refs;           // 이 객체를 유지할 책임이 있는 참조 수
    struct list_elem elem;   // 부모의 자식 목록에 연결할 요소
};

/* [YYJ] 새 자식에게 명령어와 공유 종료 기록을 함께 전달한다. */
struct process_start_info {
    char *cmdline;
    struct child_status *status;
};

struct
    process_file { /* [YYJ] 프로세스별 FD 번호와 해당 파일 핸들을 연결한다. */
    int fd;        /* 사용자에게 반환한 2 이상의 파일 디스크립터다. */
    struct file *file;     /* 이 기록이 소유하는 열린 파일 객체다. */
    struct list_elem elem; /* thread의 open_files 목록에 연결할 요소다. */
}; /* process_file 정의를 끝낸다. */

/* [YYJ] 부모가 만들고, 자식의 복제 결과를 확인한 뒤 부모가 해제하는 전달 객체다. */
struct
    fork_start_info { /* 서로 별개인 실행 문맥·종료 기록·완료 알림을 한곳에 묶는다. */
    struct thread *parent; /* 복제 중인 원본 주소 공간을 가진 부모 스레드다. */
    struct intr_frame
        frame; /* 부모 syscall 진입 당시의 사용자 레지스터 값 복사본이다. */
    struct child_status
        *status; /* 부모·자식이 각 참조 하나를 보유할 종료 기록이다. */
    struct semaphore
        completed; /* 복제 준비 완료 알림이다. status->exited와 역할이 다르다. */
    bool success;  /* 주소 공간 등 현재 단계의 복제가 성공했는지 기록한다. */
}; /* 이 객체의 수명은 부모가 completed를 받고 결과를 읽을 때까지다. */
/* 선언 연결: 기존 static void __do_fork (void *);, child_status_create(), child_status_put()의 전방 선언이 
이미 있으므로 그대로 사용한다. fork_start_info는 process.c 안에서만 사용하므로 헤더에 노출하지 않는다. */

static void process_cleanup(void);
static bool load(const char *file_name, struct intr_frame *if_);
static void initd(void *f_name);
static void __do_fork(void *);

static void child_status_put(struct child_status *status); /* [YYJ] */
static struct child_status *child_status_create(void);     /* [YYJ] */
void process_terminate(int status) NO_RETURN;              /* [YYJ] */
static bool setup_arguments(struct intr_frame *if_, char **argv,
                            int argc); /* [YYJ] */
static bool process_duplicate_files(
    struct thread *parent); /* 자식에서 부모 FD와 실행 파일 핸들을 복제한다. */
static void process_close_all_files(
    void); /* 종료할 때 현재 프로세스의 일반 FD들을 모두 닫는다. */

/* General process initializer for initd and other process. */
static void process_init(void)
{
    struct thread *current = thread_current();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t process_create_initd(const char *file_name)
{ /* [YYJ] 최초 사용자 프로세스를 생성하고 부모의 자식 목록에 등록한다. */
    // char *fn_copy;
    // tid_t tid;
    /* 스레드 이름에는 전체 명령어 대신 프로그램 이름을 사용한다. */
    const char *program = file_name;

    while (*program == ' ') {
        program++;
    }

    if (*program == '\0') {
        return TID_ERROR;
    }

    char name[16];
    strlcpy(name, program, sizeof name);

    char *space = strchr(name, ' ');
    if (space != NULL) {
        *space = '\0';
    }

    /* 명령어는 생성 중 변경되지 않도록 커널 페이지에 복사한다. */
    char *fn_copy = palloc_get_page(0);
    if (fn_copy == NULL) {
        return TID_ERROR;
    }

    if (strlcpy(fn_copy, file_name, PGSIZE) >= PGSIZE) {
        palloc_free_page(fn_copy);
        return TID_ERROR;
    }

    // /* Make a copy of FILE_NAME.
    //  * Otherwise there's a race between the caller and load(). */
    // fn_copy = palloc_get_page (0);
    // if (fn_copy == NULL)
    // 	return TID_ERROR;
    // strlcpy (fn_copy, file_name, PGSIZE);

    // /* Create a new thread to execute FILE_NAME. */
    // tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);
    // if (tid == TID_ERROR)
    // 	palloc_free_page (fn_copy);
    // return tid;

    /* 부모·자식이 공유할 기록을 생성한다. 참조 수는 2다. */
    struct child_status *status = child_status_create();
    if (status == NULL) {
        palloc_free_page(fn_copy);
        return TID_ERROR;
    }

    /* 자식 시작 함수에 넘길 자료를 묶는다. */
    struct process_start_info *start = malloc(sizeof *start);
    if (start == NULL) {
        child_status_put(status);
        child_status_put(status);
        palloc_free_page(fn_copy);
        return TID_ERROR;
    }

    start->cmdline = fn_copy;
    start->status = status;

    /* 자식이 생성 직후 종료할 수도 있으므로 먼저 목록에 등록한다. */
    list_push_back(&thread_current()->children, &status->elem);

    tid_t tid = thread_create(name, PRI_DEFAULT, initd, start);

    if (tid == TID_ERROR) {
        /* 자식에게 넘기지 못했으므로 부모가 모든 할당을 정리한다. */
        list_remove(&status->elem);
        free(start);
        palloc_free_page(fn_copy);
        child_status_put(status);
        child_status_put(status);
        return TID_ERROR;
    }

    /* 부모 참조가 남아 있으므로 자식이 먼저 종료해도 기록은 유효하다. */
    status->tid = tid;
    return tid;
}

/* A thread function that launches first user process. */
static void initd(void *f_name)
{

    /* [YYJ] 부모에게 받은 자료를 인수하고 최초 프로그램을 실행한다. */
    struct process_start_info *start = f_name;
    struct thread *curr = thread_current();

    /* 명령어는 process_exec()이 사용하고 해제한다. */
    char *cmdline = start->cmdline;

    /* 부모와 공유하는 내 종료 기록을 연결한다. */
    curr->as_child = start->status;
    curr->is_user_process = true;

    /* 내용은 인수했으므로 전달용 포장 구조체만 해제한다. */
    free(start);
#ifdef VM
    supplemental_page_table_init(&curr->spt);
#endif

    process_init();

    if (process_exec(cmdline) < 0) {
        process_terminate(
            -1); /* [YYJ] 실행 준비에 실패한 경우에만 비정상 종료한다. 기존 PANIC은 커널 전체를 멈춘다. */
    }
    NOT_REACHED();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
/* [YYJ] 자식을 생성하고, 자식의 복제 성공 여부를 확인한 뒤 부모에게 반환한다. */
tid_t process_fork(const char *name, struct intr_frame *if_ UNUSED)
{
    /* Clone current thread to new thread.*/
    // return thread_create (name,
    // 		PRI_DEFAULT, __do_fork, thread_current ());

    struct child_status *status =
        child_status_create(); /* 부모용 1개·자식용 예약 1개의 참조를 만든다. */
    if (status == NULL) {      /* 종료 기록을 만들지 못했는지 확인한다. */
        return TID_ERROR;      /* 아직 다른 자원이 없으므로 실패만 반환한다. */
    }
    struct fork_start_info *start = malloc(
        sizeof *start);  /* 큰 레지스터 복사본은 커널 스택 대신 힙에 둔다. */
    if (start == NULL) { /* 전달 객체를 만들지 못하면 예약 참조도 취소한다. */
        child_status_put(status); /* 부모용 참조를 반납한다. */
        child_status_put(
            status);      /* 아직 자식에게 넘기지 못한 참조도 반납한다. */
        return TID_ERROR; /* 생성 실패를 반환한다. */
    } /* 전달 객체 할당 실패 처리를 끝낸다. */
    start->parent = thread_current(); /* 복제할 부모 스레드 주소를 보관한다. */
    start->frame =
        *if_; /* 사용자 레지스터 값을 복사한다. parent->tf는 사용하지 않는다. */
    start->status =
        status; /* 자식이 자기 종료 기록을 연결할 수 있도록 주소를 보관한다. */
    start->success = false; /* 명시적인 성공 알림 전까지는 실패 상태로 둔다. */
    sema_init(&start->completed,
              0); /* 부모가 복제 완료 알림을 기다리도록 0으로 초기화한다. */
    list_push_back(
        &thread_current()->children,
        &status
             ->elem); /* 자식이 즉시 종료해도 부모 참조가 남도록 먼저 등록한다. */
    tid_t tid = thread_create(name, PRI_DEFAULT, __do_fork,
                              start); /* 자식에게 전달 객체 주소를 넘긴다. */
    if (tid == TID_ERROR) { /* 자식 스레드 자체를 생성하지 못한 경우다. */
        list_remove(
            &status->elem); /* 성공한 자식 목록에 실패 기록을 남기지 않는다. */
        free(start);        /* 자식이 받지 못한 전달 객체를 부모가 해제한다. */
        child_status_put(status); /* 부모용 참조를 반납한다. */
        child_status_put(status); /* 자식용 예약 참조도 부모가 반납한다. */
        return TID_ERROR;         /* 스레드 생성 실패를 반환한다. */
    } /* 스레드 생성 실패 처리를 끝낸다. */
    status->tid = tid; /* 반환할 자식 번호와 종료 기록을 연결한다. */
    sema_down(
        &start
             ->completed); /* 파일 시스템 락을 보유하지 않은 상태로 복제 결과를 기다린다. */
    bool success =
        start
            ->success; /* 전달 객체를 해제하기 전에 결과를 지역 변수로 복사한다. */
    free(start);       /* 자식은 완료 알림 뒤 이 객체를 다시 사용하지 않는다. */
    if (!success) {    /* 자식은 만들어졌지만 자원 복제가 실패한 경우다. */
        list_remove(
            &status
                 ->elem); /* 실패한 fork를 wait 가능한 자식으로 남기지 않는다. */
        child_status_put(
            status); /* 부모 참조만 반납한다. 자식 참조는 자식 종료 경로가 반납한다. */
        return TID_ERROR; /* 자식 tid 대신 실패를 반환한다. */
    } /* 복제 실패 처리를 끝낸다. */
    return tid; /* 성공한 자식의 번호를 부모 사용자 프로그램에 돌려준다. */
} /* process_fork 정의를 끝낸다. */

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. 
 *  Project 2의 즉시 페이지 복제 방식에서 사용한다. */
/* [YYJ] 부모 사용자 페이지 하나를 자식의 별도 물리 페이지에 복사한다. */
static bool duplicate_pte(uint64_t *pte, void *va, void *aux)
{                                              /* 페이지 순회 콜백이다. */
    struct thread *current = thread_current(); /* 현재 실행 중인 자식이다. */
    struct thread *parent =
        (struct thread *)aux; /* aux로 전달받은 부모 스레드다. */
    void *parent_page;        /* 부모 페이지를 커널에서 접근할 주소다. */
    void *newpage;            /* 자식에게 새로 할당할 페이지 주소다. */
    bool writable;            /* 원본 페이지의 쓰기 가능 여부다. */

    /* 1. TODO: If the parent_page is kernel page, then return immediately. */
    /* 1. 커널 가상 주소라면 복제하지 않고 건너뛴다. */
    if (is_kernel_vaddr(va)) { /* 사용자 페이지인지 먼저 구분한다. */
        return true;           /* 건너뛰기는 실패가 아니므로 순회를 계속한다. */
    }

    /* 2. Resolve VA from the parent's page map level 4. */
    /* 2. 부모의 페이지 테이블에서 va에 연결된 페이지를 찾는다. */
    parent_page =
        pml4_get_page(parent->pml4, va); /* 커널 접근 주소를 얻는다. */
    if (parent_page == NULL) {           /* 원본 페이지 조회 실패를 확인한다. */
        return false;                    /* 페이지 복제 순회를 중단한다. */
    }

    /* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */
    /* 3. 자식 전용 사용자 페이지를 할당한다. */
    newpage = palloc_get_page(PAL_USER); /* 사용자 메모리 풀에서 할당한다. */
    if (newpage == NULL) {               /* 메모리 할당 실패를 확인한다. */
        return false; /* 아직 새 페이지가 없으므로 바로 실패를 반환한다. */
    }

    /* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */
    /* 4. 페이지 내용을 복사하고 원본의 쓰기 권한을 확인한다. */
    memcpy(newpage, parent_page, PGSIZE); /* 한 페이지 전체를 복사한다. */
    writable = is_writable(pte) != 0;     /* 원본 PTE의 쓰기 권한을 읽는다. */

    /* 5. Add new page to child's page table at address VA with WRITABLE
	     permission. */
    /* 5. 자식의 동일한 가상 주소에 새 페이지를 연결한다. */
    if (!pml4_set_page(current->pml4, va, newpage,
                       writable)) { /* 매핑을 시도한다. */
        /* 6. TODO: if fail to insert page, do error handling. */
        /* 6. 매핑 실패 시 아직 연결하지 못한 페이지를 직접 해제한다. */
        palloc_free_page(
            newpage); /* 주소 공간에 연결되지 않은 할당을 반납한다. */
        return false; /* 앞서 매핑한 페이지는 자식의 종료 경로에서 정리한다. */
    } /* 매핑 실패 처리를 끝낸다. */
    return true; /* 매핑한 페이지는 이제 자식 주소 공간이 소유한다. */
}
#endif /* Project 2 전용 복제 구간을 끝낸다. */

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
/* [YYJ] 새 자식에서 부모의 사용자 실행 상태를 복제한다. */
static void __do_fork(void *aux)
{                          /* 스레드 시작 함수 형식에 맞춰 void *를 받는다. */
    struct intr_frame if_; /* 자식이 사용자 모드로 복귀할 실행 문맥이다. */
    struct fork_start_info *start = aux;       /* 부모가 만든 전달 객체다. */
    struct thread *parent = start->parent;     /* 복제할 부모 스레드 주소다. */
    struct thread *current = thread_current(); /* 현재 실행 중인 자식이다. */

    /* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
    /* TODO [YYJ 번역]: process_fork가 받은 사용자 실행 문맥을 전달받는다. */
    struct intr_frame *parent_if =
        &start->frame; /* 부모가 복사해 둔 사용자 레지스터다. */
    bool succ = true;  /* 자원 복제 성공 여부를 보관한다. */

    /* TODO [YYJ 추가]: 실패하더라도 종료 기록을 정리할 수 있도록 연결한다. */
    current->as_child =
        start->status; /* 자식이 소유할 종료 기록 참조를 연결한다. */
    current->is_user_process =
        false; /* 준비 실패한 자식의 종료 메시지는 출력하지 않는다. */

    process_init(); /* 자원 복제 전에 기존 공통 초기화를 수행한다. */

    /* 1. Read the cpu context to local stack. */
    /* TODO [YYJ 번역]: 부모의 사용자 실행 문맥을 자식의 지역 변수로 복사한다. */
    memcpy(&if_, parent_if,
           sizeof(struct intr_frame)); /* 레지스터 값을 실제로 복사한다. */

    /* 2. Duplicate PT */
    /* TODO [YYJ 번역]: 자식의 페이지 테이블을 생성하고 부모의 주소 공간을 복제한다. */
    current->pml4 = pml4_create(); /* 자식 전용 페이지 테이블을 만든다. */
    if (current->pml4 == NULL) {   /* 페이지 테이블 생성 실패를 검사한다. */
        goto error;                /* 공통 실패 알림·종료 경로로 이동한다. */
    }

    process_activate(
        current); /* 자식의 주소 공간과 커널 진입 스택 정보를 활성화한다. */

#ifdef VM
    supplemental_page_table_init(&current->spt);
    if (!supplemental_page_table_copy(&current->spt, &parent->spt)) {
        goto error;
    }
#else /* 현재 Project 2에서는 사용자 페이지를 즉시 복제한다. */
    if (!pml4_for_each(parent->pml4, duplicate_pte,
                       parent)) { /* 각 페이지를 duplicate_pte로 복제한다. */
        goto error; /* 한 페이지라도 실패하면 전체 fork를 실패 처리한다. */
    }
#endif /* 주소 공간 복제 방식의 분기를 끝낸다. */

    /* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/
    /* TODO [YYJ 번역]:
     * 파일 객체 복제에는 include/filesys/file.h의 file_duplicate를 사용한다.
     * 자식의 자원 복제 성공 여부가 확정되기 전까지 부모는 fork에서 반환하면 안 된다. */
    lock_acquire(&filesys_lock); /* 파일 객체 복제를 공통 락으로 보호한다. */
    succ = process_duplicate_files(
        parent);                 /* 부모 FD와 실행 파일 보유권을 복제한다. */
    lock_release(&filesys_lock); /* 성공·실패와 관계없이 락을 반납한다. */
                                 // process_init ();

    /* Finally, switch to the newly created process. */
    // if (succ)
    // 	do_iret (&if_);
    if (!succ) {    /* 파일 복제 실패를 검사한다. */
        goto error; /* 부분 복제한 자원은 자식 종료 경로에서 정리한다. */
    }

    /* TODO [YYJ 추가]: 자식 반환값을 설정하고 부모에게 복제 성공을 알린다. */
    if_.R.rax = 0;                   /* 자식에게 보이는 fork 반환값은 0이다. */
    current->is_user_process = true; /* 사용자 실행 준비가 끝났다고 표시한다. */

    /* Finally, switch to the newly created process. */
    /* TODO [YYJ 번역]: 준비된 자식의 실행 문맥으로 사용자 코드에 복귀한다. */
    if (succ) {                /* 모든 자원 복제가 성공한 경우다. */
        start->success = true; /* 부모가 확인할 성공 결과를 먼저 기록한다. */
        sema_up(
            &start
                 ->completed); /* 부모를 깨운다. 이후 start에 다시 접근하지 않는다. */
        do_iret(&if_); /* 복제한 레지스터로 자식의 사용자 코드에 복귀한다. */
        NOT_REACHED(); /* 정상적으로 사용자 모드에 복귀하면 도달하지 않는다. */
    } /* 성공 경로를 끝낸다. */

error: /* 모든 복제 실패가 모이는 공통 경로다. */
       // thread_exit ();
    /* TODO [YYJ 추가]: 실패해도 부모에게 알리고 자식의 부분 자원을 정리한다. */
    start->success = false; /* 부모가 TID_ERROR를 반환하도록 실패를 기록한다. */
    sema_up(&start->completed); /* 실패 시에도 부모의 대기를 반드시 해제한다. */
    process_terminate(
        -1); /* 이후 start를 읽지 않고 기존 종료 경로로 정리한다. */
} /* __do_fork 정의를 끝낸다. */
/* process_init()는 현재 실질적인 초기화가 없는 기존 함수다. FD 목록은 아래 
thread.c 초기화 코드에서 스레드 생성 시 준비하고 부모 FD는 완료 알림 전에 
복제한다. 성공 알림 이후에 복제하면 부모가 FD를 닫거나 변경할 수 있어 잘못된다. */

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int process_exec(void *f_name)
{
    /* initd()에서 전달받은 커널 명령어 복사본이다. */
    char *file_name = f_name;
    // bool success;

    /* 큰 지역 배열 대신 별도 페이지에 인자 주소들을 저장한다. */
    char **argv = palloc_get_page(0);
    if (argv == NULL) {
        palloc_free_page(file_name);
        return -1;
    }

    int argc = 0;
    const size_t max_args = PGSIZE / sizeof *argv - 1;
    char *save_ptr = NULL;
    bool success = false;

    // /* We cannot use the intr_frame in the thread structure.
    //  * This is because when current thread rescheduled,
    //  * it stores the execution information to the member. */
    /* 사용자 실행 문맥을 초기화한다. */
    //	struct intr_frame _if;
    struct intr_frame _if = {0};
    _if.ds = _if.es = _if.ss = SEL_UDSEG;
    _if.cs = SEL_UCSEG;
    _if.eflags = FLAG_IF | FLAG_MBS;

    /* 공백으로 단어를 나눈다. 연속 공백은 건너뛴다. */
    for (char *token = strtok_r(file_name, " ", &save_ptr); token != NULL;
         token = strtok_r(NULL, " ", &save_ptr)) {

        if ((size_t)argc >= max_args) {
            goto done;
        }

        argv[argc++] = token;
    }

    /* 빈 명령어는 실행할 수 없다. */
    if (argc == 0) {
        goto done;
    }

    argv[argc] = NULL;

    // /* We first kill the current context */
    /* 기존 주소 공간을 정리한다. */
    process_cleanup();

    // /* And then load the binary */
    /* 전체 명령어가 아니라 첫 단어인 실행 파일 이름을 전달한다. */
    // success = load (file_name, &_if);
    lock_acquire(
        &filesys_lock); /* [JE/YYJ] ELF 열기·읽기·닫기 중 다른 파일 시스템 호출을 막는다. */
    success =
        load(argv[0],
             &_if); /* [YJH] 기존 로더가 실행 파일과 사용자 스택을 준비한다. */
    lock_release(
        &filesys_lock); /* load가 성공하든 실패하든 여기서 락을 반납한다. */
    /* 현재 load 내부에서 락을 잡지 않으므로 위 위치에서 한 번만 잡는다. JE가 load 내부에 같은
	락을 넣으면 중복 획득을 제거해야 한다. process_wait()와 fork 완료 대기 중에는 이 락을 보유하지 않는다. */

    // /* If load failed, quit. */
    // palloc_free_page (file_name);
    // if (!success)
    // 	return -1;

    /* load()가 준비한 사용자 스택에 인자들을 배치한다. */
    if (success) {
        success = setup_arguments(&_if, argv, argc);
    }

done:
    /* 사용자 스택으로 복사가 끝났거나 실패했으므로 임시 메모리를 정리한다. */
    palloc_free_page(argv);
    palloc_free_page(file_name);

    if (!success) {
        return -1;
    }

    /* Start switched process. */
    /* 준비한 레지스터와 스택으로 사용자 프로그램을 시작한다. */
    do_iret(&_if);
    NOT_REACHED();
}

/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int process_wait(tid_t child_tid UNUSED)
{
    /* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */

    /* [YYJ] 직접 만든 자식의 종료를 한 번 기다리고 종료값을 받는다. */
    struct thread *curr = thread_current();

    for (struct list_elem *e = list_begin(&curr->children);
         e != list_end(&curr->children); e = list_next(e)) {
        struct child_status *status = list_entry(e, struct child_status, elem);

        if (status->tid != child_tid) {
            continue;
        }

        /* 먼저 목록에서 빼서 같은 자식을 다시 기다리지 못하게 한다. */
        list_remove(&status->elem);

        /* 자식이 아직 종료하지 않았다면 여기서 대기한다. */
        sema_down(&status->exited);

        /* 깨어난 뒤 종료 결과를 지역 변수에 복사한다. */
        int result = status->exit_status;

        /* 부모가 보유하던 참조를 반납한다. */
        child_status_put(status);

        return result;
    }

    /* 내 자식이 아니거나 이미 기다린 자식이다. */
    return -1;
}

/* Exit the process. This function is called by thread_exit (). */
/* [YYJ] 프로세스 자원을 정리하고 부모에게 종료를 알린다. */
void process_exit(void)
{
    struct thread *curr = thread_current();
    /* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
    /* 사용자 프로세스의 종료 메시지만 출력한다. */
    if (curr->is_user_process) {
        printf("%s: exit(%d)\n", curr->name, curr->exit_status);
    }

    process_close_all_files(); /* [YYJ] 부모에게 종료를 알리기 전에 일반 FD와 기록을 모두 정리한다. */

    /* 기존 주소 공간 정리 순서는 유지한다. */
    process_cleanup();

    /* 내가 기다리지 않은 자식들의 부모 측 참조를 반납한다.
     * 자식이 살아 있다면 자식 측 참조가 객체를 유지한다. */
    while (!list_empty(&curr->children)) {
        struct list_elem *e = list_pop_front(&curr->children);
        struct child_status *child = list_entry(e, struct child_status, elem);

        child_status_put(child);
    }

    /* 내 부모에게 종료 결과를 전달한다. */
    struct child_status *status = curr->as_child;

    if (status != NULL) {
        curr->as_child = NULL;
        status->exit_status = curr->exit_status;

        /* 부모를 깨운다. 부모가 아직 기다리지 않아도 알림은 남는다. */
        sema_up(&status->exited);

        /* 자식인 내가 보유하던 참조를 반납한다. */
        child_status_put(status);
    }
}

/*
 * TODO [YYJ]: process_terminate
 *
 * 역할
 * - 종료 상태를 기록한 뒤 현재 프로세스를 종료한다.
 *
 * 작성 / 선언 파일
 * - 구현: pintos/userprog/process.c.
 * - 공개 선언: pintos/include/userprog/process.h.
 *
 * 입력 / 출력
 * - status: 부모에게 전달할 종료 상태.
 * - 반환하지 않는다.
 *
 * 로직 / 분기
 * 1. 현재 스레드의 exit_status에 status를 저장한다.
 * 2. thread_exit()을 호출한다.
 * - 실제 자원 정리와 종료 알림은 process_exit()이 담당한다.
 *
 * 호출
 * - syscall.c의 syscall_handler(), SYS_EXIT 분기.
 * - syscall.c의 sys_write(), 사용자 주소 검증 실패 분기.
 * - process.c의 initd(), 최초 프로그램 실행 실패 분기.
 * - 이후 사용자 예외 종료 경로도 이 종료 정책에 연결한다.
 *
 * 기존 함수 / 자료 사용
 * - threads/thread.c: thread_current(), thread_exit().
 *
 * 주의 / 소유권
 * - thread_exit()이 이미 process_exit()을 호출한다.
 * - process_exit()에서 이 함수를 호출하면 재귀 종료가 된다.
 * - 여기서 종료 메시지와 자원 정리를 중복 실행하지 않는다.
 *
 * 검증
 * - exit 테스트: exit(57) 이후 사용자 코드가 실행되지 않아야 한다.
 * - 부모의 process_wait() 반환값도 57인지 별도 확인한다.
 * - 잘못된 사용자 접근의 종료 상태는 -1이어야 한다.
 */
void process_terminate(int status)
{ /* status: 전달받은 정수 종료값. */
    /* 현재 스레드의 주소를 얻고, 그 구조체의 종료값 필드에 저장한다. */
    thread_current()->exit_status = status;

    /* 내부에서 process_exit()을 호출하고 스레드를 종료한다.
     * 이 함수 호출 이후에는 여기로 돌아오지 않는다. */
    thread_exit();
}

/*
 * TODO [YYJ]: child_status_create
 *
 * 역할
 * - 부모와 자식이 공유할 종료 상태 객체를 생성한다.
 *
 * 작성 파일
 * - pintos/userprog/process.c, static 함수.
 *
 * 입력 / 출력
 * - 인자 없음.
 * - 반환값: 초기화된 객체 주소. 할당 실패 시 NULL.
 *
 * 로직 / 분기
 * 1. 객체를 동적 할당한다. 실패하면 NULL을 반환한다.
 * 2. tid를 TID_ERROR, exit_status를 -1로 설정한다.
 * 3. exited 세마포어를 0으로 초기화한다.
 * 4. 참조 수 보호용 락을 초기화한다.
 * 5. 부모 참조와 자식에게 넘길 참조를 합쳐 refs = 2로 설정한다.
 *
 * 호출
 * - process.c의 process_create_initd(), thread_create() 호출 전.
 * - 이후 process_fork()에서도 같은 규칙으로 재사용한다.
 *
 * 기존 함수 / 자료 사용
 * - threads/malloc.c: malloc().
 * - threads/synch.c: sema_init(), lock_init().
 *
 * 소유권
 * - 성공하면 부모와 자식에게 참조 하나씩을 예약한다.
 * - 자식 생성 실패 시 호출자가 예약한 두 참조를 모두 놓아야 한다.
 *
 * 검증
 * - 초기 프로세스 생성 및 정상 종료.
 * - 자식 생성 실패 시 객체·명령어·시작 정보 누수 여부 확인.
 */

static struct child_status *child_status_create(void)
{

    /* 구조체 하나 크기의 메모리를 할당하고 그 주소를 child에 저장한다. sizeof *child는 구조체 크기이며, 실제 메모리를 읽는 표현은 아니다. */
    struct child_status *child = malloc(sizeof *child);

    /* 메모리 할당에 실패했는지 확인한다. */
    if (child == NULL) {
        return NULL; /* 실패를 호출자에게 알리고 즉시 종료한다. */
    }

    child->tid =
        TID_ERROR; /* 아직 자식을 생성하기 전이므로 유효한 tid가 없다고 기록한다. */
    child->exit_status =
        -1; /* 종료 결과의 기본값을 -1로 둔다. 종료 완료를 뜻하지는 않는다. */

    /* 구조체 안의 세마포어 주소를 전달하여 초기값을 0으로 설정한다. 아직 종료 알림이 없으므로, 이후 sema_down()은 대기하게 된다. */
    sema_init(&child->exited, 0);
    /* 참조 수 변경을 보호할 락을 초기화한다. 락 획득은 아니다. */
    lock_init(&child->ref_lock);

    /* 부모가 보유할 참조 1개와 자식에게 넘길 참조 1개를 예약한다. */
    child->refs = 2;
    /* 초기화한 객체의 주소를 호출자에게 반환한다.
     * malloc()으로 할당했으므로 함수가 끝나도 객체는 유지된다. */
    return child;
}

/*
 * TODO [YYJ]: child_status_put
 *
 * 역할
 * - 종료 상태 객체의 참조 하나를 내려놓고 마지막 참조이면 해제한다.
 *
 * 작성 파일
 * - pintos/userprog/process.c, static 함수.
 *
 * 입력 / 출력
 * - status: 호출자가 참조 하나를 보유한 종료 상태 객체.
 * - 반환값 없음.
 *
 * 로직 / 분기
 * 1. ref_lock을 획득한다.
 * 2. 참조 수를 1 감소시키고 마지막 참조인지 저장한다.
 * 3. 락을 해제한다.
 * 4. 마지막 참조였다면 객체를 해제한다.
 *
 * 호출
 * - process_wait(): 종료 상태를 읽은 뒤 부모 참조 반납.
 * - process_exit(): 자식 종료 알림 뒤 자신의 자식 측 참조 반납.
 * - process_exit(): 기다리지 않은 자식들의 부모 측 참조 반납.
 * - 생성 실패 정리 경로: 넘기지 못한 예약 참조 반납.
 *
 * 기존 함수 / 자료 사용
 * - threads/synch.c: lock_acquire(), lock_release().
 * - threads/malloc.c: free().
 *
 * 주의 / 소유권
 * - 호출 후에는 반납한 참조를 통해 객체에 접근하지 않는다.
 * - 해제할 객체 안의 락을 잡은 상태로 free()하지 않는다.
 *
 * 검증
 * - 자식이 먼저 종료한 경우와 부모가 먼저 기다린 경우 모두 확인.
 * - 이후 부모 선종료·미대기 자식 시나리오에서도 누수가 없어야 한다.
 */
/* [YYJ] 참조 하나를 반납하고, 마지막 참조라면 객체를 해제한다. */
static void child_status_put(struct child_status *status)
{
    /* status는 정수 종료값이 아니라 종료 상태 객체의 주소다. 호출자가 NULL을 넘기는 잘못된 사용을 검사한다. */
    ASSERT(status != NULL);
    /* 참조 수를 확인하고 변경하는 동안 사용할 락을 획득한다. 다른 실행 흐름이 같은 락을 보유하고 있으면 기다린다. */
    lock_acquire(&status->ref_lock);

    /* 반납할 참조가 존재하는지 검사한다. refs는 unsigned이므로 0에서 감소시키면 큰 양수로 바뀐다. */
    ASSERT(status->refs > 0);
    /* 호출자가 보유한 참조 하나를 반납한다. */
    status->refs--;

    /* 마지막 참조인지 락을 잡은 상태에서 판단하여 지역 변수에 저장한다. 0이면 true, 아직 참조가 남아 있으면 false다. */
    bool last_ref = (status->refs == 0);
    /* 참조 수 변경이 끝났으므로 락을 해제한다. 락도 객체 안에 있으므로 객체를 해제하기 전에 수행해야 한다. */
    lock_release(&status->ref_lock);

    /* 더 이상 객체를 유지할 참조가 없는 경우에만 해제한다. */
    if (last_ref) {
        free(status); /* malloc()으로 받은 메모리를 반납한다. */
    }
}

/* [YYJ] 조회만 한다. 반환 포인터를 사용하는 동안 호출자가 filesys_lock을 유지한다. */
struct file *process_get_file(int fd)
{ /* 현재 프로세스의 FD에 대응하는 파일을 찾는다. */
    struct list *files =
        &thread_current()->open_files; /* 현재 프로세스의 목록을 얻는다. */
    for (struct list_elem *e = list_begin(files); e != list_end(files);
         e = list_next(e)) { /* 기록들을 순회한다. */
        struct process_file *entry =
            list_entry(e, struct process_file,
                       elem);  /* 목록 요소에서 실제 기록을 얻는다. */
        if (entry->fd == fd) { /* 요청 번호와 같은지 비교한다. */
            return entry
                ->file; /* 소유권 이전 없이 열린 파일 주소를 반환한다. */
        }
    } /* 검색을 끝낸다. */
    return NULL; /* 이 프로세스에 없는 FD다. */
} /* process_get_file 정의를 끝낸다. */

/* [YYJ] 빈 번호 중 가장 작은 2 이상의 번호를 배정한다. 호출자는 filesys_lock을 보유한다. */
int process_add_file(struct file *file)
{ /* 성공 때만 file의 소유권을 가져온다. */
    struct process_file *entry =
        malloc(sizeof *entry); /* 새 FD 기록 하나를 할당한다. */
    if (entry == NULL) {       /* 기록 할당 실패를 확인한다. */
        return -1;             /* 파일은 호출자가 닫도록 소유권을 유지시킨다. */
    }
    /* [YJH] 목록 전체를 번호마다 다시 훑으면 열린 파일이 n개일 때 번호 하나를 정하는 데 약 n*n/2번 비교한다.
     * [YJH] multi-oom은 세대마다 파일을 126개씩 열고 자식이 부모의 열린 파일을 물려받아 n이 천 단위로 커지므로,
     *       fd 배정이 제곱으로 느려져 600초 안에 끝나지 않았다(세대당 8.6초에서 13.9초로 증가).
     * [YJH] open_files를 fd 오름차순으로 유지하고 한 번만 훑어 가장 작은 빈 번호를 찾는다. 비교 횟수는 약 n번이다.
     * [YJH] 순서를 유지하는 조건: 이 목록에 기록을 넣는 곳은 여기와 process_duplicate_files뿐이고,
     *       후자는 부모 목록의 순서 그대로 복사하므로 오름차순이 깨지지 않는다. 닫기는 기록을 지우기만 한다. */
    struct list *files =
        &thread_current()
             ->open_files; /* [YJH] 번호 순으로 유지되는 현재 프로세스의 FD 목록이다. */
    int fd = 2;            /* 표준 입력 0과 표준 출력 1을 건너뛴다. */
    struct list_elem *
        e; /* [YJH] 새 기록을 끼워 넣을 자리를 가리킨다. 처음으로 번호가 어긋나는 칸이거나 목록의 끝이다. */
    for (e = list_begin(files); e != list_end(files);
         e = list_next(e)) { /* [YJH] 목록을 한 번만 훑는다. */
        struct process_file *current_entry = list_entry(
            e, struct process_file, elem); /* [YJH] 현재 칸의 FD 기록이다. */
        if (current_entry->fd !=
            fd) { /* [YJH] 번호 순 목록이므로 기대한 번호와 다른 첫 칸 앞이 가장 작은 빈 번호다. */
            break; /* [YJH] 빈 번호 fd를 찾았다. */
        }
        if (fd ==
            INT32_MAX) { /* int 번호를 증가시켜 오버플로시키지 않도록 검사한다. */
            free(entry); /* 아직 등록하지 않은 기록을 반납한다. */
            return -1;   /* 표현 가능한 FD 번호가 없음을 알린다. */
        } /* FD 범위 초과 처리를 끝낸다. */
        fd++; /* [YJH] 이 번호는 쓰고 있으므로 다음 번호를 기대한다. */
    } /* [YJH] 빈 번호를 찾았거나 목록 끝에 도달했다. */
    entry->fd = fd;     /* 배정한 번호를 기록한다. */
    entry->file = file; /* 성공 경로에서 파일 보유권을 기록으로 넘긴다. */
    list_insert(
        e,
        &entry
             ->elem); /* [YJH] e 앞에 넣어 번호 순서를 유지한다. 끝까지 훑었으면 목록 맨 뒤에 들어간다. */
    return fd;        /* 사용자에게 전달할 번호를 반환한다. */
} /* process_add_file 정의를 끝낸다. */

/* [YYJ] 호출자가 filesys_lock을 보유한 상태에서 FD 하나를 닫는다. */
void process_close_file(int fd)
{ /* 유효하지 않은 번호는 아무 작업 없이 무시한다. */
    struct list *files =
        &thread_current()->open_files; /* 내 FD 목록만 검색한다. */
    for (struct list_elem *e = list_begin(files); e != list_end(files);
         e = list_next(e)) { /* 각 기록을 확인한다. */
        struct process_file *entry = list_entry(
            e, struct process_file, elem); /* 실제 FD 기록을 얻는다. */
        if (entry->fd == fd) {             /* 닫을 번호를 찾았는지 확인한다. */
            list_remove(
                &entry
                     ->elem); /* 해제할 기록이 목록에 남지 않도록 먼저 제거한다. */
            file_close(
                entry->file); /* 이 프로세스가 보유한 파일 객체만 닫는다. */
            free(entry);      /* FD 기록 자체도 반납한다. */
            return;           /* 해제한 요소로 순회를 계속하지 않는다. */
        } /* 일치 여부 분기를 끝낸다. */
    } /* 유효하지 않은 FD면 아무 기록도 변경하지 않는다. */
} /* process_close_file 정의를 끝낸다. */

/* [YYJ] 자식이 filesys_lock을 잡고 호출한다. 일부 실패 시 정리는 자식의 process_exit가 맡는다. */
static bool process_duplicate_files(struct thread *parent)
{ /* 원본은 fork 완료를 기다리는 부모다. */
    struct thread *current =
        thread_current(); /* 복제한 파일을 소유할 자식을 얻는다. */
    if (parent->running_file !=
        NULL) { /* 부모가 실행 파일 보유권을 가지고 있는지 확인한다. */
        current->running_file = file_duplicate(
            parent
                ->running_file); /* 실행 파일의 쓰기 금지 상태도 함께 복제한다. */
        if (current->running_file ==
            NULL) {       /* 실행 파일 복제 실패를 확인한다. */
            return false; /* 부모가 성공한 fork로 오인하지 않도록 실패를 반환한다. */
        }
    } /* 실행 파일 보유권 복제를 끝낸다. */
    for (struct list_elem *e = list_begin(&parent->open_files);
         e != list_end(&parent->open_files);
         e = list_next(e)) { /* 부모의 일반 FD를 순회한다. */
        struct process_file *source = list_entry(
            e, struct process_file, elem); /* 부모 FD 기록을 얻는다. */
        struct process_file *copy =
            malloc(sizeof *copy); /* 자식 FD 기록을 별도로 만든다. */
        if (copy == NULL) {       /* 기록 할당 실패를 확인한다. */
            return false; /* 앞서 복제한 기록은 자식 종료 시 정리한다. */
        }
        copy->fd =
            source
                ->fd; /* 중간에 빈 번호가 있더라도 부모의 정확한 FD 번호를 보존한다. */
        copy->file = file_duplicate(
            source
                ->file); /* 복제 시점 파일 위치를 가진 별도 파일 객체를 만든다. */
        if (copy->file == NULL) { /* 파일 객체 복제 실패를 검사한다. */
            free(copy);           /* 목록에 넣지 못한 기록을 즉시 반납한다. */
            return false; /* 기존 복제 기록은 종료 경로에서 정리하게 한다. */
        } /* 파일 복제 실패 처리를 끝낸다. */
        list_push_back(&current->open_files,
                       &copy->elem); /* 완성된 기록만 자식 목록에 등록한다. */
    } /* 모든 일반 FD의 복제를 끝낸다. */
    return true; /* 실행 파일과 모든 FD를 복제했다. */
} /* process_duplicate_files 정의를 끝낸다. */

/* [YYJ] process_exit에서 호출한다. exec의 주소 공간 교체에서는 호출하지 않는다. */
static void process_close_all_files(void)
{ /* 현재 프로세스의 일반 FD 전부를 정리한다. */
    struct list *files =
        &thread_current()->open_files; /* 종료할 프로세스의 목록을 얻는다. */
    lock_acquire(&filesys_lock); /* file_close의 파일 시스템 접근을 보호한다. */
    while (!list_empty(files)) { /* 모든 FD 기록이 없어질 때까지 반복한다. */
        struct list_elem *e =
            list_pop_front(files); /* 목록에서 먼저 기록을 분리한다. */
        struct process_file *entry = list_entry(
            e, struct process_file, elem); /* 분리한 실제 기록을 얻는다. */
        file_close(entry->file);           /* 보유하던 파일 객체를 닫는다. */
        free(entry);                       /* 기록 메모리도 반납한다. */
    } /* 일반 FD 정리를 끝낸다. */
    lock_release(&filesys_lock); /* 종료 알림과 참조 반납 전에 락을 해제한다. */
} /* process_close_all_files 정의를 끝낸다. */

/* Free the current process's resources. */
static void process_cleanup(void)
{
    struct thread *curr = thread_current();

    if (curr->running_file !=
        NULL) { /* 이전 실행 파일 보유권이 있는 경우만 정리한다. */
        lock_acquire(
            &filesys_lock); /* 실행 파일 close도 공통 락으로 보호한다. */
        file_close(
            curr->running_file); /* 이 프로세스의 쓰기 금지 보유권을 반납한다. */
        curr->running_file =
            NULL; /* 다음 정리에서 같은 파일을 다시 닫지 않도록 한다. */
        lock_release(
            &filesys_lock); /* 주소 공간 정리 전에 파일 시스템 락을 반납한다. */
    } /* 실행 파일 정리를 끝낸다. */

#ifdef VM
    supplemental_page_table_kill(&curr->spt);
#endif

    uint64_t *pml4;
    /* Destroy the current process's page directory and switch back
	 * to the kernel-only page directory. */
    pml4 = curr->pml4;
    if (pml4 != NULL) {
        /* Correct ordering here is crucial.  We must set
		 * cur->pagedir to NULL before switching page directories,
		 * so that a timer interrupt can't switch back to the
		 * process page directory.  We must activate the base page
		 * directory before destroying the process's page
		 * directory, or our active page directory will be one
		 * that's been freed (and cleared). */
        curr->pml4 = NULL;
        pml4_activate(NULL);
        pml4_destroy(pml4);
    }
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void process_activate(struct thread *next)
{
    /* Activate thread's page tables. */
    pml4_activate(next->pml4);

    /* Set thread's kernel stack for use in processing interrupts. */
    tss_update(next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
};

struct ELF64_PHDR {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack(struct intr_frame *if_);
static bool validate_segment(const struct Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool load(const char *file_name, struct intr_frame *if_)
{
    struct thread *t = thread_current();
    struct ELF ehdr;
    struct file *file = NULL;
    off_t file_ofs;
    bool success = false;
    int i;

    /* Allocate and activate page directory. */
    t->pml4 = pml4_create();
    if (t->pml4 == NULL) {
        goto done;
    }
    process_activate(thread_current());

    /* Open executable file. */
    file = filesys_open(file_name);
    if (file == NULL) {
        printf("load: %s: open failed\n", file_name);
        goto done;
    }

    file_deny_write(
        file); /* [YYJ] 검사·로딩 중에도 실행 파일이 변경되지 않도록 쓰기를 금지한다. */

    /* Read and verify executable header. */
    if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr ||
        memcmp(ehdr.e_ident, "\177ELF\2\1\1", 7) || ehdr.e_type != 2 ||
        ehdr.e_machine != 0x3E // amd64
        || ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Phdr) ||
        ehdr.e_phnum > 1024) {
        printf("load: %s: error loading executable\n", file_name);
        goto done;
    }

    /* Read program headers. */
    file_ofs = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++) {
        struct Phdr phdr;

        if (file_ofs < 0 || file_ofs > file_length(file)) {
            goto done;
        }
        file_seek(file, file_ofs);

        if (file_read(file, &phdr, sizeof phdr) != sizeof phdr) {
            goto done;
        }
        file_ofs += sizeof phdr;
        switch (phdr.p_type) {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
            /* Ignore this segment. */
            break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
            goto done;
        case PT_LOAD:
            if (validate_segment(&phdr, file)) {
                bool writable = (phdr.p_flags & PF_W) != 0;
                uint64_t file_page = phdr.p_offset & ~PGMASK;
                uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
                uint64_t page_offset = phdr.p_vaddr & PGMASK;
                uint32_t read_bytes, zero_bytes;
                if (phdr.p_filesz > 0) {
                    /* Normal segment.
						 * Read initial part from disk and zero the rest. */
                    read_bytes = page_offset + phdr.p_filesz;
                    zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) -
                                  read_bytes);
                } else {
                    /* Entirely zero.
						 * Don't read anything from disk. */
                    read_bytes = 0;
                    zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
                }
                if (!load_segment(file, file_page, (void *)mem_page, read_bytes,
                                  zero_bytes, writable)) {
                    goto done;
                }
            } else {
                goto done;
            }
            break;
        }
    }

    /* Set up stack. */
    if (!setup_stack(if_)) {
        goto done;
    }

    /* Start address. */
    if_->rip = ehdr.e_entry;

    /* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

    t->running_file =
        file; /* 성공한 실행 파일의 보유권을 현재 프로세스로 넘긴다. */
    file =
        NULL; /* done의 file_close가 성공한 실행 파일을 곧바로 닫지 않도록 지역 보유권을 비운다. */
    success = true;

done:
    /* We arrive here whether the load is successful or not. */
    file_close(file);
    return success;
}
/* 로딩 실패는 지역 file을 닫고, 성공은 running_file로 유지한다. 이후 스택 인자 구성 실패도 
process_terminate→process_exit→process_cleanup이 실행 파일을 닫는다. fork는 file_duplicate가 기존 deny_write 상태를 복제한다. */

/*
 * TODO [YJH]: setup_arguments
 *
 * 역할
 * - 커널에 있는 인자 문자열을 사용자 스택으로 복사하고,
 *   사용자 프로그램의 argc, argv, rsp를 설정한다.
 *
 * 작성 파일
 * - pintos/userprog/process.c
 * - 이 파일에서만 사용하므로 static 함수.
 *
 * 입력 / 출력
 * - if_: load()가 실행 주소와 사용자 스택을 준비한 실행 문맥.
 * - argv: 커널에 있는 인자 문자열 주소 배열.
 * - argc: 프로그램 이름을 포함한 인자 개수.
 * - 반환값: 스택 구성이 성공하면 true, 공간이 부족하면 false.
 *
 * 로직 / 분기
 * 1. 문자열, 정렬 여백, argv 배열, NULL, 가짜 반환 주소의
 *    총 크기가 준비된 사용자 스택 한 페이지에 들어가는지 확인한다.
 * 2. 각 문자열을 사용자 스택에 복사한다.
 * 3. argv의 각 항목을 복사된 문자열의 사용자 주소로 바꾼다.
 * 4. 스택 위치를 8바이트 경계로 내린다.
 * 5. argv[argc]에 해당하는 NULL과 각 문자열 주소를 쌓는다.
 * 6. if_->R.rdi에 argc, if_->R.rsi에 사용자 argv 주소를 설정한다.
 * 7. 가짜 반환 주소 0을 쌓고 if_->rsp를 갱신한다.
 * - 크기 초과라면 사용자 스택에 쓰기 전에 false를 반환한다.
 *
 * 호출
 * - process.c의 process_exec():
 *   load(argv[0], &if_) 성공 후, do_iret() 호출 전.
 *
 * 기존 함수 / 자료 사용
 * - lib/string.c: strlen(), memcpy().
 * - include/threads/vaddr.h: USER_STACK, PGSIZE.
 * - include/threads/interrupt.h: struct intr_frame.
 * - process.c: 기존 load(), setup_stack().
 *
 * 소유권
 * - 사용자 스택 페이지와 커널 argv 배열을 해제하지 않는다.
 * - argv 배열 내용은 사용자 주소로 변경된다.
 * - 커널 argv 배열과 명령어 페이지는 process_exec()이 해제한다.
 *
 * 검증
 * - 아래 공통 검증 명령의 args-* 5개.
 * - argc, 인자 순서, 연속 공백 처리, argv[argc] == NULL 확인.
 * - argv 주소의 8바이트 정렬 확인.
 */
static bool setup_arguments(struct intr_frame *if_, char **argv, int argc)
{
    /* [YYJ] 사용자 스택에 쓰기 전에 전체 필요 공간을 검사한다. */
    if (argc <= 0) {
        return false;
    }

    size_t string_bytes = 0;

    for (int i = 0; i < argc; i++) {
        size_t length = strlen(argv[i]) + 1;

        if (length > PGSIZE - string_bytes) {
            return false;
        }

        string_bytes += length;
    }

    /* 문자열 복사 후 8바이트 정렬에 필요한 여백이다. */
    size_t padding = (if_->rsp - string_bytes) % 8;

    /* 인자 주소 argc개 + 마지막 NULL + 가짜 반환 주소. */
    size_t pointer_bytes = ((size_t)argc + 2) * sizeof(char *);

    if (string_bytes + padding + pointer_bytes > PGSIZE) {
        return false;
    }

    // 문자열 본체를 역순으로 스택에 복사하기
    for (int i = argc - 1; i >= 0; i--) {
        // 단어 길이 구하기 ('\0' 포함)
        int len = strlen(argv[i]) + 1;

        // 스택 공간 확보하기 (주소 감소)
        if_->rsp -= len;

        // 방금 만든 빈 방에 글자 복사하기
        // 목적지: 방금 확보한 스택 방 (if_->rsp)
        // 원본: 원래 단어가 있던 곳 (argv[i])
        // 크기: len
        memcpy((void *)if_->rsp, argv[i], len);

        // 주소 기억해두기
        // 원래 argv[i] 에는 '커널 메모리'의 주소가 들어있었음
        // 유저 프로그램은 커널 메모리를 못 봄
        // 방금 유저 스택으로 복사해온 새 주소(if_->rsp)로 argv[i]의 내용을 덮어씀

        argv[i] = (char *)if_->rsp;
    }

    while (if_->rsp % 8 != 0) {
        if_->rsp--;
        *(uint8_t *)if_->rsp = 0;
    }

    if_->rsp -=
        8; // 포인터 1개 (8바이트) 가 들어갈 방을 확보하기 위해, 스택을 8바이트 내림
    *(char **)if_->rsp =
        0; // 8바이트를 0으로 덮어씀: argv[argc] 는 반드시 NULL 값 (0) 이어야 함

    for (int i = argc - 1; i >= 0; i--) {
        if_->rsp -= 8;
        *(char **)if_->rsp = argv[i];
    }

    if_->R.rdi = argc;
    if_->R.rsi = if_->rsp;

    if_->rsp -= 8;
    *(uint64_t *)if_->rsp = 0;

    return true;
}

/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool validate_segment(const struct Phdr *phdr, struct file *file)
{
    /* p_offset and p_vaddr must have the same page offset. */
    if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) {
        return false;
    }

    /* p_offset must point within FILE. */
    if (phdr->p_offset > (uint64_t)file_length(file)) {
        return false;
    }

    /* p_memsz must be at least as big as p_filesz. */
    if (phdr->p_memsz < phdr->p_filesz) {
        return false;
    }

    /* The segment must not be empty. */
    if (phdr->p_memsz == 0) {
        return false;
    }

    /* The virtual memory region must both start and end within the
	   user address space range. */
    if (!is_user_vaddr((void *)phdr->p_vaddr)) {
        return false;
    }
    if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz))) {
        return false;
    }

    /* The region cannot "wrap around" across the kernel virtual
	   address space. */
    if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr) {
        return false;
    }

    /* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
    if (phdr->p_vaddr < PGSIZE) {
        return false;
    }

    /* It's okay. */
    return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page(void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable)
{
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    file_seek(file, ofs);
    while (read_bytes > 0 || zero_bytes > 0) {
        /* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* Get a page of memory. */
        uint8_t *kpage = palloc_get_page(PAL_USER);
        if (kpage == NULL) {
            return false;
        }

        /* Load this page. */
        if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes) {
            palloc_free_page(kpage);
            return false;
        }
        memset(kpage + page_read_bytes, 0, page_zero_bytes);

        /* Add the page to the process's address space. */
        if (!install_page(upage, kpage, writable)) {
            printf("fail\n");
            palloc_free_page(kpage);
            return false;
        }

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
    }
    return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool setup_stack(struct intr_frame *if_)
{
    uint8_t *kpage;
    bool success = false;

    kpage = palloc_get_page(PAL_USER | PAL_ZERO);
    if (kpage != NULL) {
        success = install_page(((uint8_t *)USER_STACK) - PGSIZE, kpage, true);
        if (success) {
            if_->rsp = USER_STACK;
        } else {
            palloc_free_page(kpage);
        }
    }
    return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool install_page(void *upage, void *kpage, bool writable)
{
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
    return (pml4_get_page(t->pml4, upage) == NULL &&
            pml4_set_page(t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

/* [SHW] 코드 로드시 lazy_load_segment에 넘겨줄 파일 정보*/
struct load_info {
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
};

static bool lazy_load_segment(struct page *page, void *aux)
{
    /* TODO: Load the segment from the file */
    /* TODO: This called when the first page fault occurs on address VA. */
    /* TODO: VA is available when calling this function. */
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable)
{
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    while (read_bytes > 0 || zero_bytes > 0) {
        /* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* TODO: Set up aux to pass information to the lazy_load_segment. */

        /* [SHW] lazy_load_segment에 인자로 넘겨줄 파일 정보*/
        struct load_info *load_info = malloc(sizeof(struct load_info));
        if (load_info == NULL) {
            return false;
        }
        load_info->file = file;
        load_info->ofs = ofs;
        load_info->read_bytes = page_read_bytes;
        load_info->zero_bytes = page_zero_bytes;

        void *aux = load_info;
        if (!vm_alloc_page_with_initializer(VM_ANON, upage, writable,
                                            lazy_load_segment, aux)) {
            free(load_info);
            return false;
        }

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += page_read_bytes;
    }
    return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool setup_stack(struct intr_frame *if_)
{
    bool success = false;
    void *stack_bottom = (void *)(((uint8_t *)USER_STACK) - PGSIZE);

    /* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
    /* TODO: Your code goes here */

    return success;
}
#endif /* VM */
