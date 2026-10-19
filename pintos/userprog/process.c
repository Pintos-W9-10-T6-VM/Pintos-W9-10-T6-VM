#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "threads/malloc.h"		/* [YYJ] */
#include "threads/synch.h"		/* [YYJ] */
#ifdef VM
#include "vm/vm.h"
#endif

/* [YYJ] 자식 스레드가 해제된 뒤에도 종료 상태를 보관한다. */
struct child_status {
    tid_t tid;					// 어느 자식의 기록인지
    int exit_status;			// 자식의 종료 결과 
    struct semaphore exited;	// 자식 종료를 부모에게 알리는 세마포어
    struct lock ref_lock;		// 참조 수를 변경할 때 사용할 락
    unsigned refs;				// 이 객체를 유지할 책임이 있는 참조 수
    struct list_elem elem;		// 부모의 자식 목록에 연결할 요소
};

/* [YYJ] 새 자식에게 명령어와 공유 종료 기록을 함께 전달한다. */
struct process_start_info {
    char *cmdline;
    struct child_status *status;
};

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

static void child_status_put(struct child_status *status); 	/* [YYJ] */
static struct child_status *child_status_create(void);		/* [YYJ] */
void process_terminate(int status) NO_RETURN;				/* [YYJ] */
static bool setup_arguments(struct intr_frame *if_, char **argv, int argc);	/* [YYJ] */

