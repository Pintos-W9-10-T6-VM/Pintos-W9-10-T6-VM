#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include <hash.h>
#include "threads/palloc.h"

enum vm_type {
    /* page not initialized */
    /* [한국어 번역] 초기화되지 않은 페이지. */
    VM_UNINIT = 0,
    /* page not related to the file, aka anonymous page */
    /* [한국어 번역] 파일과 관련이 없는 페이지로, 익명 페이지라고도 한다. */
    VM_ANON = 1,
    /* page that realated to the file */
    /* [한국어 번역] 파일과 관련된 페이지. */
    VM_FILE = 2,
    /* page that hold the page cache, for project 4 */
    /* [한국어 번역] 프로젝트 4에서 페이지 캐시를 보유하는 페이지. */
    VM_PAGE_CACHE = 3,

    /* Bit flags to store state */
    /* [한국어 번역] 상태를 저장하는 비트 플래그. */

    /* Auxillary bit flag marker for store information. You can add more
	 * markers, until the value is fit in the int. */
    /* [한국어 번역]
     * 정보를 저장하는 보조 비트 플래그 표시자.
     * 값이 int 범위에 들어가는 한 표시자를 더 추가할 수 있다.
     */
    VM_MARKER_0 = (1 << 3),
    VM_MARKER_1 = (1 << 4),

    /* DO NOT EXCEED THIS VALUE. */
    /* [한국어 번역] 이 값을 초과하지 말 것. */
    VM_MARKER_END = (1 << 31),
};

#include "vm/uninit.h"
#include "vm/anon.h"
#include "vm/file.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif

struct page_operations;
struct thread;

#define VM_TYPE(type) ((type) & 7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
/* [한국어 번역]
 * "page"의 표현이다.
 * uninit_page, file_page, anon_page, 페이지 캐시(프로젝트 4)라는
 * 네 가지 "자식 클래스"를 가지는 일종의 "부모 클래스"다.
 * 이 구조체에 미리 정의된 멤버를 제거하거나 수정하지 말 것.
 */
struct page {
    const struct page_operations *operations;
    void *va;            /* Address in terms of user space */
    /* [한국어 번역] 사용자 주소 공간에서의 주소. */
    struct frame *frame; /* Back reference for frame */
    /* [한국어 번역] 프레임을 가리키는 역참조. */

    /* Your implementation */
    /* [한국어 번역] 직접 구현할 부분. */

    /* [SHW] 페이지 폴트 전, 쓰기 권한을 저장*/
    bool writable;
    /* [SHW] 페이지가 스택을 위한 페이지인지 표시*/
    bool is_stack;

    /* [YYJ] 파일이 큰 경우 여러 페이지일 수도 있으니. */
    size_t mmap_how_many_page;

    struct hash_elem spt_elem; /* Element in SPT. */
    /* Per-type data are binded into the union.
	 * Each function automatically detects the current union */
    /* [한국어 번역]
     * 타입별 데이터는 union에 묶여 있다.
     * 각 함수는 현재 union을 자동으로 감지한다.
     */
    union {
        struct uninit_page uninit;
        struct anon_page anon;
        struct file_page file;
#ifdef EFILESYS
        struct page_cache page_cache;
#endif
    };
};

/* The representation of "frame" */
/* [한국어 번역] "frame"의 표현. */
struct frame {
    void *kva;
    struct page *page;
};

/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */
/* [한국어 번역]
 * 페이지 연산을 위한 함수 테이블.
 * C에서 "인터페이스"를 구현하는 한 가지 방법이다.
 * 구조체 멤버에 "메서드" 테이블을 넣고 필요할 때마다 호출한다.
 */
struct page_operations {
    bool (*swap_in)(struct page *, void *);
    bool (*swap_out)(struct page *);
    void (*destroy)(struct page *);
    enum vm_type type;
};

#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)
#define destroy(page)                                                          \
    if ((page)->operations->destroy)                                           \
    (page)->operations->destroy(page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
/* [한국어 번역]
 * 현재 프로세스의 메모리 공간을 표현한다.
 * 이 구조체에 특정 설계를 따르도록 강제하지 않는다.
 * 설계는 전적으로 구현자에게 맡긴다.
 */
struct supplemental_page_table {
    struct hash pages; /* hash value keyed by va */
};

#include "threads/thread.h"
bool supplemental_page_table_init(struct supplemental_page_table *spt);
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
                                  struct supplemental_page_table *src);
void supplemental_page_table_kill(struct supplemental_page_table *spt);
struct page *spt_find_page(struct supplemental_page_table *spt, void *va);
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page);
void spt_remove_page(struct supplemental_page_table *spt, struct page *page);

void vm_init(void);
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user,
                         bool write, bool not_present);

#define vm_alloc_page(type, upage, writable)                                   \
    vm_alloc_page_with_initializer((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
                                    bool writable, vm_initializer *init,
                                    void *aux);
void vm_dealloc_page(struct page *page);
bool vm_claim_page(void *va);
enum vm_type page_get_type(struct page *page);

#endif /* VM_VM_H */
