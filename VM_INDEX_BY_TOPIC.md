# Project 3 코드 색인 — 기능별

[사전순으로 보기](VM_INDEX_AZ.md)

기준: 2026-10-29 · `YYJ` / `a392757` (pull 반영 코드).

제공한 Project 3·Appendix 14개 문서의 코드 항목과 `vm1.png`, VM 구현에 필요한 보조 API와 팀 구현 항목을 수록했다. 총 **360개** 코드 항목.

위치 표기는 `pintos/` 기준이며, 저장소 루트 파일은 별도 표시한다. 파일·줄 링크는 VS Code에서 열리며, `선언`은 헤더 링크다. 줄 번호는 코드 수정 시 갱신해야 한다.

`구현됨`은 해당 본문이 작성됐다는 뜻이며 VM 전체 완성·테스트 PASS를 뜻하지 않는다. `부분 구현`은 일부 동작·지원 상태·실패 정리가 미완성, `미구현`은 핵심 동작 없음, `정의됨`은 타입·상수·멤버 등이 존재, `골격`은 빈 구조체다.

`코드 없음`은 문서 예시 또는 현재 저장소에 없는 API다. 예시는 함수 추가 지시가 아니다. **COW는 Extra, P4 표시는 이번 필수 범위 밖, inspect는 수정 금지**다.