/* General process initializer for initd and other process. */
static void
process_init (void) {
	struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t process_create_initd (const char *file_name) { /* [YYJ] 최초 사용자 프로세스를 생성하고 부모의 자식 목록에 등록한다. */
	// char *fn_copy;
	// tid_t tid;
	/* 스레드 이름에는 전체 명령어 대신 프로그램 이름을 사용한다. */
    const char *program = file_name;

	while (*program == ' ')
	{
    	program++;
	}

	if (*program == '\0')
	{
		return TID_ERROR;
	}

	char name[16];
	strlcpy(name, program, sizeof name);

	char *space = strchr(name, ' ');
	if (space != NULL)
	{
		*space = '\0';
	}

	/* 명령어는 생성 중 변경되지 않도록 커널 페이지에 복사한다. */
	char *cmdline = palloc_get_page(0);
	if (cmdline == NULL)
	{
		return TID_ERROR;
	}

	if (strlcpy(cmdline, file_name, PGSIZE) >= PGSIZE)
	{
		palloc_free_page(cmdline);
		return TID_ERROR;
	}
	
    /* 부모·자식이 공유할 기록을 생성한다. 참조 수는 2다. */
    struct child_status *status = child_status_create();
    if (status == NULL) 
	{
        palloc_free_page(cmdline);
        return TID_ERROR;
    }
	

    /* 자식 시작 함수에 넘길 자료를 묶는다. */
    struct process_start_info *start = malloc(sizeof *start);
    if (start == NULL) 
	{
        child_status_put(status);
        child_status_put(status);
        palloc_free_page(cmdline);
        return TID_ERROR;
    }
	
    start->cmdline = cmdline;
    start->status = status;


    /* 자식이 생성 직후 종료할 수도 있으므로 먼저 목록에 등록한다. */
    list_push_back(&thread_current()->children, &status->elem);

    tid_t tid = thread_create(name, PRI_DEFAULT, initd, start);

    if (tid == TID_ERROR) 
	{
        /* 자식에게 넘기지 못했으므로 부모가 모든 할당을 정리한다. */
        list_remove(&status->elem);
        free(start);
        palloc_free_page(cmdline);
        child_status_put(status);
        child_status_put(status);
        return TID_ERROR;
    }

    /* 부모 참조가 남아 있으므로 자식이 먼저 종료해도 기록은 유효하다. */
    status->tid = tid;
    return tid;


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
}

/* A thread function that launches first user process. */
static void initd (void *f_name) {


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
	supplemental_page_table_init (&curr->spt);
#endif

	process_init ();

	if (process_exec (cmdline) < 0)
		process_terminate(-1); /* [YYJ] 실행 준비에 실패한 경우에만 비정상 종료한다. 기존 PANIC은 커널 전체를 멈춘다. */
	NOT_REACHED ();

}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
	/* Clone current thread to new thread.*/
	return thread_create (name,
			PRI_DEFAULT, __do_fork, thread_current ());
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
	struct thread *current = thread_current ();
	struct thread *parent = (struct thread *) aux;
	void *parent_page;
	void *newpage;
	bool writable;

	/* 1. TODO: If the parent_page is kernel page, then return immediately. */

	/* 2. Resolve VA from the parent's page map level 4. */
	parent_page = pml4_get_page (parent->pml4, va);

	/* 3. TODO: Allocate new PAL_USER page for the child and set result to
	 *    TODO: NEWPAGE. */

	/* 4. TODO: Duplicate parent's page to the new page and
	 *    TODO: check whether parent's page is writable or not (set WRITABLE
	 *    TODO: according to the result). */

	/* 5. Add new page to child's page table at address VA with WRITABLE
	 *    permission. */
	if (!pml4_set_page (current->pml4, va, newpage, writable)) {
		/* 6. TODO: if fail to insert page, do error handling. */
	}
	return true;
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
	struct intr_frame if_;
	struct thread *parent = (struct thread *) aux;
	struct thread *current = thread_current ();
	/* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
	struct intr_frame *parent_if;
	bool succ = true;

	/* 1. Read the cpu context to local stack. */
	memcpy (&if_, parent_if, sizeof (struct intr_frame));

	/* 2. Duplicate PT */
	current->pml4 = pml4_create();
	if (current->pml4 == NULL)
		goto error;

	process_activate (current);
#ifdef VM
	supplemental_page_table_init (&current->spt);
	if (!supplemental_page_table_copy (&current->spt, &parent->spt))
		goto error;
#else
	if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
		goto error;
#endif

	/* TODO: Your code goes here.
	 * TODO: Hint) To duplicate the file object, use `file_duplicate`
	 * TODO:       in include/filesys/file.h. Note that parent should not return
	 * TODO:       from the fork() until this function successfully duplicates
	 * TODO:       the resources of parent.*/

	process_init ();

	/* Finally, switch to the newly created process. */
	if (succ)
		do_iret (&if_);
error:
	thread_exit ();
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int
process_exec (void *f_name) {
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
    for (char *token = strtok_r(file_name, " ", &save_ptr);
         token != NULL;
         token = strtok_r(NULL, " ", &save_ptr)) {

        if ((size_t) argc >= max_args)
            goto done;

        argv[argc++] = token;
    }

    /* 빈 명령어는 실행할 수 없다. */
    if (argc == 0)
        goto done;

    argv[argc] = NULL;

	// /* We first kill the current context */
    /* 기존 주소 공간을 정리한다. */
    process_cleanup();

	// /* And then load the binary */
    /* 전체 명령어가 아니라 첫 단어인 실행 파일 이름을 전달한다. */
	// success = load (file_name, &_if);
    success = load(argv[0], &_if);

	// /* If load failed, quit. */
	// palloc_free_page (file_name);
	// if (!success)
	// 	return -1;

    /* load()가 준비한 사용자 스택에 인자들을 배치한다. */
    if (success)
        success = setup_arguments(&_if, argv, argc);

	done:
		/* 사용자 스택으로 복사가 끝났거나 실패했으므로 임시 메모리를 정리한다. */
		palloc_free_page(argv);
		palloc_free_page(file_name);

		if (!success)
			return -1;

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
int process_wait (tid_t child_tid) {
	/* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
	 * XXX:       to add infinite loop here before
	 * XXX:       implementing the process_wait. */


	/* TODO [YJH 연습]: 직접 구현하기.
	 * (a) children 목록에서 tid가 child_tid인 기록 찾기 (없으면 -1)
	 * (b) 찾은 기록을 목록에서 빼기
	 * (c) 자식이 끝날 때까지 기다리기
	 * (d) 종료값 읽고, 내 몫 반납하고, 종료값 반환 */

	struct thread *curr = thread_current(); // 지금 실행 중인 스레드 = wait를 부른 부모

	struct list_elem *e; // 목록에서 현재 보고 있는 칸(고리)

	// 부모의 children 목록(= 자식마다 하나씩 있는 종료 기록들)을 처음부터 끝까지 훑는다
	for (e = list_begin(&curr->children); // 첫 칸에서 시작
		 e != list_end(&curr->children);  // 끝(경계 표시)에 닿으면 반복 종료
		 e = list_next(e))				  // 한 칸씩 앞으로
	{
		// 칸(e)은 기록 안의 elem 필드를 가리킬 뿐이라서,
		// list_entry로 그 elem이 들어 있는 기록 전체(child_status)의 주소를 얻는다
		struct child_status *status = list_entry(e, struct child_status, elem);

		// 이 기록이 내가 기다리려는 자식의 것인가? (기록의 tid와 인자로 받은 child_tid 비교)
		if (status->tid == child_tid)
		{
			// (b) 목록에서 이 기록을 뺀다.
			//     이후 같은 tid로 다시 wait하면 목록에 없어서 못 찾고 -1이 된다 (wait-twice)
			list_remove(&status->elem);

			// (c) 자식이 끝날 때까지 기다린다.
			//     exited 세마포어는 처음에 0이라서 여기서 잠든다.
			//     자식이 process_exit에서 sema_up을 하면 깨어나 통과한다.
			//     자식이 이미 끝났다면 값이 1이라 기다리지 않고 바로 통과한다.
			sema_down(&status->exited);

			// (d) 깨어났으면 자식이 종료값을 이미 적어 뒀다 (적고 나서 sema_up 하는 순서라서).
			//     기록은 곧 반납하니, 반납하기 전에 값을 지역 변수에 복사해 둔다.
			int result = status->exit_status;

			// 부모 몫의 참조를 반납한다.
			// 자식도 이미 반납했다면 refs가 0이 되어 기록이 free된다.
			// (이 줄 이후에는 status를 읽으면 안 된다. 그래서 위에서 result에 먼저 복사)
			child_status_put(status);

			return result;
		}
	}

	// 끝까지 돌았는데도 못 찾았다: 내 자식이 아니거나, 이미 한 번 기다린 자식이다.
	// 기다리지 않고 바로 -1을 반환한다.
	return -1;
}

/* Exit the process. This function is called by thread_exit (). */
/* [YYJ] 프로세스 자원을 정리하고 부모에게 종료를 알린다. */
void process_exit (void) {
	struct thread *curr = thread_current ();
	/* TODO: Your code goes here.
	 * TODO: Implement process termination message (see
	 * TODO: project2/process_termination.html).
	 * TODO: We recommend you to implement process resource cleanup here. */
    /* 사용자 프로세스의 종료 메시지만 출력한다. */
    if (curr->is_user_process)
        printf("%s: exit(%d)\n", curr->name, curr->exit_status);

    /* 기존 주소 공간 정리 순서는 유지한다. */
    process_cleanup();

    /* 내가 기다리지 않은 자식들의 부모 측 참조를 반납한다.
     * 자식이 살아 있다면 자식 측 참조가 객체를 유지한다. */
    while (!list_empty(&curr->children)) {
        struct list_elem *e = list_pop_front(&curr->children);
        struct child_status *child =
            list_entry(e, struct child_status, elem);

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
void process_terminate(int status) { /* status: 전달받은 정수 종료값. */
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

static struct child_status *child_status_create(void) {

    /* 구조체 하나 크기의 메모리를 할당하고 그 주소를 child에 저장한다. sizeof *child는 구조체 크기이며, 실제 메모리를 읽는 표현은 아니다. */
    struct child_status *child = malloc(sizeof *child);

    /* 메모리 할당에 실패했는지 확인한다. */
    if (child == NULL)
        return NULL; /* 실패를 호출자에게 알리고 즉시 종료한다. */

    
    child->tid = TID_ERROR; /* 아직 자식을 생성하기 전이므로 유효한 tid가 없다고 기록한다. */
    child->exit_status = -1; /* 종료 결과의 기본값을 -1로 둔다. 종료 완료를 뜻하지는 않는다. */

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
static void child_status_put(struct child_status *status) {
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
    if (last_ref)
        free(status); /* malloc()으로 받은 메모리를 반납한다. */
}


/* Free the current process's resources. */
static void
process_cleanup (void) {
	struct thread *curr = thread_current ();

#ifdef VM
	supplemental_page_table_kill (&curr->spt);
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
		pml4_activate (NULL);
		pml4_destroy (pml4);
	}
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
	/* Activate thread's page tables. */
	pml4_activate (next->pml4);

	/* Set thread's kernel stack for use in processing interrupts. */
	tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

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

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes,
		bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load (const char *file_name, struct intr_frame *if_) {
	struct thread *t = thread_current ();
	struct ELF ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pml4 = pml4_create ();
	if (t->pml4 == NULL)
		goto done;
	process_activate (thread_current ());

	/* Open executable file. */
	file = filesys_open (file_name);
	if (file == NULL) {
		printf ("load: %s: open failed\n", file_name);
		goto done;
	}

	/* Read and verify executable header. */
	if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
			|| memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
			|| ehdr.e_type != 2
			|| ehdr.e_machine != 0x3E // amd64
			|| ehdr.e_version != 1
			|| ehdr.e_phentsize != sizeof (struct Phdr)
			|| ehdr.e_phnum > 1024) {
		printf ("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length (file))
			goto done;
		file_seek (file, file_ofs);

		if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
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
				if (validate_segment (&phdr, file)) {
					bool writable = (phdr.p_flags & PF_W) != 0;
					uint64_t file_page = phdr.p_offset & ~PGMASK;
					uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
					uint64_t page_offset = phdr.p_vaddr & PGMASK;
					uint32_t read_bytes, zero_bytes;
					if (phdr.p_filesz > 0) {
						/* Normal segment.
						 * Read initial part from disk and zero the rest. */
						read_bytes = page_offset + phdr.p_filesz;
						zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
								- read_bytes);
					} else {
						/* Entirely zero.
						 * Don't read anything from disk. */
						read_bytes = 0;
						zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
					}
					if (!load_segment (file, file_page, (void *) mem_page,
								read_bytes, zero_bytes, writable))
						goto done;
				}
				else
					goto done;
				break;
		}
	}

	/* Set up stack. */
	if (!setup_stack (if_))
		goto done;

	/* Start address. */
	if_->rip = ehdr.e_entry;

	/* TODO: Your code goes here.
	 * TODO: Implement argument passing (see project2/argument_passing.html). */

	success = true;

done:
	/* We arrive here whether the load is successful or not. */
	file_close (file);
	return success;
}

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
static bool setup_arguments(struct intr_frame *if_, char **argv, int argc){
    /* [YYJ] 사용자 스택에 쓰기 전에 전체 필요 공간을 검사한다. */
    if (argc <= 0)
        return false;

    size_t string_bytes = 0;

    for (int i = 0; i < argc; i++) {
        size_t length = strlen(argv[i]) + 1;

        if (length > PGSIZE - string_bytes)
            return false;

        string_bytes += length;
    }

    /* 문자열 복사 후 8바이트 정렬에 필요한 여백이다. */
    size_t padding = (if_->rsp - string_bytes) % 8;

    /* 인자 주소 argc개 + 마지막 NULL + 가짜 반환 주소. */
    size_t pointer_bytes = ((size_t) argc + 2) * sizeof(char *);

    if (string_bytes + padding + pointer_bytes > PGSIZE)
        return false;


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

	if_->rsp -= 8;				// 포인터 1개 (8바이트) 가 들어갈 방을 확보하기 위해, 스택을 8바이트 내림
	*(char **)if_->rsp = 0;		// 8바이트를 0으로 덮어씀: argv[argc] 는 반드시 NULL 값 (0) 이어야 함

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
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (uint64_t) file_length (file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	   user address space range. */
	if (!is_user_vaddr ((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	   address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

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
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	file_seek (file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page (PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page (kpage);
			return false;
		}
		memset (kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page (upage, kpage, writable)) {
			printf("fail\n");
			palloc_free_page (kpage);
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
static bool
setup_stack (struct intr_frame *if_) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page (PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
		if (success)
			if_->rsp = USER_STACK;
		else
			palloc_free_page (kpage);
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
static bool
install_page (void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current ();

	/* Verify that there's not already a page at that virtual
	 * address, then map our page there. */
	return (pml4_get_page (t->pml4, upage) == NULL
			&& pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
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
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		if (!vm_alloc_page_with_initializer (VM_ANON, upage,
					writable, lazy_load_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
	bool success = false;
	void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

	/* TODO: Map the stack on stack_bottom and claim the page immediately.
	 * TODO: If success, set the rsp accordingly.
	 * TODO: You should mark the page is stack. */
	/* TODO: Your code goes here */

	return success;
}
#endif /* VM */
