# Dream Journal UI

> **Status**: Draft (R2)
> **Author**: bong9tutor + ux-designer
> **Created**: 2026-04-18
> **Last Updated**: 2026-04-18 (R2 — Stillness Budget 우선순위 명시, OQ-DJ-1 해결, Dream Trigger GDD 링크 추가)
> **Implements Pillar**: Pillar 3 (말할 때만 말한다 — 꿈의 희소성 보존),
>   Pillar 1 (조용한 존재 — 조용한 발견, 알림 없음),
>   Pillar 2 (선물의 시학 — 통계 없음, 시적 텍스트만)
> **Priority**: MVP | **Layer**: Presentation | **Category**: UI
> **Effort**: M (2–3 세션)
> **Depends On**: Data Pipeline (#2), Dream Trigger (#9), Save/Load Persistence (#3),
>   Game State Machine (#5), Stillness Budget (#10)
> **System Number**: #13

---

## Overview

Dream Journal UI는 플레이어가 21일 동안 이끼 아이로부터 받은 꿈 조각들을 조용히
다시 읽을 수 있는 **읽기 전용 일기 인터페이스**다. 꿈은 한 플레이스루에 3–5회만
주어지는 희소한 사건이며, 이 일기는 그 기억들로 돌아가는 유일한 경로다. 시스템은
여섯 가지 책임을 소유한다 — (1) 언록된 꿈을 받은 날 순서로 목록 표시,
(2) 꿈 텍스트를 한 번에 하나씩 표시, (3) 단순한 앞뒤 탐색, (4) Waiting 상태에서만
접근 가능한 접근 제어, (5) 꿈이 아직 하나도 없을 때의 빈 상태 처리, (6) 총
슬롯 수 대비 채워진 슬롯 수 표시(내용을 미리 노출하지 않고).
꿈 텍스트는 `Pipeline.GetDreamBodyText(FName DreamId)` API를 통해 on-demand로
로드된다. 페이지 전환 애니메이션은 Stillness Budget의 `Standard` 슬롯을 소비한다.
이 시스템은 UE 5.6 UMG 위젯으로 구현되며, 800×600 최소 해상도에서 텍스트가
읽을 수 있어야 한다.

---

## Player Fantasy

밤 중에 서랍 깊은 곳에서 일기장을 꺼내는 것처럼, 당신은 그 창을 연다. 달이
세 번 바뀌는 동안 이끼 아이가 당신에게 건넨 말들 — 두 개, 혹은 세 개, 혹은
어쩌면 네 개. 그것들은 거기 있다. 줄지어 정렬된 데이터베이스가 아니라 **손때
묻은 쪽지들처럼**, 처음 읽었을 때와 똑같은 자리에.

꿈이 없는 날이 더 많았다는 것을 당신은 안다. 그래서 지금 펼쳐진 페이지가
더 소중하다. 읽고, 페이지를 넘기고, 다시 읽는다 — 게임이 끝나도 이 기억들은
이 일기장 안에 있을 것이다.

**이 판타지가 실패하는 순간**:

- 꿈 목록이 데이터 테이블처럼 보일 때 ("꿈 3개 / 5개 완료" 같은 게임화 지표)
- 새 꿈이 알림으로 강요될 때 (Pillar 1 위반 — 플레이어가 스스로 발견해야 함)
- 텍스트에 통계·힌트·게임플레이 정보가 섞일 때 (Pillar 2 위반)
- 일기를 열려고 했는데 "지금은 열 수 없습니다" 같은 에러 메시지가 뜰 때
  (접근 제어는 진입점 숨김으로 처리해야 함)

**Pillar 연결**:

- **Pillar 3 (말할 때만 말한다)** — 꿈의 희소성은 일기 안에서도 보존된다.
  채워지지 않은 슬롯은 "아직 없음"이지 "당신이 놓쳤음"이 아니다. 언제 몇
  개의 꿈이 올 수 있는지를 UI가 미리 알려주지 않는다
- **Pillar 1 (조용한 존재)** — 새 꿈이 도착해도 아이콘이 조용히 빛날 뿐,
  알림 다이얼로그나 Badge 카운트 없음. 플레이어는 다음 번 일기를 열었을
  때 비로소 새 페이지가 생겼음을 안다
- **Pillar 2 (선물의 시학)** — 꿈 텍스트는 시적 문장만. 태그·트리거 조건·
  게임플레이 연관 정보는 일체 표시하지 않음

---

## Detailed Design

### Core Rules

#### CR-1 — 접근 제어 (Access Control)

Dream Journal UI 진입점(책상 위 일기장 아이콘)은 **오직 `EGameState::Waiting`
상태에서만 활성화**된다. 다른 상태에서는 아이콘이 보이지 않거나 비활성 처리된다.

| 상태 | 진입점 가시성 | 진입 가능 여부 |
|---|---|---|
| `Waiting` | **표시** | **가능** |
| `Offering` | 숨김 또는 비활성 | 불가 |
| `Dawn` | 숨김 | 불가 |
| `Dream` | 숨김 | 불가 (꿈 진행 중) |
| `Farewell` | 숨김 | 불가 (게임 종료 상태) |
| `Menu` | 숨김 | 불가 |

진입점을 숨기는 이유는 에러 메시지 노출 방지다(Pillar 1). "지금은 열 수 없습니다"
같은 메시지는 금지된다. 아이콘이 없으면 시도할 수 없다.

**구현 참고**: Dream Journal 진입점 위젯은 `FOnGameStateChanged` delegate를 구독하여
`EGameState::Waiting` 진입 시 표시, 이탈 시 숨김 처리한다.

#### CR-2 — 꿈 목록 표시 (Dream List Display)

언록된 꿈은 **받은 날(DayIndex) 오름차순**으로 목록에 표시된다. 정렬, 필터,
검색 기능은 없다. 이 일기는 데이터베이스가 아니다.

목록 항목은 최소 정보만 표시한다:
- **꿈을 받은 날** — "Day N" 형식 또는 해당 날의 계절 이름. 코드 번호나
  인덱스는 표시하지 않음
- **꿈 제목 (있는 경우)** — `UDreamDataAsset` 내 선택적 `FText` 제목 필드.
  없으면 날짜만 표시
- **선택 시 해당 꿈 페이지로 이동**

목록 항목에는 **"태그 없음, 트리거 조건 없음, 진행도 수치 없음"** 원칙을 강제한다
(Pillar 2).

#### CR-3 — 꿈 텍스트 표시 (Dream Text Display)

꿈 페이지에는 꿈 본문 텍스트만 표시된다. 레이아웃은:

```
[위: 날짜 또는 계절 표기 — 작고 희미하게]

[중앙: 꿈 본문 FText — 읽기에 최적화된 크기]

[아래: 이전/다음 탐색 버튼 — 작게, 방해되지 않게]
```

꿈 텍스트는 `Pipeline.GetDreamBodyText(FName DreamId) → FText`로 on-demand 로드된다.
**로드 실패(nullptr 반환) 시 해당 페이지는 빈 텍스트로 표시 + 탐색 버튼만 활성화**
(fail-safe — 일기가 닫히거나 에러 메시지가 뜨는 것보다 빈 페이지가 낫다).

통계, 게임플레이 힌트, 기술적 ID, 성장 태그는 **일체 표시하지 않는다**.

#### CR-4 — 탐색 (Navigation)

탐색은 단순하게:
- **이전 꿈** — 목록의 앞 항목으로
- **다음 꿈** — 목록의 뒤 항목으로
- **목록으로 돌아가기** — 꿈 페이지에서 목록으로

꿈이 1개뿐이라면 이전/다음 버튼은 비활성(Disabled)이 아니라 **숨겨진다** — 버튼이
없으면 "더 없음"을 자연스럽게 전달. "페이지 1/1" 같은 숫자 표시 금지.

키보드: 좌/우 방향키로 꿈 간 이동. ESC 또는 백스페이스로 목록 복귀. 탭으로
버튼 포커스 순환.

마우스: 버튼 클릭. 아이콘에서 클릭으로 열기.

#### CR-5 — 빈 상태 (Empty State)

꿈이 하나도 없는 상태에서 일기를 열었을 때:
- **빈 페이지 + 짧은 시적 문장** 하나를 표시. 예: *"이끼 아이는 아직 꿈을
  꾸지 않았다."* (정확한 문장은 Writer가 결정)
- 꿈 슬롯 수를 암시하는 어떤 정보도 없음 ("0/5 언록됨" 같은 표현 금지)
- 탐색 버튼 없음

**이 빈 상태는 에러가 아니다.** 차분하고 기대를 담은 분위기여야 한다.

#### CR-6 — 새 꿈 알림 (New Dream Indicator)

Dream Trigger(#9)가 꿈을 언록하면:
- 일기 아이콘이 **조용히 빛나는 시각 큐** 하나만 표시 — 펄스 애니메이션 1회
  (Stillness Budget `Standard` Motion 슬롯 소비)
- Badge 숫자, 팝업, 토스트, 진동 없음 (Pillar 1)
- 빛남은 플레이어가 일기를 열 때까지 지속. 열고 나면 해제
- **플레이어가 세션 중에 일기를 열지 않아도 다음 세션에도 빛남 유지** (세이브에
  "새 꿈 확인 여부" 플래그 필요 — CR-7 참조)

#### CR-7 — "새 꿈 확인" 플래그 (New Dream Seen Flag)

새 꿈이 있지만 플레이어가 아직 읽지 않은 상태를 추적하기 위해,
`UMossSaveData` 안에 **`bool bHasUnseenDream`** 플래그를 포함한다.

- Dream Trigger(#9)가 꿈을 언록할 때 `bHasUnseenDream = true`로 세이브
- 플레이어가 해당 꿈 페이지를 열면 `bHasUnseenDream = false`로 세이브
- 세이브 이유: `ESaveReason::EDreamReceived` (언록 시), 별도 reason 없음
  (페이지 열 때는 저장 불필요 — 다음 정상 세이브 시 반영)

> **OQ-DJ-1 RESOLVED**: MVP는 단일 `bool bHasUnseenDream` 사용.
> 꿈 3-5개 규모에서 개별 추적은 과도하며, "하나라도 안 읽은 꿈이 있는가"만
> 판단하면 아이콘 빛남 동작을 충분히 구현할 수 있다.
> Full Vision에서 `TSet<FName> UnseenDreamIds`로 확장 검토.

#### CR-8 — 슬롯 표시 (Slot Indicator)

일기 표지 또는 목록 화면에 "채워진 슬롯 수"를 암시하는 시각 요소를 배치할 수 있다.
규칙:

- **채워진 슬롯만 표시** — 비어있는 슬롯 개수를 알 수 없게 (총 꿈 수를 알면
  Pillar 3 희소성을 전략화하려는 유혹이 생김)
- 표현 방법: 작은 압화(pressed flower), 책갈피, 잉크 자국 등 — 아트 디렉터 결정
- 숫자 "2/5", 퍼센트, 별 UI 금지

> **Open Question OQ-DJ-2**: 슬롯 표시를 완전히 없애는 것 vs. 채워진 슬롯만 암시하는
> 것 중 어느 쪽이 Pillar 3에 더 충실한가? 완전 비표시가 더 순수하지만, 목록이
> 단순 텍스트 리스트라면 "몇 개나 됐지?"라는 호기심이 자연스럽게 생긴다. 이
> 결정은 아트 디렉터와 협의 후 확정.

### States

Dream Journal UI는 4개 내부 상태를 순환한다.

| 상태 | 설명 | 진입 조건 | 이탈 조건 |
|---|---|---|---|
| **Closed** | 일기 닫힘. 아이콘만 존재 | 초기 / 일기 닫기 | 아이콘 클릭 (Waiting 상태에서만) |
| **Opening** | 열기 트랜지션 | Closed → 클릭 | 트랜지션 완료 |
| **Reading** | 일기 내용 표시 중 (목록 또는 꿈 페이지) | Opening 완료 | 닫기 버튼 또는 ESC |
| **Closing** | 닫기 트랜지션 | Reading → 닫기 | 트랜지션 완료 |

**트랜지션 설계**:
- Opening: 책이 펼쳐지는 모션. Stillness Budget `Motion, Standard` 슬롯 요청.
  Budget Deny 시 트랜지션 없이 즉시 Reading 진입 (일기는 항상 열린다 — Deny가
  접근을 막으면 안 됨)
- Closing: Opening의 역재생. 동일 Budget 규칙
- 트랜지션 중 탐색 입력은 버퍼링 없이 무시 (트랜지션이 짧아 사용성 문제 없음)

### Interactions with Game State Machine (#5)

| 이벤트 | Journal UI 반응 |
|---|---|
| `FOnGameStateChanged(_, Waiting)` | 진입점 아이콘 표시 활성화 |
| `FOnGameStateChanged(Waiting, _)` | 진입점 아이콘 숨김. 일기가 열려 있으면 즉시 닫기 (Closing 트랜지션 생략 — 강제 닫기) |
| `FOnGameStateChanged(_, Dream)` | 일기 진입점 숨김. 이미 열려 있으면 강제 닫기 (꿈 시퀀스가 우선) |
| `FOnGameStateChanged(_, Farewell)` | 일기 진입점 영구 숨김 (Farewell은 종단) |

**강제 닫기(forced close)**: Waiting→Dream 또는 Waiting→Farewell 전환 시 일기가
열려 있으면 Closing 트랜지션 없이 즉시 위젯 제거. 이는 Dream 시퀀스나 Farewell
연출의 Budget 슬롯을 즉시 확보하기 위함이다.

### Interactions with Data Pipeline (#2)

| API | 호출 시점 | 실패 처리 |
|---|---|---|
| `Pipeline.GetAllDreamAssets() → TArray<UDreamDataAsset*>` | 일기 열 때 언록된 꿈 ID 목록 구성용 (Save에서 IDs 획득 후 Asset 조회) | 빈 배열 반환 시 빈 상태로 처리 |
| `Pipeline.GetDreamBodyText(FName DreamId) → FText` | 꿈 페이지 열 때 on-demand | `FText::GetEmpty()` 반환 시 빈 페이지 표시 (에러 없음) |

Dream Journal UI는 Pipeline에 **pull**만 한다. Pipeline은 Journal에 push하지 않는다.

### Interactions with Save/Load Persistence (#3)

Journal UI는 Save/Load에 대해 **read-only**다. 직접 `SaveAsync()`를 호출하지 않는다.

| Save/Load가 제공하는 것 | 시점 |
|---|---|
| 언록된 꿈 ID 목록 (`TArray<FName> UnlockedDreamIds`) | Journal 열 때 1회 read |
| `bHasUnseenDream` 플래그 | 아이콘 빛남 판단에 사용 |

**`bHasUnseenDream = false` 갱신**: 플레이어가 꿈 페이지를 열 때 in-memory 변경.
디스크 commit은 다음 `ESaveReason::ECardOffered` 또는 `ESaveReason::EDreamReceived`
저장 시 자동으로 포함된다 (부분 저장 없음 원칙 — Save/Load #3 §Core Rules §1).

> **구현 참고**: Journal UI는 `UMossSaveData`의 typed slice accessor를 통해 읽는다.
> Dream Trigger(#9) GDD가 `UnlockedDreamIds` 필드의 write 소유권을 정의한다.
> Save/Load GDD §7 "Dream Journal UI — read only" contract 준수.

### Interactions with Stillness Budget (#10)

| 요청 | 채널 | 우선순위 | 시점 |
|---|---|---|---|
| 일기 Opening 트랜지션 | `Motion` | `Standard` | 아이콘 클릭 시 |
| 일기 Closing 트랜지션 | `Motion` | `Standard` | 닫기 버튼 또는 ESC |
| 페이지 넘김 애니메이션 | `Motion` | `Standard` | 이전/다음 버튼 클릭 시 |
| 새 꿈 아이콘 펄스 | `Motion` | `Standard` | 새 꿈 언록 시 1회 |
| 꿈 텍스트 Fade-in | `Motion` | `Standard` | 꿈 페이지 최초 표시 시 |

**Budget Deny 처리**: 모든 애니메이션은 선택적(optional). Budget Deny 시 해당
트랜지션을 건너뛰고 즉시 목표 상태로 전환. **일기를 여는 행위 자체는 Budget에
의해 차단되지 않는다** — 트랜지션이 없을 뿐이다.

> **우선순위 정책 명시**: Dream Journal의 모든 트랜지션(Opening/Closing/페이지 넘김/아이콘 펄스)은
> **Standard 우선순위**를 사용한다. 이는 UI 트랜지션이며 꿈 시퀀스·성장 전환 같은 내러티브
> 모멘트가 아니다. Stillness Budget GDD §Downstream에 본 시스템이 Narrative로 기재되어
> 있다면 그것은 오기입이다 — **본 GDD의 Standard가 정확한 값. Stillness Budget GDD 수정 필요.**

`Stillness.IsReducedMotion() == true` 시:
- 모든 트랜지션 생략 (Motion 슬롯 요청 없이 즉시 전환)
- 꿈 텍스트는 Fade-in 없이 즉시 표시
- 새 꿈 아이콘 펄스 생략 (정적 하이라이트로 대체 — 아트 결정)

---

## Formulas

### F-DJ-1 — 가시 텍스트 행 수 (Visible Text Lines)

꿈 페이지에서 한 번에 표시 가능한 텍스트 행 수:

```
L_visible = floor((H_content - P_top - P_bottom) / LineHeight)
```

| 변수 | 심볼 | 타입 | 범위 | 설명 |
|---|---|---|---|---|
| 콘텐츠 영역 높이 | H_content | float (px) | 0–H_viewport | 날짜 헤더와 탐색 버튼 제외 순수 텍스트 영역 |
| 상단 패딩 | P_top | float (px) | 8–24 | 헤더 하단 여백 |
| 하단 패딩 | P_bottom | float (px) | 8–24 | 탐색 버튼 상단 여백 |
| 행 높이 | LineHeight | float (px) | FontSize × LineHeightMultiplier | 글꼴 크기 × 행간 계수 |
| **가시 행 수** | L_visible | int | 1–50 | 화면에 한 번에 표시 가능한 텍스트 행 수 |

**예시 계산 (800×600 최소 해상도)**:

- 날짜 헤더 높이: 32px
- 탐색 버튼 영역: 40px
- 상하 패딩: 각 16px
- H_content = 600 − 32 − 40 − 32 = **496px** (위젯 위치·마진 포함 실 값은 다를 수 있음)
- FontSize: 16px, LineHeightMultiplier: 1.5 → LineHeight = 24px
- L_visible = floor((496 − 16 − 16) / 24) = floor(464 / 24) = **19행**

19행은 일반적인 꿈 텍스트(4–8 문장)를 한 화면에 표시하기에 충분하다.
꿈 텍스트가 L_visible을 초과할 경우 스크롤 허용 (CR-3에서 스크롤 정책 별도 결정).

> **Open Question OQ-DJ-3**: 꿈 텍스트가 한 화면을 넘길 경우 스크롤을 허용하는가,
> 아니면 자동 폰트 축소(auto-fit)로 한 화면에 맞추는가? 스크롤은 "책 페이지처럼"
> 읽기 경험에 더 자연스러울 수 있지만, auto-fit은 구현이 간단하다. 꿈 텍스트가
> 작가에 의해 통제된다면 길이를 제한하는 것이 가장 단순한 해결책이다.
> Writer GDD 협의 후 확정 권장.

### F-DJ-2 — 목록 항목 최대 표시 수 (Max List Items Visible)

목록 화면에서 스크롤 없이 보이는 항목 수:

```
N_visible = floor((H_list - P_header) / H_item)
```

| 변수 | 심볼 | 타입 | 범위 | 설명 |
|---|---|---|---|---|
| 목록 영역 높이 | H_list | float (px) | 0–H_viewport | 헤더(일기장 제목) 제외 영역 |
| 헤더 패딩 | P_header | float (px) | 16–32 | 타이틀과 첫 항목 사이 여백 |
| 항목 높이 | H_item | float (px) | 40–60 | 날짜 + 제목(선택) + 탭 터치 영역 |
| **가시 항목 수** | N_visible | int | 1–20 | 스크롤 없이 보이는 꿈 항목 수 |

**예시 (800×600, H_item=48px, P_header=24px)**:
N_visible = floor((600 − 80 − 24) / 48) = floor(496 / 48) = **10항목**

MVP 꿈 최대 5개 기준으로 스크롤 불필요. Full 50개 기준 → 스크롤 구현 필요.

---

## Edge Cases

### E-DJ-1 — 꿈이 0개 (Empty Playthrough)

플레이어가 일기를 여는 시점까지 꿈을 하나도 받지 못한 경우:
- 목록 없이 **빈 상태 화면** 즉시 표시 (CR-5)
- 이전/다음 버튼 없음
- "아직 꿈이 없음"을 표현하는 시적 문장 1개 (에러 메시지 형식 금지)
- 탐색: 닫기 버튼 또는 ESC만 활성

### E-DJ-2 — 모든 꿈 언록 (All Dreams Received)

플레이어가 이번 플레이스루의 모든 꿈을 받은 경우:
- 목록에 모든 꿈 표시 (날짜 순)
- "더 이상 없다"는 명시적 표시 없음 — 목록이 끝나면 끝이다
- 이전/다음 버튼은 첫 꿈에서 이전 비활성, 마지막 꿈에서 다음 비활성
  (숨기기 대신 비활성 처리 — 목록이 있을 때는 버튼이 있어야 탐색 방향을 알 수 있음)

### E-DJ-3 — 일기 열람 중 Dream 상태 진입

플레이어가 일기를 읽고 있는 중에 `Waiting → Dream` 전환이 발생하는 경우:
- 일기 위젯을 **즉시 제거** (Closing 트랜지션 생략)
- Dream 시퀀스가 전체 화면 우선권을 가짐 (GSM Rule, Stillness Budget Narrative 우선순위)
- `bHasUnseenDream` in-memory 상태는 이미 false로 변경됐다면 그대로 유지
  (중단된 읽기도 "봤음"으로 간주 — 다시 강요하는 것은 Pillar 1 위반)

> **Open Question OQ-DJ-4**: 일기를 읽다가 강제 닫힌 경우 해당 꿈을 "읽음"으로
> 처리하는가? 절반만 읽었는데 "읽음"이 되는 것이 불만스러울 수 있다. 그러나
> 플레이어에게 "당신은 이 꿈을 읽다가 방해받았습니다. 다시 읽으세요"라고 알리는
> 것은 Pillar 1에 위배된다. 단순하게 "열었으면 읽은 것"으로 처리하는 것을 기본으로
> 권장.

### E-DJ-4 — 창 리사이즈 중 읽기

일기가 열려 있는 상태에서 창 크기가 변경되는 경우:
- UMG DPI Scaling에 의해 자동 재배치
- 텍스트 표시 영역(F-DJ-1)을 재계산하여 적용
- 읽던 위치(스크롤 오프셋 있는 경우) 유지 시도. 재계산 후 텍스트가 모두
  표시되면 스크롤 위치 초기화
- 최소 해상도 800×600에서 CR-3의 텍스트 가독성 보장 (F-DJ-1 공식 참조)

### E-DJ-5 — Pipeline DegradedFallback 상태

`UDataPipelineSubsystem`이 DegradedFallback 상태일 때 일기를 열 경우:
- `GetDreamBodyText()` → `FText::GetEmpty()` 반환
- 꿈 목록은 Save 데이터의 `UnlockedDreamIds`로 구성 가능 (Pipeline과 독립)
- 각 꿈 페이지 진입 시 본문 텍스트만 빈 것으로 표시 (E-DJ-1과 동일 처리)
- `UE_LOG(LogDreamJournalUI, Warning, ...)` 1회 기록
- 플레이어에게 에러 표시 없음 (빈 페이지가 조용한 실패)

### E-DJ-6 — 저장 데이터 없음 (Fresh Start)

`FOnLoadComplete(true, false)` — 완전 첫 실행의 경우:
- `UnlockedDreamIds` 빈 배열 → E-DJ-1 빈 상태 처리
- 일기 아이콘은 표시되지만 빛남 없음 (`bHasUnseenDream = false` 초기값)

### E-DJ-7 — 동시에 여러 꿈이 언록되는 경우

Dream Trigger(#9)가 동일 세션에서 복수의 꿈을 연속으로 언록하는 경우:
- `bHasUnseenDream = true`는 1회만 설정 (이미 true면 중복 세이브 불필요)
- 아이콘 펄스는 1회만 (Stillness Budget Standard 슬롯 소비는 1회)
- 목록에 모든 언록된 꿈이 날짜순으로 표시됨

---

## Dependencies

### 업스트림 (이 시스템이 소비)

| 시스템 | 타입 | 소비 내용 | Contract |
|---|---|---|---|
| **Data Pipeline (#2)** | Hard | `Pipeline.GetDreamBodyText(FName)`, `Pipeline.GetAllDreamAssets()` | Pull only. DegradedFallback 시 빈 텍스트 — E-DJ-5 처리 |
| **[Dream Trigger (#9)](dream-trigger-system.md)** | Hard | 꿈 언록 이벤트 (FOnDreamTriggered), 언록 ID 목록 | Save에 기록된 IDs를 통해 간접 소비. Dream GDD가 UnlockedDreamIds 쓰기 소유 |
| **Save/Load Persistence (#3)** | Hard | `UnlockedDreamIds`, `bHasUnseenDream` 읽기 | Read-only. Save/Load GDD §7 contract |
| **Game State Machine (#5)** | Hard | `FOnGameStateChanged` 구독, `EGameState` 열거 | Waiting 진입 시 활성, 다른 모든 상태에서 숨김 |
| **Stillness Budget (#10)** | Soft | Motion·Standard 슬롯 요청 (트랜지션) | Deny 시 트랜지션 생략. 일기 접근 자체는 Budget과 무관 |

### 다운스트림 (이 시스템이 영향을 주는 것)

| 시스템 | 방향 | 내용 |
|---|---|---|
| **Save/Load Persistence (#3)** | Write (indirect) | `bHasUnseenDream = false` in-memory 갱신 — 다음 저장 시 포함 |
| **Stillness Budget (#10)** | Read/Release | 트랜지션 후 Handle Release |

### 양방향 계약 명시

- **Save/Load → Dream Journal**: unlocked dream IDs + `bHasUnseenDream` 제공
- **Dream Journal → Save/Load**: `bHasUnseenDream = false` in-memory 갱신 (직접 SaveAsync 호출 없음)
- **Dream Trigger (#9) 참고**: Dream Trigger GDD([dream-trigger-system.md](dream-trigger-system.md)) 참조.
  Dream Trigger가 `UnlockedDreamIds`를 `UMossSaveData`에 쓰고, Journal은 그것을 읽는다는
  단방향 계약을 전제한다.

---

## Tuning Knobs

모든 값은 DataAsset 또는 UMG 위젯 프로퍼티로 외부화. 코드 내 리터럴 금지.

| Knob | 기본값 | 범위 | 영향 | 위치 |
|---|---|---|---|---|
| `FontSizeBody` | 16px | 12–24px | 꿈 본문 텍스트 크기 | `UDreamJournalConfig` DataAsset |
| `FontSizeDate` | 12px | 10–18px | 날짜 표기 크기 | `UDreamJournalConfig` DataAsset |
| `LineHeightMultiplier` | 1.5 | 1.2–2.0 | 행간. 가독성과 정보 밀도 균형 | `UDreamJournalConfig` DataAsset |
| `PageTransitionDurationSec` | 0.25 | 0.1–0.8 | 페이지 넘김 애니메이션 시간. 너무 짧으면 거칠어 보임 | `UDreamJournalConfig` DataAsset |
| `OpenCloseDurationSec` | 0.3 | 0.15–0.6 | 일기 열기·닫기 트랜지션 시간 | `UDreamJournalConfig` DataAsset |
| `IconPulseDurationSec` | 1.2 | 0.5–3.0 | 새 꿈 아이콘 펄스 애니메이션 1사이클 시간 | `UDreamJournalConfig` DataAsset |
| `EmptyStateText` | (작가 결정) | — | 꿈이 없을 때 표시할 시적 문장 | `UDreamJournalConfig` DataAsset (FText) |
| `MaxBodyTextLength` | 500자 | 100–1000자 | 꿈 본문 최대 길이 권장값 (강제 아님 — 작가 가이드라인). F-DJ-1 스크롤 정책과 연관 | Writer 가이드라인 |
| `PaddingHorizontal` | 24px | 12–48px | 텍스트 영역 좌우 여백 | UMG 위젯 프로퍼티 |
| `PaddingVertical` | 16px | 8–32px | 날짜 헤더·탐색 버튼과 텍스트 영역 간 여백 | UMG 위젯 프로퍼티 |

**안전 범위 참고**:
- `FontSizeBody` 12px 미만 → 800×600 해상도에서 가독성 위험. 접근성 요구사항 위반
- `PageTransitionDurationSec` 0.8초 초과 → 느린 반응으로 불편함. Stillness Budget
  Standard 슬롯을 오래 점유하여 다른 효과 억제 시간 증가
- `OpenCloseDurationSec` 0.6초 초과 → 일기 접근성 저하 (빠른 재열기 시 답답함)

---

## Acceptance Criteria

### 접근 제어 테스트

| ID | 타입 | Given | When | Then | Evidence |
|---|---|---|---|---|---|
| **AC-DJ-01** | AUTOMATED | `EGameState::Offering` | 일기 아이콘 클릭 시도 | 아이콘 비활성 / 숨김. 일기 위젯 미생성 | Widget 가시성 Assert |
| **AC-DJ-02** | AUTOMATED | `EGameState::Waiting` | 일기 아이콘 클릭 | Opening 트랜지션 후 Reading 상태 진입 | State machine 상태 Assert |
| **AC-DJ-03** | AUTOMATED | 일기 Reading 중 `FOnGameStateChanged(Waiting, Dream)` 수신 | — | 일기 위젯 즉시 제거 (Closing 트랜지션 없음) | Widget 존재 여부 Assert |
| **AC-DJ-04** | MANUAL | `EGameState::Dream` 진행 중 | 일기 아이콘 확인 | 아이콘 미표시 또는 비활성 | 플레이테스터 체크리스트 |

### 탐색 테스트

| ID | 타입 | Given | When | Then | Evidence |
|---|---|---|---|---|---|
| **AC-DJ-05** | AUTOMATED | 꿈 3개 언록 | 일기 열기 → 목록 확인 | 받은 날 오름차순 정렬, DayIndex 작은 것부터 | 목록 순서 Assert |
| **AC-DJ-06** | AUTOMATED | 꿈 3개 언록, 첫 꿈 페이지 | 이전 버튼 존재 여부 | 이전 버튼 숨김 (첫 항목이므로) | Button 가시성 Assert |
| **AC-DJ-07** | AUTOMATED | 꿈 3개, 마지막 꿈 페이지 | 다음 버튼 존재 여부 | 다음 버튼 숨김 (마지막 항목이므로) | Button 가시성 Assert |
| **AC-DJ-08** | AUTOMATED | 꿈 1개 언록 | 일기 열기 → 이전/다음 버튼 확인 | 이전/다음 버튼 모두 숨김 | Button 가시성 Assert |
| **AC-DJ-09** | AUTOMATED | Reading 상태 | ESC 키 입력 | Closing 트랜지션 후 Closed 상태 | State machine Assert |
| **AC-DJ-10** | MANUAL | 키보드만 사용 | 일기 열기 → 탐색 → 닫기 전 과정 | 마우스 없이 전체 플로우 완료 가능 | 플레이테스터 체크리스트 |

### 빈 상태 테스트

| ID | 타입 | Given | When | Then | Evidence |
|---|---|---|---|---|---|
| **AC-DJ-11** | AUTOMATED | 꿈 0개 (Fresh Start 또는 언록 전) | 일기 열기 | 목록 없이 빈 상태 화면 표시. `EmptyStateText` FText 표시됨 | Widget 상태 Assert |
| **AC-DJ-12** | AUTOMATED | 빈 상태 화면 | 이전/다음 버튼 확인 | 이전/다음 버튼 없음 | Button 가시성 Assert |
| **AC-DJ-13** | CODE_REVIEW | 빈 상태 화면 텍스트 | 텍스트 내용 검토 | "0개", "0/N", 숫자·퍼센트 포함하지 않음 | PR 리뷰에서 FText 내용 확인 |

### 새 꿈 알림 테스트

| ID | 타입 | Given | When | Then | Evidence |
|---|---|---|---|---|---|
| **AC-DJ-14** | AUTOMATED | `bHasUnseenDream = true` | 일기 아이콘 렌더 | 펄스 애니메이션 재생 또는 정적 하이라이트 (Reduced Motion) | Animation 상태 Assert |
| **AC-DJ-15** | AUTOMATED | `bHasUnseenDream = true` | 꿈 페이지 열기 | `bHasUnseenDream = false` in-memory 변경 | 메모리 상태 Assert |
| **AC-DJ-16** | AUTOMATED | 세션 종료 후 재시작 (bHasUnseenDream = true로 세이브됨) | 앱 재시작 → Waiting 진입 | 아이콘 펄스 재생 | Save/Load 왕복 후 Widget 상태 Assert |
| **AC-DJ-17** | CODE_REVIEW | — | Dream Journal UI 코드 검토 | `SaveAsync()` 직접 호출 없음. `bHasUnseenDream` in-memory 갱신만 존재 | PR grep: `SaveAsync` in DreamJournalUI sources |

### 텍스트 표시 테스트

| ID | 타입 | Given | When | Then | Evidence |
|---|---|---|---|---|---|
| **AC-DJ-18** | MANUAL | 800×600 해상도 | 꿈 페이지 열기 | 본문 텍스트가 읽기 가능 (최소 16px, 겹침·잘림 없음) | 스크린샷 + 리드 사인오프 |
| **AC-DJ-19** | MANUAL | Reduced Motion 활성화 | 일기 열기·페이지 넘기기 | 모든 트랜지션 즉시 전환. 텍스트 정상 표시 | 플레이테스터 체크리스트 |
| **AC-DJ-20** | AUTOMATED | `Pipeline.GetDreamBodyText()` → `FText::GetEmpty()` 반환 시뮬 | 꿈 페이지 진입 | 빈 텍스트 표시. 에러 메시지 없음. 탐색 버튼 정상 작동 | Widget 상태 Assert + 에러 로그 없음 Assert |

### Pillar 준수 테스트

| ID | 타입 | Given | When | Then | Evidence |
|---|---|---|---|---|---|
| **AC-DJ-21** | CODE_REVIEW | — | Dream Journal UI 전체 코드 및 위젯 검토 | 통계·수치(꿈 카운트, 퍼센트, 인덱스)·게임플레이 힌트·태그 정보가 화면에 표시되지 않음 | PR 리뷰: UI 텍스트 필드 점검 |
| **AC-DJ-22** | CODE_REVIEW | — | 꿈 목록 표시 코드 검토 | "N/M 언록됨", "N개 남음" 류 문자열 없음 | PR grep: `/[0-9]+\/[0-9]+/`, `remaining`, `total` 패턴 |

---

## Implementation Notes

### UMG 위젯 구조 제안

```
WBP_DreamJournalOverlay (루트)
  ├── WBP_DreamJournalIcon (책상 위 진입점, Waiting 시에만 표시)
  └── WBP_DreamJournalPanel (일기 패널 본체, Opening/Reading/Closing 시 활성)
        ├── WBP_DreamList (목록 화면)
        │     ├── [꿈 수만큼] WBP_DreamListItem
        │     └── WBP_EmptyState (꿈 0개 시)
        └── WBP_DreamPage (꿈 페이지)
              ├── TXT_DateLabel
              ├── TXT_BodyText
              ├── BTN_Previous
              └── BTN_Next
```

`WBP_DreamJournalOverlay`는 HUD 레이어에 상시 존재하며 GSM 상태에 따라
내부 위젯 가시성만 전환한다. 위젯 생성/소멸 대신 가시성 전환 패턴이 UMG 성능에 유리하다.

### Data 흐름 요약

```
[앱 시작]
Save/Load → 복원 → UMossSaveData.UnlockedDreamIds + bHasUnseenDream

[Waiting 상태 진입 시]
FOnGameStateChanged → WBP_DreamJournalIcon 가시화
bHasUnseenDream 확인 → 펄스 애니메이션 판단

[아이콘 클릭 시]
Opening 트랜지션 → Stillness.Request(Motion, Standard)
Save 슬라이스에서 UnlockedDreamIds 로드 → WBP_DreamList 또는 WBP_EmptyState 표시

[꿈 항목 클릭 시]
Pipeline.GetDreamBodyText(DreamId) → WBP_DreamPage 표시
bHasUnseenDream = false (in-memory)

[닫기]
Closing 트랜지션 → Stillness.Release(Handle)
```

### 접근성 체크리스트 (Accessibility)

- [x] 키보드만으로 전체 플로우 완료 가능 (AC-DJ-10)
- [x] 마우스 없이 게임패드로 탐색 가능 (방향키 → 버튼 포커스, 확인 버튼 → 선택)
  — Vertical Slice Input Abstraction Layer 완성 후 검증
- [x] 최소 폰트 크기 16px at 800×600 (AC-DJ-18)
- [x] 색상만으로 정보 전달하지 않음 (새 꿈 표시는 색상+모양 조합)
- [x] Reduced Motion 지원 (AC-DJ-19)
- [x] 깜빡임 없음 — 아이콘 펄스는 `IconPulseDurationSec` ≥ 1.0초 권장 (느린 펄스는 광과민성 위험 없음)
- [x] 모든 텍스트는 `FText` (현지화 대응 준비)
- [ ] **미결**: Title & Settings UI(#16, VS) 완성 후 Text Scale 옵션과 연동 필요
  (`FontSizeBody` 동적 조정 API 설계 — Accessibility Layer GDD #18 위임)

### UE 5.6 구현 참고

- UMG `UUserWidget` 기반. `NativeConstruct()` / `NativeDestruct()` 에서 delegate 구독/해제
- `FOnGameStateChanged` 구독: `UMossGameStateSubsystem::GetSubsystem<>()`에서 delegate 획득.
  `GetWorld()` null 체크 필수 (에디터 PIE 환경)
- `Pipeline.GetDreamBodyText()`: 동기 반환. 꿈 DataAsset은 Initialize 시 일괄 로드됨
  (Data Pipeline §R2) — 페이지 진입 시 별도 비동기 로드 불필요
- 위젯 애니메이션: UMG 내장 Animation 트랙 사용. Reduced Motion 시 `PlayAnimation()`을
  호출하지 않고 최종 프레임 상태를 직접 적용 (`SetVisibility()` + 파라미터 직접 설정)
- `Save/Load` 슬라이스 접근: `UMossGameInstance::GetSaveData()` 또는 저장 Subsystem의
  typed accessor 경유. 직접 `USaveGame` 캐스팅 금지

### 미결 사항 요약

| ID | 질문 | 영향도 | 결정 필요 시점 |
|---|---|---|---|
| ~~OQ-DJ-1~~ | ~~`bHasUnseenDream`이 단일 bool인가, 꿈별 읽음 여부 `TMap`인가?~~ | ~~Medium~~ | **RESOLVED** — 단일 `bool bHasUnseenDream` 확정 (MVP). Full Vision에서 `TSet<FName>` 확장 검토 |
| OQ-DJ-2 | 슬롯 표시 완전 생략 vs. 채워진 슬롯만 암시 표시? | Medium (Pillar 3 해석) | 아트 디렉터 협의 |
| OQ-DJ-3 | 꿈 텍스트가 한 화면을 넘길 경우 스크롤 vs. auto-fit? | Low (MVP 꿈 5개 규모라면 대부분 한 화면 이내) | Writer와 텍스트 길이 협의 후 |
| OQ-DJ-4 | 일기를 읽다가 강제 닫힌 경우 "읽음"으로 처리할 것인가? | Low | 구현 시 기본값 결정 |

---

## Open Questions (요약)

1. ~~**OQ-DJ-1**~~ **RESOLVED** — 단일 `bool bHasUnseenDream` 확정. 꿈 3-5개 규모에서 개별 추적은 과도. Full Vision에서 `TSet<FName> UnseenDreamIds` 확장 검토.
2. **OQ-DJ-2** (Medium) — 아직 받지 못한 슬롯을 UI에서 완전히 숨길지, 채워진 슬롯만
   암시할지. Pillar 3 해석에 따라 달라짐 — 아트 디렉터·게임 디자이너 협의 필요.
3. **OQ-DJ-3** (Low) — 꿈 본문이 F-DJ-1 L_visible을 초과할 경우 스크롤 또는 auto-fit.
   Writer 텍스트 길이 가이드라인과 연계.
4. **OQ-DJ-4** (Low) — 강제 닫기 시 읽음 처리 여부.