1. [SPT · Page](#topic-spt) (45)
2. [UNINIT · ANON · Lazy](#topic-anon) (23)
3. [Frame · Claim](#topic-frame) (6)
4. [Fault · Stack](#topic-fault) (14)
5. [Swap · Disk](#topic-swap) (16)
6. [FILE · mmap](#topic-file) (35)
7. [Fork · 수명 · COW](#topic-lifecycle) (21)
8. [PML4 · 주소](#topic-address) (52)
9. [메모리 할당](#topic-allocation) (14)
10. [Hash · Bitmap](#topic-hash) (55)
11. [동기화 · Thread](#topic-thread) (63)
12. [디버깅 · 검사 API](#topic-debug) (16)

[도표 대조](#comparison) · [도구·명령](#tools) · [참고](#sources)

<a id="topic-spt"></a>

## 1. SPT · Page

참고: [Memory Management](https://casys-kaist.github.io/pintos-kaist/project3/vm_management.html) · [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `destroy` | 매크로 | 종류별 destroy 콜백 호출 | [include/vm/vm.h:117](pintos/include/vm/vm.h#L117) | 정의됨 |
| `EFILESYS` | 빌드 플래그 | 확장 파일 시스템 분기 — P4 | [filesys/Make.vars:3](pintos/filesys/Make.vars#L3) | 정의됨 |
| `hashing()` | 함수 | SPT의 페이지 번호를 해시값으로 변환 | [vm/vm.c:78](pintos/vm/vm.c#L78) | 구현됨 |
| `less()` | 함수 | SPT 페이지의 va를 비교하는 콜백 | [vm/vm.c:69](pintos/vm/vm.c#L69) | 구현됨 |
| `struct page` | 구조체 | 가상 페이지의 공통 정보 | [include/vm/vm.h:60](pintos/include/vm/vm.h#L60) | 정의됨 |
| `page.anon` | 멤버 | 익명 페이지의 union 데이터 | [include/vm/vm.h:84](pintos/include/vm/vm.h#L84) | 정의됨 |
| `page.file` | 멤버 | 파일 페이지의 union 데이터 | [include/vm/vm.h:85](pintos/include/vm/vm.h#L85) | 정의됨 |
| `page.frame` | 멤버 | 연결된 물리 프레임 | [include/vm/vm.h:64](pintos/include/vm/vm.h#L64) | 정의됨 |
| `page.is_stack` | 멤버 | 스택 페이지 표시; VM_MARKER_0에서 설정 | [include/vm/vm.h:73](pintos/include/vm/vm.h#L73) | 정의됨 |
| `page.operations` | 멤버 | 종류별 연산 테이블 포인터 | [include/vm/vm.h:61](pintos/include/vm/vm.h#L61) | 정의됨 |
| `page.page_cache` | 멤버 | 페이지 캐시 union 데이터 — P4 | [include/vm/vm.h:87](pintos/include/vm/vm.h#L87) | 정의됨 |
| `page.spt_elem` | 멤버 | page를 SPT 해시에 연결하는 요소 | [include/vm/vm.h:75](pintos/include/vm/vm.h#L75) | 정의됨 |
| `page.uninit` | 멤버 | 미초기화 페이지의 union 데이터 | [include/vm/vm.h:83](pintos/include/vm/vm.h#L83) | 정의됨 |
| `page.va` | 멤버 | 사용자 가상 페이지 주소 | [include/vm/vm.h:62](pintos/include/vm/vm.h#L62) | 정의됨 |
| `page.writable` | 멤버 | 사용자 페이지의 쓰기 허용 여부 | [include/vm/vm.h:71](pintos/include/vm/vm.h#L71) | 정의됨 |
| `struct page_cache` | 구조체 | 페이지 캐시 메타데이터 — P4 | [include/filesys/page_cache.h:8](pintos/include/filesys/page_cache.h#L8) | **골격** |
| `page_get_type()` | 함수 | 초기화 후의 실제 페이지 종류 조회 | [vm/vm.c:52](pintos/vm/vm.c#L52) · [선언](pintos/include/vm/vm.h#L153) | 구현됨 |
| `struct page_operations` | 구조체 | 페이지 종류별 연산 인터페이스 | [include/vm/vm.h:108](pintos/include/vm/vm.h#L108) | 정의됨 |
| `page_operations.destroy` | 함수 포인터 | 종류별 자원을 정리하는 콜백 | [include/vm/vm.h:111](pintos/include/vm/vm.h#L111) | 정의됨 |
| `page_operations.swap_in` | 함수 포인터 | 내용을 프레임으로 가져오는 콜백 | [include/vm/vm.h:109](pintos/include/vm/vm.h#L109) | 정의됨 |
| `page_operations.swap_out` | 함수 포인터 | 페이지를 내보내는 콜백 | [include/vm/vm.h:110](pintos/include/vm/vm.h#L110) | 정의됨 |
| `page_operations.type` | 멤버 | 연산 테이블의 페이지 종류 | [include/vm/vm.h:112](pintos/include/vm/vm.h#L112) | 정의됨 |
| `spt_find_page()` | 함수 | 페이지 시작 주소로 정렬한 뒤 해시 검색 | [vm/vm.c:164](pintos/vm/vm.c#L164) · [선언](pintos/include/vm/vm.h#L138) | 구현됨 |
| `spt_insert_page()` | 함수 | 해시에 페이지 등록; 주소 중복 시 실패 | [vm/vm.c:178](pintos/vm/vm.c#L178) · [선언](pintos/include/vm/vm.h#L139) | 구현됨 |
| `spt_remove_page()` | 함수 | page 해제 호출만 존재; 해시 등록 제거 없음 | [vm/vm.c:185](pintos/vm/vm.c#L185) · [선언](pintos/include/vm/vm.h#L140) | **부분 구현** |
| `struct supplemental_page_table` | 구조체 | 프로세스별 페이지 해시 테이블 | [include/vm/vm.h:129](pintos/include/vm/vm.h#L129) | 정의됨 |
| `supplemental_page_table.pages` | 멤버 | 사용자 페이지 주소를 키로 관리하는 해시 | [include/vm/vm.h:130](pintos/include/vm/vm.h#L130) | 정의됨 |
| `supplemental_page_table_copy()` | 함수 | UNINIT·ANON 복제; FILE·swap 상태 미지원 | [vm/vm.c:404](pintos/vm/vm.c#L404) · [선언](pintos/include/vm/vm.h#L135) | **부분 구현** |
| `supplemental_page_table_init()` | 함수 | 해시 SPT 초기화; 성공 여부를 bool로 반환 | [vm/vm.c:356](pintos/vm/vm.c#L356) · [선언](pintos/include/vm/vm.h#L134) | 구현됨 |
| `supplemental_page_table_kill()` | 함수 | 해시 파괴·buckets 초기화; 종류별 정리 미완성 | [vm/vm.c:435](pintos/vm/vm.c#L435) · [선언](pintos/include/vm/vm.h#L137) | **부분 구현** |
| `swap_in` | 매크로 | 종류별 swap_in 콜백 호출 | [include/vm/vm.h:115](pintos/include/vm/vm.h#L115) | 정의됨 |
| `swap_out` | 매크로 | 종류별 swap_out 콜백 호출 | [include/vm/vm.h:116](pintos/include/vm/vm.h#L116) | 정의됨 |
| `VM` | 빌드 플래그 | Project 3 VM 분기 활성화 | [vm/Make.vars:3](pintos/vm/Make.vars#L3) | 정의됨 |
| `vm_alloc_page` | 매크로 | 별도 적재 콜백 없이 페이지 등록 요청 | [include/vm/vm.h:146](pintos/include/vm/vm.h#L146) | 정의됨 |
| `VM_ANON` | 열거값 | 익명 페이지 종류 | [include/vm/vm.h:13](pintos/include/vm/vm.h#L13) | 정의됨 |
| `vm_dealloc_page()` | 함수 | 종류별 정리 후 page 객체 해제; 수정 금지 | [vm/vm.c:307](pintos/vm/vm.c#L307) · [선언](pintos/include/vm/vm.h#L151) | 구현됨 |
| `VM_FILE` | 열거값 | 파일 기반 페이지 종류 | [include/vm/vm.h:16](pintos/include/vm/vm.h#L16) | 정의됨 |
| `vm_init()` | 함수 | VM 하위 시스템과 검사 인터럽트 초기화 | [vm/vm.c:30](pintos/vm/vm.c#L30) · [선언](pintos/include/vm/vm.h#L142) | **부분 구현** |
| `VM_MARKER_0` | 열거값 | 추가 상태 표시 비트; 스택 표시에 활용 | [include/vm/vm.h:30](pintos/include/vm/vm.h#L30) | 정의됨 |
| `VM_MARKER_1` | 열거값 | 추가 상태 표시 비트 | [include/vm/vm.h:31](pintos/include/vm/vm.h#L31) | 정의됨 |
| `VM_MARKER_END` | 열거값 | 보조 플래그 값의 상한 | [include/vm/vm.h:35](pintos/include/vm/vm.h#L35) | 정의됨 |
| `VM_PAGE_CACHE` | 열거값 | 페이지 캐시 종류 — P4 | [include/vm/vm.h:19](pintos/include/vm/vm.h#L19) | 정의됨 |
| `enum vm_type` | enum | 페이지 종류와 보조 플래그 | [include/vm/vm.h:7](pintos/include/vm/vm.h#L7) | 정의됨 |
| `VM_TYPE` | 매크로 | 보조 플래그를 제외한 페이지 종류 | [include/vm/vm.h:48](pintos/include/vm/vm.h#L48) | 정의됨 |
| `VM_UNINIT` | 열거값 | 첫 접근 전 미초기화 상태 | [include/vm/vm.h:10](pintos/include/vm/vm.h#L10) | 정의됨 |

<a id="topic-anon"></a>

## 2. UNINIT · ANON · Lazy

참고: [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `anon_destroy()` | 함수 | 익명 프레임·스왑 자원 정리 | [vm/anon.c:68](pintos/vm/anon.c#L68) | **미구현** |
| `anon_initializer()` | 함수 | ANON 타입 검사·연산 테이블 전환; swap 상태 미구현 | [vm/anon.c:35](pintos/vm/anon.c#L35) · [선언](pintos/include/vm/anon.h#L11) | **부분 구현** |
| `anon_ops` | 연산 테이블 | 익명 페이지의 연산 연결; 수정 금지 | [vm/anon.c:17](pintos/vm/anon.c#L17) | 정의됨 |
| `struct anon_page` | 구조체 | 익명 페이지의 종류별 메타데이터 | [include/vm/anon.h:7](pintos/include/vm/anon.h#L7) | **골격** |
| `lazy_load_segment()` | 함수 | 파일 읽기·나머지 0 채움·load_info 해제 | [userprog/process.c:1428](pintos/userprog/process.c#L1428) | 구현됨 |
| `struct load_info` | 구조체 | 실행 파일 지연 적재·UNINIT 복제용 정보 | [include/userprog/process.h:10](pintos/include/userprog/process.h#L10) | 정의됨 |
| `load_info.file` | 멤버 | 내용을 읽어 올 실행 파일 포인터 | [include/userprog/process.h:11](pintos/include/userprog/process.h#L11) | 정의됨 |
| `load_info.ofs` | 멤버 | 실행 파일에서 읽기 시작할 위치 | [include/userprog/process.h:12](pintos/include/userprog/process.h#L12) | 정의됨 |
| `load_info.read_bytes` | 멤버 | 해당 페이지에 파일에서 읽을 바이트 수 | [include/userprog/process.h:13](pintos/include/userprog/process.h#L13) | 정의됨 |
| `load_info.zero_bytes` | 멤버 | 파일 내용 뒤에 0으로 채울 바이트 수 | [include/userprog/process.h:14](pintos/include/userprog/process.h#L14) | 정의됨 |
| `load_segment()` (VM) | 함수 | 페이지별 load_info 생성·lazy 콜백과 SPT 등록 | [userprog/process.c:1464](pintos/userprog/process.c#L1464) | 구현됨 |
| `memset()` | 함수 | 페이지의 남은 영역 등을 지정 바이트로 채움 | [lib/string.c:279](pintos/lib/string.c#L279) · [선언](pintos/include/lib/string.h#L19) | 구현됨 |
| `uninit_destroy()` | 함수 | 미적재 페이지의 aux 해제 | [vm/uninit.c:83](pintos/vm/uninit.c#L83) | 구현됨 |
| `uninit_initialize()` | 함수 | 종류 전환 후 내용 적재 콜백 호출 | [vm/uninit.c:59](pintos/vm/uninit.c#L59) | 구현됨 |
| `uninit_new()` | 함수 | UNINIT 객체와 초기화 콜백 설정; 수정 금지 | [vm/uninit.c:39](pintos/vm/uninit.c#L39) · [선언](pintos/include/vm/uninit.h#L24) | 구현됨 |
| `uninit_ops` | 연산 테이블 | UNINIT의 적재·파괴 연산 연결; 수정 금지 | [vm/uninit.c:30](pintos/vm/uninit.c#L30) | 정의됨 |
| `struct uninit_page` | 구조체 | 첫 접근 때 사용할 초기화 정보 | [include/vm/uninit.h:13](pintos/include/vm/uninit.h#L13) | 정의됨 |
| `uninit_page.aux` | 멤버 | 적재 콜백에 전달할 부가 정보 | [include/vm/uninit.h:18](pintos/include/vm/uninit.h#L18) | 정의됨 |
| `uninit_page.init` | 함수 포인터 | 실행 파일 등 내용을 채우는 콜백 | [include/vm/uninit.h:16](pintos/include/vm/uninit.h#L16) | 정의됨 |
| `uninit_page.page_initializer` | 함수 포인터 | ANON·FILE로 전환하는 콜백 | [include/vm/uninit.h:21](pintos/include/vm/uninit.h#L21) | 정의됨 |
| `uninit_page.type` | 멤버 | 초기화 이후 사용할 페이지 종류 | [include/vm/uninit.h:17](pintos/include/vm/uninit.h#L17) | 정의됨 |
| `vm_alloc_page_with_initializer()` | 함수 | 타입별 초기화 선택·권한/스택 표시·UNINIT 등록 | [vm/vm.c:103](pintos/vm/vm.c#L103) · [선언](pintos/include/vm/vm.h#L148) | 구현됨 |
| `vm_initializer` | typedef | 페이지 내용 적재 콜백의 함수 타입 | [include/vm/uninit.h:8](pintos/include/vm/uninit.h#L8) | 정의됨 |

<a id="topic-frame"></a>

## 3. Frame · Claim

참고: [Memory Management](https://casys-kaist.github.io/pintos-kaist/project3/vm_management.html) · [Memory Allocation](https://casys-kaist.github.io/pintos-kaist/appendix/memory_allocation.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `struct frame` | 구조체 | 물리 프레임과 연결된 page 정보 | [include/vm/vm.h:94](pintos/include/vm/vm.h#L94) | 정의됨 |
| `frame.kva` | 멤버 | 프레임에 접근할 커널 가상 주소 | [include/vm/vm.h:95](pintos/include/vm/vm.h#L95) | 정의됨 |
| `frame.page` | 멤버 | 프레임에 연결된 가상 페이지 | [include/vm/vm.h:96](pintos/include/vm/vm.h#L96) | 정의됨 |
| `vm_claim_page()` | 함수 | VA로 SPT 조회 후 claim; 없으면 false | [vm/vm.c:315](pintos/vm/vm.c#L315) · [선언](pintos/include/vm/vm.h#L152) | 구현됨 |
| `vm_do_claim_page()` | 함수 | 권한별 PML4 매핑·적재; 적재 실패 복구 미완성 | [vm/vm.c:330](pintos/vm/vm.c#L330) | **부분 구현** |
| `vm_get_frame()` | 함수 | user pool 프레임 확보; 부족 시 퇴거 대신 PANIC | [vm/vm.c:225](pintos/vm/vm.c#L225) | **부분 구현** |

<a id="topic-fault"></a>

## 4. Fault · Stack

참고: [Stack Growth](https://casys-kaist.github.io/pintos-kaist/project3/stack_growth.html) · [FAQ](https://casys-kaist.github.io/pintos-kaist/project3/FAQ.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `struct gp_registers` | 구조체 | 일반 레지스터 묶음 | [include/threads/interrupt.h:19](pintos/include/threads/interrupt.h#L19) | 정의됨 |
| `struct intr_frame` | 구조체 | 예외·시스템 콜 시 저장한 실행 문맥 | [include/threads/interrupt.h:37](pintos/include/threads/interrupt.h#L37) | 정의됨 |
| `intr_frame.error_code` | 멤버 | 폴트 원인 비트 | [include/threads/interrupt.h:52](pintos/include/threads/interrupt.h#L52) | 정의됨 |
| `intr_frame.R` | 멤버 | 일반 레지스터 묶음 | [include/threads/interrupt.h:40](pintos/include/threads/interrupt.h#L40) | 정의됨 |
| `intr_frame.rsp` | 멤버 | 저장된 스택 포인터 | [include/threads/interrupt.h:60](pintos/include/threads/interrupt.h#L60) | 정의됨 |
| `page_fault()` | 함수 | 폴트 정보 해석 후 VM 처리에 위임 | [userprog/exception.c:125](pintos/userprog/exception.c#L125) | 구현됨 |
| `PF_P` | 매크로 | 보호 위반인지 구분하는 비트 | [include/userprog/exception.h:5](pintos/include/userprog/exception.h#L5) | 정의됨 |
| `PF_U` | 매크로 | 사용자 모드 접근인지 구분하는 비트 | [include/userprog/exception.h:7](pintos/include/userprog/exception.h#L7) | 정의됨 |
| `PF_W` | 매크로 | 쓰기 접근인지 구분하는 비트 | [include/userprog/exception.h:6](pintos/include/userprog/exception.h#L6) | 정의됨 |
| `setup_stack()` (VM) | 함수 | 스택 표시 페이지 등록·즉시 claim·RSP 설정 | [userprog/process.c:1508](pintos/userprog/process.c#L1508) | 구현됨 |
| `thread.syscall_prev_rsp` | 멤버 | 시스템 콜 진입 시 저장한 사용자 RSP | [include/threads/thread.h:147](pintos/include/threads/thread.h#L147) | 정의됨 |
| `USER_STACK` | 매크로 | 사용자 스택의 상단 주소 | [include/threads/vaddr.h:38](pintos/include/threads/vaddr.h#L38) | 정의됨 |
| `vm_stack_growth()` | 함수 | 정렬한 주소에 스택 등록·claim; 등록 결과 미검사 | [vm/vm.c:247](pintos/vm/vm.c#L247) | **부분 구현** |
| `vm_try_handle_fault()` | 함수 | SPT 조회·스택 확장 판단; 쓰기 인자 미사용 | [vm/vm.c:263](pintos/vm/vm.c#L263) · [선언](pintos/include/vm/vm.h#L143) | **부분 구현** |

<a id="topic-swap"></a>

## 5. Swap · Disk

참고: [Swap In/Out](https://casys-kaist.github.io/pintos-kaist/project3/swapping.html) · [Introduction](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `anon_swap_in()` | 함수 | 스왑 슬롯에서 익명 페이지 복원 | [vm/anon.c:54](pintos/vm/anon.c#L54) | **미구현** |
| `anon_swap_out()` | 함수 | 익명 페이지를 스왑 슬롯에 보존 | [vm/anon.c:61](pintos/vm/anon.c#L61) | **미구현** |
| `struct disk` | 구조체 | ATA 디스크의 장치·용량·통계 | [devices/disk.c:91](pintos/devices/disk.c#L91) | 정의됨 |
| `disk_get()` | 함수 | 채널·장치 번호로 디스크 조회 | [devices/disk.c:269](pintos/devices/disk.c#L269) · [선언](pintos/include/devices/disk.h#L30) | 구현됨 |
| `disk_init()` | 함수 | 디스크 탐지와 초기화 | [devices/disk.c:162](pintos/devices/disk.c#L162) · [선언](pintos/include/devices/disk.h#L27) | 구현됨 |
| `disk_print_stats()` | 함수 | 디스크 읽기·쓰기 통계 출력 | [devices/disk.c:233](pintos/devices/disk.c#L233) · [선언](pintos/include/devices/disk.h#L28) | 구현됨 |
| `disk_read()` | 함수 | 디스크 섹터 하나 읽기 | [devices/disk.c:301](pintos/devices/disk.c#L301) · [선언](pintos/include/devices/disk.h#L32) | 구현됨 |
| `DISK_SECTOR_SIZE` | 매크로 | 섹터 한 개의 바이트 수: 512 | [include/devices/disk.h:9](pintos/include/devices/disk.h#L9) | 정의됨 |
| `disk_sector_t` | typedef | 디스크 섹터 번호 타입 | [include/devices/disk.h:17](pintos/include/devices/disk.h#L17) | 정의됨 |
| `disk_size()` | 함수 | 전체 섹터 수 조회 | [devices/disk.c:285](pintos/devices/disk.c#L285) · [선언](pintos/include/devices/disk.h#L31) | 구현됨 |
| `disk_write()` | 함수 | 디스크 섹터 하나 쓰기 | [devices/disk.c:332](pintos/devices/disk.c#L332) · [선언](pintos/include/devices/disk.h#L33) | 구현됨 |
| `PRDSNu` | 매크로 | 섹터 번호용 printf 형식 지정자 | [include/devices/disk.h:25](pintos/include/devices/disk.h#L25) | 정의됨 |
| `swap_disk` | 전역 변수 | 스왑 장치 포인터; 현재 NULL 설정 | [vm/anon.c:10](pintos/vm/anon.c#L10) | 정의됨 |
| `vm_anon_init()` | 함수 | 익명 페이지용 스왑 장치 준비 | [vm/anon.c:26](pintos/vm/anon.c#L26) · [선언](pintos/include/vm/anon.h#L10) | **미구현** |
| `vm_evict_frame()` | 함수 | 페이지 퇴거 후 프레임 재사용 | [vm/vm.c:207](pintos/vm/vm.c#L207) | **미구현** |
| `vm_get_victim()` | 함수 | 퇴거할 프레임 선택 | [vm/vm.c:192](pintos/vm/vm.c#L192) | **미구현** |

<a id="topic-file"></a>

## 6. FILE · mmap

참고: [Memory Mapped Files](https://casys-kaist.github.io/pintos-kaist/project3/memory_mapped_files.html) · [Swap In/Out](https://casys-kaist.github.io/pintos-kaist/project3/swapping.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `close()` (사용자) | 함수 | FD 닫기 요청 | [lib/user/syscall.c:125](pintos/lib/user/syscall.c#L125) · [선언](pintos/include/lib/user/syscall.h#L37) | 구현됨 |
| `do_mmap()` | 함수 | 파일 매핑 검증·lazy 페이지 등록 | [vm/file.c:59](pintos/vm/file.c#L59) · [선언](pintos/include/vm/file.h#L14) | **미구현** |
| `do_munmap()` | 함수 | 매핑 해제와 필요한 writeback | [vm/file.c:66](pintos/vm/file.c#L66) · [선언](pintos/include/vm/file.h#L16) | **미구현** |
| `struct file` | 구조체 | 열린 파일과 파일 위치·쓰기 금지 | [filesys/file.c:7](pintos/filesys/file.c#L7) | 정의됨 |
| `file_allow_write()` | 함수 | 실행 파일 쓰기 금지 해제 | [filesys/file.c:131](pintos/filesys/file.c#L131) · [선언](pintos/include/filesys/file.h#L23) | 구현됨 |
| `file_backed_destroy()` | 함수 | 파일 매핑의 보유 자원 정리 | [vm/file.c:52](pintos/vm/file.c#L52) | **미구현** |
| `file_backed_initializer()` | 함수 | 파일 페이지로 전환하고 상태 준비 | [vm/file.c:27](pintos/vm/file.c#L27) · [선언](pintos/include/vm/file.h#L13) | **부분 구현** |
| `file_backed_swap_in()` | 함수 | 매핑 파일에서 페이지 내용 읽기 | [vm/file.c:38](pintos/vm/file.c#L38) | **미구현** |
| `file_backed_swap_out()` | 함수 | dirty 내용의 파일 writeback | [vm/file.c:45](pintos/vm/file.c#L45) | **미구현** |
| `file_close()` | 함수 | 파일 핸들 보유권 반납 | [filesys/file.c:53](pintos/filesys/file.c#L53) · [선언](pintos/include/filesys/file.h#L12) | 구현됨 |
| `file_deny_write()` | 함수 | 실행 파일 쓰기 금지 보유 | [filesys/file.c:119](pintos/filesys/file.c#L119) · [선언](pintos/include/filesys/file.h#L22) | 구현됨 |
| `file_duplicate()` | 함수 | 위치·쓰기 금지를 포함한 핸들 복제 | [filesys/file.c:40](pintos/filesys/file.c#L40) · [선언](pintos/include/filesys/file.h#L11) | 구현됨 |
| `file_length()` | 함수 | 파일 길이 조회 | [filesys/file.c:141](pintos/filesys/file.c#L141) · [선언](pintos/include/filesys/file.h#L28) | 구현됨 |
| `file_ops` | 연산 테이블 | 파일 기반 페이지의 연산 연결; 수정 금지 | [vm/file.c:12](pintos/vm/file.c#L12) | 정의됨 |
| `struct file_page` | 구조체 | 파일 매핑의 종류별 메타데이터 | [include/vm/file.h:9](pintos/include/vm/file.h#L9) | **골격** |
| `file_read()` | 함수 | 현재 파일 위치에서 읽기 | [filesys/file.c:73](pintos/filesys/file.c#L73) · [선언](pintos/include/filesys/file.h#L16) | 구현됨 |
| `file_read_at()` | 함수 | 지정 오프셋에서 읽기 | [filesys/file.c:85](pintos/filesys/file.c#L85) · [선언](pintos/include/filesys/file.h#L17) | 구현됨 |
| `file_reopen()` | 함수 | 독립된 파일 보유권 다시 열기 | [filesys/file.c:33](pintos/filesys/file.c#L33) · [선언](pintos/include/filesys/file.h#L10) | 구현됨 |
| `file_seek()` | 함수 | 현재 파일 위치 변경 | [filesys/file.c:149](pintos/filesys/file.c#L149) · [선언](pintos/include/filesys/file.h#L26) | 구현됨 |
| `file_write()` | 함수 | 현재 파일 위치에 쓰기 | [filesys/file.c:97](pintos/filesys/file.c#L97) · [선언](pintos/include/filesys/file.h#L18) | 구현됨 |
| `file_write_at()` | 함수 | 지정 오프셋에 쓰기 | [filesys/file.c:111](pintos/filesys/file.c#L111) · [선언](pintos/include/filesys/file.h#L19) | 구현됨 |
| `filesize()` (사용자) | 함수 | 파일 크기 조회 요청 | [lib/user/syscall.c:100](pintos/lib/user/syscall.c#L100) · [선언](pintos/include/lib/user/syscall.h#L32) | 구현됨 |
| `filesys_lock` | 전역 변수 | 로더·파일 시스템의 공통 접근 락 | [userprog/syscall.c:25](pintos/userprog/syscall.c#L25) | 정의됨 |
| `main()` (mmap 예제) | 예시 함수 | 파일을 mmap해 출력하는 명세 예제 | [명세 예시](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html) | 코드 없음 |
| `mmap()` (사용자) | 함수 | 사용자 매핑 요청; 커널 분기 없음 | [lib/user/syscall.c:135](pintos/lib/user/syscall.c#L135) · [선언](pintos/include/lib/user/syscall.h#L42) | **부분 구현** |
| `munmap()` (사용자) | 함수 | 사용자 해제 요청; 커널 분기 없음 | [lib/user/syscall.c:140](pintos/lib/user/syscall.c#L140) · [선언](pintos/include/lib/user/syscall.h#L43) | **부분 구현** |
| `open()` (사용자) | 함수 | 파일 열기 요청 | [lib/user/syscall.c:95](pintos/lib/user/syscall.c#L95) · [선언](pintos/include/lib/user/syscall.h#L31) | 구현됨 |
| `read()` (사용자) | 함수 | 버퍼로 읽기 요청 | [lib/user/syscall.c:105](pintos/lib/user/syscall.c#L105) · [선언](pintos/include/lib/user/syscall.h#L33) | 구현됨 |
| `remove()` (사용자) | 함수 | 파일 삭제 요청 | [lib/user/syscall.c:90](pintos/lib/user/syscall.c#L90) · [선언](pintos/include/lib/user/syscall.h#L30) | 구현됨 |
| `seek()` (사용자) | 함수 | 파일 위치 변경 요청 | [lib/user/syscall.c:115](pintos/lib/user/syscall.c#L115) · [선언](pintos/include/lib/user/syscall.h#L35) | 구현됨 |
| `SYS_MMAP` | 열거값 | mmap 시스템 콜 번호 | [include/lib/syscall-nr.h:23](pintos/include/lib/syscall-nr.h#L23) | 정의됨 |
| `SYS_MUNMAP` | 열거값 | munmap 시스템 콜 번호 | [include/lib/syscall-nr.h:24](pintos/include/lib/syscall-nr.h#L24) | 정의됨 |
| `sys_seek()` | 함수 | 커널의 seek 시스템 콜 처리 | [userprog/syscall.c:277](pintos/userprog/syscall.c#L277) | 구현됨 |
| `vm_file_init()` | 함수 | 파일 기반 VM 하위 시스템 준비 | [vm/file.c:21](pintos/vm/file.c#L21) · [선언](pintos/include/vm/file.h#L12) | **미구현** |
| `write()` (사용자) | 함수 | 버퍼의 내용 쓰기 요청 | [lib/user/syscall.c:110](pintos/lib/user/syscall.c#L110) · [선언](pintos/include/lib/user/syscall.h#L34) | 구현됨 |

<a id="topic-lifecycle"></a>

## 7. Fork · 수명 · COW

참고: [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html) · [COW Extra](https://casys-kaist.github.io/pintos-kaist/project3/cow.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `__do_fork()` | 함수 | 자식 실행 문맥·주소 공간·FD 복제 | [userprog/process.c:340](pintos/userprog/process.c#L340) | **부분 구현** |
| `anon_page_copy()` | 함수 | 상주 ANON 프레임을 자식에 복사; swap 미지원 | [vm/vm.c:388](pintos/vm/vm.c#L388) | **부분 구현** |
| `destructor()` | 함수 | SPT 삭제 콜백; 종류별 자원 정리 미완성 | [vm/vm.c:88](pintos/vm/vm.c#L88) | **부분 구현** |
| `exec()` (사용자) | 함수 | 사용자 exec 요청 래퍼 | [lib/user/syscall.c:75](pintos/lib/user/syscall.c#L75) · [선언](pintos/include/lib/user/syscall.h#L27) | 구현됨 |
| `exit()` (사용자) | 함수 | 사용자 종료 요청 래퍼 | [lib/user/syscall.c:64](pintos/lib/user/syscall.c#L64) · [선언](pintos/include/lib/user/syscall.h#L25) | 구현됨 |
| `fork()` (사용자) | 함수 | 사용자 fork 요청 래퍼 | [lib/user/syscall.c:70](pintos/lib/user/syscall.c#L70) · [선언](pintos/include/lib/user/syscall.h#L26) | 구현됨 |
| `initd()` | 함수 | 최초 실행 정보 인수와 프로그램 실행 | [userprog/process.c:183](pintos/userprog/process.c#L183) | **부분 구현** |
| `memcpy()` | 함수 | aux·프레임의 지정 바이트 수 복사 | [lib/string.c:6](pintos/lib/string.c#L6) · [선언](pintos/include/lib/string.h#L7) | 구현됨 |
| `process_activate()` | 함수 | 프로세스 PML4와 커널 스택 활성화 | [userprog/process.c:957](pintos/userprog/process.c#L957) · [선언](pintos/include/userprog/process.h#L22) | 구현됨 |
| `process_cleanup()` | 함수 | SPT·실행 파일·PML4 정리 | [userprog/process.c:917](pintos/userprog/process.c#L917) | **부분 구현** |
| `process_create_initd()` | 함수 | 최초 사용자 프로세스 생성 | [userprog/process.c:96](pintos/userprog/process.c#L96) · [선언](pintos/include/userprog/process.h#L17) | 구현됨 |
| `process_exec()` | 함수 | 기존 이미지 정리·SPT 재초기화·새 ELF 적재 | [userprog/process.c:441](pintos/userprog/process.c#L441) · [선언](pintos/include/userprog/process.h#L19) | **부분 구현** |
| `process_exit()` | 함수 | FD·자식 기록·주소 공간 종료 정리 | [userprog/process.c:564](pintos/userprog/process.c#L564) · [선언](pintos/include/userprog/process.h#L21) | **부분 구현** |
| `process_fork()` | 함수 | 자식 생성 후 복제 결과 대기 | [userprog/process.c:217](pintos/userprog/process.c#L217) · [선언](pintos/include/userprog/process.h#L18) | **부분 구현** |
| `process_init()` | 함수 | 새 주소 공간용 초기화 연결 | [userprog/process.c:86](pintos/userprog/process.c#L86) | **미구현** |
| `process_terminate()` | 함수 | 종료값 저장 후 스레드 종료 | [userprog/process.c:639](pintos/userprog/process.c#L639) · [선언](pintos/include/userprog/process.h#L23) | 구현됨 |
| `process_wait()` | 함수 | 직접 자식의 종료 결과 한 번 대기 | [userprog/process.c:526](pintos/userprog/process.c#L526) · [선언](pintos/include/userprog/process.h#L20) | 구현됨 |
| `syscall_handler()` | 함수 | 사용자 RSP 저장·요청 분기; mmap·munmap 미연결 | [userprog/syscall.c:362](pintos/userprog/syscall.c#L362) | **부분 구현** |
| `uninit_page_copy()` | 함수 | load_info aux 복제·즉시 claim; NULL aux 미지원 | [vm/vm.c:369](pintos/vm/vm.c#L369) | **부분 구현** |
| `vm_handle_wp()` | 함수 | 쓰기 보호 폴트 처리 — COW Extra | [vm/vm.c:257](pintos/vm/vm.c#L257) | **미구현** |
| `wait()` (사용자) | 함수 | 사용자 wait 요청 래퍼 | [lib/user/syscall.c:80](pintos/lib/user/syscall.c#L80) · [선언](pintos/include/lib/user/syscall.h#L28) | 구현됨 |

<a id="topic-address"></a>

## 8. PML4 · 주소

참고: [Virtual Address](https://casys-kaist.github.io/pintos-kaist/appendix/virtual_address.html) · [Page Table](https://casys-kaist.github.io/pintos-kaist/appendix/page_table.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `BITMASK` | 매크로 | 지정 위치의 비트 마스크 생성 | [include/threads/vaddr.h:15](pintos/include/threads/vaddr.h#L15) | 정의됨 |
| `is_kern_pte` | 매크로 | PTE의 커널 전용 여부 | [include/threads/mmu.h:25](pintos/include/threads/mmu.h#L25) | 정의됨 |
| `is_kernel_vaddr` | 매크로 | 커널 영역 주소인지 검사 | [include/threads/vaddr.h:44](pintos/include/threads/vaddr.h#L44) | 정의됨 |
| `is_user_pte` | 매크로 | PTE의 사용자 접근 허용 여부 | [include/threads/mmu.h:24](pintos/include/threads/mmu.h#L24) | 정의됨 |
| `is_user_vaddr` | 매크로 | 사용자 영역 주소인지 검사 | [include/threads/vaddr.h:41](pintos/include/threads/vaddr.h#L41) | 정의됨 |
| `is_writable` | 매크로 | PTE의 쓰기 허용 여부 | [include/threads/mmu.h:23](pintos/include/threads/mmu.h#L23) | 정의됨 |
| `KERN_BASE` | 매크로 | 커널 가상 주소 공간의 시작 | [include/threads/vaddr.h:35](pintos/include/threads/vaddr.h#L35) | 정의됨 |
| `PDPE` | 매크로 | 주소에서 PDPE 인덱스 추출 | [include/threads/pte.h:26](pintos/include/threads/pte.h#L26) | 정의됨 |
| `pdpe_destroy()` | 함수 | 디렉터리 포인터의 하위 테이블 해제 | [threads/mmu.c:195](pintos/threads/mmu.c#L195) | 구현됨 |
| `PDPESHIFT` | 매크로 | PDPE 인덱스의 시작 비트 | [include/threads/pte.h:21](pintos/include/threads/pte.h#L21) | 정의됨 |
| `PDX` | 매크로 | 주소에서 디렉터리 인덱스 추출 | [include/threads/pte.h:27](pintos/include/threads/pte.h#L27) | 정의됨 |
| `PDXSHIFT` | 매크로 | 디렉터리 인덱스의 시작 비트 | [include/threads/pte.h:22](pintos/include/threads/pte.h#L22) | 정의됨 |
| `pg_no` | 매크로 | 주소에서 가상 페이지 번호 추출 | [include/threads/vaddr.h:26](pintos/include/threads/vaddr.h#L26) | 정의됨 |
| `pg_ofs` | 매크로 | 주소에서 페이지 내 오프셋 추출 | [include/threads/vaddr.h:24](pintos/include/threads/vaddr.h#L24) | 정의됨 |
| `pg_round_down` | 매크로 | 페이지 시작 주소로 내림 | [include/threads/vaddr.h:32](pintos/include/threads/vaddr.h#L32) | 정의됨 |
| `pg_round_up` | 매크로 | 페이지 경계로 올림 | [include/threads/vaddr.h:29](pintos/include/threads/vaddr.h#L29) | 정의됨 |
| `PGBITS` | 매크로 | 페이지 오프셋의 비트 수 | [include/threads/vaddr.h:19](pintos/include/threads/vaddr.h#L19) | 정의됨 |
| `pgdir_destroy()` | 함수 | 페이지 디렉터리의 하위 테이블 해제 | [threads/mmu.c:184](pintos/threads/mmu.c#L184) | 구현됨 |
| `PGMASK` | 매크로 | 페이지 오프셋 비트 마스크 | [include/threads/vaddr.h:21](pintos/include/threads/vaddr.h#L21) | 정의됨 |
| `PGSHIFT` | 매크로 | 페이지 오프셋의 시작 비트 | [include/threads/vaddr.h:18](pintos/include/threads/vaddr.h#L18) | 정의됨 |
| `PGSIZE` | 매크로 | 페이지 크기: 4096바이트 | [include/threads/vaddr.h:20](pintos/include/threads/vaddr.h#L20) | 정의됨 |
| `PML4` | 매크로 | 주소에서 PML4 인덱스 추출 | [include/threads/pte.h:25](pintos/include/threads/pte.h#L25) | 정의됨 |
| `pml4_activate()` | 함수 | CR3에 사용할 주소 공간 선택 | [threads/mmu.c:224](pintos/threads/mmu.c#L224) · [선언](pintos/include/threads/mmu.h#L14) | 구현됨 |
| `pml4_clear_page()` | 함수 | 사용자 페이지의 present 매핑 해제 | [threads/mmu.c:272](pintos/threads/mmu.c#L272) · [선언](pintos/include/threads/mmu.h#L17) | 구현됨 |
| `pml4_create()` | 함수 | 새 최상위 페이지 테이블 생성 | [threads/mmu.c:101](pintos/threads/mmu.c#L101) · [선언](pintos/include/threads/mmu.h#L11) | 구현됨 |
| `pml4_destroy()` | 함수 | 사용자 매핑·하위 테이블 해제 | [threads/mmu.c:207](pintos/threads/mmu.c#L207) · [선언](pintos/include/threads/mmu.h#L13) | 구현됨 |
| `pml4_for_each()` | 함수 | 사용자 PTE마다 콜백 실행 | [threads/mmu.c:160](pintos/threads/mmu.c#L160) · [선언](pintos/include/threads/mmu.h#L12) | 구현됨 |
| `pml4_get_page()` | 함수 | 사용자 VA의 커널 접근 주소 조회 | [threads/mmu.c:233](pintos/threads/mmu.c#L233) · [선언](pintos/include/threads/mmu.h#L15) | 구현됨 |
| `pml4_is_accessed()` | 함수 | 접근 여부 조회 | [threads/mmu.c:320](pintos/threads/mmu.c#L320) · [선언](pintos/include/threads/mmu.h#L20) | 구현됨 |
| `pml4_is_dirty()` | 함수 | 변경 여부 조회 | [threads/mmu.c:292](pintos/threads/mmu.c#L292) · [선언](pintos/include/threads/mmu.h#L18) | 구현됨 |
| `pml4_set_accessed()` | 함수 | accessed 비트 설정·해제 | [threads/mmu.c:328](pintos/threads/mmu.c#L328) · [선언](pintos/include/threads/mmu.h#L21) | 구현됨 |
| `pml4_set_dirty()` | 함수 | dirty 비트 설정·해제 | [threads/mmu.c:300](pintos/threads/mmu.c#L300) · [선언](pintos/include/threads/mmu.h#L19) | 구현됨 |
| `pml4_set_page()` | 함수 | 사용자 VA에 프레임과 권한 매핑 | [threads/mmu.c:253](pintos/threads/mmu.c#L253) · [선언](pintos/include/threads/mmu.h#L16) | 구현됨 |
| `pml4e_walk()` | 함수 | PML4에서 VA의 PTE까지 탐색 | [threads/mmu.c:68](pintos/threads/mmu.c#L68) · [선언](pintos/include/threads/mmu.h#L10) | 구현됨 |
| `PML4SHIFT` | 매크로 | PML4 인덱스의 시작 비트 | [include/threads/pte.h:20](pintos/include/threads/pte.h#L20) | 정의됨 |
| `pt_destroy()` | 함수 | 말단 테이블의 매핑 프레임·테이블 해제 | [threads/mmu.c:173](pintos/threads/mmu.c#L173) | 구현됨 |
| `PTE_A` | 매크로 | accessed 비트 | [include/threads/pte.h:42](pintos/include/threads/pte.h#L42) | 정의됨 |
| `PTE_ADDR` | 매크로 | PTE의 프레임 주소 비트 추출 | [include/threads/pte.h:29](pintos/include/threads/pte.h#L29) | 정의됨 |
| `PTE_ADDR_MASK` | 매크로 | PTE 주소 비트 마스크 | [include/threads/pte.h:37](pintos/include/threads/pte.h#L37) | 정의됨 |
| `PTE_AVL` | 매크로 | 운영체제용 여유 비트 | [include/threads/pte.h:38](pintos/include/threads/pte.h#L38) | 정의됨 |
| `PTE_D` | 매크로 | dirty 비트 | [include/threads/pte.h:43](pintos/include/threads/pte.h#L43) | 정의됨 |
| `PTE_FLAGS` | 매크로 | PTE 플래그 비트 마스크 | [include/threads/pte.h:36](pintos/include/threads/pte.h#L36) | 정의됨 |
| `pte_for_each_func` | typedef | PTE 순회 콜백의 함수 타입 | [include/threads/mmu.h:8](pintos/include/threads/mmu.h#L8) | 정의됨 |
| `pte_get_paddr` | 매크로 | PTE에서 프레임 주소 부분 추출 | [include/threads/mmu.h:27](pintos/include/threads/mmu.h#L27) | 정의됨 |
| `PTE_P` | 매크로 | present 비트 | [include/threads/pte.h:39](pintos/include/threads/pte.h#L39) | 정의됨 |
| `PTE_U` | 매크로 | 사용자 접근 허용 비트 | [include/threads/pte.h:41](pintos/include/threads/pte.h#L41) | 정의됨 |
| `PTE_W` | 매크로 | 쓰기 허용 비트 | [include/threads/pte.h:40](pintos/include/threads/pte.h#L40) | 정의됨 |
| `ptov` | 매크로 | 물리 주소를 커널 가상 주소로 변환 | [include/threads/vaddr.h:49](pintos/include/threads/vaddr.h#L49) | 정의됨 |
| `PTX` | 매크로 | 주소에서 말단 PTE 인덱스 추출 | [include/threads/pte.h:28](pintos/include/threads/pte.h#L28) | 정의됨 |
| `PTXSHIFT` | 매크로 | 말단 PTE 인덱스의 시작 비트 | [include/threads/pte.h:23](pintos/include/threads/pte.h#L23) | 정의됨 |
| `stat_page()` | 예시 함수 | PTE 순회 사용 예시 | [명세 예시](https://casys-kaist.github.io/pintos-kaist/appendix/virtual_address.html) | 코드 없음 |
| `vtop` | 매크로 | 커널 가상 주소를 물리 주소로 변환 | [include/threads/vaddr.h:53](pintos/include/threads/vaddr.h#L53) | 정의됨 |

<a id="topic-allocation"></a>

## 9. 메모리 할당

참고: [Memory Allocation](https://casys-kaist.github.io/pintos-kaist/appendix/memory_allocation.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `calloc()` | 함수 | 원소 배열 할당 후 0으로 초기화 | [threads/malloc.c:152](pintos/threads/malloc.c#L152) · [선언](pintos/include/threads/malloc.h#L9) | 구현됨 |
| `free()` | 함수 | 커널 메모리 블록 반납 | [threads/malloc.c:207](pintos/threads/malloc.c#L207) · [선언](pintos/include/threads/malloc.h#L11) | 구현됨 |
| `malloc()` | 함수 | 지정 바이트 수의 커널 메모리 할당 | [threads/malloc.c:84](pintos/threads/malloc.c#L84) · [선언](pintos/include/threads/malloc.h#L8) | 구현됨 |
| `PAL_ASSERT` | 열거값 | 페이지 할당 실패 시 panic | [include/threads/palloc.h:9](pintos/include/threads/palloc.h#L9) | 정의됨 |
| `PAL_USER` | 열거값 | 사용자 풀에서 프레임 할당 | [include/threads/palloc.h:11](pintos/include/threads/palloc.h#L11) | 정의됨 |
| `PAL_ZERO` | 열거값 | 할당한 페이지를 0으로 초기화 | [include/threads/palloc.h:10](pintos/include/threads/palloc.h#L10) | 정의됨 |
| `enum palloc_flags` | enum | 페이지 할당 옵션 | [include/threads/palloc.h:8](pintos/include/threads/palloc.h#L8) | 정의됨 |
| `palloc_free_multiple()` | 함수 | 연속된 여러 페이지 반납 | [threads/palloc.c:307](pintos/threads/palloc.c#L307) · [선언](pintos/include/threads/palloc.h#L21) | 구현됨 |
| `palloc_free_page()` | 함수 | 페이지 하나 반납 | [threads/palloc.c:335](pintos/threads/palloc.c#L335) · [선언](pintos/include/threads/palloc.h#L20) | 구현됨 |
| `palloc_get_multiple()` | 함수 | 연속된 여러 페이지 할당 | [threads/palloc.c:266](pintos/threads/palloc.c#L266) · [선언](pintos/include/threads/palloc.h#L19) | 구현됨 |
| `palloc_get_page()` | 함수 | 페이지 하나 할당 | [threads/palloc.c:301](pintos/threads/palloc.c#L301) · [선언](pintos/include/threads/palloc.h#L18) | 구현됨 |
| `palloc_init()` | 함수 | 사용자·커널 페이지 풀 준비 | [threads/palloc.c:242](pintos/threads/palloc.c#L242) · [선언](pintos/include/threads/palloc.h#L17) | 구현됨 |
| `realloc()` | 함수 | 기존 블록의 크기 변경 | [threads/malloc.c:188](pintos/threads/malloc.c#L188) · [선언](pintos/include/threads/malloc.h#L10) | 구현됨 |
| `user_page_limit` | 전역 변수 | 사용자 풀의 최대 페이지 수 | [threads/palloc.c:40](pintos/threads/palloc.c#L40) | 정의됨 |

<a id="topic-hash"></a>

## 10. Hash · Bitmap

참고: [Hash Table](https://casys-kaist.github.io/pintos-kaist/appendix/hash_table.html) · [Introduction](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `struct bitmap` | 구조체 | 슬롯 등의 사용 여부를 저장하는 비트 배열 | [lib/kernel/bitmap.c:27](pintos/lib/kernel/bitmap.c#L27) | 정의됨 |
| `bitmap_all()` | 함수 | 구간의 모든 비트가 1인지 검사 | [lib/kernel/bitmap.c:268](pintos/lib/kernel/bitmap.c#L268) · [선언](pintos/include/lib/kernel/bitmap.h#L33) | 구현됨 |
| `bitmap_any()` | 함수 | 구간에 1인 비트가 있는지 검사 | [lib/kernel/bitmap.c:254](pintos/lib/kernel/bitmap.c#L254) · [선언](pintos/include/lib/kernel/bitmap.h#L31) | 구현됨 |
| `bitmap_buf_size()` | 함수 | 비트맵 구성에 필요한 버퍼 크기 | [lib/kernel/bitmap.c:105](pintos/lib/kernel/bitmap.c#L105) · [선언](pintos/include/lib/kernel/bitmap.h#L13) | 구현됨 |
| `bitmap_contains()` | 함수 | 구간에 주어진 값이 존재하는지 검사 | [lib/kernel/bitmap.c:235](pintos/lib/kernel/bitmap.c#L235) · [선언](pintos/include/lib/kernel/bitmap.h#L30) | 구현됨 |
| `bitmap_count()` | 함수 | 구간에서 주어진 값의 비트 수 | [lib/kernel/bitmap.c:215](pintos/lib/kernel/bitmap.c#L215) · [선언](pintos/include/lib/kernel/bitmap.h#L29) | 구현됨 |
| `bitmap_create()` | 함수 | 비트맵 메모리 할당 | [lib/kernel/bitmap.c:72](pintos/lib/kernel/bitmap.c#L72) · [선언](pintos/include/lib/kernel/bitmap.h#L11) | 구현됨 |
| `bitmap_create_in_buf()` | 함수 | 주어진 버퍼에 비트맵 구성 | [lib/kernel/bitmap.c:90](pintos/lib/kernel/bitmap.c#L90) · [선언](pintos/include/lib/kernel/bitmap.h#L12) | 구현됨 |
| `bitmap_destroy()` | 함수 | 비트맵 메모리 반납 | [lib/kernel/bitmap.c:113](pintos/lib/kernel/bitmap.c#L113) · [선언](pintos/include/lib/kernel/bitmap.h#L14) | 구현됨 |
| `bitmap_dump()` | 함수 | 비트맵 내용을 디버깅 출력 | [lib/kernel/bitmap.c:347](pintos/lib/kernel/bitmap.c#L347) · [선언](pintos/include/lib/kernel/bitmap.h#L49) | 구현됨 |
| `BITMAP_ERROR` | 매크로 | 조건에 맞는 비트 구간이 없음 | [include/lib/kernel/bitmap.h:36](pintos/include/lib/kernel/bitmap.h#L36) | 정의됨 |
| `bitmap_file_size()` | 함수 | 비트맵 저장에 필요한 파일 크기 | [lib/kernel/bitmap.c:317](pintos/lib/kernel/bitmap.c#L317) · [선언](pintos/include/lib/kernel/bitmap.h#L43) | 구현됨 |
| `bitmap_flip()` | 함수 | 비트 하나의 값 반전 | [lib/kernel/bitmap.c:170](pintos/lib/kernel/bitmap.c#L170) · [선언](pintos/include/lib/kernel/bitmap.h#L23) | 구현됨 |
| `bitmap_mark()` | 함수 | 비트 하나를 사용 상태로 설정 | [lib/kernel/bitmap.c:144](pintos/lib/kernel/bitmap.c#L144) · [선언](pintos/include/lib/kernel/bitmap.h#L21) | 구현됨 |
| `bitmap_none()` | 함수 | 구간의 모든 비트가 0인지 검사 | [lib/kernel/bitmap.c:261](pintos/lib/kernel/bitmap.c#L261) · [선언](pintos/include/lib/kernel/bitmap.h#L32) | 구현됨 |
| `bitmap_read()` | 함수 | 파일에서 비트맵 읽기 — FILESYS | [lib/kernel/bitmap.c:324](pintos/lib/kernel/bitmap.c#L324) · [선언](pintos/include/lib/kernel/bitmap.h#L44) | 구현됨 |
| `bitmap_reset()` | 함수 | 비트 하나를 미사용 상태로 설정 | [lib/kernel/bitmap.c:156](pintos/lib/kernel/bitmap.c#L156) · [선언](pintos/include/lib/kernel/bitmap.h#L22) | 구현됨 |
| `bitmap_scan()` | 함수 | 조건에 맞는 연속 비트 구간 검색 | [lib/kernel/bitmap.c:279](pintos/lib/kernel/bitmap.c#L279) · [선언](pintos/include/lib/kernel/bitmap.h#L37) | 구현됨 |
| `bitmap_scan_and_flip()` | 함수 | 연속 비트 구간 검색 후 값 반전 | [lib/kernel/bitmap.c:303](pintos/lib/kernel/bitmap.c#L303) · [선언](pintos/include/lib/kernel/bitmap.h#L38) | 구현됨 |
| `bitmap_set()` | 함수 | 비트 하나에 값 설정 | [lib/kernel/bitmap.c:132](pintos/lib/kernel/bitmap.c#L132) · [선언](pintos/include/lib/kernel/bitmap.h#L20) | 구현됨 |
| `bitmap_set_all()` | 함수 | 모든 비트를 같은 값으로 설정 | [lib/kernel/bitmap.c:192](pintos/lib/kernel/bitmap.c#L192) · [선언](pintos/include/lib/kernel/bitmap.h#L27) | 구현됨 |
| `bitmap_set_multiple()` | 함수 | 연속 비트 구간에 값 설정 | [lib/kernel/bitmap.c:200](pintos/lib/kernel/bitmap.c#L200) · [선언](pintos/include/lib/kernel/bitmap.h#L28) | 구현됨 |
| `bitmap_size()` | 함수 | 비트맵의 전체 비트 수 | [lib/kernel/bitmap.c:124](pintos/lib/kernel/bitmap.c#L124) · [선언](pintos/include/lib/kernel/bitmap.h#L17) | 구현됨 |
| `bitmap_test()` | 함수 | 비트 하나의 현재 값 조회 | [lib/kernel/bitmap.c:182](pintos/lib/kernel/bitmap.c#L182) · [선언](pintos/include/lib/kernel/bitmap.h#L24) | 구현됨 |
| `bitmap_write()` | 함수 | 파일에 비트맵 기록 — FILESYS | [lib/kernel/bitmap.c:337](pintos/lib/kernel/bitmap.c#L337) · [선언](pintos/include/lib/kernel/bitmap.h#L45) | 구현됨 |
| `struct foo` | 예시 구조체 | 해시 원소를 담는 가상 구조체 | [명세 예시](https://casys-kaist.github.io/pintos-kaist/appendix/hash_table.html) | 코드 없음 |
| `struct hash` | 구조체 | 해시 테이블 본체 | [include/lib/kernel/hash.h:57](pintos/include/lib/kernel/hash.h#L57) | 정의됨 |
| `hash_action_func` | typedef | 원소별 처리 콜백 타입 | [include/lib/kernel/hash.h:54](pintos/include/lib/kernel/hash.h#L54) | 정의됨 |
| `hash_apply()` | 함수 | 모든 원소에 처리 콜백 실행 | [lib/kernel/hash.c:155](pintos/lib/kernel/hash.c#L155) · [선언](pintos/include/lib/kernel/hash.h#L85) | 구현됨 |
| `hash_bytes()` | 함수 | 바이트열의 해시값 계산 | [lib/kernel/hash.c:248](pintos/lib/kernel/hash.c#L248) · [선언](pintos/include/lib/kernel/hash.h#L95) | 구현됨 |
| `hash_clear()` | 함수 | 원소 정리 후 빈 테이블 유지 | [lib/kernel/hash.c:51](pintos/lib/kernel/hash.c#L51) · [선언](pintos/include/lib/kernel/hash.h#L75) | 구현됨 |
| `hash_cur()` | 함수 | 현재 순회 원소 조회 | [lib/kernel/hash.c:226](pintos/lib/kernel/hash.c#L226) · [선언](pintos/include/lib/kernel/hash.h#L88) | 구현됨 |
| `hash_delete()` | 함수 | 동일 키의 원소 분리 | [lib/kernel/hash.c:139](pintos/lib/kernel/hash.c#L139) · [선언](pintos/include/lib/kernel/hash.h#L82) | 구현됨 |
| `hash_destroy()` | 함수 | 원소와 테이블의 버킷 메모리 정리 | [lib/kernel/hash.c:82](pintos/lib/kernel/hash.c#L82) · [선언](pintos/include/lib/kernel/hash.h#L76) | 구현됨 |
| `struct hash_elem` | 구조체 | 구조체에 삽입할 해시 연결 요소 | [include/lib/kernel/hash.h:29](pintos/include/lib/kernel/hash.h#L29) | 정의됨 |
| `hash_empty()` | 함수 | 빈 테이블인지 확인 | [lib/kernel/hash.c:238](pintos/lib/kernel/hash.c#L238) · [선언](pintos/include/lib/kernel/hash.h#L92) | 구현됨 |
| `hash_entry` | 매크로 | 해시 요소에서 포함 구조체 주소 복원 | [include/lib/kernel/hash.h:38](pintos/include/lib/kernel/hash.h#L38) | 정의됨 |
| `hash_find()` | 함수 | 동일 키의 원소 조회 | [lib/kernel/hash.c:127](pintos/lib/kernel/hash.c#L127) · [선언](pintos/include/lib/kernel/hash.h#L81) | 구현됨 |
| `hash_first()` | 함수 | 순회 시작 위치 설정 | [lib/kernel/hash.c:189](pintos/lib/kernel/hash.c#L189) · [선언](pintos/include/lib/kernel/hash.h#L86) | 구현됨 |
| `hash_hash_func` | typedef | 키의 해시값 계산 함수 타입 | [include/lib/kernel/hash.h:44](pintos/include/lib/kernel/hash.h#L44) | 정의됨 |
| `hash_init()` | 함수 | 해시·비교 함수를 지정하여 초기화 | [lib/kernel/hash.c:24](pintos/lib/kernel/hash.c#L24) · [선언](pintos/include/lib/kernel/hash.h#L74) | 구현됨 |
| `hash_insert()` | 함수 | 중복이 없으면 원소 삽입 | [lib/kernel/hash.c:94](pintos/lib/kernel/hash.c#L94) · [선언](pintos/include/lib/kernel/hash.h#L79) | 구현됨 |
| `hash_int()` | 함수 | 정수의 해시값 계산 | [lib/kernel/hash.c:281](pintos/lib/kernel/hash.c#L281) · [선언](pintos/include/lib/kernel/hash.h#L97) | 구현됨 |
| `struct hash_iterator` | 구조체 | 해시 테이블 순회 위치 | [include/lib/kernel/hash.h:67](pintos/include/lib/kernel/hash.h#L67) | 정의됨 |
| `hash_less_func` | typedef | 두 키의 순서 비교 함수 타입 | [include/lib/kernel/hash.h:49](pintos/include/lib/kernel/hash.h#L49) | 정의됨 |
| `hash_next()` | 함수 | 다음 해시 원소로 이동 | [lib/kernel/hash.c:207](pintos/lib/kernel/hash.c#L207) · [선언](pintos/include/lib/kernel/hash.h#L87) | 구현됨 |
| `hash_replace()` | 함수 | 동일 키의 원소 교체 | [lib/kernel/hash.c:110](pintos/lib/kernel/hash.c#L110) · [선언](pintos/include/lib/kernel/hash.h#L80) | 구현됨 |
| `hash_size()` | 함수 | 원소 개수 조회 | [lib/kernel/hash.c:232](pintos/lib/kernel/hash.c#L232) · [선언](pintos/include/lib/kernel/hash.h#L91) | 구현됨 |
| `hash_string()` | 함수 | NUL 종료 문자열의 해시값 계산 | [lib/kernel/hash.c:265](pintos/lib/kernel/hash.c#L265) · [선언](pintos/include/lib/kernel/hash.h#L96) | 구현됨 |
| `struct list` | 구조체 | 프레임 등 연결 목록의 본체 | [include/lib/kernel/list.h:93](pintos/include/lib/kernel/list.h#L93) | 정의됨 |
| `struct list_elem` | 구조체 | 구조체에 삽입할 목록 연결 요소 | [include/lib/kernel/list.h:87](pintos/include/lib/kernel/list.h#L87) | 정의됨 |
| `list_entry` | 매크로 | 목록 요소에서 포함 구조체 주소 복원 | [include/lib/kernel/list.h:103](pintos/include/lib/kernel/list.h#L103) | 정의됨 |
| `page_hash()` | 예시 함수 | 가상 주소를 키로 해싱하는 부록 예시 | [명세 예시](https://casys-kaist.github.io/pintos-kaist/appendix/hash_table.html) | 코드 없음 |
| `page_less()` | 예시 함수 | 가상 주소의 순서를 비교하는 부록 예시 | [명세 예시](https://casys-kaist.github.io/pintos-kaist/appendix/hash_table.html) | 코드 없음 |
| `page_lookup()` | 예시 함수 | 해시로 페이지를 찾는 부록 예시 | [명세 예시](https://casys-kaist.github.io/pintos-kaist/appendix/hash_table.html) | 코드 없음 |

<a id="topic-thread"></a>

## 11. 동기화 · Thread

참고: [Synchronization](https://casys-kaist.github.io/pintos-kaist/appendix/synchronization.html) · [Threads](https://casys-kaist.github.io/pintos-kaist/appendix/threads.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `barrier` | 매크로 | 컴파일러의 메모리 연산 재배치 제한 | [include/threads/synch.h:47](pintos/include/threads/synch.h#L47) | 정의됨 |
| `cond_broadcast()` | 함수 | 모든 조건 대기자 깨우기 | [threads/synch.c:632](pintos/threads/synch.c#L632) · [선언](pintos/include/threads/synch.h#L40) | 구현됨 |
| `cond_init()` | 함수 | 조건 변수 초기화 | [threads/synch.c:530](pintos/threads/synch.c#L530) · [선언](pintos/include/threads/synch.h#L37) | 구현됨 |
| `cond_signal()` | 함수 | 조건 대기자 한 명 깨우기 | [threads/synch.c:601](pintos/threads/synch.c#L601) · [선언](pintos/include/threads/synch.h#L39) | 구현됨 |
| `cond_wait()` | 함수 | 락을 놓고 신호 대기 후 재획득 | [threads/synch.c:564](pintos/threads/synch.c#L564) · [선언](pintos/include/threads/synch.h#L38) | 구현됨 |
| `struct condition` | 구조체 | 조건을 기다리는 스레드 목록 | [include/threads/synch.h:33](pintos/include/threads/synch.h#L33) | 정의됨 |
| `intr_disable()` | 함수 | 인터럽트 비활성화 | [threads/interrupt.c:145](pintos/threads/interrupt.c#L145) · [선언](pintos/include/threads/interrupt.h#L16) | 구현됨 |
| `intr_enable()` | 함수 | 인터럽트 활성화 | [threads/interrupt.c:130](pintos/threads/interrupt.c#L130) · [선언](pintos/include/threads/interrupt.h#L15) | 구현됨 |
| `intr_get_level()` | 함수 | 현재 인터럽트 상태 조회 | [threads/interrupt.c:109](pintos/threads/interrupt.c#L109) · [선언](pintos/include/threads/interrupt.h#L13) | 구현됨 |
| `enum intr_level` | enum | 인터럽트 활성화 상태 | [include/threads/interrupt.h:8](pintos/include/threads/interrupt.h#L8) | 정의됨 |
| `INTR_OFF` | 열거값 | 인터럽트 비활성화 | [include/threads/interrupt.h:9](pintos/include/threads/interrupt.h#L9) | 정의됨 |
| `INTR_ON` | 열거값 | 인터럽트 활성화 | [include/threads/interrupt.h:10](pintos/include/threads/interrupt.h#L10) | 정의됨 |
| `intr_set_level()` | 함수 | 상태 변경 후 이전 상태 반환 | [threads/interrupt.c:124](pintos/threads/interrupt.c#L124) · [선언](pintos/include/threads/interrupt.h#L14) | 구현됨 |
| `intr_yield_on_return()` | 함수 | 인터럽트 복귀 때 선점 요청 | [threads/interrupt.c:259](pintos/threads/interrupt.c#L259) · [선언](pintos/include/threads/interrupt.h#L73) | 구현됨 |
| `struct lock` | 구조체 | 소유자가 있는 배타적 접근 락 | [include/threads/synch.h:20](pintos/include/threads/synch.h#L20) | 정의됨 |
| `lock_acquire()` | 함수 | 필요하면 기다린 뒤 락 획득 | [threads/synch.c:309](pintos/threads/synch.c#L309) · [선언](pintos/include/threads/synch.h#L27) | 구현됨 |
| `lock_held_by_current_thread()` | 함수 | 현재 스레드의 락 소유 여부 | [threads/synch.c:497](pintos/threads/synch.c#L497) · [선언](pintos/include/threads/synch.h#L30) | 구현됨 |
| `lock_init()` | 함수 | 소유자 없는 락 초기화 | [threads/synch.c:292](pintos/threads/synch.c#L292) · [선언](pintos/include/threads/synch.h#L26) | 구현됨 |
| `lock_release()` | 함수 | 현재 소유자의 락 반납 | [threads/synch.c:445](pintos/threads/synch.c#L445) · [선언](pintos/include/threads/synch.h#L29) | 구현됨 |
| `lock_try_acquire()` | 함수 | 기다리지 않고 락 획득 시도 | [threads/synch.c:425](pintos/threads/synch.c#L425) · [선언](pintos/include/threads/synch.h#L28) | 구현됨 |
| `PRI_DEFAULT` | 매크로 | 기본 스레드 우선순위 | [include/threads/thread.h:27](pintos/include/threads/thread.h#L27) | 정의됨 |
| `PRI_MAX` | 매크로 | 최고 스레드 우선순위 | [include/threads/thread.h:28](pintos/include/threads/thread.h#L28) | 정의됨 |
| `PRI_MIN` | 매크로 | 최저 스레드 우선순위 | [include/threads/thread.h:26](pintos/include/threads/thread.h#L26) | 정의됨 |
| `ready_list` | 전역 변수 | 실행 준비 스레드의 목록 | [threads/thread.c:30](pintos/threads/thread.c#L30) | 정의됨 |
| `sema_down()` | 함수 | 카운터 감소; 불가능하면 대기 | [threads/synch.c:131](pintos/threads/synch.c#L131) · [선언](pintos/include/threads/synch.h#L14) | 구현됨 |
| `sema_init()` | 함수 | 세마포어의 초기 카운터 설정 | [threads/synch.c:108](pintos/threads/synch.c#L108) · [선언](pintos/include/threads/synch.h#L13) | 구현됨 |
| `sema_try_down()` | 함수 | 기다리지 않고 카운터 감소 시도 | [threads/synch.c:190](pintos/threads/synch.c#L190) · [선언](pintos/include/threads/synch.h#L15) | 구현됨 |
| `sema_up()` | 함수 | 카운터 증가와 대기자 깨우기 | [threads/synch.c:217](pintos/threads/synch.c#L217) · [선언](pintos/include/threads/synch.h#L16) | 구현됨 |
| `struct semaphore` | 구조체 | 카운터와 대기 스레드 목록 | [include/threads/synch.h:8](pintos/include/threads/synch.h#L8) | 정의됨 |
| `struct thread` | 구조체 | 프로세스 상태와 커널 스택의 소유자 | [include/threads/thread.h:95](pintos/include/threads/thread.h#L95) | 정의됨 |
| `thread.elem` | 멤버 | 실행·수면·대기 목록의 연결 요소 | [include/threads/thread.h:106](pintos/include/threads/thread.h#L106) | 정의됨 |
| `thread.magic` | 멤버 | 커널 스택 손상 검사 값 | [include/threads/thread.h:152](pintos/include/threads/thread.h#L152) | 정의됨 |
| `thread.name` | 멤버 | 스레드의 디버깅 이름 | [include/threads/thread.h:99](pintos/include/threads/thread.h#L99) | 정의됨 |
| `thread.pml4` | 멤버 | 프로세스의 최상위 페이지 테이블 | [include/threads/thread.h:132](pintos/include/threads/thread.h#L132) | 정의됨 |
| `thread.priority` | 멤버 | 현재 스레드 우선순위 | [include/threads/thread.h:100](pintos/include/threads/thread.h#L100) | 정의됨 |
| `thread.running_file` | 멤버 | 실행 파일의 쓰기 금지 보유권 | [include/threads/thread.h:142](pintos/include/threads/thread.h#L142) | 정의됨 |
| `thread.spt` | 멤버 | 프로세스의 보조 페이지 테이블 | [include/threads/thread.h:146](pintos/include/threads/thread.h#L146) | 정의됨 |
| `thread.status` | 멤버 | 스케줄러 상태; 종료값과 별개 | [include/threads/thread.h:98](pintos/include/threads/thread.h#L98) | 정의됨 |
| `thread.tf` | 멤버 | 스케줄링용 저장 실행 문맥 | [include/threads/thread.h:151](pintos/include/threads/thread.h#L151) | 정의됨 |
| `thread.tid` | 멤버 | 스레드 식별자 | [include/threads/thread.h:97](pintos/include/threads/thread.h#L97) | 정의됨 |
| `thread_block()` | 함수 | 현재 스레드를 대기 상태로 변경 | [threads/thread.c:298](pintos/threads/thread.c#L298) · [선언](pintos/include/threads/thread.h#L178) | 구현됨 |
| `THREAD_BLOCKED` | 열거값 | 이벤트나 자원 대기 상태 | [include/threads/thread.h:16](pintos/include/threads/thread.h#L16) | 정의됨 |
| `thread_create()` | 함수 | 새 스레드와 커널 스택 생성 | [threads/thread.c:219](pintos/threads/thread.c#L219) · [선언](pintos/include/threads/thread.h#L176) | 구현됨 |
| `thread_current()` | 함수 | 현재 실행 중인 스레드 조회 | [threads/thread.c:341](pintos/threads/thread.c#L341) · [선언](pintos/include/threads/thread.h#L181) | 구현됨 |
| `THREAD_DYING` | 열거값 | 종료 후 자원 회수 대기 상태 | [include/threads/thread.h:17](pintos/include/threads/thread.h#L17) | 정의됨 |
| `thread_exit()` | 함수 | 프로세스 정리 후 현재 스레드 종료 | [threads/thread.c:364](pintos/threads/thread.c#L364) · [선언](pintos/include/threads/thread.h#L185) | 구현됨 |
| `thread_func` | typedef | 새 스레드 시작 함수 타입 | [include/threads/thread.h:175](pintos/include/threads/thread.h#L175) | 정의됨 |
| `thread_get_priority()` | 함수 | 현재 스레드의 우선순위 조회 | [threads/thread.c:579](pintos/threads/thread.c#L579) · [선언](pintos/include/threads/thread.h#L193) | 구현됨 |
| `thread_init()` | 함수 | 부팅 초기 스레드 시스템 준비 | [threads/thread.c:118](pintos/threads/thread.c#L118) · [선언](pintos/include/threads/thread.h#L169) | 구현됨 |
| `THREAD_MAGIC` | 매크로 | 스레드 구조체·스택 손상 검사 값 | [threads/thread.c:22](pintos/threads/thread.c#L22) | 정의됨 |
| `thread_name()` | 함수 | 현재 스레드 이름 조회 | [threads/thread.c:333](pintos/threads/thread.c#L333) · [선언](pintos/include/threads/thread.h#L183) | 구현됨 |
| `thread_print_stats()` | 함수 | 스레드 실행 통계 출력 | [threads/thread.c:198](pintos/threads/thread.c#L198) · [선언](pintos/include/threads/thread.h#L173) | 구현됨 |
| `THREAD_READY` | 열거값 | 실행 준비 상태 | [include/threads/thread.h:15](pintos/include/threads/thread.h#L15) | 정의됨 |
| `THREAD_RUNNING` | 열거값 | 현재 CPU에서 실행 중 | [include/threads/thread.h:14](pintos/include/threads/thread.h#L14) | 정의됨 |
| `thread_set_priority()` | 함수 | 현재 스레드의 기본 우선순위 변경 | [threads/thread.c:543](pintos/threads/thread.c#L543) · [선언](pintos/include/threads/thread.h#L194) | 구현됨 |
| `thread_start()` | 함수 | idle 스레드 생성과 스케줄러 시작 | [threads/thread.c:158](pintos/threads/thread.c#L158) · [선언](pintos/include/threads/thread.h#L170) | 구현됨 |
| `enum thread_status` | enum | 스케줄러가 관리하는 스레드 상태 | [include/threads/thread.h:13](pintos/include/threads/thread.h#L13) | 정의됨 |
| `thread_tick()` | 함수 | 타이머 틱의 실행 시간 통계 처리 | [threads/thread.c:174](pintos/threads/thread.c#L174) · [선언](pintos/include/threads/thread.h#L172) | 구현됨 |
| `thread_tid()` | 함수 | 현재 스레드 ID 조회 | [threads/thread.c:357](pintos/threads/thread.c#L357) · [선언](pintos/include/threads/thread.h#L182) | 구현됨 |
| `thread_unblock()` | 함수 | 대기 스레드를 실행 준비로 변경 | [threads/thread.c:316](pintos/threads/thread.c#L316) · [선언](pintos/include/threads/thread.h#L179) | 구현됨 |
| `thread_yield()` | 함수 | 다른 실행 준비 스레드에 CPU 양보 | [threads/thread.c:387](pintos/threads/thread.c#L387) · [선언](pintos/include/threads/thread.h#L186) | 구현됨 |
| `TID_ERROR` | 매크로 | 스레드 생성 실패 식별자 | [include/threads/thread.h:23](pintos/include/threads/thread.h#L23) | 정의됨 |
| `tid_t` | typedef | 스레드 식별자 타입 | [include/threads/thread.h:22](pintos/include/threads/thread.h#L22) | 정의됨 |

<a id="topic-debug"></a>

## 12. 디버깅 · 검사 API

참고: [Debugging Tools](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) · [Development Tools](https://casys-kaist.github.io/pintos-kaist/appendix/development_tools.html)

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `ASSERT` | 매크로 | 조건이 거짓이면 위치와 역추적 출력 | [include/lib/debug.h:28](pintos/include/lib/debug.h#L28) | 정의됨 |
| `debug_backtrace()` | 함수 | 현재 실행 흐름의 호출 스택 출력 | [lib/debug.c:12](pintos/lib/debug.c#L12) · [선언](pintos/include/lib/debug.h#L18) | 구현됨 |
| `debug_backtrace_all()` | 함수 | 부록에 있으나 현재 저장소에는 없음 | [부록](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 코드 없음 |
| `debug_panic()` | 함수 | panic 정보와 역추적 출력 후 중단 | [lib/kernel/debug.c:14](pintos/lib/kernel/debug.c#L14) · [선언](pintos/include/lib/debug.h#L16) | 구현됨 |
| `inspect()` | 함수 | VM 채점용 물리 주소 검사 — 수정 금지 | [vm/inspect.c:9](pintos/vm/inspect.c#L9) | 구현됨 |
| `intr_entry` | 어셈블리 | 저장 문맥을 구성하는 인터럽트 진입점 | [threads/intr-stubs.S:16](pintos/threads/intr-stubs.S#L16) | 구현됨 |
| `intr_handler()` | 함수 | 인터럽트·예외의 공통 디스패처 | [threads/interrupt.c:325](pintos/threads/interrupt.c#L325) | 구현됨 |
| `NO_INLINE` | 매크로 | 함수 인라인 전개 금지 속성 | [include/lib/debug.h:9](pintos/include/lib/debug.h#L9) | 정의됨 |
| `NO_RETURN` | 매크로 | 반환하지 않는 함수 속성 | [include/lib/debug.h:8](pintos/include/lib/debug.h#L8) | 정의됨 |
| `NOT_REACHED` | 매크로 | 도달 불가능한 경로 검출 | [include/lib/debug.h:33](pintos/include/lib/debug.h#L33) | 정의됨 |
| `PANIC` | 매크로 | 오류 메시지 출력 후 커널 중단 | [include/lib/debug.h:14](pintos/include/lib/debug.h#L14) | 정의됨 |
| `printf()` (공통) | 함수 | 커널·사용자 공통 서식 출력 | [lib/stdio.c:76](pintos/lib/stdio.c#L76) · [선언](pintos/include/lib/stdio.h#L19) | 구현됨 |
| `PRINTF_FORMAT` | 매크로 | printf 형식·인자 타입 검사 속성 | [include/lib/debug.h:10](pintos/include/lib/debug.h#L10) | 정의됨 |
| `register_disk_inspect_intr()` | 함수 | 디스크 채점용 카운터 인터럽트 등록 | [devices/disk.c:739](pintos/devices/disk.c#L739) · [선언](pintos/include/devices/disk.h#L35) | 구현됨 |
| `register_inspect_intr()` | 함수 | VM 채점용 인터럽트 등록 — 수정 금지 | [vm/inspect.c:20](pintos/vm/inspect.c#L20) · [선언](pintos/include/vm/inspect.h#L3) | 구현됨 |
| `UNUSED` | 매크로 | 사용하지 않는 인자의 경고 억제 | [include/lib/debug.h:7](pintos/include/lib/debug.h#L7) | 정의됨 |

<a id="comparison"></a>

## 도표·명세 대조

[도표 vm1.png](vm1.png)의 함수 22개와 매크로 2개는 모두 위 색인에 있다. 이 이름들은 제공한 Project 3·Appendix에서도 확인되며, 도표에만 등장하는 별도 함수는 없다.

- 도표의 `PALLOC_USER` → `PAL_USER`, `VM_MARKER` → `VM_MARKER_0`, 제목의 `PLM4` → `PML4`.
- `uninit->init`·`uninit->page_initializer`는 독립 함수가 아니라 `uninit_page`의 콜백 멤버다.

<details>
<summary>문서·현재 코드의 이름과 경로 차이</summary>

- 문서 오타: `uninit_intialize` → `uninit_initialize`, `file_backed_destory` → `file_backed_destroy`, `pgdir_destory` → `pgdir_destroy`. ([Memory Management](https://casys-kaist.github.io/pintos-kaist/project3/vm_management.html) · [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html) · [Memory Mapped Files](https://casys-kaist.github.io/pintos-kaist/project3/memory_mapped_files.html) · [Swap In/Out](https://casys-kaist.github.io/pintos-kaist/project3/swapping.html) · [Page Table](https://casys-kaist.github.io/pintos-kaist/appendix/page_table.html))
- 문서의 `vm_file_initializer`·`file_initializer`는 실제 `file_backed_initializer`로, `file_backed_init`는 `vm_file_init`로 찾는다. ([Memory Mapped Files](https://casys-kaist.github.io/pintos-kaist/project3/memory_mapped_files.html) · [Swap In/Out](https://casys-kaist.github.io/pintos-kaist/project3/swapping.html))
- 경로 차이: `block.h`·`block.c` 대신 `devices/disk.h`·`devices/disk.c`, `include/vm/page.h`·`vm/vm.h` 대신 `include/vm/vm.h`, `include/vm/uninit.c` 대신 `vm/uninit.c`. ([Introduction](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html) · [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html))
- `page_hash`·`page_less`·`page_lookup`·`stat_page`·`foo`·mmap 예제의 `main`은 설명용 예시다. 해시 부록의 `page.addr`는 현재 `page.va`와 구분한다. ([Hash Table](https://casys-kaist.github.io/pintos-kaist/appendix/hash_table.html) · [Virtual Address](https://casys-kaist.github.io/pintos-kaist/appendix/virtual_address.html) · [Introduction](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html))
- `debug_backtrace_all`은 현재 선언·정의가 없다. 역추적 예시의 커널 `seek` 위치는 현재 `sys_seek`이며, `printf` 본문은 공통 `lib/stdio.c`에 있다. ([Debugging Tools](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html))
- `mmap`·`munmap` 사용자 래퍼와 번호는 존재하지만 커널 분기는 없다. `load_segment`·`setup_stack` 링크는 비VM 구현이 아닌 VM 분기다. ([Memory Mapped Files](https://casys-kaist.github.io/pintos-kaist/project3/memory_mapped_files.html) · [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html))
- 현재 해시 함수·`hash_hash_func`의 반환형은 부록의 `unsigned`와 달리 `uint64_t`다. ([Hash Table](https://casys-kaist.github.io/pintos-kaist/appendix/hash_table.html))

</details>

<a id="tools"></a>

## 도구·명령

외부 도구·명령은 커널 코드 미구현 항목과 구분한다. 에디터 단축키는 API 색인에 포함하지 않는다.

| 이름 | 종류 | 한 줄 역할 | 위치 | 구현 상태 |
|---|---|---|---|---|
| `backtrace` | 도구 | 역추적 주소를 함수·소스 위치로 변환 | [utils/backtrace:1](pintos/utils/backtrace#L1) | 구현됨 |
| `pintos` | 도구 | QEMU 실행과 GDB 연결 옵션 | [utils/pintos:1](pintos/utils/pintos#L1) | 구현됨 |
| `pintos-mkdisk` | 도구 | Pintos 디스크 이미지 생성 | [utils/pintos-mkdisk:1](pintos/utils/pintos-mkdisk#L1) | 구현됨 |
| `make TAGS` | Make 대상 | Emacs 태그 인덱스 생성 | [Makefile:21](pintos/Makefile#L21) | 정의됨 |
| `make tags` | Make 대상 | vi 계열 태그 인덱스 생성 | [Makefile:24](pintos/Makefile#L24) | 정의됨 |
| `make cscope` | Make 대상 | 심볼·호출 위치 검색 인덱스 생성 | [Makefile:30](pintos/Makefile#L30) | 정의됨 |
| `cscope.files` | Make 대상 | cscope가 분석할 소스 목록 | [Makefile:27](pintos/Makefile#L27) | 정의됨 |
| `cscope` | 외부 도구 | 함수 정의·호출 위치 검색 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/development_tools.html) | 외부 도구 |
| `gdb` | 외부 도구 | 원격 커널 디버거 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 도구 |
| `target remote localhost:1234` | GDB 명령 | QEMU의 디버깅 포트에 연결 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `c / continue` | GDB 명령 | 다음 중단점까지 실행 재개 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `break 함수 / 파일:줄 / 주소` | GDB 명령 | 함수·위치·주소에 중단점 설정 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `p / print 표현식` | GDB 명령 | 표현식 평가·출력 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `l / list 주소` | GDB 명령 | 주소 주변의 소스 표시 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `bt / backtrace` | GDB 명령 | 현재 호출 스택 표시 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `p/a 주소` | GDB 명령 | 주소에 해당하는 심볼 표시 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `disassemble 함수` | GDB 명령 | 함수의 어셈블리 표시 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `info gdb` | 셸 명령 | GDB 매뉴얼 열기 | [사용법](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) | 외부 명령 |
| `make fast` | Make 대상 | VM 병렬 테스트; 기존 결과 삭제·icount 사용 | [Makefile (루트):28](Makefile#L28) | 정의됨 |
| `make fcheck` | Make 대상 | VM 병렬 테스트; 기존 결과 삭제·icount 없음 | [Makefile (루트):29](Makefile#L29) | 정의됨 |
| `PINTOS_QEMU_EXTRA` | 환경 변수 | QEMU 실행 시 추가 옵션 전달 | [utils/pintos:139](pintos/utils/pintos#L139) | 정의됨 |

<a id="sources"></a>

## 참고

제공한 14개 문서를 모두 대조했다. 추가 Threads·Synchronization은 VM 관련 항목만 보강했다. 일반 지역변수·매개변수·C 기본 문법은 제외했다.

- Project 3: [Introduction](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html) · [Memory Management](https://casys-kaist.github.io/pintos-kaist/project3/vm_management.html) · [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html) · [Stack Growth](https://casys-kaist.github.io/pintos-kaist/project3/stack_growth.html) · [Memory Mapped Files](https://casys-kaist.github.io/pintos-kaist/project3/memory_mapped_files.html) · [Swap In/Out](https://casys-kaist.github.io/pintos-kaist/project3/swapping.html) · [COW Extra](https://casys-kaist.github.io/pintos-kaist/project3/cow.html) · [FAQ](https://casys-kaist.github.io/pintos-kaist/project3/FAQ.html)
- 제공 Appendix: [Memory Allocation](https://casys-kaist.github.io/pintos-kaist/appendix/memory_allocation.html) · [Virtual Address](https://casys-kaist.github.io/pintos-kaist/appendix/virtual_address.html) · [Page Table](https://casys-kaist.github.io/pintos-kaist/appendix/page_table.html) · [Debugging Tools](https://casys-kaist.github.io/pintos-kaist/appendix/debugging_tools.html) · [Development Tools](https://casys-kaist.github.io/pintos-kaist/appendix/development_tools.html) · [Hash Table](https://casys-kaist.github.io/pintos-kaist/appendix/hash_table.html)
- 추가 Appendix: [Synchronization](https://casys-kaist.github.io/pintos-kaist/appendix/synchronization.html) · [Threads](https://casys-kaist.github.io/pintos-kaist/appendix/threads.html)
