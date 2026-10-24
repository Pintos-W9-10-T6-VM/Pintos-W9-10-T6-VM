<p align="center">
  <img src="assets/readme-hero.svg" alt="Pintos — Jungle Team 06 · Project 3 Virtual Memory" width="100%" />
</p>

<div align="center">

### 🌱 가상 주소에서 물리 메모리까지, 필요한 순간에 연결합니다.

**KRAFTON JUNGLE · WEEK 09–10 · TEAM 06**

Project 2의 사용자 프로그램 실행 기반 위에 가상 메모리를 구현합니다.<br />
페이지를 필요할 때 적재하고, 제한된 물리 메모리를 회수·재사용하며, 파일을 메모리에 연결합니다.

**[🧩 Issues](https://github.com/Pintos-W9-10-T6-VM/Pintos-W9-10-T6-VM/issues)** &nbsp; / &nbsp;
**[🔀 Pull Requests](https://github.com/Pintos-W9-10-T6-VM/Pintos-W9-10-T6-VM/pulls)** &nbsp; / &nbsp;
**[📘 VM GitBook](https://casys-kaist.github.io/pintos-kaist/project3/introduction.html)** &nbsp; / &nbsp;
**[📏 Convention](CONVENTION.md)** &nbsp; / &nbsp;
**[📝 PR Template](.github/pull_request_template.md)** &nbsp; / &nbsp;
**[🚀 Quick Start](#quick-start)**

</div>

<br />

## 🛠️ What we're building

이번 프로젝트의 초점은 **Project 3 — Virtual Memory**입니다.
가상 페이지의 정보와 물리 프레임을 관리하고, 페이지 폴트에 맞춰 적재·복원·스택 확장을 처리합니다.
기존 프로그램 실행·시스템 콜·fork·exec·wait·exit 동작도 VM 환경에서 유지합니다.

<table>
  <tr>
    <td width="33%" valign="top">
      <h3>🗺️ 01 / MAP MEMORY</h3>
      <strong>페이지와 프레임 관리</strong>
      <p>SPT에 가상 페이지 정보를 기록하고, 물리 프레임 확보와 페이지 테이블 매핑을 연결합니다.</p>
      <sub>Supplemental Page Table · Frame · Claim</sub>
    </td>
    <td width="33%" valign="top">
      <h3>⏳ 02 / LOAD ON DEMAND</h3>
      <strong>필요한 순간의 적재</strong>
      <p>실행 파일을 지연 적재하고, 유효한 페이지 폴트를 처리하며, 사용자 스택을 필요한 만큼 확장합니다.</p>
      <sub>Lazy Loading · Anonymous Page · Stack Growth</sub>
    </td>
    <td width="33%" valign="top">
      <h3>🔄 03 / RECLAIM &amp; MAP</h3>
      <strong>메모리 회수와 파일 매핑</strong>
      <p>물리 메모리가 부족하면 페이지를 퇴거·복원하고, 파일 매핑의 변경 내용을 필요한 시점에 반영합니다.</p>
      <sub>Eviction · Swap · mmap / munmap</sub>
    </td>
  </tr>
</table>

위 구분은 프로젝트의 기능 영역입니다. 개인별 담당 배정은 별도로 합의합니다.

| 필수 구현 영역 | 확인할 동작 | 공식 명세 |
|:---|:---|:---|
| 페이지·프레임 관리 | SPT 조회·등록, 프레임 확보, claim과 매핑, 실패 시 자원 회수 | [Memory Management](https://casys-kaist.github.io/pintos-kaist/project3/vm_management.html) |
| 익명 페이지·지연 적재 | UNINIT 초기화, 실행 파일 지연 적재, SPT 복제와 정리 | [Anonymous Page](https://casys-kaist.github.io/pintos-kaist/project3/anon.html) |
| 스택 확장 | 사용자 RSP와 접근 위치로 확장 여부 판단, 최대 1 MiB 제한 | [Stack Growth](https://casys-kaist.github.io/pintos-kaist/project3/stack_growth.html) |
| 파일 매핑 | mmap·munmap, 독립 파일 보유권, dirty 데이터 writeback | [Memory Mapped Files](https://casys-kaist.github.io/pintos-kaist/project3/memory_mapped_files.html) |
| 페이지 교체·swap | 희생 프레임 선정, 익명 페이지 swap, 파일 기반 페이지 복원·반영 | [Swap In/Out](https://casys-kaist.github.io/pintos-kaist/project3/swapping.html) |

위 표는 구현 목표입니다. 현재 VM 핵심 함수에는 TODO가 남아 있으며, 기능 완료와 테스트 결과는 관련 이슈·PR에 기록합니다.

<details>
<summary><strong>🌿 Extra / Copy-on-Write</strong></summary>

[COW](https://casys-kaist.github.io/pintos-kaist/project3/cow.html)는 선택 과제로 분리합니다.
fork에서 물리 페이지를 공유하다가 쓰기 시 복사하는 방식이며, 팀의 구현 여부는 추후 결정합니다.
현재 [VM 테스트 설정](pintos/vm/Make.vars)에는 COW 테스트도 포함되어 있으므로, 전체 테스트 결과를 정리할 때 필수 VM 결과와 Extra 결과를 구분합니다.
테스트 등록 여부는 구현 완료를 의미하지 않습니다.

</details>
<br />

## ⚙️ Built on a shared stack

| Layer | Stack | Purpose |
|:---|:---|:---|
| Architecture | x86-64 | KAIST Pintos의 64비트 커널 환경 |
| Language | C · Assembly | 페이지·프레임·폴트 처리와 기존 프로세스 기능 연동 |
| Toolchain | GCC · GNU Make · GDB | 빌드와 디버깅 |
| Convention | clangd · clang-format · clang-tidy | 저장 시 포맷, 코드 스타일·이름 검사 |
| Runtime | QEMU | 커널 실행과 테스트 |
| Workspace | Ubuntu 22.04 · Docker · VS Code Dev Containers | 팀이 공유하는 개발 환경 |

환경 설정은 [Dockerfile](.devcontainer/Dockerfile)과 [devcontainer.json](.devcontainer/devcontainer.json)에 있습니다.
<br />

## 🤝 Three developers. One system.

**작은 변경을 일찍 공유하고, 공통 인터페이스와 실행 흐름을 함께 검증합니다.**

| 개인 브랜치 | 담당 기능 | 관련 이슈 | 마감 |
|:---:|:---|:---|:---|
| CJG | 미정 | 미정 | 미정 |
| YYJ | 미정 | 미정 | 미정 |
| SHW | 미정 | 미정 | 미정 |

구현 범위를 3분할한 뒤 개인별 마감일을 정합니다.
공통 함수·구조체·동기화 조건과 파트 간 의존성을 먼저 합의합니다.

<table>
  <tr>
    <td width="50%" valign="top">
      <h3>📌 함께 맞출 것</h3>
      <p>SPT·프레임 상태, 페이지 권한, lazy aux와 파일 보유권, swap 슬롯 수명, 락 순서와 자원 해제 책임을 공유합니다.</p>
    </td>
    <td width="50%" valign="top">
      <h3>🔍 함께 확인할 것</h3>
      <p>각 담당은 기능 테스트와 실패 분석을 책임집니다. 공통 코드 변경과 파트 간 연결에서 발생한 실패는 관련 담당이 함께 확인합니다.</p>
    </td>
  </tr>
</table>
<br />

## 🌿 Five branches. One workflow.

브랜치는 **main · develop · CJG · YYJ · SHW** 다섯 개를 기준으로 운영합니다.

| 브랜치 | 역할 | PR 대상 |
|:---:|:---|:---:|
| main | 통합 검증한 결과 | — |
| develop | 팀 작업 통합 | main |
| CJG | 개인 작업 | develop |
| YYJ | 개인 작업 | develop |
| SHW | 개인 작업 | develop |

<div align="center">

**CJG · YYJ · SHW** &nbsp; → &nbsp; **PR → develop** &nbsp; → &nbsp; **통합 검증 → PR → main**

</div>

- 개인 브랜치는 최초에 `develop`에서 생성합니다.
- 작업 시작 전과 PR 병합 전 최신 `develop`을 개인 브랜치에 merge합니다.
- 충돌은 개인 브랜치에서 해결하고 관련 테스트를 다시 실행합니다.
- 병합 방식은 **merge commit**을 사용합니다.
- 개인 브랜치는 계속 재사용하며 병합 후 삭제하지 않습니다.
- `main`, `develop`에 직접 Push하거나 강제 Push하지 않습니다.
- 이슈를 해결하는 작은 단위로 PR을 나눕니다.

<details>
<summary><strong>💻 개인 브랜치 동기화 예시</strong></summary>

작업 내용을 먼저 커밋해 작업 트리를 정리합니다.
아래 `YYJ`는 본인 브랜치 이름으로 바꿉니다.

```bash
git fetch origin
git switch YYJ
git merge --ff-only origin/YYJ
git merge origin/develop
```

충돌이 발생하면 파일을 수정한 뒤 merge를 완료합니다.

```bash
git add <해결한_파일>
git commit
```

이후 관련 테스트를 실행하고 개인 브랜치를 올립니다.

```bash
git push origin YYJ
```

`--ff-only`가 실패하면 로컬과 원격의 분기 상태를 확인합니다. 강제 Push로 덮어쓰지 않습니다.

</details>
<br />

## 👀 Review together. Merge responsibly.

| 항목 | 팀 운영 기준 |
|:---|:---|
| 변경 경로 | main·develop 모두 PR을 통해 반영 |
| 필수 승인 수 | **0명** — 병합 권한이 있는 작성자가 본인 PR 병합 가능 |
| 병합 권한 | 저장소 Write 이상 |
| 보호 대상 | main·develop의 강제 push와 삭제 차단 |

- GitHub ruleset은 PR 필수와 승인 수 0명으로 맞춥니다. 위 표는 운영 정책이며 실제 GitHub 설정은 별도로 확인합니다.
- 승인 없이 병합할 수 있어도 관련 담당에게 리뷰를 요청하고, PR에 변경 내용과 검증 결과를 남깁니다.
- 실제 병합 충돌은 승인 수와 관계없이 먼저 해결합니다.
- 실패 테스트와 공통 코드 영향은 병합 전에 공유합니다.
- 리뷰에서 제기된 문제는 해결 여부와 후속 조치를 남깁니다.
- CI와 이슈 자동 배정은 추후 도입합니다.
<br />

## 📝 Commit with context.

첫 줄은 **변경 목적과 결과**, 본문은 **파일별 변경 함수와 설명**으로 작성합니다.

```text
SPT 페이지 조회 및 중복 등록 검사 구현

vm.c / Update: spt_find_page(), spt_insert_page() / 페이지 기준 주소 조회와 중복 등록 거부

관련 이슈: #번호
```

| 항목 | 규칙 |
|:---|:---|
| 첫 줄 | 변경 목적과 결과를 한국어로 요약 |
| 본문 | `파일 / 변경 종류: 함수·항목 / 변경 설명` |
| 변경 종류 | `Create`, `Update`, `Delete`, `Fix` |
| 파일 구분 | 파일마다 줄을 나누고 같은 파일의 함수는 쉼표로 구분 |
| 줄 번호 | 필요할 때만 작성 |
| 관련 이슈 | 관련 이슈가 있으면 번호 기록 |
| 변경 단위 | 하나의 커밋에는 설명 가능한 하나의 목적 |

- 문서·설정 변경은 함수 대신 수정한 항목을 적습니다.
- PR 제목도 변경 목적을 요약하는 한국어 문장으로 작성합니다.
- Git이 생성하는 merge commit 메시지는 그대로 사용할 수 있습니다.
- 요청하지 않은 AI 공동 작성자 표시나 홍보 서명을 커밋·PR에 자동으로 추가하지 않습니다.
<br />

## 💬 Explain the reason in code.

코드 스타일의 상세 기준은 **[CONVENTION.md](CONVENTION.md)**입니다.

| 항목 | 공통 기준 |
|:---|:---|
| 들여쓰기 | 스페이스 4칸 |
| 중괄호 | 한 줄 본문에도 사용. 함수는 다음 줄, 제어문은 같은 줄에서 시작 |
| 공백·길이 | 제어문 뒤 한 칸, 함수 이름과 괄호는 붙임, 한 줄 80자 기준 |
| 이름 | 함수·변수·구조체는 snake_case, 상수 매크로·enum 값은 대문자 |

Pintos가 제공하는 인터페이스 이름은 유지하고, 새 이름은 컨벤션에 맞춥니다.
새 함수나 의미 있게 수정한 처리 블록에는 필요한 설명과 본인 브랜치를 남깁니다.

```c
/* [YYJ] 한 번도 적재되지 않은 페이지도 파일 정보를 보유하므로
 * 종료 시 lazy aux의 보유권을 함께 반납한다. */
```

후속 작업은 담당 브랜치와 관련 이슈를 함께 기록합니다.

```c
/* TODO [CJG] #번호: fork 부분 복제 실패 시 페이지와 파일 보유권 회수 검증. */
```

- 브랜치 표기는 `CJG`, `YYJ`, `SHW`를 사용합니다.
- 주석은 동작의 의도, 주의할 조건, 동기화 이유를 설명합니다.
- 필요한 곳에 입력 조건·반환값·메모리 소유권·락 보유 조건을 적습니다.
- 단순 대입처럼 코드만으로 명확한 내용에는 주석을 강제하지 않습니다.
- 기존 라이선스와 유효한 설명 주석은 유지합니다.
- 관련 없는 파일 전체 포맷 변경을 섞지 않습니다.
- 불필요한 디버깅 출력은 제거하고 과제에서 요구하는 출력은 유지합니다.
<br />

## 🤝 Shared code, shared understanding.

- 공통 함수·구조체를 바꾸기 전에 이슈나 PR에서 변경 범위를 공유합니다.
- 같은 파일을 수정할 때는 담당 함수와 수정 위치를 먼저 맞춥니다.
- 다른 PR에 의존하면 선행 PR과 병합 순서를 기록합니다.
- 충돌 해결 시 상대 코드의 변경 의도를 확인합니다.
- 실패 테스트는 이름·증상·실행 환경을 기록합니다.
- 남은 작업은 이슈로 남기고 담당자와 후속 조치를 정합니다.
<br />

## 🧪 Quality is part of the work.

현재 [VM 빌드의 테스트 설정](pintos/vm/Make.vars)은 다음 영역을 포함합니다.
아래 내용은 검증 범위이며 현재 통과 결과를 의미하지 않습니다.

| Test group | What we check |
|:---|:---|
| Virtual Memory | 지연 적재, 스택 증가, 페이지 교체·swap, mmap |
| User Programs Regression | VM 환경의 인자 전달·시스템 콜·fork·exec·wait·exit |
| File System Base | 파일 생성·읽기·쓰기와 기본 파일 시스템 동작 |
| Threads Regression | 기존 스레드·동기화·스케줄링 동작의 회귀 |
| COW Extra | 선택 과제. 필수 VM 결과와 분리해 기록 |

테스트 프로그램·기댓값은 수정하지 않습니다. 미구현 기능의 실패와 기존 동작의 회귀를 구분합니다.

서로의 구현이 만나는 경계는 함께 확인합니다.

| Integration check | 확인할 연결 |
|:---|:---|
| page fault → SPT → claim | 유효한 주소·권한 판별, 프레임 확보·매핑·내용 복원 |
| syscall → 사용자 버퍼 | 지연 적재·스택 증가와 주소 검증, I/O 중 페이지 수명 |
| eviction → swap / file | 소유 주소 공간의 매핑·dirty 정보, 보존·복원, 락 순서 |
| fork → SPT·파일 복제 | 미접근·상주·swap 페이지의 복제와 부분 실패 정리 |
| exec / exit → 자원 정리 | SPT·프레임·매핑·파일·swap 슬롯의 해제 책임과 순서 |

검증 결과는 **PASS / FAIL / 미실행**으로 기록합니다.
실패 원인과 미실행 이유를 적고, 미구현 기능의 실패와 기존 동작의 회귀를 구분합니다.
<br />

## ✅ Before opening a PR.

[PR 템플릿](.github/pull_request_template.md)은 새 PR의 기본 본문입니다.
각 PR에서 내용을 수정하고 해당 없는 항목은 `없음`으로 표시합니다.

- [ ] PR 목적과 관련 없는 변경을 섞지 않았습니다.
- [ ] 관련 이슈와 담당 브랜치를 기록했습니다.
- [ ] 커밋 메시지에 변경 목적과 파일별 변경 내용을 기록했습니다.
- [ ] 필요한 코드 주석에 브랜치 표기와 동작 이유를 작성했습니다.
- [ ] CONVENTION.md 기준을 확인하고 코드 변경 시 `make lint` 결과를 기록했습니다.
- [ ] 최신 대상 브랜치를 반영하고 충돌을 해결했습니다.
- [ ] 빌드·관련 테스트 결과와 실패·미실행 이유를 기록했습니다.
- [ ] 공통 함수·구조체 변경과 다른 담당에게 미치는 영향을 설명했습니다.
- [ ] 해당 변경의 자원 소유권·실패 정리·락 순서와 fork/exec/exit 영향을 확인했습니다.
- [ ] 필수 VM과 선택 Extra의 검증 결과를 구분했습니다.
- [ ] 불필요한 디버깅 출력·생성 파일·인증 정보를 제외했습니다.

커밋마다 템플릿 파일 자체를 수정할 필요는 없습니다.
체크리스트는 작성자의 확인 기록이며 자동 검사는 아닙니다.
템플릿은 기본 브랜치인 `main`에 반영된 뒤 새 PR의 기본 본문으로 사용됩니다. ([GitHub 안내](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/creating-a-pull-request-template-for-your-repository))
<br />

<a id="quick-start"></a>

## 🚀 Quick Start

### 1. 팀 저장소 가져오기

호스트 터미널에서 실행합니다.

```bash
git clone https://github.com/Pintos-W9-10-T6-VM/Pintos-W9-10-T6-VM.git
cd Pintos-W9-10-T6-VM
git switch develop
```

개인 작업을 시작할 때 본인 브랜치로 전환합니다.

```bash
git switch YYJ
```

`YYJ`는 본인 브랜치인 `CJG`, `YYJ`, `SHW` 중 하나로 바꿉니다.
아직 원격에 개인 브랜치가 없다면 `git switch -c YYJ origin/develop`로 생성하고 최초 push에 `git push -u origin YYJ`를 사용합니다.

### 2. 개발 컨테이너 열기

1. Docker를 실행합니다.
2. VS Code에서 저장소의 최상위 폴더를 엽니다.
3. Dev Containers 확장을 설치합니다.
4. 명령 팔레트에서 **`Dev Containers: Reopen in Container`**를 실행합니다.

컨테이너에는 Git·GCC·Make·GDB·QEMU와 버전을 고정한 clangd·clang-format·clang-tidy가 설치됩니다.
컨벤션 도구 도입 전의 컨테이너를 사용 중이면 **`Dev Containers: Rebuild Container`**를 실행합니다.
C 파일은 clangd로 저장 시 포맷되며, 처음 생성한 컨테이너는 작업 폴더의 `pintos/activate`를 bashrc에 등록합니다.

### 3. 빌드와 테스트

**개발 컨테이너의 Bash 터미널**에서 실행합니다. 호스트 PowerShell과 실행 위치를 구분합니다.

```bash
cd "$(git rev-parse --show-toplevel)"
source pintos/activate
cd pintos/vm
make
cd build
```

관련 테스트는 `pintos/vm/build`에서 선택해 실행합니다. 아래는 스택 증가와 파일 매핑의 예시이며, 구현한 기능에 맞춰 대상을 고릅니다.

```bash
make tests/vm/pt-grow-stack.result tests/vm/mmap-read.result
```

전체 테스트도 같은 위치에서 실행합니다.

```bash
make check
cat results
```

결과는 `pintos/vm/build/results`와 각 테스트의 `.result`에서 확인합니다.
실행 로그는 `.output`, 표준 오류는 `.errors`입니다. 입력과 추가 파일은 `tests/vm/Make.tests`, 기대 판정은 각 `.ck`, 프로그램 동작은 `.c`를 함께 읽습니다.
현재 VM 구현이 진행 중이므로 미구현 기능의 테스트는 실패할 수 있습니다. 명령 예시는 PASS 결과가 아닙니다.

### 4. 컨벤션 검사

컨테이너 안에서 **저장소 루트**로 돌아와 검사합니다.

```bash
cd "$(git rev-parse --show-toplevel)"
make lint
```

모양을 자동으로 고칠 때는 루트에서 `make format`을 실행합니다. C 소스·헤더가 수정되므로 diff를 확인하고 관련 변경만 포함합니다.
검사 범위·도구 버전·이름 규칙은 [CONVENTION.md](CONVENTION.md)를 따릅니다.

<details>
<summary><strong>🔧 환경·디버깅 메모</strong></summary>

- 실행 환경은 `linux/amd64` 기반 Ubuntu 22.04입니다.
- `pintos` 명령을 찾지 못하면 `pintos/activate`를 실행했는지 확인합니다.
- 가상 메모리 분기는 `pintos/vm`에서 빌드합니다. `pintos/userprog` 빌드의 PASS만으로 VM을 검증했다고 기록하지 않습니다.
- COW 테스트는 현재 전체 목록에 포함되므로 필수 VM 결과와 따로 해석합니다.
- 실행 권한 오류가 나면 `pintos/utils`의 실행 스크립트 권한을 확인합니다.
- 줄바꿈은 저장소의 `.gitattributes`를 따릅니다.
- 전체 파일이 변경으로 표시되면 실제 코드 차이와 줄바꿈 차이를 구분합니다.
- 환경 문제를 공유할 때는 실행 위치·명령·오류 로그를 함께 기록합니다.

</details>
<br />

## 🧭 Read the code. Trace the state.

```text
Pintos-W9-10-T6-VM/
├── .devcontainer/          # 팀 개발 환경
├── .github/
│   └── pull_request_template.md
├── assets/                 # README 시각 자료
├── CONVENTION.md            # 코드 스타일·이름 규칙
├── Makefile                 # 루트 make lint / make format
├── scripts/                 # 컨벤션 검사 도구
├── pintos/
│   ├── vm/                 # SPT·프레임·페이지 폴트·swap·mmap
│   ├── userprog/           # 로더·시스템 콜·프로세스와 VM의 연결
│   ├── include/            # 구조체·공통 인터페이스
│   ├── threads/            # 스레드·스케줄러·동기화
│   ├── filesys/            # 파일·파일 시스템
│   ├── lib/                # 커널·사용자 공통 라이브러리
│   ├── tests/
│   │   ├── vm/             # Project 3 필수 기능·COW Extra 테스트
│   │   ├── userprog/       # Project 2 테스트
│   │   ├── filesys/        # 파일 시스템 테스트
│   │   └── threads/        # Project 1 회귀 테스트
│   └── LICENSE
└── README.md
```

먼저 읽을 VM 코드와 호출 흐름:

**[exception.c](pintos/userprog/exception.c)** → **[vm.c](pintos/vm/vm.c)** → **[uninit.c](pintos/vm/uninit.c)** → **[anon.c](pintos/vm/anon.c)** / **[file.c](pintos/vm/file.c)**

프로그램 적재·fork·exec·종료는 **[process.c](pintos/userprog/process.c)**, 사용자 메모리 접근과 mmap 시스템 콜 연결은 **[syscall.c](pintos/userprog/syscall.c)**에서 함께 확인합니다.

함께 확인할 공통 인터페이스:

**[vm.h](pintos/include/vm/vm.h)** · **[thread.h](pintos/include/threads/thread.h)** · **[mmu.h](pintos/include/threads/mmu.h)** · **[file.h](pintos/include/filesys/file.h)**
<br />

## 📚 Learning is a deliverable.

과제 목적 → 필요한 개념 → 자료구조 → 상태 변화 → 실제 코드 → 테스트를 연결합니다.

구현은 직접 하고, AI는 개념 설명·힌트·코드 읽기·리뷰·디버깅에 활용합니다.
테스트가 확인하는 동작과 실패 원인을 자기 말로 설명하는 것까지 학습 목표에 포함합니다.
AI를 활용한 경우에도 변경 내용과 검증에 대한 책임은 작성자가 집니다.

| Team record | 기록 위치 |
|:---|:---|
| 역량 목표 수립 | 이번 VM 프로젝트의 학습 목표 이슈 |
| 업무 검토·담당·마감 | 관련 작업 이슈 |
| 역량 달성률 평가 | 관련 학습·회고 이슈 |
| 팀 협업 규칙 합의 | 협업 룰 이슈와 README |
| What I Learned | 이번 프로젝트의 학습·회고 이슈 |

진행 상황은 [팀 Issues](https://github.com/Pintos-W9-10-T6-VM/Pintos-W9-10-T6-VM/issues)에서 관리합니다.
개인별 구현 범위와 마감일은 합의 후 갱신합니다.
<br />

---

<div align="center">

**🌴 JUNGLE TEAM 06**<br />
<sub>Build together. Understand together.</sub>

<br /><br />

<sub>Based on <a href="https://github.com/casys-kaist/pintos-kaist">KAIST Pintos</a> and the <a href="https://github.com/krafton-jungle/pintos_22.04_lab_docker">Krafton Jungle Docker environment</a>.<br />
원본 및 수정 코드의 라이선스는 <a href="pintos/LICENSE">pintos/LICENSE</a>를 따릅니다.</sub>

</div>
