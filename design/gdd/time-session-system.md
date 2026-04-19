# Time & Session System

> **Status**: **Approved** — R3 (lean review APPROVED 2026-04-17. R2 6 BLOCKER 해소 + 8 RECOMMENDED 적용 + OQ-4/OQ-6 RESOLVED. RECOMMENDED 2건 구현 이관)
> **Author**: bong9tutor + claude-code session
> **Last Updated**: 2026-04-17 (R3)
> **Implements Pillar**: Pillar 4 (끝이 있다), Pillar 1 (조용한 존재 — 애매한 시간 이벤트의 silent narrative absorption, modal/차단 금지), Pillar 3 (말할 때만 말한다 — 시스템 내러티브 cap 강제)
> **Priority**: MVP | **Layer**: Foundation | **Category**: Core
> **Prototype reference**: `prototypes/time-session/REPORT.md` (12/12 scenarios passed; LONG_GAP modal 제거 후 재검증 필요)
> **R2 변경 요약**: LONG_GAP modal → silent auto-farewell (Pillar 1) | NarrativeCount cap=3/playthrough (Pillar 3) | Rule precedence 명시 | UE 5.6 lifecycle 정정 | `IMossClockSource` 시스템 의존성 격상 | Forward Tamper 명시적 수용 + negative AC | OQ-5 `UDeveloperSettings` 권장
> **R3 변경 요약**: UE 5.6 lifecycle 재정정 (`FTSTicker` + `FWorldDelegates`) | stale R1 텍스트 4곳 sweep | `IMossClockSource` 순수 C++ 4 virtual methods (OQ-6 RESOLVED) | `LongGapSilent` → `LongGapAutoFarewell` (OQ-4 RESOLVED) | SessionEnding+suspend 상태 보강 | AC 5건 재작성 + 5건 신규 (29 total) | `ESaveReason` 정식 cross-ref | Player Fantasy 어조 교정 | Stillness Budget 슬롯 contract | Formula 2 `M₀` sanity check | NarrativeCap count-based 확정 | `CODE_REVIEW` AC 타입 정의

---

## Overview

Time & Session System은 Moss Baby의 21일 실시간 경험을 지탱하는 **시간 인프라 계층**이다. 두 개의 독립 시계 — wall clock (`FDateTime::UtcNow()`) + monotonic counter (`FPlatformTime::Seconds()`) — 를 결합해 세션 간 경과를 측정하고, 시스템 시간 조작·절전/재개·NTP 동기화·DST 전환·초장기 미실행 상황을 **8개 분류**(4 between-session + 4 in-session)로 변환해 Game State Machine이 소비할 수 있는 **6개 Action 신호**로 내보낸다.

플레이어는 이 시스템을 직접 마주하지 않는다. 그러나 이 계층 없이는 Tamagotchi 모델의 "앱 꺼둔 사이에 자연스럽게 시간이 흘렀음" 감각이 성립하지 않는다. Anti-pillar Quiet Presence를 지키기 위해 애매한 시간 이벤트는 **조용히 흡수**된다 — 경고·"시간 이상" 알림 금지, UI 차단 프롬프트 금지, 시스템 내러티브 라인은 플레이스루 전체에서 **최대 `NarrativeCapPerPlaythrough` (기본 3회)**로 강제 제한.

---

## Player Fantasy

