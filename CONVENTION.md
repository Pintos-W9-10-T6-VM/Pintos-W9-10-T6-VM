# 코딩 컨벤션

Linux kernel 스타일을 기준으로 두 가지만 바꿨다. 들여쓰기는 **스페이스 4칸**(커널은 탭 8칸), 중괄호는 **항상** 쓴다.

| 표시 | 누가 맞추나 |
|---|---|
| <img src="assets/convention/wand-sparkles.svg" width="16" height="16" alt="자동 수정"> | 저장하면 자동으로 고쳐진다 (`.clang-format`) |
| <img src="assets/convention/triangle-alert.svg" width="16" height="16" alt="경고"> | 에디터에 노란 밑줄이 뜬다 (`.clang-tidy`) |
| <img src="assets/convention/eye.svg" width="16" height="16" alt="리뷰"> | 도구가 못 잡는다. 리뷰에서 본다 |

`make lint`는 <img src="assets/convention/wand-sparkles.svg" width="16" height="16" alt="자동 수정"> 자동 수정 항목과 <img src="assets/convention/triangle-alert.svg" width="16" height="16" alt="경고"> 경고 항목을 모두 검사한다.

---

## <img src="assets/convention/wand-sparkles.svg" width="16" height="16" alt="자동 수정"> A. 모양

### A1. 들여쓰기는 스페이스 4칸

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
int f(int x)
{
	if (x) {
	  return 1;
	}
	return 0;
}
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
int f(int x)
{
    if (x) {
        return 1;
    }
    return 0;
}
```

### A2. 중괄호 위치: 함수만 다음 줄, 나머지는 같은 줄

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
int f(int x) {
    if (x)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
int f(int x)
{
    if (x) {
        return 1;
    } else {
        return 0;
    }
}
```

### A3. 본문이 한 줄이어도 중괄호 <img src="assets/convention/wand-sparkles.svg" width="16" height="16" alt="자동 수정"> <img src="assets/convention/triangle-alert.svg" width="16" height="16" alt="경고">

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
if (fd < 0) return -1;
while (n--)
    count++;
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
if (fd < 0) {
    return -1;
}
while (n--) {
    count++;
}
```

### A4. 공백: 제어문 뒤에는 있고, 함수 호출 뒤에는 없다

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
if(x==1&&y>0){
    foo (a,b);
}
for(i=0;i<n;i++){
    char* p=bar ();
}
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
if (x == 1 && y > 0) {
    foo(a, b);
}
for (i = 0; i < n; i++) {
    char *p = bar();
}
```

- `if (`, `for (`, `while (`, `switch (`: 키워드 뒤 한 칸
- `foo(`: 함수 이름 뒤는 붙인다
- 이항 연산자 양옆, 쉼표 뒤 한 칸
- 포인터 `*`는 이름 쪽: `char *p`

### A5. `switch`의 `case`는 `switch`와 같은 깊이

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
switch (n) {
    case 0:
        return 1;
    default:
        return 0;
}
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
switch (n) {
case 0:
    return 1;
default:
    return 0;
}
```

### A6. 한 줄은 80자까지

넘으면 저장할 때 알아서 줄바꿈된다. 이어지는 줄은 4칸 더 들여쓴다.

---

## <img src="assets/convention/triangle-alert.svg" width="16" height="16" alt="경고"> B. 이름

### B1. 함수·변수·구조체는 snake_case

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
struct FileDesc {
    int fd;
};
int openFile(const char *fileName);
int FdCount;
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
struct file_desc {
    int fd;
};
int open_file(const char *file_name);
int fd_count;
```

### B2. 상수 매크로와 enum 값은 대문자

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
#define maxFd 128
enum color { red, Green };
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
#define MAX_FD 128
enum color { RED, GREEN };
```

매크로 *함수*는 소문자도 된다: `list_entry(e, struct thread, elem)`.

---

## <img src="assets/convention/eye.svg" width="16" height="16" alt="리뷰"> C. 이름 짓는 규칙

도구가 못 잡는 부분이다. 이름을 미리 다 정하는 대신, 아래 규칙으로 고르면 서로 비슷한 이름이 나온다.

### C1. 다른 파일에서 부르는 함수: `모듈_동사[_목적어]`

모듈 이름이 맨 앞에 와서, 이름만 보고 어느 파일에 있는지 안다.

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
void create_thread(void);
void *get_page_palloc(void);
void push_back_list(struct list *l, struct list_elem *e);
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답** (Pintos 함수)
```c
tid_t thread_create(const char *name, int priority, thread_func *f, void *aux);
void *palloc_get_page(enum palloc_flags flags);
void list_push_back(struct list *list, struct list_elem *elem);
```

파일 안에서만 쓰는 `static` 함수는 모듈 접두어를 생략해도 된다. 이때는 `동사_목적어` 순서로 쓴다(`init_thread`, `allocate_tid`).

### C2. 동사는 뜻을 하나로 고정한다

| 동사 | 뜻 | Pintos 예 |
|---|---|---|
| `init` | 이미 있는 메모리를 초기화한다. 할당하지 않는다 | `lock_init`, `list_init` |
| `create` / `destroy` | 할당해서 만든다 / 해제한다 | `pml4_create`, `pml4_destroy` |
| `open` / `close` | 열고 닫는다. 반드시 짝으로 쓴다 | `file_open`, `file_close` |
| `get` / `set` | 값을 읽는다 / 쓴다. 부작용이 없다 | `thread_get_priority` |
| `add` / `remove` | 모음에 넣는다 / 뺀다 | `list_remove` |
| `find` / `lookup` | 찾는다. 없으면 `NULL`을 돌려준다 | `hash_find` |
| `is_` / `has_` | 참·거짓을 묻는다. `bool`을 돌려준다 | `is_user_vaddr` |

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답** 짝이 안 맞는다
```c
struct table *table_create(void);
void table_free(struct table *t);
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
struct table *table_create(void);
void table_destroy(struct table *t);
```

### C3. 변수는 명사. 쓰이는 범위가 넓을수록 길게 짓는다

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
static int n; /* 전역인데 무엇의 개수인지 모른다 */
for (int index_of_loop = 0; index_of_loop < 10; index_of_loop++) {
}
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
static int open_file_cnt;
for (int i = 0; i < 10; i++) {
}
```

