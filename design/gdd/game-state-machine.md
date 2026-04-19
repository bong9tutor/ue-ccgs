# Game State Machine (+ Visual State Director)

> **Status**: In Design
> **Author**: bong9tutor + claude-code session
> **Last Updated**: 2026-04-17
> **Implements Pillar**: Pillar 1 (조용한 존재 — 상태 전환은 조용하게), Pillar 3 (말할 때만 말한다 — Dream 상태는 희소), Pillar 4 (끝이 있다 — Farewell은 종결)
> **Priority**: MVP | **Layer**: Core | **Category**: Core
> **Effort**: L (4+ 세션)

---

## Overview

Game State Machine은 Moss Baby의 6개 게임플레이 상태 — **Dawn**(첫 세션 의식), **Offering**(카드 건넴), **Waiting**(고요한 관찰), **Dream**(희소한 꿈 공개), **Farewell**(21일 작별), **Menu**(타이틀 화면) — 간의 전환을 관장하는 **중앙 상태 인프라**다. Time & Session System(#1)이 분류한 6개 Action 신호(`ETimeAction`)와 Save/Load(#3)의 초기화 결과(`FOnLoadComplete`)를 소비해 현재 상태를 결정하고, 그 결정을 6개 downstream 시스템(Lumen Dusk Scene, Card System, Card Hand UI, Audio, Title & Settings UI, Farewell Archive)에 전파한다.

R2 개정에서 Visual State Director가 이 시스템에 흡수되었다. 상태별 시각 파라미터 — 색온도(2,200K–4,500K), 광원 각도, 대비, 앰비언트 톤 — 를 **Material Parameter Collection(MPC)**에 매핑해 전파하며, Art Bible §2의 6개 상태 무드 명세가 그 매핑의 설계 근거다. 이 통합으로 "지금 게임이 어떤 감정 상태인가"의 판단과 "그 상태가 어떻게 보이는가"의 전파가 하나의 시스템에서 원자적으로 처리된다.

플레이어는 이 시스템을 직접 조작하지 않는다. 카드를 건네면 Offering→Waiting, 꿈이 도착하면 Waiting→Dream, 21일이 끝나면 Farewell — 모든 전환은 다른 시스템의 이벤트에 의해 자동으로 일어난다. 이 시스템이 없다면 6개 downstream 시스템이 각자 "지금 어떤 상태인지"를 독립 추적해야 하고, Art Bible이 정의한 상태별 무드(색온도·광원·대비)가 일관되게 적용될 보장이 사라진다.

## Player Fantasy

대부분의 날은 고요하다. 호박빛 새벽이 테라리움을 물들이고, 선물을 건네면 황금빛이 절정에 이르고, 구리빛 저녁이 내리며 이끼 아이는 그냥 거기 있다. 이 리듬은 플레이어가 만든 것이 아니다 — 세계가 스스로 숨을 쉬고, 플레이어는 그 호흡 안에 포함되어 있을 뿐이다.

그러다 어느 밤, 예고 없이 세계가 달빛으로 뒤집힌다. 따뜻한 호박빛만 보아온 플레이어에게 4,200K의 차가운 달빛은 "그가 말했다"의 물리적 표현이다. 이끼 아이가 꿈을 꾼다. 그 순간의 빛은 21일 중 단 두세 번뿐이기에, 플레이어는 그것을 계절의 변화처럼 기억한다. Game State Machine이 만드는 감정은 "조작"이 아니라 "동거" — 빛의 변화를 의식하지 못하는 날들이 쌓여야 비로소 어느 한 순간의 변화가 심장에 닿는다.

**앵커 모먼트**: Dream 상태 진입. Waiting의 구리빛 고요에서 Dream의 차가운 달빛으로 전환되는 바로 그 순간. Art Bible이 규정한 유일한 냉색 조명이 화면을 감싸는 0.8–1.5초. 이것이 이 시스템이 존재하는 이유다.

**Pillar 연결**:
- **Pillar 1 (조용한 존재)** — 상태 전환은 UI 알림·모달·카운트다운 없이 빛과 색으로만 표현된다. 플레이어는 "상태가 바뀌었다"는 사실 자체를 의식하지 않는다 — 세계의 기분이 달라졌을 뿐
- **Pillar 3 (말할 때만 말한다)** — Dream 전환은 21일 중 3–5회 (Dream Trigger GDD §꿈 빈도 목표 채택값 — cross-review D-1 해소). 이 희소성이 감정을 만든다. 동일한 6개 상태를 반복 순환하되 Dream은 "예고 없이 찾아오는 계절"처럼 배치됨
- **Pillar 4 (끝이 있다)** — Farewell 상태(2,200K, 촛불 직전 호박빛)는 가장 따뜻한 빛이면서 마지막 빛이다. 이 색온도는 오직 Day 21에서만 사용되어 "완성"의 시각적 코딩이 됨

## Detailed Design

### Core Rules

#### Rule 1 — 상태 열거

Game State Machine은 6개 상태를 단일 `EGameState` enum으로 정의한다.

| State | 값 | 성격 | 영속화 대상 |
|---|---|---|---|
| `Menu` | 0 | 앱 진입 게이트 (콜드 스타트 대기) | 아니오 |
| `Dawn` | 1 | 새로운 날의 의식 (2–3초 비주얼) | 아니오 |
| `Offering` | 2 | 카드 선택·건넴 진행 | 아니오 |
| `Waiting` | 3 | 고요한 관찰 (대부분의 시간) | **예** |
| `Dream` | 4 | 희소한 꿈 공개 (21일 중 3–5회 — Dream Trigger GDD 채택값) | 아니오 |
| `Farewell` | 5 | 작별 (종단 상태 — 재진입 불가) | **예** |

**영속화 규칙**: 세이브에 기록되는 상태는 오직 `Waiting`과 `Farewell`이다. Dawn, Offering, Dream은 실행 중 일시 상태(transient)이며, 앱 종료 시 Waiting으로 정규화된다. 로드 시 `Dream`이 저장되어 있으면 `Waiting`으로 강등 (Edge Case E1).

**메마름(Withered) 서브컨디션**: `bWithered`는 `EGameState`의 새 값이 아니라 `FSessionRecord`의 `bool` 필드. `ADVANCE_SILENT` 또는 `ADVANCE_WITH_NARRATIVE` 수신 시 DayIndex 점프폭(`NewDayIndex − PrevDayIndex > WitheredThresholdDays`, 기본 3)이 임계를 초과하면 설정, 카드 건넴(`FOnCardOffered`) 시 해제. `LONG_GAP_SILENT`는 항상 DayIndex=21(Farewell 직행)이므로 withered 경로에 관여하지 않음. 영속화 대상 — 세이브에 기록. 메마름 Waiting은 MPC 파라미터를 약간 조정(채도 감소, SSS 약화)하지만 별도 상태 전환 없이 Waiting 내에서 처리. game-concept.md의 "메마름 상태 → 첫 카드로 복구" 약속 이행.

#### Rule 2 — 상태 전환 우선순위

모든 상태 전환은 4단계 우선순위 시스템을 따른다. **높은 우선순위가 현재 진행 중인 전환을 인터럽트할 수 있다.**

| 우선순위 | 설명 | 예시 |
|---|---|---|
| **P0 — 즉시 인터럽트** | 시스템 수준 강제 전환. 진행 중인 모든 상태를 중단 | Farewell 진입, FOnLoadComplete 분기 |
| **P1 — 자동 순서 전환** | 이전 상태 완료 후 자연스러운 다음 상태 | Dawn→Offering, Offering→Waiting |
| **P2 — 조건부 이벤트 전환** | 외부 이벤트 + 조건(Budget 슬롯 등) 충족 시 | Waiting→Dream |
| **P3 — 플레이어 명시 요청** | MVP에서 미사용. VS Title & Settings UI(#16)에서 활성화 | Menu 진입 (VS) |

#### Rule 3 — Visual State Director (MPC 매핑)

상태 전환 시 `UMaterialParameterCollection`의 파라미터를 목표값으로 블렌딩한다. Art Bible §2의 6개 상태 무드 명세가 각 상태의 MPC 목표값을 정의한다.

**MPC 파라미터 세트**:

| 파라미터 | 타입 | 설명 | 범위 |
|---|---|---|---|
| `ColorTemperatureK` | float | 주광원 색온도 | 2,200–4,500 |
| `LightAngleDeg` | float | 주광원 수평 각도 | 8–45 |
| `ContrastLevel` | float | 씬 대비 | 0.0–1.5 |
| `AmbientR` | float | 보조광 R | 0.0–1.0 |
| `AmbientG` | float | 보조광 G | 0.0–1.0 |
| `AmbientB` | float | 보조광 B | 0.0–1.0 |
| `AmbientIntensity` | float | 보조광 HDR 강도 | 0.0–5.0 |
| `MossBabySSSBoost` | float | 이끼 아이 SSS/Emissive 강도 | 0.0–0.5 |

> **LightAngleDeg 범위 참고**: Art Bible §1 원칙 2의 전역 범위는 15–35°이나, Art Bible §2 Dream 상태("위에서 살짝 앞으로")는 의도적 예외. Dream의 40°는 "이것은 다른 순간"의 시각적 코딩을 위해 허용된다. Art Bible §1에 Dream 예외 조항 추가 필요.
>
> **MPC와 Light Actor의 관계**: MPC scalar는 머티리얼 그래프에서 읽히며, Light Actor의 Temperature/Intensity를 자동 반영하지 않는다. 주광원 색온도·각도·강도의 실제 변경은 GSM이 MPC 갱신과 별도로 Light Actor 프로퍼티를 구동하거나, Post-process LUT 블렌딩으로 색온도 인지를 대체해야 한다. 이 동기화 아키텍처는 OQ-6 ADR에서 결정. Lumen Dusk Scene(#11) GDD에서 경계 확정.

**상태별 MPC 목표값**:

| State | ColorTemp | LightAngle | Contrast | Ambient (R, G, B) | AmbientIntensity | MossBabySSSBoost |
|---|---|---|---|---|---|---|
| Menu | 3,600 | 25 | 0.3 | (0.65, 0.55, 0.75) | 1.5 | 0.00 |
| Dawn | 2,800 | 15 | 0.3 | (0.45, 0.40, 0.65) | 1.2 | 0.10 |
| Offering | 3,200 | 30 | 0.6 | (0.55, 0.50, 0.70) | 1.8 | 0.15 |
| Waiting | 2,800 | 20 | 0.3 | (0.50, 0.45, 0.65) | 1.0 | 0.10 |
| Dream | 4,200 | 40 | 0.9 | (0.60, 0.55, 0.85) | 2.5 | 0.40 |
| Farewell | 2,200 | 12 | 0.7 | (0.70, 0.50, 0.40) | 0.8 | 0.25 |

> **모든 MPC 목표값은 구현 단계에서 DataAsset(`UGameStateMPCAsset`)으로 외부화 — 코드 내 리터럴 금지.** 위 표는 Art Bible §2에서 도출한 초기 설계값이며, 튜닝 Knob 섹션의 `MPC_*` 파라미터로 노출된다.

#### Rule 4 — 블렌딩

상태 전환 시 MPC 파라미터는 **Lerp로 부드럽게 전환**된다.

- **블렌드 시간**: `StateBlendDurationSec` (기본 1.0초, 상태별 오버라이드 가능)
- **블렌드 커브**: SmoothStep `ease(x) = x²(3 − 2x)` (Formula 1). 커브 에셋 지원은 VS에서 검토
- **블렌드 중 새 전환 요청**: P0 전환만 인터럽트 가능. 현재 블렌드 진행 위치에서 새 목표로 즉시 리타겟 (중간값에서 시작 — 끊김 없음)
- **블렌드 중 앱 종료**: 블렌드 진행률은 저장하지 않음. 재시작 시 목표 상태의 MPC값을 즉시 적용 (0프레임 블렌드)

> **구현 참고 (UE 5.6)**: `UGameInstanceSubsystem`(`UMossGameStateSubsystem`)은 Tick 가상 함수를 제공하지 않음. MPC Lerp 갱신은 `FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &TickMPC), 0.0f)` 로 매 프레임 콜백 등록 — Time GDD와 동일 패턴. `FTSTicker`는 Game pause 상태와 무관하게 구동됨. `Deinitialize()`에서 ticker handle 해제 필수. MPC 갱신은 초기화 시 `UMaterialParameterCollectionInstance*`를 캐싱하고 `FName` 파라미터 이름도 캐싱하여 프레임당 탐색 비용 절감.

#### Rule 5 — ADVANCE_WITH_NARRATIVE 오버레이

시스템 내러티브(`ADVANCE_WITH_NARRATIVE`)는 별도 상태가 아니라 **Waiting 상태 내 일시적 콘텐츠 오버레이**로 처리한다.

- GSM은 Waiting 상태를 유지 (MPC 변경 없음)
- Stillness Budget `Narrative` 슬롯을 배타적으로 Request (동일 프레임에 다른 Narrative 이벤트 금지)
- 슬롯 획득 성공 → Content 레이어(텍스트/사운드)를 downstream에 전파 → 완료 후 Release
- `NarrativeCapPerPlaythrough(3)` 검사는 Time System이 발행 전 수행 — GSM은 cap 검사를 하지 않음
- Dream Trigger(#9)의 꿈 텍스트와는 완전히 별개 — 시스템 피드백 보이스

### States and Transitions

#### 전체 전환 표

| From | To | 트리거 | Guard | 우선순위 |
|---|---|---|---|---|
| `Menu` | `Dawn` | `FOnLoadComplete(true, false)` — 첫 실행 | — | P0 |
| `Menu` | `Dawn` | `FOnLoadComplete(false, _)` + 날짜 전진 | `DayIndex` 변화 감지 | P0 |
| `Menu` | `Waiting` | `FOnLoadComplete(false, _)` + 같은 날 | 카드 이미 건넨 상태 | P0 |
| `Menu` | `Offering` | `FOnLoadComplete(false, _)` + 같은 날 | 카드 아직 미건넴 | P0 |
| `Menu` | `Menu` (손상 분기) | `FOnLoadComplete(true, true)` | 양 슬롯 손상 | P0 |
| `Menu` | `Farewell` | `FOnLoadComplete(false, _)` | `LastPersistedState == Farewell` | P0 |
| `Dawn` | `Offering` | Dawn 시퀀스 완료 + CardSystem Ready | — | P1 |
| `Offering` | `Waiting` | **`FOnCardProcessedByGrowth` (Stage 2, ADR-0005)** — Growth CR-1 처리 완료 후 | — | P1 |
| `Waiting` | `Dream` | `FOnDreamTriggered` | Stillness Budget `Narrative` 슬롯 획득 | P2 |
| `Waiting` | `Farewell` | `ADVANCE_SILENT` | `DayIndex == GameDurationDays` + 카드 건넴 완료 | P0 |
| `Waiting` | `Waiting` (self-loop) | `ADVANCE_SILENT` | `DayIndex < GameDurationDays` + 점프폭 ≤ `WitheredThresholdDays` | — |
| `Waiting` | `Waiting` (withered) | `ADVANCE_SILENT` | `DayIndex < GameDurationDays` + 점프폭 > `WitheredThresholdDays` | P1 |
| `Waiting` | `Farewell` | `LONG_GAP_SILENT` | — (항상 DayIndex=21, Farewell 직행) | P0 |
| `Offering` | `Dawn` | `ADVANCE_SILENT` | DayIndex 전진 | P1 |
| `Dream` | `Waiting` | Dream 시퀀스 완료 | Budget Release 완료 | P1 |
| `Dream` | `Farewell` | `LONG_GAP_SILENT` | — (항상 DayIndex=21, Farewell 직행) | P0 |

**금지된 전환**:
- `Farewell → Any` — Farewell은 종단 상태. 새 플레이스루는 FSessionRecord 초기화(`"다시 시작"`) 후 Menu부터
- `Dawn → Dream` — Dawn 중 Dream 트리거 불가. Dawn 완료 → Offering → Waiting 거쳐야 Dream 조건 평가
- `Offering → Dream` — Offering 완료 전 Dream 불가. 카드 건넴 → Waiting 진입 후 동일 프레임에서 Dream 조건 즉시 평가 가능

#### 상태별 진입/이탈 액션

**Menu**:
- Entry: MPC 즉시 적용 (블렌드 없음, 콜드 스타트이므로) → Title 비주얼 표시 → Save/Load Initialize 대기
- Exit: `FOnLoadComplete` 수신 후 분기

**Dawn**:
- Entry: MPC Lerp 시작 (Menu → Dawn, 0.8–1.0초) → CardSystem에 Prepare 신호 → `FOnGameStateChanged(Dawn)` 브로드캐스트
- Exit: Dawn 시퀀스 완료 확인 (블렌드 완료 + 최소 체류 시간 경과) + CardSystem Ready 확인

**Offering**:
- Entry: MPC Lerp 시작 (Dawn → Offering, 0.8–1.0초) → Card Hand UI에 Show 신호 → `FOnGameStateChanged(Offering)` 브로드캐스트
- Exit: **`FOnCardProcessedByGrowth` 수신 (ADR-0005 Stage 2)** → Card Hand UI Hide. Growth 태그 가산 + SaveAsync 완료 후에만 GSM이 상태 전환 (Day 21 Final Form 순서 보장)

**Waiting**:
- Entry: MPC Lerp 시작 (이전 상태 → Waiting, 1.0–1.2초) → `FOnGameStateChanged(Waiting)` 브로드캐스트 → Time System이 `FOnDayAdvanced(DayIndex)` 발행 (GSM은 구독자, 발행자 아님) → Dream Trigger 평가 촉발
- Self-loop (ADVANCE_SILENT, DayIndex 전진): Time System이 `FOnDayAdvanced(DayIndex)` 발행 (GSM은 수신만). DayIndex 점프폭 > `WitheredThresholdDays`이면 `bWithered = true` + MPC 채도/SSS 약화 적용. 점프폭 ≤ 임계이면 MPC 변경 없음
- Exit: Dream 또는 Farewell 전환 시작

**Dream**:
- Entry: Stillness Budget `Narrative` 슬롯 Request → **실패 시 전환 연기** (Edge Case E5) → 성공 시 MPC Lerp 시작 (Waiting → Dream, 1.2–1.5초, 가장 긴 블렌드) → `FOnGameStateChanged(Dream)` 브로드캐스트
- Exit: Dream 콘텐츠 완료 → MPC Lerp 시작 (Dream → Waiting, 1.0–1.2초) → Stillness Budget Release → `FOnGameStateChanged(Waiting)` 브로드캐스트

**Farewell**:
- Entry: Stillness Budget `Narrative` 슬롯 Request → MPC Lerp 시작 (현재 → Farewell, 1.5–2.0초, 가장 느린 전환) → `FOnGameStateChanged(Farewell)` 브로드캐스트 → `FOnFarewellReached(Reason)` 전파 → 세이브 커밋 `ESaveReason::EFarewellReached`
- Exit: 없음 (종단)

> **구현 참고 (UE 5.6)**: GSM은 `UGameInstanceSubsystem`(`UMossGameStateSubsystem`) 기반. `UMossTimeSessionSubsystem`과 동일 레벨에서 `GetGameInstance()->GetSubsystem<>()` 대칭 통신. MPC 접근은 `GetWorld()`로 World Context 획득. Subsystem 간 참조는 **Lazy-init 패턴** — 첫 사용 시점에 `GetSubsystem<>()`으로 획득하고 `TWeakObjectPtr`로 캐싱. `PostInitialize()` 시점에서는 등록 순서가 보장되지 않아 다른 서브시스템이 미생성일 수 있으므로, `PostInitialize()`에서의 즉시 캐싱은 null 크래시 위험. 사용 시 null check + re-acquire 패턴 적용.
>
> **Dawn "새로운 날" 감지**: GSM이 Menu에서 복원 시 `FOnLoadComplete`와 `FOnTimeActionResolved`를 순차 수신한다. `ADVANCE_SILENT` 또는 `ADVANCE_WITH_NARRATIVE`가 DayIndex를 변화시켰으면 Dawn 진입. `HOLD_LAST_TIME` 또는 DayIndex 불변이면 Waiting/Offering으로 직접 복원.

### Interactions with Other Systems

#### 1. Time & Session System (#1) — 인바운드

GSM은 `FOnTimeActionResolved(ETimeAction)`를 구독한다. Time은 GSM에 일방향 발행 — GSM의 현재 상태를 Time이 읽지 않음.

| Action | GSM 반응 |
|---|---|
| `START_DAY_ONE` | Menu → Dawn 전환. `DayIndex = 1` |
| `ADVANCE_SILENT` | DayIndex 변화 시 Dawn → Offering → Waiting 시퀀스 재생. 불변 시 현재 상태 유지 |
| `ADVANCE_WITH_NARRATIVE` | Waiting 상태에서 Narrative 오버레이 실행 (Rule 5). DayIndex 변화 시 Dawn 시퀀스 선행 |
| `HOLD_LAST_TIME` | 현재 상태 유지. DayIndex·MPC 불변 |
| `ADVANCE_SILENT` / `ADVANCE_WITH_NARRATIVE` | DayIndex 전진. 점프폭(`NewDayIndex − PrevDayIndex`) > `WitheredThresholdDays`(기본 3)이면 `bWithered = true` 설정 + Waiting(고요한 대기 / Quiet Rest) 진입. 카드 건넴(`FOnCardOffered`)으로 해제 (Pillar 1: 죄책감 금지). `DayIndex == GameDurationDays` + 카드 건넴 완료 시 Farewell P0 |
| `LONG_GAP_SILENT` | DayIndex를 21로 clamp (Time GDD 처리). 항상 Farewell P0 직행 — withered 경로 없음. `LONG_GAP_SILENT`는 `WallDelta > 21일`일 때만 발행되므로 DayIndex=GameDurationDays 보장 |
| `LOG_ONLY` | GSM에 전파 없음 |

#### 2. Save/Load Persistence (#3) — 양방향

| 방향 | 데이터 | 시점 |
|---|---|---|
| Save → GSM | `FOnLoadComplete(bFreshStart, bHadPreviousData)` | 앱 시작 시 1회 |
| Save → GSM | `LastPersistedState` (FSessionRecord 확장) | 로드 시 1회 |
| GSM → Save | `LastPersistedState` 갱신 | Waiting 또는 Farewell 진입 시 |

- `FSessionRecord`에 `EGameState LastPersistedState` 필드 추가 (기본값 `Waiting`)
- GSM은 영속화 대상 상태(Waiting, Farewell)에 진입할 때만 `LastPersistedState`를 갱신하고 저장 트리거

#### 3. Stillness Budget (#10) — 양방향

| 방향 | 데이터 | 시점 |
|---|---|---|
| GSM → Budget | `Request(Channel, Priority)` | Dream·Farewell·Narrative 오버레이 진입 시 |
| Budget → GSM | `FStillnessHandle` (Grant/Deny) | 즉시 응답 |
| Budget → GSM | `OnBudgetRestored(Channel)` | ReducedMotion OFF 복귀 시 |
| GSM → Budget | `Release(Handle)` | 상태 이탈·시퀀스 완료 시 |

- Dream/Farewell 진입 시 `Narrative` 우선순위로 Request. Background/Standard 슬롯을 선점 가능
- Dream Budget 획득 실패 시 전환 연기 — `OnBudgetRestored` 구독으로 재시도 (Edge Case E5)
- ADVANCE_WITH_NARRATIVE 오버레이도 `Narrative` 슬롯 배타 소비 (Time GDD §Interaction 2 contract)

#### 4. Card System (#8) + Growth Accumulation Engine (#7) — 양방향 (ADR-0005 2단계 Delegate)

| 방향 | 데이터 | 시점 |
|---|---|---|
| GSM → Card | Dawn 상태 진입 시 Prepare 신호 (직접 호출) | 새 날 시작 |
| Card → GSM | CardSystem Ready (3장 준비 완료, 직접 호출) | Dawn 중 |
| **Growth → GSM** | **`FOnCardProcessedByGrowth(const FGrowthProcessResult&)` — Stage 2** | Card 건넴 후 Growth CR-1 처리 + (Day 21이면 CR-5 FinalForm 결정) + SaveAsync 완료 후 |
| GSM → Card | `FOnGameStateChanged` 브로드캐스트 | 상태 전환 시 |

- **ADR-0005 (2026-04-19) 적용**: GSM은 Card의 Stage 1 `FOnCardOffered`를 **직접 구독하지 않는다**. Growth가 CR-1 완료 후 Stage 2 `FOnCardProcessedByGrowth`를 broadcast하며 GSM이 이를 구독 — Day 21 순서 (Growth 태그 가산 → FinalFormId 결정 → GSM Farewell P0) C++ 호출 스택으로 강제
- `FGrowthProcessResult.bIsDay21Complete == true`이면 GSM은 Offering→Waiting 체류 없이 Farewell P0 즉시 전환
- GSM은 Card System의 내부 로직(덱 구성, 시즌 가중치)에 관여하지 않음
- Card System은 `FOnDayAdvanced`(Time 발행)를 직접 구독해 덱 재생성 — GSM 경유 불필요

#### 5. Downstream 소비자 — 아웃바운드 전용

GSM은 `FOnGameStateChanged(EGameState OldState, EGameState NewState)` delegate로 모든 downstream에 상태 변화를 브로드캐스트한다.

| System | 구독 목적 | MVP/VS |
|---|---|---|
| **Moss Baby Character (#6)** | 상태별 SSS/Emissive 강도, 자세 반응, 브리딩 리듬. MPC 읽기 전용 | MVP |
| Lumen Dusk Scene (#11) | 상태별 환경 에셋·카메라·DoF 조정 | MVP |
| Card Hand UI (#12) | Offering 시 카드 패 표시/숨김 | MVP |
| Dream Journal UI (#13) | Dream 시 일기 갱신 알림 | MVP |
| Audio System (#15) | 상태별 앰비언트·SFX 레이어 전환 | VS |
| Title & Settings UI (#16) | Menu 상태 표시/숨김 | VS |
| Farewell Archive (#17) | Farewell 진입 시 아카이브 생성 시작 | VS |

- MPC 파라미터는 GSM이 직접 갱신 — downstream은 MPC를 참조하는 머티리얼만 갖추면 자동 반영
- `FOnGameStateChanged`는 MPC 블렌드 **시작 시점**에 발행 (블렌드 완료가 아님). Downstream이 블렌드 완료를 알아야 하면 별도 `FOnBlendComplete` delegate 추가 (필요 시 ADR로 결정)

> **구현 참고**: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameStateChanged, EGameState, OldState, EGameState, NewState)` + `UPROPERTY(BlueprintAssignable)`. Blueprint 프로토타이핑 지원. OldState 포함 — Dream→Waiting 복귀와 Offering→Waiting 진입을 downstream이 구분해야 하며, Farewell Archive(#17)는 "어떤 상태에서 Farewell에 도달했는가"를 알아야 한다.

## Formulas

### Formula 1 — MPC 블렌드 보간 (GSM.MPCBlendValue)

상태 전환 시 각 MPC 파라미터의 시간 `t`에서의 현재 값.

`V(t) = V_start + (V_target − V_start) × ease(t / D_blend)`

`ease(x) = x²(3 − 2x)` [SmoothStep]

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| V_start | V₀ | float | 파라미터 의존 | 전환 시작 시점의 MPC 파라미터 값 (중단된 블렌드 시 현재 값) |
| V_target | V₁ | float | 파라미터 의존 | 목표 상태의 MPC 목표값 (Rule 3 테이블 참조) |
| t | t | float (sec) | [0, D_blend] | 전환 시작으로부터 경과 시간 |
| D_blend | D | float (sec) | [0.8, 2.0] | 해당 전환의 블렌드 지속 시간 (Formula 2 출력) |
| ease(x) | — | float | [0, 1] | SmoothStep 이징. x=0→0, x=0.5→0.5, x=1→1 |

**Precondition**: `D_blend > 0`. `D_blend = 0`이면 Lerp를 건너뛰고 `V(t) = V_target`을 즉시 반환 (ReducedMotion 및 앱 재시작 경로). `V_start ≈ V_target`(`|V_target − V_start| < 0.001f`)이면 Lerp 생략, 즉시 완료 처리. **부동소수점 방어**: `x = clamp(t / D_blend, 0.0f, 1.0f)` — `t`가 `D_blend`를 epsilon만큼 초과해도 overshoot 방지.

**Output Range**: [min(V₀, V₁), max(V₀, V₁)] — SmoothStep은 overshoot 없음.

**Example**: Waiting→Dream 전환, `ColorTemperatureK`: V₀=2,800, V₁=4,200, D=1.35s, t=0.675s
- x = 0.675/1.35 = 0.5 → ease(0.5) = 0.25 × 2.0 = 0.5
- V(0.675) = 2,800 + (4,200 − 2,800) × 0.5 = **3,500K** (중간값)

**Mid-transition restart**: P0 인터럽트로 블렌드 중단 시, 현재 `V(t)`가 새 전환의 `V_start`가 됨. D_blend는 새 전환 쌍으로 재설정.

### Formula 2 — 상태 전환 블렌드 지속 시간 (GSM.StateBlendDuration)

`D_blend = D_min[transition] + (D_max[transition] − D_min[transition]) × BlendDurationBias`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| D_min[transition] | D_min | float (sec) | [0.8, 1.5] | 전환 쌍별 최단 블렌드 시간 (DataAsset) |
| D_max[transition] | D_max | float (sec) | [1.0, 2.0] | 전환 쌍별 최장 블렌드 시간 (DataAsset) |
| BlendDurationBias | B | float | [0.0, 1.0] | 전역 감성 속도 조절. 0=빠름, 1=느림. 기본 0.5 |

**Precondition**: `D_max ≥ D_min`. DataAsset 에디터에서 `PostEditChangeProperty` 검증 강제 (`WITH_EDITOR` 가드 내). **런타임 방어**: Shipping 빌드에서 DataAsset 직렬화 손상 시를 대비해 `D_blend = clamp(D_blend, D_min, max(D_min, D_max))` 런타임 clamp 적용. `B`는 `clamp(BlendDurationBias, 0.0, 1.0)` 적용.

**Output Range**: [D_min, D_max]

**전환별 기본값 테이블** (DataAsset에 외부화):

| 전환 | D_min (sec) | D_max (sec) | Bias=0.5 결과 |
|---|---|---|---|
| Menu→Dawn | 0.8 | 1.0 | 0.9 |
| Dawn→Offering | 0.8 | 1.0 | 0.9 |
| Offering→Waiting | 1.0 | 1.2 | 1.1 |
| Waiting→Dream | 1.2 | 1.5 | 1.35 |
| Dream→Waiting | 1.0 | 1.2 | 1.1 |
| Any→Farewell | 1.5 | 2.0 | 1.75 |

**Example**: Any→Farewell, BlendDurationBias=0.7 (느리게)
- D = 1.5 + (2.0 − 1.5) × 0.7 = 1.5 + 0.35 = **1.85초**

### Formula 3 — Dawn 최소 체류 시간 (GSM.DawnDwellTime)

Dawn은 "새로운 날의 의식"으로 블렌드 완료 후에도 최소 시간 동안 체류한다.

`T_wait = max(0, DawnMinDwellSec − D_blend_elapsed)`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| DawnMinDwellSec | T_dwell | float (sec) | [2.0, 5.0] | Dawn 최소 체류 시간 (DataAsset). 기본 3.0. Tuning Knobs 안전 범위와 일치 |
| D_blend_elapsed | D_e | float (sec) | [0.8, 2.0] | Menu→Dawn 블렌드 완료까지 경과 시간 |
| T_wait | T_w | float (sec) | [0, ~3.2] | 블렌드 완료 후 추가 대기 시간 |

**Output Range**: [0, DawnMinDwellSec − D_min[Menu→Dawn]]. 기본값 기준 ~2.2s이나, 극단 튜닝(DawnMinDwellSec=5.0, D_blend=0.8s)에서 최대 4.2s. 총 Dawn 체류 상한: `D_blend + max(T_wait, CardSystemReadyTimeoutSec)` — 극단값에서 최대 ~11s.

**Example**: DawnMinDwellSec=3.0, D_blend_elapsed=0.9s
- T_wait = max(0, 3.0 − 0.9) = **2.1초 대기 후 Offering 전환**

CardSystem Ready 신호가 T_wait 이후에 도착하면 Ready까지 추가 대기 (Formula에 포함되지 않음 — Guard 조건).

## Edge Cases

- **E1 — 로드 시 저장 상태가 Dream인 경우**: Waiting으로 강등. Dream은 transient 상태이며 영속화 대상이 아님. MPC는 Waiting 목표값으로 즉시 적용.

- **E2 — 블렌드 진행 중 앱 종료**: 블렌드 진행률은 저장하지 않음. 재시작 시 목표 상태의 MPC값을 0프레임에 즉시 적용. 플레이어는 중간 색온도를 보지 않음.

- **E3 — Dawn 또는 Offering 중 Dream 트리거 시도**: 전환 거부. Dawn 완료 → Offering → Waiting 순서를 거쳐야 Dream 조건 평가가 시작됨. Dream Trigger는 Waiting 상태에서만 평가.

- **E4 — Dream 진행 중 LONG_GAP_SILENT 수신**: Dream 즉시 종료. Stillness Budget `Narrative` 슬롯 강제 Release (RAII). `LONG_GAP_SILENT`는 항상 DayIndex=21이므로 즉시 Farewell P0 전환. MPC는 Dream(4,200K) → Farewell(2,200K)로 Lerp.

- **E5 — Dream 진입 시 Stillness Budget 획득 실패**: Dream 전환 연기(defer). GSM은 `OnBudgetRestored(Motion)` 구독 → 슬롯 해제 시 재시도. Waiting 상태 유지. 연기 중 `DreamDeferMaxSec`(기본 60초) 초과 시 Budget을 강제 선점(P0급)하여 Dream 진입 — 앵커 모먼트 소실 방지. 연기 대기 중 추가 `FOnDreamTriggered` 도착 시 무시(최초 트리거 유지). 연기 중 DayIndex 전진(Farewell 조건 충족) 시 Dream 연기를 취소하고 Farewell 전환.

- **E6 — Dawn 중 LONG_GAP_SILENT 수신**: Dawn 시퀀스 완료를 기다리지 않고 즉시 Dawn→Farewell P0 전환 (`LONG_GAP_SILENT`는 항상 DayIndex=21). Stillness Budget 핸들 미보유 상태이므로 Release 불필요.

- **E7 — Offering 중 LONG_GAP_SILENT 수신**: Card Hand UI 강제 Hide → 즉시 Farewell P0 전환 (`LONG_GAP_SILENT`는 항상 DayIndex=21). 해당 날 카드 건넴은 무효.

- **E18 — 장기 부재 후 복귀 (withered 진입)**: `ADVANCE_SILENT` 수신 시 `NewDayIndex − PrevDayIndex > WitheredThresholdDays`(기본 3)이면 `bWithered = true` 설정. Waiting(메마름) 진입 — MPC 채도 감소, `MossBabySSSBoost` 약화. 첫 카드 건넴(`FOnCardOffered`)으로 `bWithered = false` 해제 + MPC 정상 Waiting 파라미터 복원. Pillar 1 준수: 페널티 없음, 복귀 메시지 없음, "놓친 날"에 대한 알림 없음. 메마름은 시각적 신호일 뿐 게임플레이 불이익 아님.

- **E8 — 빠른 연속 ADVANCE_SILENT (다일 어드밴스)**: DayIndex는 최종 목표값으로 즉시 갱신. Dawn→Offering→Waiting 시퀀스는 **1회만** 재생. 중간 날짜마다 Dawn을 별도 재생하지 않음. 5일분 Dawn을 강제 관람하는 것은 Pillar 1 위반.

- **E9 — Dawn 중 CardSystem이 Ready 미반환 (타임아웃)**: `CardSystemReadyTimeoutSec`(Tuning Knob, 기본 5.0초) 초과 시 Dawn→Offering 강제 전환. Card Hand UI에 Degraded 플래그 전달. 빈 패 Offering은 `FOnCardOffered` 없이 Waiting으로 자동 전환. Dawn 무한 체류 교착 방지.

- **E10 — MPC 블렌드 중 ReducedMotion 토글 ON**: 현재 Lerp 즉시 중단, 목표 MPC값을 0프레임에 적용 (앱 종료 시와 동일 동작). **`MossBabySSSBoost` 포함**: SSS 목표값도 즉시 적용 — 시각 결과(Dream에서 이끼 아이가 빛남)는 유지되며 전환 애니메이션만 생략. SSS는 상태값이지 모션이 아니므로 ReducedMotion에서도 목표값 유지가 적절. ReducedMotion OFF 복귀 시 MPC 재블렌드 불필요 — 이미 목표값 도달. 접근성 토글은 즉각 반응.

- **E11 — Farewell Entry 중 앱 크래시 (세이브 커밋 전)**: `LastPersistedState`는 여전히 Waiting. 재기동 시 Waiting/Offering으로 복원. Day 21 조건 여전히 충족 시 해당 세션에서 Farewell 재진입. 미완료 Farewell은 "없던 일" — 데이터 무결성 우선.

- **E12 — FOnLoadComplete와 FOnTimeActionResolved 수신 순서 역전**: `FOnLoadComplete` 미수신 상태에서 `FOnTimeActionResolved` 도착 시, Time 신호를 단일 버퍼에 보관하고 적용 보류. `FOnLoadComplete` 수신 후 버퍼를 소비해 상태 결정. "어떤 플레이스루인지" 모르는 상태에서 Time 신호 해석 불가.

- **E13 — Day 21 카드 건넴 직후 Farewell 즉시 도달 (ADR-0005 2단계 Delegate)**: Card `FOnCardOffered` 발행 → Growth CR-1 태그 가산 + CR-5 FinalForm 결정 + `FOnFinalFormReached(FormId)` 발행 + `SaveAsync(ECardOffered)` → Growth가 Stage 2 `FOnCardProcessedByGrowth.Broadcast({CardId, bIsDay21Complete=true, FinalFormId})` → GSM 수신 시 Waiting 체류 없이 즉시 Farewell P0 전환. `FOnDayAdvanced`는 미발행 — Dream Trigger가 종료된 게임에서 꿈 평가하는 사태 방지. **순서는 C++ 호출 스택으로 강제** — UE Multicast Delegate 등록 순서 비공개 계약 회피.

- **E14 — 같은 날 재진입 (카드 이미 건넨 후)**: `FOnLoadComplete(false, _)` + DayIndex 불변 + 카드 건넨 상태 → Dawn 없이 즉시 Waiting 복원. Dawn은 "새로운 날"의 의식이므로 같은 날 재생 금지 (Pillar 1).

- **E15 — Menu 손상 분기 (양 슬롯 손상)**: `FOnLoadComplete(true, true)` → Menu 유지. Title & Settings UI(VS에서는 MVP 최소 분기)에 손상 신호 전달 → "이끼 아이는 깊이 잠들었습니다" + "다시 시작" 확인. 확인 후 FSessionRecord 초기화 → Menu→Dawn P0 전환. 자동 Dawn 진입 금지 — 21일 리셋은 명시적 승인 필요 (Pillar 4).

- **E16 — Farewell 진입 시 Stillness Budget 획득 실패**: Farewell은 P0 종단 전환이므로 Budget 획득 실패와 무관하게 전환 강행. Budget 요청은 best-effort (시각 연출용) — 실패 시 MPC 블렌드 즉시 적용(0프레임), Farewell 종단 상태 정상 진입. E5(Dream defer)와 달리 연기하지 않음. **감정적 트레이드오프 인지**: Player Fantasy는 Farewell을 "가장 느린 전환(1.5–2.0초)"으로 기술하지만, Budget 실패 시 0프레임 적용으로 이 약속이 깨짐. 이것은 데이터 무결성(종단 상태 보장) > 감정 연출의 명시적 설계 결정임.

- **E17 — Offering 상태에서 ADVANCE_SILENT 수신**: 해당 날 카드는 건네지 않은 것으로 처리 (`FOnCardOffered` 미발행). Card Hand UI Hide → DayIndex 전진 → Dawn 시퀀스 재생(새 날). Pillar 1 준수 — 강제 카드 선택 없음, 놓친 하루에 대한 페널티 없음.

## Dependencies

| System | 방향 | 성격 | 인터페이스 | MVP/VS |
|---|---|---|---|---|
| **Time & Session (#1)** | 인바운드 | **Hard** | `FOnTimeActionResolved(ETimeAction)`, `FOnFarewellReached(EFarewellReason)`, `FOnDayAdvanced(int32)` | MVP |
| **Save/Load Persistence (#3)** | 양방향 | **Hard** | `FOnLoadComplete(bool, bool)` (인바운드), `FSessionRecord.LastPersistedState` (양방향) | MVP |
| **Stillness Budget (#10)** | 양방향 | **Soft** | `Request(Channel, Priority)` → `FStillnessHandle`, `OnBudgetRestored` | MVP |
| **Card System (#8)** | 양방향 | **Hard** | GSM→Card: Prepare 신호 (직접 호출). Card→GSM: Ready (직접 호출). **Card의 `FOnCardOffered`는 GSM이 직접 구독하지 않음** (ADR-0005) | MVP |
| **Growth Accumulation Engine (#7)** | 인바운드 | **Hard** | **`FOnCardProcessedByGrowth(const FGrowthProcessResult&)` — Stage 2 delegate (ADR-0005 2026-04-19)**. Day 21 순서 보장의 중심 | MVP |
| **Lumen Dusk Scene (#11)** | 아웃바운드 | **Soft** | `FOnGameStateChanged(EGameState)` + MPC 자동 반영 | MVP |
| **Card Hand UI (#12)** | 아웃바운드 | **Hard** | `FOnGameStateChanged` (Show/Hide 트리거) | MVP |
| **Dream Journal UI (#13)** | 아웃바운드 | **Soft** | `FOnGameStateChanged` (Dream 상태 시 갱신 알림) | MVP |
| **Audio System (#15)** | 아웃바운드 | **Soft** | `FOnGameStateChanged` (상태별 레이어 전환) | VS |
| **Title & Settings UI (#16)** | 아웃바운드 | **Soft** | `FOnGameStateChanged` (Menu 표시/숨김) + 손상 분기 신호 | VS |
| **Farewell Archive (#17)** | 아웃바운드 | **Soft** | `FOnGameStateChanged(Farewell)` (아카이브 생성 시작) | VS |

**Hard dependency**: GSM이 이 시스템 없이 작동 불가.
**Soft dependency**: GSM은 작동하지만 해당 시스템의 기능이 누락됨 (빈 비주얼, 무음 등).

**양방향 일관성 확인**: Time GDD §Interaction 2에 "GSM이 Action 소비" 명시 ✓. Save/Load GDD에 `FOnLoadComplete` 정의 ✓. Card System GDD는 미작성 — `FOnCardOffered` 인터페이스를 Card GDD 작성 시 확인 필요.

## Tuning Knobs

| Knob | 타입 | 기본값 | 안전 범위 | 게임플레이 영향 |
|---|---|---|---|---|
| `BlendDurationBias` | float | 0.5 | [0.0, 1.0] | 전역 전환 속도. 0=빠름, 1=느림. 너무 빠르면 상태 변화를 인식 못함; 너무 느리면 조작 응답이 둔해 보임 |
| `D_min[transition]` | float (sec) × 6 | 전환별 상이 | [0.3, 1.5] | 전환별 최소 블렌드 시간. 0.3 미만이면 SmoothStep 효과 무의미 |
| `D_max[transition]` | float (sec) × 6 | 전환별 상이 | [0.5, 3.0] | 전환별 최대 블렌드 시간. 3.0 초과 시 플레이어가 "멈췄다"고 느낌 |
| `DawnMinDwellSec` | float (sec) | 3.0 | [2.0, 5.0] | Dawn 최소 체류 시간. 너무 짧으면 "새로운 날" 의식감 사라짐; 너무 길면 Pillar 1 위반 (기다리게 만듦) |
| `CardSystemReadyTimeoutSec` | float (sec) | 5.0 | [3.0, 10.0] | Dawn 중 CardSystem Ready 대기 상한. 초과 시 E9 Degraded Fallback. 너무 짧으면 정상 로드를 실패로 오판 |
| `DreamDeferMaxSec` | float (sec) | 60.0 | [30.0, 120.0] | Dream Budget 획득 실패 시 연기 상한. 초과 시 Budget 강제 선점(P0급)으로 Dream 진입. 30 미만이면 Budget 경쟁 시 너무 공격적; 120 초과 시 앵커 모먼트 소실 위험 |
| `MPC_ColorTemperatureK[state]` | float × 6 | 상태별 상이 | [2,000, 5,000] | 상태별 색온도 목표. Art Bible §2 기반. 2,000 미만은 가시 범위 벗어남; 5,000 초과는 Terrarium Dusk 위반 |
| `MPC_LightAngleDeg[state]` | float × 6 | 상태별 상이 | [5, 45] | 상태별 주광원 각도. 5° 미만은 수평 피어싱; 45° 초과는 정오 느낌 → Art Bible 원칙 2 위반 |
| `MPC_ContrastLevel[state]` | float × 6 | 상태별 상이 | [0.1, 1.5] | 상태별 대비. 0.1 미만은 구분 불가; 1.5 초과는 드라마틱 과잉 |
| `MPC_Ambient[state]` | FLinearColor × 6 | 상태별 상이 | R,G,B ∈ [0.0, 1.0] | 상태별 보조광 색상 |
| `MPC_AmbientIntensity[state]` | float × 6 | 상태별 상이 | [0.0, 5.0] | 상태별 보조광 HDR 강도. Art Bible §2 기반 |
| `MPC_MossBabySSSBoost[state]` | float × 6 | 상태별 상이 | [0.0, 0.5] | 상태별 이끼 아이 SSS/Emissive 강도. Dream(0.40)이 최대 — Art Bible §2 "안에서 빛나는 것처럼" |
| `WitheredThresholdDays` | int32 | 3 | [2, 7] | DayIndex 점프폭이 이 값 초과 시 `bWithered = true` 설정. 2 미만이면 1일 공백에도 메마름 → Pillar 1 위반; 7 초과면 장기 부재에도 시각 피드백 없음 |

**Knob 상호작용 주의**:
- `BlendDurationBias`와 `DawnMinDwellSec`는 독립적이지만 결합 효과 발생: Bias=1.0 + DawnMinDwell=5.0이면 Dawn 체류가 6초 이상 → Pillar 1 위반 영역
- `MPC_ColorTemperatureK[Dream]`을 따뜻한 톤(< 3,000)으로 설정하면 Dream의 "유일한 냉색" 시각 코딩이 사라져 Player Fantasy 파괴

**외부화 구조**: 모든 Knob은 `UGameStateMPCAsset : UDataAsset`에 `UPROPERTY(EditAnywhere)` 필드로 노출. GSM Subsystem의 `UPROPERTY()` Hard Reference로 직접 참조 — 비동기 로딩 불필요.

## Visual/Audio Requirements

### Visual — MPC 상태 전환 (핵심)

이 시스템의 시각적 출력은 **Section C Rule 3의 MPC 파라미터 세트**와 **Art Bible §2의 6개 상태 무드 명세**로 완전히 정의된다.

**전환별 시각적 특성**:
- **Dawn→Offering**: 색온도가 약간 올라가며(2,800→3,200K), 대비 증가. 카드에 시선을 유도하는 미세한 하이라이트 강화
- **Waiting→Dream**: 이 게임의 유일한 **냉색 전환**. 2,800K→4,200K. 1.2–1.5초에 걸쳐 세계의 분위기가 뒤집힘. 이 전환이 Player Fantasy의 앵커 모먼트
- **Any→Farewell**: 가장 느린 전환(1.5–2.0초). 2,200K — 이 게임의 가장 따뜻한 빛. 화면 가장자리 Vignette 점진 증가는 Lumen Dusk Scene(#11) 책임

**ReducedMotion 시 시각 동작**: 모든 MPC 블렌드를 0프레임 즉시 적용 (E10). 최종 시각 결과는 동일하나 전환 과정이 생략됨.

### Audio — 상태 정보 제공만

GSM 자체는 오디오를 직접 생성하지 않는다. `FOnGameStateChanged`를 통해 Audio System(#15, VS)에 상태 전환을 알리고, Audio가 상태별 앰비언트·SFX 레이어를 자체적으로 관리한다. MVP에서는 Audio System이 미구현이므로 GSM의 오디오 책임은 없음.

> **📌 Asset Spec** — Visual/Audio requirements가 정의되었습니다. Art Bible 승인 후 `/asset-spec system:game-state-machine`을 실행해 MPC DataAsset 스펙과 상태별 시각 가이드를 생성할 수 있습니다.

## UI Requirements

GSM은 직접 UI 위젯을 생성하지 않는다. UI 관련 책임은 downstream 시스템에 위임된다:
- **Card Hand UI (#12, MVP)**: Offering 상태에서 Show/Hide
- **Dream Journal UI (#13, MVP)**: Dream 상태에서 갱신 알림
- **Title & Settings UI (#16, VS)**: Menu 상태에서 표시 + 손상 분기(E15)

GSM은 `FOnGameStateChanged`로 상태를 브로드캐스트할 뿐이며, 위젯 생성·배치·입력 처리는 각 UI 시스템의 GDD에서 정의된다.

## Acceptance Criteria

### Rule 1 — EGameState 열거 및 영속화

**AC-GSM-01** | `CODE_REVIEW` | BLOCKING
- **GIVEN** `EGameState` 열거형 정의 코드, **WHEN** 정적 분석(grep + static_assert) 실행, **THEN** 값이 정확히 6개(Menu=0, Dawn=1, Offering=2, Waiting=3, Dream=4, Farewell=5)이며, `LastPersistedState` 기록 코드 경로가 `Waiting`과 `Farewell` 진입 시에만 존재

**AC-GSM-02** | `AUTOMATED` | BLOCKING
- **GIVEN** Dream 상태를 `LastPersistedState`로 담고 있는 `FSessionRecord`를 로드, **WHEN** GSM이 `FOnLoadComplete` 수신, **THEN** 실제 초기 상태는 `Waiting`이며 Dream으로 진입하지 않음 (E1)

### Rule 2 — 우선순위 시스템

**AC-GSM-03** | `AUTOMATED` | BLOCKING
- **GIVEN** Dream 상태에서 MPC 블렌드 진행 중, **WHEN** `LONG_GAP_SILENT`(P0) 도달, **THEN** Dream 즉시 종료 + Farewell 전환 + Stillness Budget 핸들 Release (E4)

**AC-GSM-04** | `AUTOMATED` | BLOCKING
- **GIVEN** Dawn→Offering 전환(P1) 진행 중, **WHEN** Dream Trigger 이벤트(P2) 도달, **THEN** Dream 전환 거부, 자동 순서 완료 (E3)

**AC-GSM-05** | `CODE_REVIEW` | BLOCKING
- **GIVEN** 상태 전환 분기 코드 전체, **WHEN** `Farewell → Any` 방향 전환 분기를 grep, **THEN** 해당 분기가 존재하지 않음 (종단 상태 불변성)

### Rule 3 — MPC 목표값 외부화

**AC-GSM-06** | `CODE_REVIEW` | BLOCKING
- **GIVEN** GSM 및 Visual State Director 구현 파일, **WHEN** MPC 파라미터 리터럴 값 grep, **THEN** 하드코딩된 수치 없음, 모든 값이 `UGameStateMPCAsset`에서만 참조

### Rule 4 — 블렌딩

**AC-GSM-07** | `AUTOMATED` | BLOCKING
- **GIVEN** Formula 1 SmoothStep 구현, **WHEN** x=0, x=0.5, x=1.0 입력, **THEN** 출력 각각 0.0, 0.5, 1.0이며 [0,1] 범위 초과 없음 (F1)

**AC-GSM-08** | `AUTOMATED` | BLOCKING
- **GIVEN** Waiting→Dream 블렌드 50% 진행 중(t=D_blend/2), **WHEN** P0 인터럽트(Farewell) 도달, **THEN** 새 전환의 V_start가 인터럽트 시점 블렌드 중간값이며 Waiting으로 먼저 돌아가지 않음 (F1 mid-transition restart)

**AC-GSM-09** | `AUTOMATED` | BLOCKING
- **GIVEN** `BlendDurationBias`=0.0, 0.5, 1.0, **WHEN** Formula 2로 Any→Farewell 블렌드 시간 계산, **THEN** 출력 각각 1.5s, 1.75s, 2.0s이며 [D_min, D_max] 범위 내 (F2)

**AC-GSM-10** | `AUTOMATED` | BLOCKING
- **GIVEN** MPC 블렌드 진행 중, **WHEN** ReducedMotion ON 토글, **THEN** 현재 Lerp 즉시 중단 + 목표 MPC값 동일 프레임 적용. OFF 복귀 시 MPC 재블렌드 미발생 (E10)

### Rule 5 — ADVANCE_WITH_NARRATIVE

**AC-GSM-11** | `INTEGRATION` | BLOCKING
- **GIVEN** GSM Waiting 상태 + Budget Narrative 슬롯 가용, **WHEN** `ADVANCE_WITH_NARRATIVE` 신호 도달, **THEN** GSM 상태 `Waiting` 유지 + MPC 파라미터 변경 없음 + `Narrative` 슬롯 점유 상태(`FStillnessHandle` 유효) 확인 — 슬롯 점유 중 두 번째 `ADVANCE_WITH_NARRATIVE` 수신 시 두 번째 요청 거부

### Formulas

**AC-GSM-12** | `AUTOMATED` | BLOCKING
- **GIVEN** DawnMinDwellSec=3.0, Menu→Dawn 블렌드 0.9s, **WHEN** Formula 3 계산, **THEN** 추가 대기 2.1s, 블렌드 완료 후 2.1s 전에 Offering 전환 미시작 (F3)

### Edge Cases

**AC-GSM-13** | `AUTOMATED` | BLOCKING
- **GIVEN** Stillness Budget 전체 점유 상태, **WHEN** `FOnDreamTriggered` 도달, **THEN** Waiting 유지 + `OnBudgetRestored` 구독 등록 + 슬롯 해제 시 Dream 재시도 (E5)

**AC-GSM-14** | `AUTOMATED` | BLOCKING
- **GIVEN** 5일치 `ADVANCE_SILENT` 빠른 연속 수신, **WHEN** 모든 신호 처리 완료, **THEN** Dawn→Offering→Waiting 시퀀스 1회만 재생 + DayIndex 최종 목표값 (E8)

**AC-GSM-15** | `AUTOMATED` | BLOCKING
- **GIVEN** Menu 상태 + `FOnLoadComplete` 미수신, **WHEN** `FOnTimeActionResolved` 먼저 도달, **THEN** Time 신호 단일 버퍼 보관 + 적용 보류 + `FOnLoadComplete` 후 올바른 분기 (E12)

**AC-GSM-16** | `AUTOMATED` | BLOCKING
- **GIVEN** DayIndex == GameDurationDays + Offering 완료, **WHEN** Waiting 진입 직후 조건 평가, **THEN** `FOnDayAdvanced` 미발행 + 즉시 Farewell P0 전환 (E13)

**AC-GSM-17** | `AUTOMATED` | BLOCKING
- **GIVEN** Dawn 상태 + `CardSystemReadyTimeoutSec` 초과, **WHEN** CardSystem Ready 미도달, **THEN** Offering 강제 전환 + Degraded 플래그 전달 (E9)

### Cross-System

**AC-GSM-18** | `INTEGRATION` | BLOCKING
- **GIVEN** GSM 임의 상태에서 전환 트리거 수신, **WHEN** 전환 처리 완료, **THEN** (1) `FOnGameStateChanged(OldState, NewState)` 브로드캐스트 1회 발생, (2) MPC `ColorTemperatureK` 파라미터가 목표값 방향으로 변화 시작, (3) MVP 필수 downstream 3개(Lumen Dusk, Card Hand UI, Dream Journal UI)의 콜백 수신 횟수 각각 1회. 브로드캐스트는 MPC Lerp 시작 이전 또는 동시 — 역순(Lerp 후 브로드캐스트)이면 실패

### Withered Lifecycle (R3 추가)

> **Vocabulary Rule (Pillar 1 보호, 2026-04-19 추가)**: `bWithered` / `WitheredThresholdDays`는 **기술적 영속화 identifier**이며 `FSessionRecord`의 bool 필드. 플레이어 대면 vocabulary·시각 표현은 [MBC CR-5 "Quiet Rest / 고요한 대기"](moss-baby-character-system.md) 의미론을 따른다. **금지**: drying, yellowing, 탈색, 건조화 시각 큐 (MBC CR-5 Pillar 1 design test — "복귀한 플레이어의 첫 반응이 '미안'이면 실패, '오래 잤네'이면 성공"). `bWithered = true`는 MBC DryingFactor 상승의 영속화 플래그일 뿐, GSM이 독립적 visual layer를 추가하지 않음. MPC `MossBabySSSBoost` 감소는 "쉼"의 차분함 (서리·차가운 톤) 표현이며 "메마름" 훼손 표현 아님.

**AC-GSM-20** | `AUTOMATED` | BLOCKING
- **GIVEN** Waiting 상태 + `PrevDayIndex = 5`, **WHEN** `ADVANCE_SILENT` 수신 + `NewDayIndex = 10`(점프폭 5 > `WitheredThresholdDays` 3), **THEN** `bWithered = true` 설정 + `FSessionRecord`에 영속화 + MPC `MossBabySSSBoost`가 Waiting 기본값(0.10)보다 감소 적용 (E18)

**AC-GSM-21** | `AUTOMATED` | BLOCKING
- **GIVEN** `bWithered = true` Waiting 상태, **WHEN** `FOnCardProcessedByGrowth` 수신 (Stage 2, ADR-0005 — Growth 처리 완료 후), **THEN** `bWithered = false` 설정 + MPC 파라미터가 정상 Waiting 목표값으로 복원 + `FSessionRecord` 갱신 영속화

### Visual (기존)

**AC-GSM-19** | `MANUAL` | ADVISORY
- **GIVEN** Waiting→Dream 전환(BlendDurationBias=0.5, D_blend=1.35s), **WHEN** 전환 중 화면 녹화, **THEN** 색온도가 구리빛(2,800K)에서 달빛(4,200K)으로 끊김·점프 없이 부드럽게 전환. QA 리드 sign-off 필요. **전제조건**: OQ-6 ADR 완료 후 실행 가능

---

| AC 유형 | 수량 | Gate |
|---|---|---|
| AUTOMATED | 15 | BLOCKING |
| INTEGRATION | 2 | BLOCKING |
| CODE_REVIEW | 3 | BLOCKING |
| MANUAL | 1 | ADVISORY |
| **합계** | **21** | — |

## Open Questions

| # | Question | Owner | Target |
|---|---|---|---|
| ~~OQ-1~~ | ~~`UWorldSubsystem` vs `UGameInstanceSubsystem`~~ | — | **RESOLVED (R2)**: `UGameInstanceSubsystem` 선택. Time System과 동일 레벨에서 `GetGameInstance()->GetSubsystem<>()` 대칭 통신. MPC 접근은 `GetWorld()`로 World Context 획득 가능 — `UWorldSubsystem` 필수 근거 없음. ADR 후보로 공식 기록. |
| OQ-2 | `FOnBlendComplete` delegate 필요 여부 — downstream이 블렌드 완료 시점을 알아야 하는가? MVP 구현 시 확인 | Card Hand UI GDD (#12) | Card Hand UI 설계 시 |
| OQ-3 | MPC 목표값 Art Bible §2 수치의 실기 검증 — UE 5.6 Lumen 환경에서 색온도·대비·앰비언트 조합이 의도한 무드를 만드는지 | Lumen Dusk Scene GDD (#11) | 첫 구현 마일스톤 |
| OQ-4 | SmoothStep 외 이징 커브 지원 — VS에서 UCurveFloat DataAsset으로 전환별 커스텀 커브 지원 검토 | VS 스프린트 | Vertical Slice |
| OQ-5 | Menu 상태와 Title & Settings UI(#16) 통합 설계 — MVP에서 Menu는 콜드 스타트 게이트만이지만, VS에서 설정 오버레이 추가 시 GSM 상태 추가 또는 기존 Menu 확장 결정 필요 | Title & Settings UI GDD (#16) | VS 설계 시 |
| ~~OQ-6~~ | ~~MPC-Light Actor 동기화 아키텍처~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0004](../../docs/architecture/adr-0004-mpc-light-actor-sync.md) — Hybrid: GSM이 Light Actor Temperature/Intensity 직접 구동 + Lumen Dusk가 Post-process LUT/Vignette/DoF 소유 + MPC 머티리얼 read-only) |