이 게임에서 가장 조용히 작동하는 약속은 이것입니다: **당신이 없었던 시간을, 게임은 당신 탓으로 돌리지 않는다.** 반나절이든 한 달이든, 모호한 모든 순간은 플레이어에게 유리한 방향으로 해석되며, "놓친 보상"도 "연속 출석"도 존재하지 않습니다. 다만 이끼 아이가 그 공백을 혼자 통과해 왔다는 사실은 — 아주 가끔, 조건이 맞을 때만 — 짧은 **시스템 내러티브 한 줄** `{{시스템 내러티브 — 정확한 문구는 Writer 위임}}`로 인정됩니다. 이는 Dream Trigger(#9)의 꿈 텍스트와는 별개의 시스템 피드백 보이스이며, 플레이스루 전체에서 **최대 3회**로 엄격히 제한됩니다 (희소성 보호 — Pillar 3). 플레이어가 의식적으로 알아차리지 못하더라도, 이 설계는 21일 내내 피부로 느끼는 안전감을 뒷받침합니다.

**Pillar 연결**:
- **Pillar 1 (조용한 존재)** — 죄책감 없음, 알림 없음, 차단 없음. "의심하지 않는 시스템". 시스템 시각이 21일을 넘겨도 게임은 묻거나 강요하지 않고 조용히 작별로 이행
- **Pillar 3 (말할 때만 말한다)** — 긴 공백 뒤의 시스템 내러티브 한 줄은 `NarrativeCapPerPlaythrough`가 강제하는 희소성. 꿈 텍스트(#9)의 영역을 침범하지 않음
- **Pillar 4 (끝이 있다)** — 21일은 플레이어가 출석한 날수가 아니라 이끼 아이가 살아낸 시간. WallDelta가 21일을 초과해도 끝은 조용히 도래

---

## Detailed Design

### Core Rules

Time & Session System의 모든 분류 판단은 앱 시작 시 1회(`ClassifyOnStartup`) 또는 세션 내 1Hz tick(`TickInSession`)에서만 실행된다. 게임플레이 루프(60fps)는 이 시스템을 직접 폴링하지 않는다.

**전제**: 이전 세션 레코드(`FSessionRecord`) 부재 → Rule 1. `CurrentSessionUuid != Prev.SessionUuid` → 앱 재시작 (Between-session, Rule 2–4). UUID 동일 → In-session (Rule 5–8).

> **Rule 평가 순서 (first match wins)**: 8개 규칙은 1→8 순서로 **순차 평가되며, 첫 매칭이 승자**가 된다. 이는 분류기 모호성을 제거하기 위한 명시 규칙이다. 예: Rule 6(SUSPEND_RESUME)과 Rule 8(IN_SESSION_DISCREPANCY)이 ΔW=2h/ΔM=0s 같은 케이스에서 동시 매칭되어 보일 수 있으나, Rule 6의 조건(`MonoΔ < 5s AND WallΔ > 60s`)이 먼저 평가되어 매칭되면 Rule 8은 평가되지 않는다. Rule 8은 Rule 5–7이 모두 미매칭일 때만 도달하는 catch-all.

**Between-Session 규칙**

1. **FIRST_RUN**: `Prev == nullptr` → `START_DAY_ONE`. `DayIndex = 1`. `NarrativeCount = 0`.
2. **BACKWARD_GAP**: `WallDelta < 0` → `HOLD_LAST_TIME`. `DayIndex`/`LastWallUtc`/`NarrativeCount` 모두 불변. **유일하게 시스템 시각을 거부하는 케이스** (세이브 데이터 보호).
3. **LONG_GAP**: `WallDelta > 21일` → `DayIndex`를 21로 clamp + `LONG_GAP_SILENT` 발행 + 즉시 `OnFarewellReached(EFarewellReason::LongGapAutoFarewell)` 트리거 + 상태 `FarewellExit` 전환. **플레이어 차단·선택 강요 없음** (Pillar 1). 21일 아크는 조용히 완성된 것으로 간주 (Pillar 4). UI 모달·프롬프트 미생성. 이 분류는 idempotent — 이후 `ClassifyOnStartup()` 재호출 시 `FarewellExit` 유지, 추가 신호 발행 없음.
4. **ACCEPTED_GAP**: `0 ≤ WallDelta ≤ 21일` → `DayIndex`를 `WallDelta`만큼 전진 (Formula 4). `WallDelta > NarrativeThresholdHours` **AND** `Prev.NarrativeCount < NarrativeCapPerPlaythrough` → `ADVANCE_WITH_NARRATIVE` + `NarrativeCount` 증분 + 즉시 영속화. 그 외(threshold 이하 또는 cap 도달) → `ADVANCE_SILENT`. **Forward Tamper 명시적 수용**: 시스템 시각을 앞당겨도 정상 경과와 알고리즘적으로 구분 불가능 — 탐지를 시도하지 않는 것이 명시적 설계 결정 (Pillar 1, "의심하지 않는 시스템"). 코드 리뷰는 forward tamper 탐지 로직 추가 시도를 차단해야 함 (AC `NO_FORWARD_TAMPER_DETECTION`).

**In-Session 규칙 (1Hz tick)**

5. **NORMAL_TICK**: `|WallΔ − MonoΔ| ≤ 5초(DefaultEpsilonSec)` → `ADVANCE_SILENT`. 양 시계 정상 동기.
6. **SUSPEND_RESUME**: `MonoΔ < 5s AND WallΔ > 60s` → `ADVANCE_SILENT`. 시스템 슬립 재개, 벽시계 기준 전진.
7. **MINOR_SHIFT**: `5초 < |WallΔ − MonoΔ| ≤ 90분(InSessionToleranceMinutes)` → `ADVANCE_SILENT`. NTP 재동기화·DST 전환 흡수.
8. **IN_SESSION_DISCREPANCY**: `|WallΔ − MonoΔ| > 90분` → `LOG_ONLY`. 진단 로그만, 진행 변경 없음.

> **구현 참고 (UE 5.6)**: NORMAL_TICK과 MINOR_SHIFT는 둘 다 `ADVANCE_SILENT`. 분기 병합 가능하나 진단 로그 품질을 위해 Enum 값은 분리 유지.
>
> **플랫폼 주의 (MEDIUM 리스크)**: Windows에서 `FPlatformTime::Seconds()`(= `QueryPerformanceCounter`)가 suspend 중 멈추는지는 드라이버 의존. `SUSPEND_RESUME` vs `ACCEPTED_GAP` 오분류 시 **fallback은 `ACCEPTED_GAP + ADVANCE_SILENT`** — 언제나 플레이어에게 유리한 쪽. 구현 시 실기 테스트 필수.

### States and Transitions

Time & Session System 내부 런타임 상태 머신. Game State Machine(#5)의 게임플레이 상태와는 독립적.

| 내부 상태 | 설명 | 진입 트리거 | 유효 전환 |
|---|---|---|---|
| `Uninitialized` | 앱 기동 직후, 분류 미실행 | 프로세스 시작 | → `Classifying` |
| `Classifying` | `ClassifyOnStartup()` 실행 중 | 자동 전환 | → `Active` (정상 분류) / `FarewellExit` (LONG_GAP_SILENT 또는 이전 FarewellExit 복원) |
| `Active` | 세션 진행, 1Hz tick 구동 | `Classifying` 완료 (FIRST_RUN/ACCEPTED_GAP/BACKWARD_GAP) | → `Suspended` (OS sleep) / `SessionEnding` (카드 건넴) |
| `Suspended` | OS suspend. Tick 일시정지 (윈도우 focus 상실은 별개 — 아래 §Window Focus vs OS Sleep) | OS sleep 신호 (Windows: `WM_POWERBROADCAST` PBT_APMSUSPEND) | → `Active` (복귀 시 `ClassifyOnStartup()` 재실행) |
| `SessionEnding` | 카드 건넴, 저장 대기 | 카드 건넴 이벤트 수신 | → `Active` (저장 완료) / 롤백 (저장 실패) / `Suspended` (저장 중 OS suspend — 아래 참조) |
| `FarewellExit` | LONG_GAP_SILENT 또는 Day 21 카드 건넴 완료. 시스템 비활성화 | `Classifying`에서 LONG_GAP_SILENT 발행 또는 `DayIndex == 21` + 건넴 완료 | (최종 — 재진입 불가) |

**전환 세부 규칙**

- `Suspended → Active` 전환 시 `ClassifyOnStartup()` 재실행 (슬립 후 기록 갱신).
- `FarewellExit`은 최종 + 멱등. 앱을 다시 열어 `Classifying`에 진입해도 이전 `FarewellExit`을 감지하면 즉시 `FarewellExit` 유지 (추가 신호 발행 없음). 새 플레이스루는 `FSessionRecord` 초기화(예: 메뉴 "다시 시작") 후 `Uninitialized`부터.
- `SessionEnding` 저장 실패(Save Integrity 오류) 시 `Active` 롤백, 조용히 재시도 + 진단 로그.
- `SessionEnding` 중 OS suspend 발생 시: Save/Load의 atomic write 보장에 의존. (a) 쓰기 미완료 — 부분 기록 없음, 직전 정상 레코드 유지. OS 복귀 시 `Suspended → Active` 복구 후 재시도. (b) 쓰기 완료 직후 suspend — 정상 커밋 보존. `NarrativeCount` mid-write 보호: `UseNarrative` 분기 판정과 `SaveAsync` 영속화는 동일 호출 내 순차 실행이며, 중간 suspend는 Save/Load의 atomic rename이 커버 (§E6 참조).

**Window Focus vs OS Sleep 구분 (분류기 전제)**

두 이벤트는 다른 OS API 신호로 구분 가능하며, **다른 상태로 매핑된다**:

| 이벤트 | OS 신호 (Windows) | 내부 상태 | 1Hz Tick | 분류 결과 |
|---|---|---|---|---|
| Window focus loss (다른 창 클릭, Alt-Tab) | `WM_ACTIVATE` LOSS | `Active` 유지 | **계속 구동** (`FTSTicker` pause-independent) | NORMAL_TICK 정상 동작 |
| OS sleep / 노트북 덮개 닫음 | `WM_POWERBROADCAST` PBT_APMSUSPEND | `Active → Suspended` | 일시정지 | 복귀 시 SUSPEND_RESUME |

Focus 이벤트로 `Suspended` 진입 금지 — UI 위에 다른 창이 떠 있는 동안에도 시간은 계속 흐르고 ticks가 누적되어야 한다 (Pillar 1: 알림 없이 흘러가는 일상).

> **구현 참고 (UE 5.6)**: `UGameInstanceSubsystem` 기반 (`UMossTimeSessionSubsystem`). `GameInstance` 직접 수정 금지 — 별도 클래스. **수명 분리**:
> - `Initialize(FSubsystemCollectionBase&)`: `IMossClockSource` 주입 + `FSessionRecord` 로드만. 이 시점에는 `GetWorld()`가 null — timer 등록·Action 발행 금지.
> - **World 준비 감지**: `FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &OnWorldReady)` 구독. `OnWorldReady()`에서 `ClassifyOnStartup()` 실행 + Action 신호 발행. **`OnWorldBeginPlay()` 사용 불가** — `UGameInstanceSubsystem`에 없는 메서드 (`UWorldSubsystem` 전용).
> - **1Hz tick 등록**: `FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &TickInSession), 1.0f)`. `FTSTicker`는 engine-level ticker로 **게임 pause 상태와 무관하게 구동** — 윈도우 focus 상실·게임 일시정지 중에도 monotonic·wall 시계 평가 계속. `FTimerManager::SetTimer`는 World 귀속 + pause 시 정지 가능 → 사용 금지.
> - `Deinitialize()`: `FTSTicker` handle 해제 + `FWorldDelegates` 구독 해제.
> - **`PostInitialize` 사용 금지** — lifecycle 모호.
> - **단일 Persistent Level 전제**; 레벨 전환 시 `OnWorldReady` 재호출로 재분류.

### Interactions with Other Systems

#### 1. Save/Load Persistence (#3) — 양방향

| 방향 | 데이터 | 소유 | 시점 |
|---|---|---|---|
| Time → Save | `FSessionRecord { SessionUuid, LastWallUtc, LastMonotonicSec, DayIndex, NarrativeCount, LastSaveReason }` | Time 생산 / Save 저장 | 카드 건넴·꿈 수신·설정 변경·내러티브 발화. `SessionEnding`에서 atomic write |
| Save → Time | 이전 `FSessionRecord` | Save 소유 | 앱 시작(`Initialize()`)에서 1회 읽기 |

- `SaveSchemaVersion` 마이그레이션은 Save/Load 책임. Time은 버전 필드를 직접 참조하지 않음.
- `DayIndex` 범위: `int32`, `1..21` (MVP 7일 아크에서도 필드 자체는 21일 수용).
- `NarrativeCount` 범위: `int32`, `[0, NarrativeCapPerPlaythrough]`. `ADVANCE_WITH_NARRATIVE` 발행 시마다 +1 후 즉시 영속화 (cap이 새지 않도록 분기와 영속화는 atomic으로 묶일 것).
- `LastSaveReason` 마지막 쓰기의 durability는 **Save/Load GDD의 contract** — Time은 발행만 책임. Save/Load GDD §5.6에 정의된 `ESaveReason` enum 4-value set (`ECardOffered=0`, `EDreamReceived=1`, `ENarrativeEmitted=2`, `EFarewellReached=3`)이 canonical 정의이며 ordinal은 stable (Save/Load GDD APPROVED 2026-04-17).

> **구현 참고**: `FSessionRecord`는 `USTRUCT()` + 모든 필드 `UPROPERTY()`. `LastMonotonicSec`은 반드시 **`double`** (`float` 금지 — 21일 = 1.8M 초 범위에서 `float`은 0.25초 단위 오차 발생; AC `SESSION_RECORD_DOUBLE_TYPE_DECLARATION` + `SESSION_RECORD_DOUBLE_PRECISION_RUNTIME`로 검증). `FGuid` / `FDateTime` / `FString` 기본 직렬화 지원. `UPROPERTY()` 누락 시 조용히 기본값으로 롤백되는 버그 위험.

#### 2. Game State Machine (#5) — 아웃바운드 전용

Time은 분류 결과를 Action 신호로 변환해 GSM에 발행. GSM은 신호 소비해 게임플레이 상태 전환.

| Action | GSM 반응 |
|---|---|
| `START_DAY_ONE` | Dawn 상태 초기화, 온보딩 시퀀스 |
| `ADVANCE_SILENT` | 현재 상태 유지, `DayIndex` 반영 |
| `ADVANCE_WITH_NARRATIVE` | Waiting 상태에서 **시스템 내러티브** 1회성 연출 `{{문구 — Writer 위임}}` (Stillness Budget 이벤트 슬롯 **배타 소비** — 동일 프레임에 Stillness Budget 다른 이벤트와 동시 발화 금지). `NarrativeCapPerPlaythrough`로 한도 강제. **Dream Trigger(#9)와 무관한 시스템 피드백 보이스** — 라벨 혼동 금지 |
| `HOLD_LAST_TIME` | 이전 상태 유지, `DayIndex` 불변 |
| `LONG_GAP_SILENT` | GSM은 즉시 기존 `Farewell` 상태로 전환. UI 모달·차단·선택 프롬프트 미생성 (Pillar 1). 플레이어 복귀 시 Farewell 연출만 재생 |
| `LOG_ONLY` | GSM에 전파 없음 |

- `LONG_GAP_SILENT`은 GSM에 **신규 상태를 요구하지 않음**. 기존 `Farewell` 상태로 전환만 하면 됨. (R1의 "GSM에 LongGapChoice 상태 신설" 제안은 Pillar 1 위반으로 R2에서 철회됨; systems-index R2의 해당 항목도 함께 철회.)
- Time → GSM 단방향. GSM의 현재 상태를 Time이 읽지 않음.

> **구현 참고**: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeActionResolved, ETimeAction, Action)` + `UPROPERTY(BlueprintAssignable)`. Blueprint 프로토타이핑 지원 + C++ `AddDynamic` 구독. 1Hz 빈도에서 dynamic delegate 오버헤드 무시 가능.

#### 3. Card System (#8) — 인바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Time 제공 | `DayIndex (int32, 1..21)` | Card 덱 구성 시 폴링 |
| Time 이벤트 | `OnDayAdvanced(int32 NewDayIndex)` | `ADVANCE_SILENT`/`ADVANCE_WITH_NARRATIVE` 완료 직후 |

- Card는 `DayIndex`로 시즌 가중치 계산 (1–7 / 8–14 / 15–21 구간).
- `OnDayAdvanced` 발행 시 Card가 새 3장 덱 재생성.
- Card는 Time 내부 상태(`Active`, `Suspended` 등)를 알지 못함.

#### 4. Dream Trigger (#9) — 인바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Time 제공 | `DayIndex`, `SessionCountToday (int32)` | 희소성 평가 시 폴링 |
| Time 이벤트 | `OnDayAdvanced` | 날짜 전진 시 재평가 트리거 |

- `SessionCountToday`: 오늘(`DayIndex` 기준) 앱 open/close 횟수. `FSessionRecord` 확장 필드 또는 런타임 누적. Dream이 "오늘 이미 꿈이 떴음" 판단에 사용.
- Dream은 Time에 역방향 쓰기 없음.

#### 5. Farewell Archive (#17) — 아웃바운드

두 진입 경로 모두 Time이 트리거:

| 경로 | 신호 |
|---|---|
| 정상 완주 | `OnFarewellReached(EFarewellReason::NormalCompletion)` — `DayIndex == 21` + 카드 건넴 완료 |
| LongGap 자동 완주 | `OnFarewellReached(EFarewellReason::LongGapAutoFarewell)` — `WallDelta > 21일`로 인한 `LONG_GAP_SILENT` 분류 결과. 플레이어 선택 없음, `DayIndex`를 21로 clamp 후 `Classifying`에서 즉시 발행 |

- Time은 두 `EFarewellReason` enum 값을 정의하고 발행만 한다. **연출·BGM·메시지·감정 톤 차등은 Farewell Archive GDD(#17)의 책임** — 이 GDD는 enum 의미만 규정. 네이밍(`LongGapAutoFarewell`)은 기술 의미(자동 작별)만 표현하며, 톤 차등(silent vs warm vs neutral)은 Farewell Archive GDD가 완전 결정 (OQ-4 RESOLVED).
- Farewell의 세이브 데이터 읽기는 Save/Load 경유 (Time 우회 금지).
- 멱등성: 두 발행 모두 `FarewellExit` 진입 후 idempotent — 재실행 시 추가 발행 없음 (E7 참조).

---

## Formulas

### Formula 1 — Wall Clock Delta

두 세션 간 벽시계 경과 시간.

`WallDelta = CurrentWallUtc − Prev.LastWallUtc`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| CurrentWallUtc | W₁ | `FDateTime` (UTC) | 현재 | 분류 시점의 `FDateTime::UtcNow()` |
| Prev.LastWallUtc | W₀ | `FDateTime` (UTC) | 이전 세션 기록 | 이전 `FSessionRecord.LastWallUtc` |
| WallDelta | ΔW | `FTimespan` | −∞ ~ +∞ | 부호 있음. 음수는 BACKWARD_GAP 시그널 |

**Output Range**: 정상 플레이 `[0, 21일]`. 극단: Backward tamper 음수, 초장기 미실행 `>21일`.
**Example**: `W₁ = 2026-04-16 20:00 UTC, W₀ = 2026-04-16 08:00 UTC → ΔW = 12h`

### Formula 2 — Monotonic Delta

Monotonic counter 경과 시간 (세션 간 리셋 고려).

`MonoDelta = max(0, CurrentMonotonicSec − Prev.LastMonotonicSec)`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| CurrentMonotonicSec | M₁ | `double` | `[0, 1.8×10⁹)` (시스템 부팅 경과초) | `FPlatformTime::Seconds()` 현재 값 |
| Prev.LastMonotonicSec | M₀ | `double` | 이전 기록 | `FSessionRecord.LastMonotonicSec` |
| MonoDelta | ΔM | `FTimespan` | `[0, +∞)` | 음수는 0으로 클램프 (앱 재시작/부팅 리셋 케이스) |

**Output Range**: 세션 내 NORMAL_TICK은 초 단위, suspend 시 0에 가까움, 세션 간 0.
**Precision**: `float`(32-bit)은 1.8M 초 범위에서 0.25초 오차 → **반드시 `double`**.
**Example**: `M₁ = 720.5s, M₀ = 600.0s → ΔM = 120.5s`; 앱 재시작 후 `M₁ = 0.8s, M₀ = 600.0s → ΔM = 0` (클램프).
**M₀ Sanity Check**: `M₀ < 0` 또는 `M₀ > 1.0×10⁹` (부팅 후 ~31.7년, 현실 상한 초과) 시 손상된 세이브 데이터로 간주. `M₀ = 0`으로 리셋 + 진단 로그 기록 → `MonoDelta = M₁` (사실상 첫 세션 취급). 이 보호는 `BACKWARD_GAP + HOLD_LAST_TIME`과 독립적 — MonoDelta 단독 보정이며 WallDelta 기반 DayIndex 계산에는 영향 없음.

### Formula 3 — Clock Discrepancy

벽시계와 monotonic 시계의 차이. 이상 감지 핵심 신호.

`Discrepancy = WallDelta − MonoDelta`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| WallDelta | ΔW | `FTimespan` | Formula 1 | |
| MonoDelta | ΔM | `FTimespan` | Formula 2 | |
| Discrepancy | D | `FTimespan` (부호 있음) | −∞ ~ +∞ | 양수: 벽시계 앞섬 (슬립·NTP 전방·DST+). 음수: 벽시계 뒤처짐 (NTP 후방). |

**Output Range**: 정상 세션 `[−5s, +5s]`. Suspend `[+60s, +수시간]`. DST `±1h`. NTP 후방 `−수초`. 그 외는 이상.
**Example**: 세션 10분 중 NTP +15초 재동기화 → `ΔW = 615s, ΔM = 600s, D = 15s`.

### Formula 4 — Day Index Advancement (ACCEPTED_GAP)

ACCEPTED_GAP 처리 시 `DayIndex` 갱신.

`NewDayIndex = clamp(int32(Prev.DayIndex) + int32(floor(WallDelta / 24h)), 1, 21)`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| Prev.DayIndex | D₀ | `int32` | `[1, 21]` | 이전 일차 |
| WallDelta | ΔW | `FTimespan` | `[0, 21일]` | Formula 1 (ACCEPTED_GAP 범위 — Rule 4 진입 시점에서 보장) |
| floor(ΔW/24h) | k | `int32` (intermediate) | `[0, 21]` | clamp 전 정수 변환. ACCEPTED_GAP 범위에서 최대 21 |
| NewDayIndex | D₁ | `int32` | `[1, 21]` | 갱신된 일차 |

**Output Range**: 항상 `[1, 21]` (clamp). `D₀ + k > 21` 시 상한 21에서 멈춤 (정상 완주 → 다음 카드 건넴이 Farewell 트리거).
**Overflow Safety**: ACCEPTED_GAP은 Rule 4에서 `0 ≤ ΔW ≤ 21일`을 전제. intermediate 합 최대 `21 + 21 = 42`로 int32 안전. `ΔW > 21일` 케이스는 Rule 3(LONG_GAP_SILENT, first match wins)이 가로채므로 Formula 4에 도달하지 않음. `FTimespan::GetTotalDays()` 반환값을 직접 int32로 캐스팅하기 전 `floor` 명시.
**Example**: `D₀ = 3, ΔW = 48h 30min → k = 2, D₁ = 5` (floor(48.5/24) = 2).
**Edge**: `ΔW < 24h` → `D₁ = D₀` (같은 날 재접속). `ΔW == 24h 정각` → `D₁ = D₀ + 1`.

### Formula 5 — Narrative Threshold (with per-playthrough cap)

`ADVANCE_WITH_NARRATIVE` vs `ADVANCE_SILENT` 분기. **희소성 보호를 위해 per-playthrough cap 강제** (Pillar 3).

```
UseNarrative = (WallDelta > NarrativeThresholdHours)
             ∧ (between-session)
             ∧ (Prev.NarrativeCount < NarrativeCapPerPlaythrough)
```

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| WallDelta | ΔW | `FTimespan` | Formula 1 | |
| NarrativeThresholdHours | T | `FTimespan` (const) | 24h (튜닝 가능) | §7 Tuning Knob |
| Prev.NarrativeCount | N | `int32` | `[0, NarrativeCapPerPlaythrough]` | `FSessionRecord` 누적 카운터 |
| NarrativeCapPerPlaythrough | C | `int32` (const) | 3 (튜닝 가능; MVP 7일은 1; 0은 영구 침묵 옵션) | §7 Tuning Knob |
| UseNarrative | — | `bool` | T/F | GSM 연출 분기 플래그 |

**Side effect (분기와 분리되면 cap이 새므로 atomic 묶음)**: `UseNarrative == true`로 판정되는 즉시 `NarrativeCount`를 +1하고 `FSessionRecord`에 영속화한다. 다음 세션이 갱신된 cap을 정확히 인지해야 함.

**Output**:
- `true` → `ADVANCE_WITH_NARRATIVE` 발행 → GSM이 Waiting 상태에서 **시스템 내러티브** `{{문구 — Writer 위임}}` 1회성 연출 (Stillness Budget 이벤트 슬롯 배타 소비). **Dream Trigger와 무관**.
- `false` → `ADVANCE_SILENT` 발행. cap 도달 후의 모든 긴 공백은 조용히 흡수 (Pillar 3 강제).

**Examples**:
- `ΔW = 12h, T = 24h, N = 0, C = 3` → `false` (threshold 미충족) → `ADVANCE_SILENT`
- `ΔW = 48h, T = 24h, N = 0, C = 3` → `true` → `ADVANCE_WITH_NARRATIVE`, N → 1
- `ΔW = 48h, T = 24h, N = 3, C = 3` → `false` (cap 도달) → `ADVANCE_SILENT`
- `ΔW = 48h, T = 24h, N = 0, C = 0` → `false` (cap 0이면 영구 침묵)

### Formula Constants Summary

| Const | Value | Unit | Tuning Knob (§7) |
|---|---|---|---|
| `GameDurationDays` | 21 | days | ✓ |
| `DefaultEpsilonSec` | 5 | seconds | ✓ |
| `InSessionToleranceMinutes` | 90 | minutes | ✓ |
| `NarrativeThresholdHours` | 24 | hours | ✓ |
| `NarrativeCapPerPlaythrough` | 3 | count | ✓ (MVP 7일은 1로 자동 축소) |
| `SuspendMonotonicThresholdSec` | 5 | seconds | ✓ |
| `SuspendWallThresholdSec` | 60 | seconds | ✓ |

---

## Edge Cases

### 경계값 (Boundary)

- **E1 — `WallDelta`가 정확히 21일**: `ACCEPTED_GAP`. Formula 4 clamp로 `DayIndex = 21`. `NarrativeCount < cap`이면 `ADVANCE_WITH_NARRATIVE`, 아니면 `ADVANCE_SILENT`. LONG_GAP 아님. 근거: 21일은 정확한 게임 완주 기간 — 이후 카드 건넴 1회로 정상 Farewell.
- **E2 — `WallDelta`가 21일 + 1초**: `LONG_GAP_SILENT` 분류. `DayIndex`를 21로 clamp + `OnFarewellReached(LongGapAutoFarewell)` 즉시 발행 + `FarewellExit` 전환. 플레이어 차단·선택 프롬프트 없음 (Pillar 1). 경계 바로 초과 시점에 자동 작별.
- **E3 — 세이브 슬롯이 비어 있음 (FIRST_RUN)**: `START_DAY_ONE`. `DayIndex = 1`, `FSessionRecord` 신규 생성.
- **E-NARRATIVE — `WallDelta`가 정확히 24h**: Formula 5의 엄격 비교 `> 24h`에 의해 `false` → `ADVANCE_SILENT`. 24h + 1초부터 내러티브 발화. 모호함 제거.

### 일상적 흐름 (Normal flow)

- **E5 — 1분 간격 재실행**: `ACCEPTED_GAP + ADVANCE_SILENT` (24h 미만). 내러티브 없음.
- **E4 — 세션 중 5시간 벽시계 점프**: `IN_SESSION_DISCREPANCY + LOG_ONLY`. `DayIndex` 불변. 진단 로그만.

### Suspend / 시스템 환경

- **E9 — 시간대 변경 (해외여행 / DST 전환)**: `FDateTime::UtcNow()`가 UTC 기반이라 로컬 시간대 변화는 영향 없음. DST 전환만 순간 시각에 반영 → `MINOR_SHIFT` 흡수. 추가 처리 불필요.
- **E14 — Windows 하이브리드 슬립 / 빠른 시작 (Fast Startup)**: monotonic이 sleep 중 멈추면 `SUSPEND_RESUME`, 프로세스 종료됐으면 새 `SessionUuid` → `ACCEPTED_GAP`. 둘 다 `ADVANCE_SILENT`로 수렴. **실기 테스트 필수** (MEDIUM 리스크).

### Crash / Corruption

- **E6 — `SessionEnding` 상태에서 강제 종료 (OS kill / 전원 차단)**: atomic write 미완료 시 부분 기록 없음. 직전 정상 레코드 유지 → 다음 실행에서 `ACCEPTED_GAP`으로 정상 복구. GSM은 `FSessionRecord.LastSaveReason`으로 "이 카드 건넴 이벤트 중복" 감지하여 중복 Farewell/Dream 방지. **`LastSaveReason` 마지막 쓰기의 durability는 Save/Load GDD의 contract** — Time은 발행만, atomic 보존은 Save/Load의 책임. 두 GDD가 합의한 reason enum 집합(예: `ECardOffered`, `EDreamReceived`, `ENarrativeEmitted`, `EFarewellReached`)을 Save/Load GDD §Edge Cases에 명시한다.
- **E11 — 세이브 스키마 버전 불일치 (앱 업그레이드 후 첫 실행)**: Save/Load의 `SaveSchemaVersion` 마이그레이션 책임. 마이그레이션이 `LastMonotonicSec = 0`을 채운 경우 Formula 2의 `max(0, ...)` 클램프가 MonoDelta를 0으로 보정 → 추가 코드 불필요. 테스트에서 명시 확인.
- **E13 — `DayIndex`가 어떤 경로로든 22 이상으로 저장됨 (구현 버그)**: `Classifying` 진입 전 sanity check (`DayIndex = clamp(Loaded, 1, 21) + log`)로 복구. Card System이 범위 외 값으로 크래시하지 않도록 방어.

### 반복 프롬프트 / 선택

- **E7 — LONG_GAP_SILENT 후 재실행 (멱등성)**: `FarewellExit` 도달 후 앱을 다시 열어도 `Classifying`은 즉시 `FarewellExit` 유지. 추가 `LONG_GAP_SILENT` 미발행, 추가 `OnFarewellReached` 미발행 — Farewell Archive가 이미 받은 신호를 재발행하면 중복 연출 위험. `Active` 미복귀. **새 플레이스루는 `FSessionRecord` 초기화(예: 메뉴 "다시 시작") 후 `Uninitialized`부터** 재시작. (R1의 `AwaitingPlayerChoice` 상태는 Blocker 1로 제거됨; 선택 미완료 강제 종료 시나리오 자체가 소멸.)
- **E10 — Backward Tamper 연속 2회**: 두 번째도 `BACKWARD_GAP + HOLD_LAST_TIME`. `LastWallUtc`를 절대 갱신하지 않는 것이 핵심 — 갱신 시 다음 정상 접속을 "정상 경과"로 오인. GDD 규칙이 이를 보장.

### 카운터 / 재진입

- **E8 — Day 21 완주 후 Farewell 미완료로 종료, 재접속**: `NewDayIndex = clamp(21 + …, 1, 21) = 21` 유지. `FarewellExit` 진입 조건(`DayIndex == 21` + 카드 건넴)은 건넴 이벤트가 없으면 미충족 → `Active` 상태 유지. 플레이어는 Day 21을 다시 경험 가능. **설계적 허용**.
- **E12 — `DayIndex = 20`에서 `WallDelta = 25h` (내러티브 + Day 21 도달 동시)**: `NewDayIndex = 21`. `NarrativeCount < cap`이면 `ADVANCE_WITH_NARRATIVE` (cap 증분 후 영속화), 아니면 `ADVANCE_SILENT`. 동일 프레임에서 Day 21 도달 + (조건부) 내러티브 발화 가능. **GSM이 이 조합을 처리해야 함** — 내러티브가 발화되면 그 직후 Day 21 Dawn 연출. 주의: `ADVANCE_WITH_NARRATIVE`는 Farewell 트리거가 아님 — Farewell은 Day 21 카드 건넴 후 별개로 발생.
- **E16 — NarrativeCount cap 도달 후 모든 ACCEPTED_GAP**: `NarrativeCount == NarrativeCapPerPlaythrough`인 모든 후속 ACCEPTED_GAP은 `WallDelta`가 24h를 초과해도 `ADVANCE_SILENT`로 분기. cap은 영구 (플레이스루 종료까지 리셋 없음). 주말 플레이어가 21일 동안 10–11회의 긴 공백을 경험하더라도 시스템 내러티브는 정확히 cap 회수만 발화 — 이 침묵은 의도된 희소성 보호 (Pillar 3).
- **E15 — 하루 100회 이상 open/close**: `WallDelta < 24h` 구간에서 DayIndex 변화 없음. `SessionCountToday`는 Dream Trigger가 "오늘 꿈 이미 발화" 판단에만 사용 → 100회도 기능적 문제 없음. **Dream Trigger GDD에서 `SessionCountToday` 리셋 기준(DayIndex 변화 vs UTC 날짜 변화) 명시 필요**.
- **E-MULTI — 동일 머신에서 앱 2개 동시 실행 (가능성 낮음)**: 각각 다른 `SessionUuid`로 같은 세이브 슬롯에 쓰려고 시도. Save/Load의 atomic write + 이중 슬롯 A/B ping-pong이 마지막 쓰기를 승자로 만듦. **플레이어 단일 인스턴스 강제는 Save Integrity 책임, Time은 관여 안 함**.

---

## Dependencies

### Upstream Dependencies (이 시스템이 의존하는 대상)

| 의존 대상 | 의존 강도 | 인터페이스 | 비고 |
|---|---|---|---|
| **`IMossClockSource` interface** | **Hard — clock injection seam** | 순수 C++ 추상 클래스 (UObject 미상속). 4 virtual methods — 아래 참조 | **프로젝트 소유 인터페이스** (OQ-6 RESOLVED). 21+개 AC 중 17개가 이 인터페이스 없이 실행 불가 — 시스템 의존성. 프로덕션 `FRealClockSource`는 `FDateTime::UtcNow()` + `FPlatformTime::Seconds()` 래핑 + `FCoreDelegates::ApplicationWillEnterBackgroundDelegate`/`…ForegroundDelegate` 구독으로 suspend/resume 캡처. 테스트 `FMockClockSource`는 시각 조작 + `SimulateSuspend()`/`SimulateResume()` 직접 호출. `Initialize()`에서 주입. UPROPERTY 참조 없으므로 `TObjectPtr` 불필요 |

**`IMossClockSource` 시그니처** (OQ-6 결정):

```cpp
class IMossClockSource
{
public:
    virtual ~IMossClockSource() = default;
    virtual FDateTime GetUtcNow() const = 0;
    virtual double GetMonotonicSec() const = 0;
    virtual void OnPlatformSuspend() {}   // default no-op; FRealClockSource overrides
    virtual void OnPlatformResume() {}    // default no-op
};
```

- `OnPlatformSuspend()`/`OnPlatformResume()`: `FRealClockSource`가 OS suspend/resume 시점 기록 → Subsystem에 전파. `FMockClockSource`는 이 메서드를 직접 호출해 suspend 시뮬레이션 가능 → SUSPEND_RESUME AC 자동화 경로 확보
| UE 엔진 API | Hard — engine | `FDateTime`, `FPlatformTime::Seconds`, `UGameInstanceSubsystem`, `FTimerManager`, `FGuid`, `UDeveloperSettings`(Tuning Knob 노출용) | 표준 UE 5.6 API |

Foundation Layer 시스템 간 의존은 없음 (Time, Data Pipeline, Save/Load, Input Abstraction은 병렬 설계 가능).

### Downstream Dependents (이 시스템에 의존하는 시스템)

| 시스템 | 의존 강도 | 인터페이스 | 데이터 방향 |
|---|---|---|---|
| **Save/Load Persistence (#3)** | Hard — bidirectional | `FSessionRecord` 구조체 직렬화 | Time → Save (쓰기), Save → Time (1회 읽기) |
| **Game State Machine (#5)** | Hard — inbound | `FOnTimeActionResolved` dynamic delegate + `ETimeAction` enum | Time → GSM (단방향) |
| **Card System (#8)** | Hard — inbound | `DayIndex` 폴링 + `OnDayAdvanced(int32)` delegate | Time → Card (단방향) |
| **Dream Trigger System (#9)** | Hard — inbound | `DayIndex` + `SessionCountToday` 폴링 + `OnDayAdvanced` delegate | Time → Dream (단방향) |
| **Farewell Archive (#17)** | Hard — inbound | `OnFarewellReached(EFarewellReason)` delegate | Time → Farewell (단방향) |
| **Growth Accumulation Engine (#7)** | Soft — 간접 | Day 경계 이벤트를 Card System 경유 간접 수신 | 직접 인터페이스 없음 |

### Bidirectional Consistency Notes

양방향 의존성 원칙(design-docs 규칙)에 따라 각 downstream GDD에 다음 참조가 명시되어야 함:

- **Save/Load GDD**: "Time & Session System (#1)의 `FSessionRecord`를 직렬화·역직렬화. `SaveSchemaVersion` 마이그레이션의 한 경로로 `FSessionRecord` 변경 이력 관리."
- **Game State Machine GDD**: "Time & Session System (#1)의 `FOnTimeActionResolved` 델리게이트 구독. `ETimeAction` 신호 소비. `LONG_GAP_SILENT`에 대응하여 기존 `Farewell` 상태로 전환."
- **Card System GDD**: "Time & Session System (#1)의 `DayIndex`를 시즌 가중치 계산 입력으로 사용. `OnDayAdvanced`에서 3장 덱 재생성."
- **Dream Trigger GDD**: "Time & Session System (#1)의 `DayIndex`·`SessionCountToday`를 희소성 평가 입력으로 사용. `SessionCountToday` 리셋 기준 명시 (E15 참조)."
- **Farewell Archive GDD**: "Time & Session System (#1)의 `OnFarewellReached(EFarewellReason)` 델리게이트로 트리거. `NormalCompletion`과 `LongGapAutoFarewell` 두 진입 경로에 따른 연출·톤 차등 결정."

### Data Ownership

| 데이터/타입 | 소유 시스템 | 읽기 허용 | 쓰기 허용 |
|---|---|---|---|
| `FSessionRecord` | Time & Session | Save/Load (직렬화) | Time & Session (유일) |
| `DayIndex` | Time & Session | Card, Dream, Farewell, GSM | Time & Session (유일) |
| `SessionUuid` | Time & Session | Save/Load | Time & Session (유일) |
| `ETimeAction` enum | Time & Session (정의) | 모든 구독자 | — |
| `EFarewellReason` enum | Time & Session (정의) | Farewell Archive | — |
| `SaveSchemaVersion` | Save/Load | Time & Session (참조 금지) | Save/Load (유일) |

---

## Tuning Knobs

모든 상수는 `UMossTimeSessionConfig : UDataAsset`에 노출되어 엔진 재빌드 없이 조정 가능해야 함.

| 노브 | 기본값 | 안전 범위 | 너무 높으면 | 너무 낮으면 | 참조 |
|---|---|---|---|---|---|
| `GameDurationDays` | 21 | `[7, 60]` | 플레이어 이탈률↑, 콘텐츠 압박↑ | 감정 아크 부족, Pillar 4 약화 | Formula 1, 4 |
| `DefaultEpsilonSec` | 5초 | `[1s, 30s]` | 실제 이상 탐지 못함 | 정상 시계 흔들림을 MINOR_SHIFT로 오분류, 로그 노이즈 | Rule 5, Formula 3 |
| `InSessionToleranceMinutes` | 90분 | `[30min, 4h]` | 2시간짜리 tampering도 흡수됨 | DST·NTP 흡수 실패, IN_SESSION_DISCREPANCY 오탐 | Rule 7 |
| `NarrativeThresholdHours` | 24h | `[6h, 72h]` | 긴 공백에도 조용, Pillar 4 힌트 약함 | 시스템 내러티브 남발, 희소성 상실 (Pillar 3 위반) | Formula 5, Rule 4 |
| `NarrativeCapPerPlaythrough` | 3 | `[0, 10]` | 주말 플레이어 10–11회 발화 → Pillar 3 위반, 시스템 보이스가 Dream과 혼동됨 | 영구 침묵, 긴 공백의 인정 부재 (Pillar 4 약화); cap=0은 의도적 Stoic Mode 옵션 | Formula 5, Rule 4 |
| `SuspendMonotonicThresholdSec` | 5초 | `[1s, 30s]` | 짧은 렌더 스톨까지 suspend로 오분류 | 진짜 suspend를 IN_SESSION_DISCREPANCY로 오분류 | Rule 6 |
| `SuspendWallThresholdSec` | 60초 | `[30s, 5min]` | 1분짜리 백그라운드 지연을 suspend로 오분류 | 짧은 절전(수분)을 놓침 | Rule 6 |

### 노브 상호작용

- **`DefaultEpsilonSec` ↔ `InSessionToleranceMinutes`**: epsilon이 tolerance보다 클 수 없음. 항상 `epsilon < tolerance`. 튜닝 시 상한 제약.
- **`GameDurationDays` ↔ `NarrativeThresholdHours`**: 게임 기간이 짧아지면(예: MVP 7일) 내러티브 임계값도 비례적으로 낮춰야 함. **MVP는 `NarrativeThresholdHours = 8h` 권장**.
- **`GameDurationDays` ↔ `NarrativeCapPerPlaythrough`**: 짧은 아크에서는 cap도 비례 축소 — **MVP 7일은 `cap = 1` 권장** (21일의 1/3 비율). cap = 0은 영구 침묵 (Stoic Mode A/B 테스트 옵션).
- **NarrativeCap 방식 확정 (R3)**: **count-based** (플레이스루 전체 누적 횟수). distance-based cooldown (마지막 발화 이후 최소 N일 간격) 대비 장점: 구현 단순, 세이브 데이터 필드 1개(`NarrativeCount`), cap 도달 후 동작 명확. 단점: 3회가 플레이스루 초반에 집중 소비될 수 있음 — 그러나 `NarrativeThresholdHours = 24h` 조건이 최소 24h 간격을 자연스럽게 보장하므로 실질적 distance-based 효과도 겸함.
- **`SuspendMonotonicThresholdSec` + `SuspendWallThresholdSec`**: 둘 다 조정해야 SUSPEND_RESUME 판정 정확도가 유지됨. 개별 조정 시 오분류 증가.

### Playtest Tuning Priorities

- **1순위**: `NarrativeCapPerPlaythrough` — 희소성 보호의 구조적 핵심 (튜닝이 아닌 설계 강제). 첫 플레이테스트에서 3 → 2 또는 1로 조정 가능성. cap = 0 변종 A/B도 검토.
- **2순위**: `NarrativeThresholdHours` — 감정 톤에 직결. 초기 24h → 플레이테스트 후 조정.
- **3순위**: `GameDurationDays` — MVP 7일, VS부터 21일.
- **4순위**: `SuspendWallThresholdSec` — Windows 실기 검증 결과에 따라.
- **기타**: epsilon/tolerance는 튜닝 필요성 낮음 — 기본값으로 충분할 가능성 큼.

---

## Visual/Audio Requirements

이 시스템은 **인프라 레이어**로 직접적인 Visual/Audio 출력이 없다. 모든 시각·음향 영향은 Game State Machine이 중계하여 Visual State Director / Audio System이 처리한다.

**유일한 간접 연결**:
- `ADVANCE_WITH_NARRATIVE` 액션 시 → Game State Machine이 Waiting 상태에서 시스템 내러티브 `{{문구 — Writer 위임}}` 연출 트리거 (Stillness Budget 1회성 이벤트 슬롯 배타 소비)

실제 연출·사운드 명세는 Game State Machine, Visual State Director, Card Hand UI GDD에서 정의.

---

## UI Requirements

이 시스템은 UI를 **소유하지도, 발행하지도 않는다**. 모든 UI 트리거는 GSM의 표준 상태 전환(Dawn / Waiting / Farewell)에 의해 발생하며, Time은 Action 신호만 보낸다.

> **📌 R2 변경 (선행 리뷰 Blocker 1 반영)**: R1 명세에 있던 `LongGapChoice` 모달 UI 요구사항은 **Pillar 1 위반(차단·선택 강요)으로 제거**됨. `WallDelta > 21일` 케이스는 `LONG_GAP_SILENT` 액션이 즉시 `OnFarewellReached(EFarewellReason::LongGapAutoFarewell)`를 트리거하여 Farewell Archive(#17)의 표준 연출만 재생한다. 별도 UI 위젯·차단 프롬프트·선택 탭 모두 불필요. Stories는 Farewell Archive UI(#17)와 Card Hand UI(#12)만 참조한다.
>
> **📌 R3 설계 결정 추적**: Forward tamper 명시적 수용(§Player Fantasy, §Rule 4, AC `NO_FORWARD_TAMPER_DETECTION`)은 Architecture 단계에서 **ADR-0001** (또는 프로젝트 첫 ADR)로 정식 기록 예정. 이 ADR이 없는 상태에서 forward tamper 탐지 코드 추가는 코드 리뷰에서 차단.

---

## Acceptance Criteria

> 이 섹션은 QA 테스터가 GDD를 읽지 않고도 독립적으로 검증할 수 있도록 Given-When-Then 형식으로 작성됨. 각 criterion은 `UE Automation Framework` 헤드리스 실행 가능 여부로 분류됨.

### Criterion FIRST_RUN
- **GIVEN** `FSessionRecord`가 존재하지 않는다 (첫 설치 또는 세이브 슬롯 전체 삭제 후 첫 실행)
- **WHEN** `UMossTimeSessionSubsystem::ClassifyOnStartup(nullptr)` 호출
- **THEN** `ETimeAction::START_DAY_ONE` 발행; `FSessionRecord.DayIndex == 1`; 신규 `FGuid SessionUuid` 생성됨
- **Evidence**: `FOnTimeActionResolved` delegate에서 `START_DAY_ONE` 수신 확인; 저장된 `FSessionRecord.DayIndex` 값 == 1
- **Type**: AUTOMATED — **Source**: Rule 1, Formula 4, E3

### Criterion BACKWARD_GAP_REJECT
- **GIVEN** 이전 세션 레코드 `Prev.LastWallUtc = T₀`; `DayIndex = 5`
- **WHEN** 시스템 시각 조작으로 `CurrentWallUtc < T₀` (`WallDelta = -48h`), 새 `SessionUuid`로 `ClassifyOnStartup()`
- **THEN** `HOLD_LAST_TIME` 발행; `DayIndex = 5` 유지; `LastWallUtc` 갱신 없음
- **Evidence**: delegate `HOLD_LAST_TIME` 수신; 저장 후 `DayIndex == 5`; `LastWallUtc == T₀` 불변
- **Type**: AUTOMATED — **Source**: Rule 2 (유일하게 시각 거부), Formula 1

### Criterion BACKWARD_GAP_REPEATED
- **GIVEN** 이미 `BACKWARD_GAP + HOLD_LAST_TIME` 발생, `LastWallUtc = T₀` 유지
- **WHEN** 다시 `WallDelta < 0`으로 `ClassifyOnStartup()` 재호출
- **THEN** 두 번째도 `HOLD_LAST_TIME`; `DayIndex`·`LastWallUtc` 모두 불변
- **Evidence**: 두 실행 전후 `LastWallUtc` 동일 값 유지
- **Type**: AUTOMATED — **Source**: Rule 2, E10

### Criterion LONG_GAP_SILENT_BOUNDARY_OVER
- **GIVEN** `Prev.DayIndex = 5`; `Prev.LastWallUtc = T₀`
- **WHEN** `WallDelta = 21일 + 1초`로 `ClassifyOnStartup()`
- **THEN** `LONG_GAP_SILENT` 발행; `DayIndex == 21` (clamp); `OnFarewellReached(EFarewellReason::LongGapAutoFarewell)` 즉시 발행; 상태 `FarewellExit` 전환; UI 차단·선택 프롬프트 미발생; `AwaitingPlayerChoice` 상태 자체가 정의되지 않음
- **Evidence**: `FOnTimeActionResolved` delegate에서 `LONG_GAP_SILENT` 1회 수신; `FOnFarewellReached` delegate에서 `Reason == LongGapAutoFarewell` 1회 수신; 상태 머신 쿼리 `FarewellExit` 반환; `DayIndex == 21`
- **Type**: AUTOMATED — **Source**: Rule 3, Formula 1, E2

### Criterion LONG_GAP_BOUNDARY_EXACT
- **GIVEN** `Prev.DayIndex = 1`; `Prev.LastWallUtc = T₀`
- **WHEN** `WallDelta = 정확히 21일 (1,814,400초)`로 `ClassifyOnStartup()`
- **THEN** `ADVANCE_WITH_NARRATIVE` 발행 (`NarrativeCount < cap` 시) 또는 `ADVANCE_SILENT` (cap 도달 시); `DayIndex == 21` (Formula 4 clamp); `LONG_GAP_SILENT` 미발행 (정확히 21일은 LONG_GAP 아님)
- **Evidence**: delegate `ADVANCE_WITH_NARRATIVE` 수신; `DayIndex == 21`; LONG_GAP 분류 미발생
- **Type**: AUTOMATED — **Source**: Rule 3 vs 4 경계, Formula 4 clamp, E1

### Criterion ACCEPTED_GAP_SILENT
- **GIVEN** `Prev.DayIndex = 3`; `Prev.LastWallUtc = T₀`
- **WHEN** `WallDelta = 12h` (24h 이하, between-session)로 `ClassifyOnStartup()`
- **THEN** `ADVANCE_SILENT` 발행; `DayIndex == 3` 유지 (24h 미만); 내러티브 연출 미발생
- **Evidence**: delegate `ADVANCE_SILENT`; `DayIndex == 3`
- **Type**: AUTOMATED — **Source**: Rule 4, Formula 4 (floor(12/24)=0), Formula 5, E5

### Criterion ACCEPTED_GAP_NARRATIVE_THRESHOLD
- **GIVEN** `Prev.DayIndex = 3`; `Prev.LastWallUtc = T₀`; `Prev.NarrativeCount = 0`; `NarrativeCapPerPlaythrough = 3`
- **WHEN** `WallDelta = 24h + 1초`로 `ClassifyOnStartup()`
- **THEN** `ADVANCE_WITH_NARRATIVE` 발행; `DayIndex == 4`; `NarrativeCount == 1` (영속화)
- **Evidence**: delegate 수신; `DayIndex == 4`; 저장된 `FSessionRecord.NarrativeCount == 1`
- **Type**: AUTOMATED — **Source**: Rule 4, Formula 5 (엄격 `>` + cap clause), E-NARRATIVE

### Criterion NARRATIVE_THRESHOLD_EXACT_BOUNDARY
- **GIVEN** `Prev.DayIndex = 3`; `Prev.LastWallUtc = T₀`; `Prev.NarrativeCount = 0`
- **WHEN** `WallDelta = 정확히 24h`로 `ClassifyOnStartup()`
- **THEN** `ADVANCE_SILENT` 발행 (내러티브 미발생); `DayIndex == 4`; `NarrativeCount == 0` 불변
- **Evidence**: delegate `ADVANCE_SILENT`; `DayIndex == 4`; `NarrativeCount` 불변
- **Type**: AUTOMATED — **Source**: Formula 5 (24h 정각은 `false`), E-NARRATIVE

### Criterion NARRATIVE_CAP_ENFORCED
- **GIVEN** `Prev.DayIndex = 10`; `Prev.NarrativeCount = 3`; `NarrativeCapPerPlaythrough = 3` (cap 도달 상태)
- **WHEN** `WallDelta = 48h`로 `ClassifyOnStartup()` (24h 임계 초과)
- **THEN** `ADVANCE_SILENT` 발행 (cap 강제로 내러티브 차단); `DayIndex == 12`; `NarrativeCount == 3` 불변
- **Evidence**: delegate `ADVANCE_SILENT` 수신 (NOT `ADVANCE_WITH_NARRATIVE`); `NarrativeCount` 불변; 진단 로그에 "cap reached, narrative suppressed" 기록
- **Type**: AUTOMATED — **Source**: Formula 5 cap clause, Rule 4, E16, Pillar 3

### Criterion DAYINDEX_FORMULA_FLOOR
- **GIVEN** `Prev.DayIndex = 3`; `Prev.LastWallUtc = T₀`
- **WHEN** `WallDelta = 48h 30min`으로 `ClassifyOnStartup()`
- **THEN** `DayIndex == 5` (floor(48.5/24) = 2, 3+2=5)
- **Evidence**: `FSessionRecord.DayIndex == 5`
- **Type**: AUTOMATED — **Source**: Formula 4 (GDD 예시와 일치)

### Criterion DAYINDEX_CLAMP_UPPER
- **GIVEN** `Prev.DayIndex = 20`; `Prev.LastWallUtc = T₀`
- **WHEN** `WallDelta = 48h`로 `ClassifyOnStartup()` (20 + 2 = 22 → clamp)
- **THEN** `DayIndex == 21` (clamp 상한); `ADVANCE_WITH_NARRATIVE` 발행 (WallDelta > 24h)
- **Evidence**: `DayIndex == 21`; delegate 수신
- **Type**: AUTOMATED — **Source**: Formula 4 clamp 상한, E12

### Criterion MONOTONIC_DELTA_CLAMP_ZERO
- **GIVEN** `Prev.LastMonotonicSec = 600.0`; 동일 세션 UUID
- **WHEN** 앱 재시작 후 `CurrentMonotonicSec = 0.5` (부팅 리셋, M₁ < M₀)로 `ClassifyOnStartup()`
- **THEN** `MonoDelta = 0` (음수 클램프); 정상 분류 진행 (오분류 없음); 반환값에 크래시·예외·assertion failure 없음
- **Evidence**: `MonotonicDelta == 0` assertion; 분류 결과가 `HOLD_LAST_TIME` 또는 `ADVANCE_SILENT` 중 하나 (WallDelta에 따라 결정); 프로세스 정상 종료
- **Type**: AUTOMATED — **Source**: Formula 2 (`max(0, M₁ − M₀)`)

### Criterion NORMAL_TICK_IN_SESSION
- **GIVEN** 세션 Active; `Prev.LastWallUtc`, `Prev.LastMonotonicSec` 기록
- **WHEN** 1Hz tick에서 `WallDelta = 1초`; `MonoDelta = 1초` (`|Δ| ≤ 5s`)
- **THEN** `ADVANCE_SILENT` 발행; `DayIndex` 변화 없음; 불필요한 로그 없음
- **Evidence**: delegate `ADVANCE_SILENT`; 1Hz 구간에 다른 신호 없음
- **Type**: AUTOMATED — **Source**: Rule 5, Formula 3

### Criterion MINOR_SHIFT_NTP
- **GIVEN** 세션 Active; `Prev.LastMonotonicSec` (10분 경과)
- **WHEN** 1Hz tick에서 NTP 재동기화 `WallDelta = 10분 15초`; `MonoDelta = 10분` (`|Δ| = 15s`, 5s < 15s ≤ 90min)
- **THEN** `ADVANCE_SILENT` 발행; `DayIndex` 불변; 플레이어 대면 경고·알림·UI 미생성; 진단 로그에만 `MINOR_SHIFT` 기록
- **Evidence**: delegate `ADVANCE_SILENT` 수신; 진단 로그에 `MINOR_SHIFT` classification 기록; `FOnTimeActionResolved`에서 `ADVANCE_SILENT` 외 다른 신호 미발행
- **Type**: AUTOMATED — **Source**: Rule 7, test Scenario 5

### Criterion MINOR_SHIFT_DST
- **GIVEN** 세션 Active; 30분 경과
- **WHEN** DST spring-forward `WallDelta = 1h 30min`; `MonoDelta = 30min` (`|Δ| = 1h` ≤ 90min)
- **THEN** `ADVANCE_SILENT` 발행; 플레이어 대면 경고·이상 알림·UI 미생성; 진단 로그에만 `MINOR_SHIFT` 기록
- **Evidence**: delegate `ADVANCE_SILENT` 수신; 진단 로그에 `MINOR_SHIFT` 기록; 플레이어 대면 시각 요소(팝업·배너·사운드) 없음
- **Type**: AUTOMATED — **Source**: Rule 7, E9, test Scenario 6

### Criterion IN_SESSION_DISCREPANCY_LOG_ONLY
- **GIVEN** 세션 Active; 2분 경과
- **WHEN** 1Hz tick에서 `WallDelta = 5h`; `MonoDelta = 2min` (`|Δ| = 4h 58min`, > 90min)
- **THEN** `LOG_ONLY` 발행; `DayIndex` 변경 없음; GSM에 전파 없음; 플레이어 알림 없음
- **Evidence**: delegate `LOG_ONLY`; GSM 상태 변화 없음; 진단 로그
- **Type**: AUTOMATED — **Source**: Rule 8, E4

### Criterion SUSPEND_RESUME_WALL_ADVANCE
- **GIVEN** 세션 Active; 동일 세션 UUID; 디바이스 = Windows 10 (S3 sleep) 또는 Windows 11 (S0ix Modern Standby) 실기
- **WHEN** 다음 절차로 OS sleep:
  1. 게임 실행 후 30초 안정화
  2. **Windows 10**: 시작 메뉴 > 절전 (S3) — 노트북: 덮개 닫기
  3. **Windows 11**: 시작 메뉴 > 절전 (S0ix Modern Standby — 디바이스 매니저 > 시스템 디바이스에서 활성 확인)
  4. 3시간 대기 (또는 BIOS RTC alarm으로 가속)
  5. 깨움 (전원 버튼 / 노트북 덮개 열기)
- **THEN** `ADVANCE_SILENT` 발행 (1회); 벽시계 기준 3시간 전진; 알림 미발생; `DayIndex` 변화 없음 (3h < 24h)
- **Evidence (체크리스트)**:
  - [ ] delegate 호출 1회 발생 확인 (logging)
  - [ ] `MonoDelta` 측정값 < `SuspendMonotonicThresholdSec` (5초)
  - [ ] `WallDelta` 측정값 ≈ 3시간 (±5분 허용)
  - [ ] `DayIndex` 변화 없음
  - [ ] 진단 로그에 `SUSPEND_RESUME` 분류 기록
- **Test Matrix (최소)**: Win10 S3 / Win11 S0ix / Win10 노트북 덮개 닫기 — 3가지 환경 각각 최소 1회. 결과 차이가 발견되면 OQ-1 갱신
- **Type**: **MANUAL** — **Source**: Rule 6, Formula 2 (MonoDelta ≈ 0 on suspend), E14, OQ-1

### Criterion FAREWELL_NORMAL_COMPLETION
- **GIVEN** `DayIndex == 21`; 세션 Active
- **WHEN** 카드 건넴 이벤트 수신 (`SessionEnding` → 저장 완료)
- **THEN** `OnFarewellReached(NormalCompletion)` 발행; 상태 `FarewellExit` 전환; 이후 `ClassifyOnStartup()` 재호출 시 `FarewellExit` 유지 (재진입 불가)
- **Evidence**: `OnFarewellReached` delegate `Reason == NormalCompletion` 수신; 다음 호출에 `Active` 미복귀
- **Type**: AUTOMATED — **Source**: States, Dep #17

### Criterion FAREWELL_LONG_GAP_SILENT
- **GIVEN** `Prev.DayIndex = 5`; `Prev.LastWallUtc = T₀`
- **WHEN** `WallDelta = 30일`로 `ClassifyOnStartup()` (LONG_GAP, 21일 초과)
- **THEN** `OnFarewellReached(EFarewellReason::LongGapAutoFarewell)` 1회 발행; 상태 `FarewellExit`; `EFarewellReason::LongGapAutoFarewell` enum 값이 `EFarewellReason::NormalCompletion`과 별개로 정의되어 있음. **연출·BGM·메시지 톤 차등은 Farewell Archive GDD #17의 책임 — 이 AC는 enum 값 분기만 검증**
- **Evidence**: delegate `Reason == LongGapAutoFarewell` 수신; `EFarewellReason` enum 정의에 두 값 모두 존재; UI 차단 위젯 미생성
- **Type**: AUTOMATED — **Source**: Rule 3, States, Dep #17

### Criterion SESSION_RECORD_DOUBLE_TYPE_DECLARATION
- **GIVEN** `FSessionRecord` 구조체 정의 코드 (`Source/MadeByClaudeCode/` 하위)
- **WHEN** `LastMonotonicSec` 필드의 타입을 정적 분석
- **THEN** `double` 타입으로 선언되어 있음 (`float` 금지). `static_assert(std::is_same_v<decltype(FSessionRecord::LastMonotonicSec), double>, "monotonic field must be double")` 컴파일 통과
- **Evidence**: 소스 grep `double LastMonotonicSec` 매치 1건; `static_assert` 컴파일 성공; `float LastMonotonicSec` grep 매치 0건
- **Type**: CODE_REVIEW — **Source**: Formula 2 precision note, Dep #3

### Criterion SESSION_RECORD_DOUBLE_PRECISION_RUNTIME
- **GIVEN** `FSessionRecord` 인스턴스 (UE Automation Framework 테스트 내)
- **WHEN** `LastMonotonicSec`에 `1814400.123` (21일 + 0.123초) 대입 → `USaveGame` 기반 직렬화 → 역직렬화 round-trip
- **THEN** 역직렬화 후 값이 원본과 ≤ 0.001초 오차로 일치 (소수 3자리 정밀도 유지)
- **Evidence**: `TestEqual` 또는 `TestNearlyEqual(loaded.LastMonotonicSec, 1814400.123, 0.001)` (UE Automation `FAutomationTestBase` 매크로)
- **Type**: AUTOMATED — **Source**: Formula 2 precision note, Dep #3

### Criterion DAYINDEX_CORRUPTION_CLAMP
- **GIVEN** 세이브 파일에 `DayIndex = 25` (범위 외) 포함
- **WHEN** `ClassifyOnStartup()` 진입 전 sanity check 실행
- **THEN** `DayIndex = clamp(25, 1, 21) = 21`로 보정; 보정 로그 기록; 크래시 없음
- **Evidence**: 반환값 `DayIndex == 21`; 보정 로그; Card System에 범위 외 값 미전달
- **Type**: AUTOMATED — **Source**: E13, Dep #8

### Criterion LONG_GAP_SILENT_IDEMPOTENT
- **GIVEN** 이미 `LONG_GAP_SILENT`로 `FarewellExit` 도달 후 앱 종료; `FSessionRecord.LastSaveReason == "EFarewellReached"` (Save/Load GDD의 `ESaveReason::EFarewellReached` 직렬화 경유)
- **WHEN** 앱을 다시 열어 `ClassifyOnStartup()` 재호출 (Save/Load가 `FSessionRecord` 역직렬화 → Time에 전달)
- **THEN** `FarewellExit` 상태 즉시 유지; 추가 `LONG_GAP_SILENT` 미발행; 추가 `OnFarewellReached` 미발행 (멱등); `Active` 미복귀
- **Evidence**: 두 번째 실행에서 `LONG_GAP_SILENT` 신호 카운트 == 0; `OnFarewellReached` 신호 카운트 == 0; 상태 머신 쿼리 `FarewellExit`
- **Type**: **INTEGRATION** (Time + Save/Load cross-system round-trip) — **Source**: Rule 3, E7, States, Save/Load GDD §Dep #1

### Criterion NO_FORWARD_TAMPER_DETECTION
- **GIVEN** Time & Session System 소스 코드 전체 (`Source/MadeByClaudeCode/` 하위)
- **WHEN** 정적 분석 / 코드 리뷰로 forward tamper 탐지 시도 패턴을 검색 (예: `tamper`, `bIsForward`, `DetectClockSkew`, `if (WallDelta > expected_max)`, `IsSuspiciousAdvance` 등)
- **THEN** **이러한 패턴이 존재하지 않음**. Forward tamper(시스템 시각 앞당김)는 Pillar 1에 의해 명시적으로 수용되며, 탐지 로직 추가는 금지. 새 ADR이 forward tamper를 다루지 않는 한 코드에도 등장 금지
- **Evidence**: 소스 grep 0 매치 (`grep -r "tamper\|DetectClockSkew\|bIsForward" Source/`); 신규 ADR이 추가된다면 "forward tamper 명시적 수용" 결정이 ADR-0NNN으로 존재해야 함
- **Type**: CODE_REVIEW (negative AC) — **Source**: Rule 4, Player Fantasy, Recommended #2

### Criterion RULE_PRECEDENCE_FIRST_MATCH_WINS
- **GIVEN** `FMockClockSource` 설정: 세션 Active, `WallDelta = 2h`, `MonoDelta = 0s` (Rule 6 SUSPEND_RESUME **AND** Rule 8 IN_SESSION_DISCREPANCY 동시 매칭 가능 케이스)
- **WHEN** `TickInSession()` 1Hz tick 실행
- **THEN** Rule 6(`SUSPEND_RESUME`, `MonoΔ < 5s AND WallΔ > 60s`)이 먼저 매칭 → `ADVANCE_SILENT` 발행; Rule 8(`IN_SESSION_DISCREPANCY`, `LOG_ONLY`)은 **평가되지 않음**
- **Evidence**: delegate `ADVANCE_SILENT` 수신 (NOT `LOG_ONLY`); 진단 로그에 `SUSPEND_RESUME` classification 기록 (NOT `IN_SESSION_DISCREPANCY`)
- **Type**: AUTOMATED — **Source**: §Core Rules "first match wins" 전제, Rule 6 vs 8

### Criterion TICK_CONTINUES_WHEN_PAUSED
- **GIVEN** 세션 Active; 1Hz tick 구동 중; `FTSTicker` 기반 등록
- **WHEN** `UGameInstance::GetWorld()->SetGamePaused(true)` 호출 후 5초 대기
- **THEN** `TickInSession()` 호출 최소 4회 발생 (±1 허용); `FTSTicker`는 engine-level이므로 게임 pause에 영향 받지 않음
- **Evidence**: tick 호출 카운터 ≥ 4; `FTimerManager` 기반 타이머는 0회 호출 (대조군)
- **Type**: AUTOMATED — **Source**: §Window Focus vs OS Sleep, UE 5.6 구현 참고

### Criterion FOCUS_LOSS_CONTINUES_TICK
- **GIVEN** 세션 Active; 1Hz tick 구동 중
- **WHEN** 윈도우 focus 상실 이벤트 (`WM_ACTIVATE` LOSS) 발생 후 3초 대기
- **THEN** `TickInSession()` 호출 최소 2회 발생; 상태 `Active` 유지 (`Suspended` 전환 없음); `NORMAL_TICK` 분류 정상 동작
- **Evidence**: tick 호출 카운터 ≥ 2; 상태 머신 쿼리 `Active`; delegate `ADVANCE_SILENT` 수신
- **Type**: AUTOMATED — **Source**: §Window Focus vs OS Sleep 표, Pillar 1

### Criterion SESSION_COUNT_TODAY_RESET_CONTRACT
- **GIVEN** `FMockClockSource`; `Prev.DayIndex = 5`; `SessionCountToday = 3`
- **WHEN** `WallDelta = 25h`로 `ClassifyOnStartup()` → `NewDayIndex = 6` (Day 전진)
- **THEN** `SessionCountToday`가 `0`으로 리셋됨 (DayIndex 변화 시 리셋 — **Dream Trigger GDD가 최종 기준 결정 전까지 이 동작이 기본**)
- **Evidence**: `SessionCountToday == 0` 확인; `DayIndex == 6` 확인
- **Type**: AUTOMATED — **Source**: §Dep #4 Dream Trigger, E15, OQ-2

### Criterion NO_FORWARD_TAMPER_DETECTION_ADR
- **GIVEN** Forward tamper 명시적 수용이 설계 결정임 (AC `NO_FORWARD_TAMPER_DETECTION` + §Player Fantasy + §Rule 4)
- **WHEN** 이 설계 결정의 추적 가능성을 확인
- **THEN** ADR-0001 (또는 지정된 ADR 번호)에 "Forward tamper 명시적 수용" 결정이 기록되어 있거나, Architecture 단계에서 생성 예정으로 마킹되어 있음. 새 코드가 이 ADR 없이 forward tamper 탐지를 추가하면 코드 리뷰에서 차단
- **Evidence**: `docs/architecture/` 하위 ADR 파일 존재 (또는 Architecture 단계 TODO 마킹); 코드 리뷰 체크리스트에 "ADR-0001 위반 여부" 항목 포함
- **Type**: CODE_REVIEW — **Source**: §Player Fantasy, §Rule 4, Recommended #12

---

### Coverage 요약

| 분류 | Criterion 수 |
|---|---|
| Between-session Rules 1–4 | 9 |
| In-session Rules 5–8 | 4 |
| Narrative cap (Pillar 3) | 1 (NARRATIVE_CAP_ENFORCED) |
| FSessionRecord precision (split) | 2 (TYPE_DECLARATION + RUNTIME) |
| LONG_GAP idempotency / Farewell | 2 (LONG_GAP_SILENT_IDEMPOTENT [INTEGRATION] + FAREWELL_LONG_GAP_SILENT) |
| Forward Tamper acceptance (negative AC) | 2 (NO_FORWARD_TAMPER_DETECTION + NO_FORWARD_TAMPER_DETECTION_ADR) |
| Rule precedence / tick continuity | 3 (RULE_PRECEDENCE_FIRST_MATCH_WINS + TICK_CONTINUES_WHEN_PAUSED + FOCUS_LOSS_CONTINUES_TICK) |
| Session lifecycle contracts | 1 (SESSION_COUNT_TODAY_RESET_CONTRACT) |
| Other (corruption clamp, monotonic clamp 등) | 4 |
| **Total** | **29** |
| AUTOMATED | 24 (82.8%) |
| INTEGRATION | 1 (3.4%) |
| CODE_REVIEW | 3 (10.3%) |
| MANUAL | 1 (3.4%) |

### Coverage Gaps

**Untestable (자동·수동 모두 고비용)**:
1. **Windows `FPlatformTime::Seconds()` suspend 동작 일관성** — 드라이버 의존, CI 헤드리스 환경 검증 불가. `SUSPEND_RESUME`이 MANUAL인 이유
2. **Forward Tamper 감지 불가능성** — "의심하지 않는 시스템" 원칙. 버그 아닌 명시적 수용 → 코드 리뷰에서 "tamper 감지 로직 추가 금지" 체크 필요
3. **`SessionCountToday` 리셋 기준** — Dream Trigger GDD 미정 (E15). Time 단독 테스트 불가

**Uncovered (다른 GDD·통합 테스트 범위)**:
| 항목 | 이유 |
|---|---|
| Tuning Knob 상호작용 불변식 | `UMossTimeSessionConfig` 구현 후 config validation 테스트 |
| SessionEnding 저장 실패 롤백 (E6) | Save/Load GDD 통합 테스트 범위 |
| E8 — Day 21 Farewell 미완료 재접속 | 상태 머신 통합 테스트 |
| E11 — SaveSchemaVersion 마이그레이션 | Save/Load 통합 테스트 |
| E-MULTI — 2개 인스턴스 동시 실행 | Save/Load 책임 |

### Test Infrastructure Needs (구현 전 준비)

> **참고**: `IMossClockSource`는 §Dependencies의 **시스템 의존성**으로 격상됨 (선행 리뷰 Blocker 5 반영). 단순 test infra가 아니므로 아래 표에서는 제외 — 시스템 자체가 이 인터페이스로 시계를 받는다.

| 인프라 | 우선순위 | 목적 |
|---|---|---|
| **`FMockClockSource`** (`IMossClockSource` 테스트 구현체) | **BLOCKING** | 시스템 의존성을 만족시키는 테스트용 구현 — 시각 조작 (`SetUtcNow()`, `SetMonotonicSec()`) + suspend 시뮬레이션 (`SimulateSuspend()` → `OnPlatformSuspend()` 호출, `SimulateResume()` → `OnPlatformResume()` 호출) |
| **SessionRecord Factory** (test fixture) | HIGH | 매 테스트 boilerplate 제거 (특히 `NarrativeCount` 다양한 값 세팅) |
| **Delegate Capture Helper** (`FOnTimeActionResolved` + `FOnFarewellReached` 버퍼링 util) | HIGH | Action 수신 여부·순서·중복 미발행 검증 |
| **Save/Load Mock** (`FSessionRecord` 직렬화 stub) | MEDIUM | E6·E11 테스트 |
| **Static Analysis Hook** (`NO_FORWARD_TAMPER_DETECTION` AC 자동화) | MEDIUM | CI에서 금지 패턴 grep 자동 실행 |

---

## Open Questions

GDD 작성 중 도출된 미결 항목. 각각의 해결 시점·담당자 명시.

| # | 질문 | 담당 | 해결 시점 |
|---|---|---|---|
| OQ-1 | Windows `FPlatformTime::Seconds()`의 suspend/resume 동작이 드라이버별로 어떻게 다른가? 실기 측정 결과가 `SuspendMonotonicThresholdSec`/`SuspendWallThresholdSec` 기본값에 어떤 영향을 주는가? | `gameplay-programmer` + `qa-lead` | **Implementation 단계** (MVP 구현 중 첫 UE 빌드 후 Windows 10/11 실기 2종 이상 테스트) |
| OQ-2 | `SessionCountToday`의 리셋 기준이 `DayIndex` 변화인가, UTC 자정인가, 로컬 자정인가? | `game-designer` + `systems-designer` | Dream Trigger GDD 작성 시점 |
| OQ-3 | MVP (7일 축약)에서 `NarrativeThresholdHours = 8h`이 적절한가? 플레이테스트 후 재조정? | `game-designer` | MVP Playtest Report 결과 반영 시 |
| OQ-4 | ~~`LongGapAutoFarewell` 경로의 Farewell 연출 톤 차등 범위~~ **RESOLVED (R3)**: 네이밍 `LongGapSilent` → `LongGapAutoFarewell` 확정. 이름은 기술 의미(자동 작별)만 표현, 톤 차등(silent vs warm vs neutral)은 Farewell Archive GDD가 완전 결정. narrative-director 권고 수용 | — | **RESOLVED** 2026-04-17 |
| ~~OQ-5~~ | ~~Tuning Knob 노출 방식~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0011](../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) — `UDeveloperSettings` 정식 채택 + ConfigRestartRequired convention) |
| OQ-6 | ~~Clock Injection 인터페이스 시그니처~~ **RESOLVED (R3)**: 순수 C++ 추상 클래스 (UObject 미상속). 4 virtual methods: `GetUtcNow()`, `GetMonotonicSec()`, `OnPlatformSuspend()` (default no-op), `OnPlatformResume()` (default no-op). `FRealClockSource`가 `FCoreDelegates` 구독으로 OS suspend/resume 캡처. §Dependencies에 전체 시그니처 명시 | — | **RESOLVED** 2026-04-17 |