- 반복문 인덱스, 짧은 함수의 지역변수는 `i`, `e`, `t`처럼 짧게 쓴다
- 개수는 `_cnt`로 끝낸다 (Pintos 관례: `page_cnt`, `arg_cnt`)
- 바이트 크기는 `size`, 원소 개수는 `cnt`로 구분한다
- `bool` 변수는 `is_`/`has_`로 시작하거나 `success`처럼 상태 이름을 쓴다

### C4. 같은 개념은 모든 파일에서 같은 이름

시스템 콜 인자 이름은 `include/lib/user/syscall.h`를 따른다. 새 개념이 생기면 이 표에 한 줄 추가한다.

| 개념 | 이름 | 쓰지 않는 이름 |
|---|---|---|
| 파일 디스크립터 | `fd` | `file_descriptor`, `fdnum`, `desc` |
| 열린 파일 | `struct file *file` | `f`, `fp`, `filep` |
| 종료 코드 | `status` | `exit_code`, `ret_status` |
| 사용자 버퍼와 길이 | `buffer`, `length` | `buf`, `len`, `n` |
| 스레드 / 프로세스 ID | `tid` (`tid_t`), `pid` (`pid_t`) | `id`, `thread_id` |
| 사용자 주소 | `uaddr` | `user_ptr`, `uptr` |

### C5. 구조체에는 `typedef`를 붙이지 않는다

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
typedef struct file_desc {
    int fd;
} file_desc_t;
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
struct file_desc {
    int fd;
};
```

`typedef`는 정수 별칭(`tid_t`, `off_t`)과 함수형(`thread_func`)에만 쓴다. `struct`가 보여야 값인지 구조체인지 바로 안다.

### C6. 숫자 상수에는 이름을 붙인다

<img src="assets/convention/x.svg" width="16" height="16" alt="오답"> **오답**
```c
if (fd >= 128) {
    return -1;
}
```
<img src="assets/convention/check.svg" width="16" height="16" alt="정답"> **정답**
```c
#define FD_MAX 128

if (fd >= FD_MAX) {
    return -1;
}
```

---

## 명령

저장소 루트에서 실행한다.

```bash
make lint     # 위반을 출력한다. 위반이 있으면 실패로 끝난다 (종료 코드 0이 아님)
make format   # 모양 위반을 고치고, 바꾼 줄을 보여 준다
```

`make lint` 출력 예:

```
pintos/userprog/syscall.c:52: layout: make format 이 고친다
    52 |     if(n>128) return -1;
       |       ^ ^^   ^          ^
pintos/userprog/syscall.c:50:12: naming: invalid case style for function 'countFiles' [readability-identifier-naming]
   50 | static int countFiles(int n)
      |            ^~~~~~~~~~
      |            count_files

모양 1줄: make format 으로 자동 수정 | 이름·중괄호 1건: 직접 수정 (CONVENTION.md)
```

- `layout`: 한 줄에 하나씩 나오고, `^`가 고칠 자리를 가리킨다. `make format`이 고친다.
- `naming`, `braces`: 맨 아래 줄이 고칠 이름이다. 이름은 직접 바꾼다.

`make format` 출력 예:

```
pintos/userprog/syscall.c
@@ -52 +54,3 @@
-    if(n>128) return -1;
+    if (n > 128) {
+        return -1;
+    }
format: 1개 파일 수정
```

C 규칙(이름 짓는 규칙)은 도구가 잡지 못한다. 리뷰에서 보거나, AI에게 이 문서를 기준으로 diff를 검토해 달라고 한다.

`make lint`는 `threads/ devices/ userprog/ filesys/ vm/`의 이름 규칙을 본다. `lib/`(C 표준 라이브러리 이름)는 이름 검사에서 제외한다. 모양 검사는 `tests/`, `utils/`를 뺀 전체가 대상이다.

예외: Pintos 관용 이름은 경고를 내지 않는다. 이름 끝 `_`는 받은 인자를 형변환해 쓰는 관용구다(`void *buf_` → `uint8_t *buf = buf_`). 이름 앞 `_`는 링커와 컴파일러 기호다(`_start`).

## 설정 (처음 한 번)

**VS Code + Dev Container**: `git pull` 후 "Rebuild Container"를 누른다. 도구, clangd 확장, 저장 시 포맷이 모두 설정돼 있다.

**Dev Container 없이**: 같은 버전을 설치한다. 버전이 다르면 서로의 줄을 계속 다시 포맷하게 된다.

```bash
pip install clangd==21.1.8 clang-format==21.1.8 clang-tidy==21.1.6
```

에디터의 clangd도 이 `clangd`를 쓰게 지정한다.

## 전체 포맷 커밋

컨벤션을 처음 적용할 때 전체 파일을 한 번 포맷했다. 그 커밋은 `.git-blame-ignore-revs`에 있다. blame에서 숨기려면 한 번 실행한다.

```bash
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

GitHub의 blame 화면은 이 파일을 자동으로 읽는다.
