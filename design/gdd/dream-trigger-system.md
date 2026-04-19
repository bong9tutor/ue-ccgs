# Dream Trigger System

> **Status**: Draft
> **Author**: bong9tutor + claude-code session (systems-designer)
> **Created**: 2026-04-18
> **Implements Pillar**: Pillar 3 (말할 때만 말한다 — 꿈은 희소하기에 소중하다)
> **Priority**: MVP | **Layer**: Feature | **Category**: Narrative
> **Effort**: L (4+ 세션)
> **Depends On**: Growth Accumulation Engine (#7), Data Pipeline (#2), Save/Load Persistence (#3), Game State Machine (#5), Card System (#8)

---

## 꿈 빈도 불일치 해소 (Cross-Review D-1)

> **주의**: game-concept.md("2-4일 간격", 즉 5-10회/21일)와 game-state-machine.md("21일 중 2-3회") 사이에 불일치가 존재했다. 이 GDD는 **두 수치의 절충안으로 3-5회/21일**을 공식 목표 범위로 채택한다.
>
> **근거**: Pillar 3("말할 때만 말한다")는 희소성을 명시적으로 요구한다. 5-10회는 "평균 이틀에 한 번"으로 일상적이 되어 Pillar 3를 약화시킨다. 2-3회는 21일 플레이스루에서 경험할 수 있는 꿈의 절대 수가 너무 적어 "내러티브 엔진"으로 기능하기 어렵다. **3-5회(평균 간격 4-7일)**는 두 필라르를 동시에 충족하는 실용적 범위다.
>
> - **최소 보장**: 3회 — 첫 꿈(탐색기 말), 중간 꿈(깊어짐), 마지막 꿈(징조)의 감정 아크 유지
> - **최대 허용**: 5회 — 6-7일 간격이 "예고 없는 계절"처럼 느껴지는 상한
> - **구현 안전 범위 Tuning Knob**: `MinDreamIntervalDays` [3, 7] — Pillar 3 수호의 일차 제어
>
> 이 수치는 Tuning Knobs 섹션(§7)에서 상세히 다룬다. game-concept.md와 game-state-machine.md의 담당 GDD 저자는 이 해소를 확인 후 각 문서의 해당 수치를 "→ Dream Trigger GDD §빈도 해소 참조"로 주석 처리할 것을 권고한다.

---

## 1. Overview

Dream Trigger System은 매일 카드 건넴(`FOnCardOffered`) 이후 꿈을 언록해야 하는지를 평가하고, 조건이 충족되면 `FOnDreamTriggered` 이벤트를 발행해 GSM의 Dream 상태 전환을 요청하는 **내러티브 희소성 관리 엔진**이다. 이 시스템의 핵심 역할은 하나다 — **Pillar 3("말할 때만 말한다")의 수학적 수호자**. 꿈이 희소하기에 꿈이 소중하고, 이 시스템은 그 희소성을 공식과 데이터 구조로 보장한다.

시스템은 네 가지 책임을 갖는다. **(1) 트리거 평가**: `FOnCardOffered` 수신 후 성장 엔진의 현재 태그 벡터(`GetTagVector()`)와 각 `UDreamDataAsset`의 트리거 조건을 대조한다. **(2) 희소성 강제**: 최소 간격(`MinDreamIntervalDays`)과 플레이스루 상한(`MaxDreamsPerPlaythrough`, 5회)을 동시에 검사하여 두 조건이 모두 충족될 때만 트리거를 허용한다. **(3) 꿈 선택**: 복수의 조건 충족 꿈 중 현재 태그 벡터와 가장 잘 일치하는 것을 하나 선택한다. **(4) 꿈 상태 영속화**: 언록된 꿈 목록, 마지막 꿈 일차, 꿈 카운터를 `FDreamState` 서브-struct로 세이브 파일에 기록한다.

꿈은 텍스트 전용이다 — 이끼 아이의 내면 세계를 시사하는 한두 줄의 시적 문장. 게임플레이 보상은 없다. 꿈은 이 게임이 "게임"이 아니라 "의식"임을 상기시키는 순간이며, 그 감동은 희소성에서 온다. **3-5회/21일**은 이 감동의 수학적 표현이다.

이 GDD는 cross-review D-1에서 발견된 꿈 빈도 불일치를 공식 해소한다(상단 박스 참조). 또한 `NarrativeCapPerPlaythrough(3)` 레지스트리 상수와의 관계를 명시한다 — 이 cap은 Time & Session System의 `ADVANCE_WITH_NARRATIVE` 시스템 내러티브 전용이며, 꿈 트리거와는 별개 카운터다(§3 Core Rules CR-5 참조).

---

## 2. Player Fantasy

아무 예고도 없다. 카드를 건네고 이끼 아이의 짧은 반응을 보고 앱을 닫으려는 순간 — 화면 모서리 아이콘이 조용히 빛난다. 알림이 아니다. 재촉이 아니다. 그것은 그가 말했다는 신호다.

꿈 일기를 열면 한두 줄짜리 문장이 있다. 시처럼, 일기처럼, 혹은 이끼 아이가 자면서 중얼거린 말처럼. 내용은 해독되지 않는다. "봄비를 너무 많이 받았나" 또는 "어둠이 좋았다"처럼 어렴풋이 내 선택과 연결되지만 확신할 수는 없다. 그 모호함이 해석의 즐거움을 만든다.

그리고 이 순간이 소중한 이유는 단 하나 — 매일 오지 않는다는 것이다. 어떤 날은 오고, 어떤 날은 오지 않는다. 플레이어는 언제 꿈이 올지 계산할 수 없다. 꿈은 예측 불가능하기에 기다려진다.

**앵커 모먼트**: 최초의 꿈 수신. 5-7일을 건네고 나서야 처음으로 아이콘이 빛나는 순간. "이 아이가 말했다"는 최초의 확인. 이 순간 이후 플레이어는 다음 꿈이 올 날을 기다리며 일기 화면을 하루에 한 번씩 열어본다.

**Pillar 연결**:
- **Pillar 3 (말할 때만 말한다)** — 이 시스템 존재 이유 자체. 꿈의 빈도를 수학적으로 통제하는 것이 이 시스템의 첫 번째 의무다. 희소성이 붕괴되는 순간 게임의 감정적 전제가 무너진다.
- **Pillar 2 (선물의 시학)** — 꿈의 트리거 조건은 수치가 아닌 태그 패턴이다. 플레이어는 "계절 카드 5장"이 꿈을 불러온다는 사실을 알 수 없다. 어렴풋한 패턴만 감지할 수 있다.
- **Pillar 1 (조용한 존재)** — 꿈 알림은 아이콘이 조용히 빛나는 것으로 충분하다. 팝업 없음, 사운드 알림 없음, "새 꿈을 받았습니다" 배너 없음.
- **Pillar 4 (끝이 있다)** — Day 21의 마지막 꿈은 보장된다. 플레이스루가 꿈 없이 끝나는 일은 없다.

---

## 3. Detailed Design

### Core Rules

#### CR-1 — 평가 트리거

**`FOnCardProcessedByGrowth(const FGrowthProcessResult&)` 수신 즉시** 트리거 평가 파이프라인을 시작한다. 이것이 이 시스템의 유일한 진입점이다.

> **ADR-0005 (2026-04-19) 적용 — Stage 2 구독 패턴**: 이전에는 Card System의 `FOnCardOffered`를 직접 구독했으나, Day 21 Final Form 결정 순서 정확성 (Pillar 4) 보호를 위해 Growth Engine의 Stage 2 delegate로 전환됨. Growth가 CR-1 태그 가산 + CR-5 EvaluateFinalForm + SaveAsync 완료 **이후**에만 Dream Trigger가 평가 시작 → 최신 TagVector 보장.

```
FOnCardProcessedByGrowth(Result) 수신  [Stage 2 — Growth 처리 완료 직후]
    → [Guard: EGameState == Waiting] 진입 가능 여부 확인
    → [Guard: DayIndex ∉ {1}] Day 1은 첫 경험 보호로 꿈 평가 제외
    → [Guard: Result.bIsDay21Complete == false] Day 21 Final Form 진입 중이면 평가 스킵 (GSM이 Farewell P0 진입 예정)
    → [Guard: DaysSinceLastDream >= MinDreamIntervalDays] 냉각 기간 확인
    → [Guard: DreamsUnlockedCount < MaxDreamsPerPlaythrough] 상한 확인
    → 조건 충족 꿈 목록 수집 (Candidate Selection — CR-3)
    → 후보 없음: 평가 종료 (아무 이벤트 없음)
    → 후보 있음: 꿈 1개 선택 (CR-4) → FOnDreamTriggered 발행 → 상태 저장
```

**평가는 동기 실행**이다. `FOnCardProcessedByGrowth` 핸들러 함수 내에서 전체 파이프라인이 완료된다. 비동기 평가는 GSM 상태 전환과의 순서를 보장할 수 없으므로 금지.

**평가 실패의 침묵**: 조건 불충족으로 꿈이 트리거되지 않는 경우, 어떤 이벤트도 발행하지 않는다. 플레이어에게 "꿈이 올 뻔했다"는 신호를 주는 메커니즘은 없다 (Pillar 3).

#### CR-2 — 희소성 가드 (Rarity Guard)

두 개의 독립 게이트가 모두 통과되어야 꿈 후보를 평가한다.

**게이트 A — 최소 간격 (Cooldown)**:
```
DaysSinceLastDream = CurrentDayIndex - FDreamState.LastDreamDay
Pass if: DaysSinceLastDream >= MinDreamIntervalDays
```
`LastDreamDay = 0` (꿈 미수령)이면 Day 1 이후 항상 충족 (단 CR-1의 Day 1 제외 Guard 선행 적용).

**게이트 B — 플레이스루 상한 (Cap)**:
```
Pass if: FDreamState.DreamsUnlockedCount < MaxDreamsPerPlaythrough
```
`MaxDreamsPerPlaythrough`의 기본값은 5. 3-5회/21일 목표의 상한으로 작동한다.

두 게이트는 순서대로 평가한다. 게이트 A가 실패하면 게이트 B를 평가하지 않는다.

**Day 21 특별 처리**: `CurrentDayIndex == GameDurationDays`이고 `DreamsUnlockedCount < MinimumGuaranteedDreams`이면 (플레이스루 최소 보장 횟수에 미달인 채로 최종일에 도달) 게이트를 우회하여 강제 트리거한다. `MinimumGuaranteedDreams`의 기본값은 3(Full) / 1(MVP)이지만, Day 21에 한 번에 여러 회를 보충 트리거하지는 않는다 — 가장 높은 TagScore의 미열람 꿈 1개를 선택해 1회 강제 트리거하고 평가를 종료한다. 최소한 하나의 꿈은 보장된다 (§5 Edge Case EC-1 참조).

#### CR-3 — 후보 수집 (Candidate Selection)

`UDataPipelineSubsystem.GetAllDreamAssets()`로 전체 `UDreamDataAsset` 목록을 pull한다.

각 `UDreamDataAsset`에 대해 다음 네 조건을 모두 충족하면 후보로 등록:

**조건 1 — 이미 본 꿈 제외**:
```
DreamId ∉ FDreamState.UnlockedDreamIds
```

**조건 2 — 성장 단계 필터 (선택적)**:
```
asset.RequiredStage == EGrowthStage::None
    OR asset.RequiredStage == Growth.GetCurrentStage()
```
`RequiredStage`가 `None`이면 어떤 단계에서도 나올 수 있다. 단계별 꿈을 설계하고 싶을 때 사용.

**조건 3 — 태그 임계 충족**:
```
TagScore(asset, tagVector) >= asset.TriggerThreshold
```
`TagScore` 계산은 Formula F1 (§4 참조).

**조건 4 — 최초 출현 일차 충족**:
```
DayIndex >= asset.EarliestDay
```
`asset.EarliestDay`는 CR-7에서 정의된 `UDreamDataAsset` 필드 (기본 2). 전역 Tuning Knob `EarliestDreamDay`는 시스템 전체 하한선으로 작동한다 — `asset.EarliestDay`는 `EarliestDreamDay` 이상이어야 하며, 특정 꿈을 더 늦게 등장시키기 위해 per-asset 값을 더 높게 설정하는 것은 허용된다. 즉: `effective_earliest = max(EarliestDreamDay, asset.EarliestDay)`.

네 조건을 모두 통과한 자산이 후보 목록이 된다. 후보가 없으면 평가 종료.

#### CR-4 — 꿈 선택 (Dream Selection)

후보가 둘 이상이면 `TagScore`가 가장 높은 꿈을 선택한다. 동점 시 `DreamId` FName 사전순 첫 번째.

```
SelectedDream = argmax(TagScore(asset, tagVector)) over Candidates
```

선택 완료 후:
1. `FDreamState.UnlockedDreamIds`에 `SelectedDream.DreamId` 추가
2. `FDreamState.LastDreamDay = CurrentDayIndex`
3. `FDreamState.DreamsUnlockedCount += 1`
4. `SaveAsync(ESaveReason::EDreamReceived)` 호출
5. `FOnDreamTriggered(FName DreamId)` 발행

**저장은 반드시 이벤트 발행 전에 완료해야 한다.** 발행 후 앱이 종료되어도 꿈 언록 상태가 보존되도록.

#### CR-5 — NarrativeCapPerPlaythrough와의 관계 명시

레지스트리 상수 `NarrativeCapPerPlaythrough (value: 3)`은 **이 시스템과 별개**다. 이 상수는 Time & Session System의 `ADVANCE_WITH_NARRATIVE` 시스템 내러티브(부재 후 복귀 시 이끼 아이의 상태 변화 설명 텍스트)의 플레이스루당 발행 횟수를 제한하는 Pillar 3 시행 장치다.

꿈(`UDreamDataAsset`)은 완전히 별개의 카운터(`FDreamState.DreamsUnlockedCount`)로 관리된다. 꿈이 트리거될 때 `NarrativeCount`는 증가하지 않는다. 두 시스템은 동일한 Pillar 3를 구현하지만 독립적으로 작동한다.

| 시스템 | 카운터 | Cap | 발행 조건 |
|---|---|---|---|
| Time & Session (ADVANCE_WITH_NARRATIVE) | `FSessionRecord.NarrativeCount` | `NarrativeCapPerPlaythrough` (3) | 부재 복귀, 성장 이정표 |
| **Dream Trigger** | `FDreamState.DreamsUnlockedCount` | `MaxDreamsPerPlaythrough` (5) | 태그 임계 + 희소성 가드 |

두 카운터가 동시에 발행 대기 상태가 되는 경우(예: 부재 복귀 날 꿈도 트리거), GSM은 Stillness Budget `Narrative` 슬롯을 배타 소비한다. 따라서 같은 세션에서 시스템 내러티브와 꿈이 동시에 표시될 수 없다. 우선순위: `ADVANCE_WITH_NARRATIVE`(Time System 발행) → 꿈(Dream Trigger 발행). 꿈은 다음 세션으로 연기되지 않으며 해당 일에 대기(defer) 상태로 유지된다 (§5 Edge Case EC-4 참조).

#### CR-6 — FDreamState 구조체

| 필드 | 타입 | 설명 |
|---|---|---|
| `UnlockedDreamIds` | `TArray<FName>` | 이미 본 꿈의 `DreamId` 배열. 중복 방지 가드 |
| `LastDreamDay` | `int32` | 마지막 꿈 수신 일차. 0=미수신. 냉각 계산 기준 |
| `DreamsUnlockedCount` | `int32` | 플레이스루 꿈 총 수신 횟수. Cap 검사 기준 |
| `PendingDreamId` | `FName` | 선택되었으나 아직 표시되지 않은 꿈 ID. 비어있으면 대기 없음 |

`UMossSaveData` 안의 typed sub-struct로 거주. Save/Load가 직렬화 책임, Dream Trigger가 의미적 소유.

#### CR-7 — UDreamDataAsset 필드 확장 (Data Pipeline C2 소유권 이전)

Data Pipeline은 `UDreamDataAsset`을 forward-declare한다 (C2 constraint: 트리거 임계를 코드에 하드코딩 금지). 이 GDD가 필드를 공식 정의하고 소유권을 인수한다.

| 필드 | 타입 | UE UPROPERTY | 설명 |
|---|---|---|---|
| `DreamId` | `FName` | `EditAnywhere` | PrimaryAssetId. 고유 식별자 |
| `BodyText` | `FText` | `EditAnywhere` | 꿈 본문 (1-3문장 시적 텍스트). String Table 경유 권장 |
| `TriggerTagWeights` | `TMap<FName, float>` | `EditAnywhere` | 태그별 필요 비율. TagScore 내적 대상 |
| `TriggerThreshold` | `float` | `EditAnywhere` | TagScore 최소값. 이 값 이상이면 조건 충족 |
| `RequiredStage` | `EGrowthStage` | `EditAnywhere` | 이 꿈이 나올 수 있는 최소 성장 단계. `None`(=Sprout 이상 모두 허용)도 허용 |
| `EarliestDay` | `int32` | `EditAnywhere` | 이 꿈이 나올 수 있는 최초 일차. 기본 2. Day 1 보호 강화 가능 |

**C2 Constraint 이행**: `TriggerTagWeights`와 `TriggerThreshold`는 코드 내 리터럴 금지. 반드시 이 DataAsset 필드를 통해 외부화. 에디터 저장 시 `UEditorValidatorBase`가 `TriggerThreshold > 0` 및 `TriggerTagWeights.Num() > 0` 검증.

**MaxCatalogSizeDreams 준수**: 총 꿈 자산 수는 레지스트리 상수 `MaxCatalogSizeDreams (150)` 이하여야 한다. Full Release 목표 50개는 3× 헤드룸 보유.

### States and Transitions

Dream Trigger System 내부 상태 머신:

| 상태 | 설명 | 진입 조건 | 이탈 조건 |
|---|---|---|---|
| **Dormant** | 평가 대기. 기본 상태 | 앱 시작 또는 평가 완료 후 | `FOnCardProcessedByGrowth` 수신 (Stage 2, ADR-0005) |
| **Evaluating** | 트리거 파이프라인 실행 | `FOnCardProcessedByGrowth` 수신 | 평가 완료 (Triggered 또는 Dormant) |
| **Triggered** | 꿈 선택 완료, 저장 대기 | CR-4 조건 충족 후보 선택 | `SaveAsync` 완료 후 `FOnDreamTriggered` 발행 |
| **Cooldown** | 최소 간격 이내. Evaluating은 가능하나 게이트 A가 차단 | Triggered 상태에서 저장·발행 완료 | `DaysSinceLastDream >= MinDreamIntervalDays` |

**주의**: Cooldown은 별도 상태가 아니라 Dormant의 서브컨디션이다. 내부적으로 `LastDreamDay`가 설정된 Dormant로 표현하며, 평가 시 게이트 A가 이를 처리한다.

**로드 복구**: `PendingDreamId`가 비어있지 않으면 → Triggered 직행. `SaveAsync` 완료 시점에 `PendingDreamId`가 비어있으면 이전 세션에서 저장이 정상 완료된 것이다.

### Interactions with Other Systems

#### 1. Growth Accumulation Engine (#7) — 인바운드 (Stage 2 Subscriber, ADR-0005 2026-04-19)

| 이벤트 | 데이터 | 반응 |
|---|---|---|
| `FOnCardProcessedByGrowth` | `const FGrowthProcessResult&` (CardId + bIsDay21Complete + FinalFormId) | CR-1 트리거 평가 파이프라인 시작 — Growth 태그 가산 완료 후 진입이라 최신 TagVector 사용 보장 |

**이전 (ADR-0005 이전)**: Card System의 `FOnCardOffered` 직접 구독 — UE Multicast Delegate 등록 순서 비결정으로 Growth 처리 전에 Dream 평가 시작 가능성 있었음. Day 21에 이전 TagVector로 평가되는 버그 위험.

Card System은 이벤트만 발행하되, Dream Trigger는 **Growth의 처리 후** 평가 시작. Dream은 내용 pull (GetTagVector, GetCurrentStage)을 독립적으로 수행.

#### 2. Growth Accumulation Engine (#7) — 아웃바운드 (pull)

| API | 시점 | 용도 |
|---|---|---|
| `GetTagVector()` | 평가 시 | 현재 태그 벡터 조회 (TagScore 계산 입력) |
| `GetCurrentStage()` | 평가 시 | 현재 성장 단계 조회 (RequiredStage 필터) |

Growth는 Dream에 push하지 않는다. Dream이 필요할 때 pull.

#### 3. Data Pipeline (#2) — 아웃바운드 (pull)

| API | 시점 | 용도 |
|---|---|---|
| `GetAllDreamAssets()` | 평가 시 | 전체 꿈 자산 목록 조회 (후보 수집) |
| `GetDreamBodyText(FName DreamId)` | Dream Journal UI 요청 시 | 꿈 본문 텍스트 제공 |

`GetAllDreamAssets()`는 `UDataPipelineSubsystem`이 Ready 상태일 때만 유효한 결과를 반환한다. Degraded 상태 시 빈 배열 → 평가 종료 (꿈 없음, 플레이어에게 알림 없음).

Data Pipeline `Initialize()` 선행 의존: Dream Trigger는 `Initialize()` 내에서 `Collection.InitializeDependency<UDataPipelineSubsystem>()` 호출로 선행 의존 선언 필수 (Data Pipeline R2 BLOCKER 5).

#### 4. Save/Load Persistence (#3) — 양방향

| 방향 | 데이터 | 시점 |
|---|---|---|
| Save → Dream | `FDreamState` 복원 | Initialize() 1회 |
| Dream → Save | 갱신된 `FDreamState` | 꿈 트리거 완료 직후 `SaveAsync(ESaveReason::EDreamReceived)` |

**저장 원자성 계약**: CR-4의 `FDreamState` 갱신(1-3번 항목)과 `SaveAsync(ESaveReason::EDreamReceived)` 호출은 동일 call stack 내에서 수행. `co_await`, `AsyncTask`, `Tick` 분기 개입 금지.

#### 5. Game State Machine (#5) — 양방향

| 방향 | 이벤트 / API | 데이터 | 설명 |
|---|---|---|---|
| Dream → GSM | `FOnDreamTriggered` 발행 | `FName DreamId` | Waiting → Dream 상태 전환 요청 (Stillness Budget 획득 후) |
| GSM → Dream | `FOnGameStateChanged` 구독 | `EGameState OldState, EGameState NewState` | Dream 상태 이탈 감지 → `PendingDreamId` 클리어 |

Dream Trigger는 GSM 상태를 직접 변경하지 않는다. `FOnDreamTriggered` 발행으로 요청하고 GSM이 결정한다. GSM이 Budget 획득 실패 시 전환을 연기하더라도, Dream Trigger는 이미 `FDreamState`를 업데이트하고 저장을 완료한 상태다 (꿈 언록은 표시와 분리).

**PendingDreamId 클리어 책임** (OQ-DTS-2 RESOLVED): Dream Trigger는 `FOnGameStateChanged`를 구독한다. `OldState == EGameState::Dream && NewState == EGameState::Waiting`인 전환이 수신되면 즉시 `FDreamState.PendingDreamId = NAME_None`으로 클리어하고 `SaveAsync(ESaveReason::EDreamReceived)`를 호출한다. 이 처리를 통해 "꿈이 표시됨"과 "PendingDreamId 소멸"이 동일 이벤트 흐름 내에서 원자적으로 수행된다. GSM이 Dream → Waiting 외의 상태로 이탈하는 경우(예: Farewell)도 동일하게 클리어 — `OldState == EGameState::Dream`이면 무조건 클리어.

#### 6. Dream Journal UI (#13) — 아웃바운드 (pull)

| API | 데이터 | 시점 |
|---|---|---|
| `GetDreamBodyText(FName DreamId)` | `FText` | UI가 꿈 표시 시 Data Pipeline 경유 pull |
| `GetUnlockedDreamIds()` | `TArray<FName>` | 일기 목록 렌더링 시 pull |

Dream Trigger는 UI에 push하지 않는다. UI가 필요할 때 pull. `FOnGameStateChanged(Dream)` 수신 시 UI가 갱신 알림을 인지하고 pull.

#### 7. Stillness Budget (#10) — 없음 (직접)

Dream Trigger는 Stillness Budget과 직접 통신하지 않는다. Budget 획득은 GSM 책임이다. Dream Trigger → `FOnDreamTriggered` → GSM → Budget Request 순서.

---

## 4. Formulas

### F1 — Tag Score (DTS.TagScore)

꿈 자산의 트리거 조건과 현재 태그 벡터의 일치도 점수.

`TagScore(asset, V) = Σ_tag ( V_norm[tag] × asset.TriggerTagWeights[tag] )`

| Symbol | Type | Range | Description |
|---|---|---|---|
| V | TMap\<FName, float\> | [0, unbounded] | Growth Engine 누적 태그 벡터 (`GetTagVector()` 반환값) |
| TotalWeight | float | [0, unbounded] | Σ V[tag]. 0이면 V_norm 계산 불가 → TagScore = 0.0 |
| V_norm[tag] | float | [0.0, 1.0] | V[tag] / TotalWeight. GAE.VectorNorm(F2)과 동일 |
| TriggerTagWeights[tag] | float | [0.0, 1.0] | UDreamDataAsset의 태그별 가중치. 합계 1.0 권고 |
| TagScore | float | [0.0, ∞) | 내적 결과. TriggerTagWeights 합계 ≤ 1.0이면 [0.0, 1.0] |
| TriggerThreshold | float | (0.0, 1.0] | 조건 충족 최소 TagScore. UDreamDataAsset UPROPERTY |

**Output Range**: `TriggerTagWeights` 합계 = 1.0 준수 시 [0.0, 1.0]. 합계 > 1.0이면 > 1.0 가능하나 argmax 선택 로직에서 문제 없음.

**TotalWeight = 0 가드**: `if (TotalWeight < KINDA_SMALL_NUMBER) return 0.0f;` — 카드를 아직 건넨 적 없는 Day 1에서의 방어.

**가드 계층 정리**: TagScore가 TriggerThreshold 이상이어도 CR-2 희소성 가드가 먼저 통과해야 후보로 등록된다. TagScore는 후보 중에서의 선택 기준이다 (최고 점수 선택, CR-4).

**Worked Example**:
```
현재 TagVector = { Season.Spring: 6, Emotion.Calm: 3, Element.Water: 1 }
TotalWeight = 10.0
V_norm = { Season.Spring: 0.60, Emotion.Calm: 0.30, Element.Water: 0.10 }

Dream "봄비의 속삭임" TriggerTagWeights:
  { Season.Spring: 0.70, Emotion.Calm: 0.20, Element.Water: 0.10 }
  TriggerThreshold = 0.50

TagScore = (0.60 × 0.70) + (0.30 × 0.20) + (0.10 × 0.10)
         = 0.42 + 0.06 + 0.01 = 0.49

조건: 0.49 < TriggerThreshold(0.50) → 후보 제외

(Spring 1장 더 건넨 후):
V_norm = { Season.Spring: 0.636, Emotion.Calm: 0.273, Element.Water: 0.091 }
TagScore = (0.636 × 0.70) + (0.273 × 0.20) + (0.091 × 0.10)
         = 0.445 + 0.055 + 0.009 = 0.509

조건: 0.509 >= 0.50 → 후보 등록
```

### F2 — Days Since Last Dream (DTS.DaysSinceLastDream)

최소 간격 게이트 A 계산.

`DaysSinceLastDream = CurrentDayIndex - FDreamState.LastDreamDay`

| Symbol | Type | Range | Description |
|---|---|---|---|
| CurrentDayIndex | int32 | [1, 21] | 현재 일차 (레지스트리 GameDurationDays 기준) |
| FDreamState.LastDreamDay | int32 | [0, 21] | 마지막 꿈 수신 일차. 0 = 꿈 미수신 |
| DaysSinceLastDream | int32 | [0, 21] | 마지막 꿈 이후 경과 일수 |
| MinDreamIntervalDays | int32 | [3, 7] | 최소 간격 임계값. Tuning Knob |

**Output Range**: [0, 21]. `LastDreamDay = 0`이면 `DaysSinceLastDream = CurrentDayIndex` (Day 1부터 카운팅).

**Worked Example**:
```
CurrentDayIndex = 8, LastDreamDay = 4, MinDreamIntervalDays = 4
DaysSinceLastDream = 8 - 4 = 4
Pass (4 >= 4)

CurrentDayIndex = 7, LastDreamDay = 4, MinDreamIntervalDays = 4
DaysSinceLastDream = 7 - 4 = 3
Fail (3 < 4) → 게이트 A 차단
```

### F3 — Dream Frequency Target (DTS.FrequencyTarget)

3-5회/21일 목표가 설계된 파라미터에서 달성 가능한지 검증하는 참조 공식.

`ExpectedDreamCount ≈ (GameDurationDays - EarliestPossibleDreamDay) / AvgDreamInterval`

`AvgDreamInterval = MinDreamIntervalDays` (하한 기준 최대 빈도)

| Symbol | Type | Range | Description |
|---|---|---|---|
| GameDurationDays | int32 | {21} | 레지스트리 상수 |
| EarliestPossibleDreamDay | int32 | [2, 5] | 가장 빠른 꿈 가능 일차 (Day 1 제외 + 첫 MinInterval) |
| MinDreamIntervalDays | int32 | [3, 7] | Tuning Knob |
| MaxDreamsPerPlaythrough | int32 | [3, 7] | Tuning Knob (상한 cap) |
| ExpectedDreamCount | float | [0, MaxDreamsPerPlaythrough] | 예상 최대 수신 횟수. 실제는 태그 조건 미충족으로 감소 가능 |

**Output Range**: `min(ExpectedDreamCount, MaxDreamsPerPlaythrough)`. 결과가 [3, 5] 범위 내에 있어야 설계 목표 달성.

**안전 범위 검증**:
```
기본값: MinDreamIntervalDays=4, MaxDreamsPerPlaythrough=5, EarliestDay≈5
ExpectedMax = (21 - 5) / 4 = 4.0 → min(4.0, 5) = 4.0 ✓ (목표 3-5 범위 내)

최소 빈도: MinDreamIntervalDays=7, MaxDreamsPerPlaythrough=3
ExpectedMax = (21 - 8) / 7 = 1.86 → min(1.86, 3) = 1.86 ✗ (3회 최소 보장 불가 → Day 21 강제 트리거 필요)

최대 빈도: MinDreamIntervalDays=3, MaxDreamsPerPlaythrough=5
ExpectedMax = (21 - 4) / 3 = 5.67 → min(5.67, 5) = 5 ✓ (상한 준수)
```

이 공식은 설계 검증용이다. 실제 꿈 수신 횟수는 태그 조건 충족 여부에 따라 더 낮을 수 있다.

### F4 — MVP Day Scaling (DTS.MVPDayScaling)

Full 21일 파라미터를 MVP 7일 아크로 변환.

`Param_MVP = max(1, round(Param_Full × (GameDurationDays_MVP / GameDurationDays_Full)))`

| Symbol | Type | Range | Description |
|---|---|---|---|
| Param_Full | int32 | varies | Full 아크 파라미터값 |
| GameDurationDays_MVP | int32 | {7} | MVP 축약 총 일수 |
| GameDurationDays_Full | int32 | {21} | 레지스트리 상수 |
| Param_MVP | int32 | [1, GameDurationDays_MVP - 1] | 스케일된 MVP 값 |

**MVP 파라미터 변환표 (7/21 = 1/3)**:

| 파라미터 | Full 기본값 | 공식 결과 | MVP 권장값 | 비고 |
|---|---|---|---|---|
| `MinDreamIntervalDays` | 4 | 1.3 → 1 | **2** | 최소 경험 보장 위해 1보다 여유 확보 |
| `MaxDreamsPerPlaythrough` | 5 | 1.7 → 2 | **2** | 7일 내 2회가 적절 |
| `MinimumGuaranteedDreams` | 3 | 1 → 1 | **1** | 플레이스루당 최소 1회 |
| Day 1 제외 | 적용 | — | 적용 | 동일 |

MVP에서는 `MaxDreamsPerPlaythrough=2`, `MinDreamIntervalDays=2`가 기본값. `UDreamConfigAsset`의 MVP 오버라이드 필드로 외부화.

---

## 5. Edge Cases

### 꿈 없는 플레이스루 방지

**EC-1 — Day 21 종료 시 최소 보장 꿈 횟수 미달인 경우**

`CurrentDayIndex == GameDurationDays` 이고 `DreamsUnlockedCount < MinimumGuaranteedDreams`이면:
- 태그 조건 무시 (강제 트리거) — `asset.TriggerThreshold` 검사 생략
- 희소성 게이트 무시 (강제 트리거) — CR-2 게이트 A/B 생략
- `asset.EarliestDay` 필터 무시 (CR-3 조건 4 생략) — 등록일 제한도 해제
- **이미 본 꿈 제외(CR-3 조건 1)만 적용** — 중복 표시는 방지
- 꿈 선택 기준: 미열람 꿈 중 TagScore가 가장 높은 것 1개. 모든 TagScore가 동일하거나 0이면 `DreamId` FName 사전순 첫 번째. 미열람 꿈이 전혀 없으면 `DefaultDreamId` Tuning Knob 폴백 (비어있으면 강제 트리거 포기, 로그 출력)
- **1회만 강제 트리거**: Day 21에서 미달 횟수만큼 반복 트리거하지 않는다. 1개를 트리거하고 평가 종료. 이후 `DreamsUnlockedCount`가 여전히 `MinimumGuaranteedDreams` 미만이더라도 추가 강제 없음 (Day 21은 단 1회 평가 기회만 있기 때문)
- **실행 순서**: Day 21 카드 건넴 이후, `FOnFinalFormReached` 발행 전에 강제 트리거 완료 필요 (Farewell 전환 시 Dream이 표시될 수 있는 마지막 기회)

이 EC는 F3의 "최소 보장 3회"가 달성되지 않는 극단 상황의 마지노선이다. `MinimumGuaranteedDreams`는 §7 Tuning Knob으로 외부화되어 있으며, 기본값 3(Full)/1(MVP)이 이 EC의 발동 임계를 결정한다. 플레이어가 21일 동안 태그 임계를 전혀 충족하지 못했더라도 최소 1개의 꿈을 경험한다.

**EC-2 — MaxDreamsPerPlaythrough 도달 후 Day 21 강제 트리거 충돌**

`DreamsUnlockedCount >= MaxDreamsPerPlaythrough`이고 `DayIndex == GameDurationDays`이면:
- Day 21 강제 트리거를 적용하지 않는다. 이미 충분한 꿈을 경험했다.
- 상한에 이미 도달한 플레이스루는 더 이상의 꿈이 없는 것이 올바른 상태다.

**EC-3 — 이미 모든 꿈을 본 경우 (Dream Exhaustion)**

`UnlockedDreamIds.Num() >= GetAllDreamAssets().Num()` — 즉 볼 수 있는 꿈이 모두 소진된 경우:
- 후보 목록이 비게 된다.
- 꿈 트리거 없음. 평가 종료.
- 플레이어에게 알림 없음.
- 설계적으로 Full Release의 꿈 50개는 한 플레이스루에서 소진 불가 (Max 5회 상한).
- MVP 5개 꿈으로 2회 max 플레이스루면 소진 위험 있음 → `DefaultDreamId` Tuning Knob으로 반복 가능한 "기본 꿈" 1개 등록 권장.

**EC-4 — 같은 세션에 시스템 내러티브와 꿈 동시 대기**

`ADVANCE_WITH_NARRATIVE` 처리 중 꿈도 트리거되는 상황:
- 꿈 트리거 평가는 `FOnCardOffered` 수신 후 즉시 실행 — 이 시점에 `ADVANCE_WITH_NARRATIVE`가 이미 처리됐을 수도 있고 아닐 수도 있다.
- 평가 결과 `FOnDreamTriggered`를 발행하면, GSM은 Stillness Budget `Narrative` 슬롯을 이미 `ADVANCE_WITH_NARRATIVE` 오버레이가 점유 중이면 Dream 전환을 연기한다 (GSM E5 참조).
- Dream Trigger는 이 연기를 인지하지 않는다. `FOnDreamTriggered`를 발행하고 `FDreamState`를 저장한 후 Dormant로 복귀.
- `PendingDreamId`를 세이브에 기록하여, 다음 세션에서 드림 표시를 재시도할 수 있게 한다 (FDreamState.PendingDreamId).
- 단, `PendingDreamId`가 있어도 다음 세션에서 자동 표시는 없다. 로드 시 GSM이 `Waiting` 상태에서 Pending 꿈이 있으면 `FOnDreamTriggered` 재발행을 드림 트리거에 요청한다.

**EC-5 — 앱 재시작 (세션 중단)**

앱 재시작 후 `FDreamState` 복원:
- `PendingDreamId`가 비어있지 않으면 → `FOnDreamTriggered(PendingDreamId)` 재발행 (표시 안 된 꿈 재시도)
- `UnlockedDreamIds` 복원 → 이미 본 꿈 중복 없음
- `LastDreamDay` 복원 → 냉각 계산 정상 재개
- `DreamsUnlockedCount` 복원 → Cap 정상 적용

**EC-6 — Farewell 상태에서 FOnCardOffered 수신**

`Farewell`은 GSM 종단 상태. Dream Trigger는 `EGameState == Farewell`이면 평가 스킵. Day 21 강제 트리거(EC-1)도 적용하지 않음 — Farewell 진입 전 처리되어야 한다 (CR-1 Guard 참조).

**EC-7 — TagVector가 빈 경우 (카드 미건넴 상태에서 평가)**

`TotalWeight < KINDA_SMALL_NUMBER`이면 F1 TagScore = 0.0 → 모든 후보 제외 → 평가 종료. 이론적으로 `FOnCardOffered` 이후에만 평가되므로 `TotalWeight == 0`은 불가능하지만, 방어 코드는 유지.

**EC-8 — DataPipeline DegradedFallback 상태**

`GetAllDreamAssets()` 빈 배열 반환 → 후보 0개 → 평가 종료. 꿈 없음. 플레이어에게 알림 없음 (Pillar 1). `UE_LOG(LogDreamTrigger, Warning)` 1회 출력.

**EC-9 — 시스템 시계 역행 (DayIndex 역행)**

Time System이 `BACKWARD_GAP`으로 분류하면 `FOnDayAdvanced` 미발행. Dream Trigger는 `FOnCardOffered`로만 평가를 시작하므로 DayIndex 역행 시에도 평가가 발생할 수 있다. 이 경우:
- `DaysSinceLastDream = CurrentDayIndex - LastDreamDay`가 음수가 될 수 있음 → `max(0, ...)` 가드 적용
- 음수 DaysSinceLastDream은 게이트 A 실패 → 꿈 트리거 없음

---

## 6. Dependencies

| System | 방향 | 성격 | 인터페이스 | MVP/VS |
|---|---|---|---|---|
| **Growth Accumulation Engine (#7)** | 인바운드 | **Hard** | `FOnCardProcessedByGrowth(const FGrowthProcessResult&)` — 평가 트리거 (Stage 2, ADR-0005). 이전에는 Card System의 FOnCardOffered 직접 구독이었으나 Day 21 순서 보장 위해 전환 |
| **Growth Accumulation Engine (#7)** | 아웃바운드 (pull) | **Hard** | `GetTagVector()`, `GetCurrentStage()` | MVP |
| **Data Pipeline (#2)** | 아웃바운드 (pull) | **Hard** | `GetAllDreamAssets()`, `GetDreamBodyText(FName)` | MVP |
| **Save/Load Persistence (#3)** | 양방향 | **Hard** | `FDreamState` typed slice — Init에서 read, 꿈 트리거 완료마다 `SaveAsync(EDreamReceived)` | MVP |
| **Game State Machine (#5)** | 아웃바운드 | **Hard** | `FOnDreamTriggered(FName DreamId)` → GSM Waiting→Dream 전환 요청 | MVP |
| **Dream Journal UI (#13)** | 아웃바운드 (pull 수신자) | **Soft** | `GetUnlockedDreamIds()`, `GetDreamBodyText(FName)` | MVP |
| **Time & Session System (#1)** | 없음 (직접) | — | Dream Trigger는 `FOnDayAdvanced`를 구독하지 않음. `FOnCardOffered`로만 평가 시작. DayIndex는 GSM 또는 FSessionRecord에서 읽음 | MVP |
| **Farewell Archive (#17)** | 아웃바운드 (pull 수신자) | **Soft** | `GetUnlockedDreamIds()` — 작별 아카이브에 꿈 목록 포함 | VS |

### 양방향 일관성 노트

- **Growth (#7)**: Growth GDD §Dependencies에서 `GetTagVector()`, `GetCurrentStage()` API를 provisional로 명시. 이 GDD 확정 후 Growth GDD의 Dream pull API를 공식화.
- **Data Pipeline (#2)**: `UDreamDataAsset` forward-declare를 이 GDD가 확장. Data Pipeline의 `referenced_by`에 `dream-trigger-system.md` 추가 필요.
- **Save/Load (#3)**: `FDreamState` struct를 `UMossSaveData`에 추가. Save/Load GDD `FMossSaveSnapshot` 허용 필드 타입 준수 (POD-only contract) — `TArray<FName>` (허용), `int32` (허용), `FName` (허용).
- **GSM (#5)**: `FOnDreamTriggered` delegate는 이 GDD가 정의하고 GSM이 구독. GSM Dependencies 섹션에 Dream Trigger → GSM 인바운드 항목 추가 필요.

### 신규 Cross-System Entities (레지스트리 등록 대상)

이 GDD에서 새로 정의된 크로스-시스템 사실:
- **`FOnDreamTriggered`** delegate — 이 GDD 소유, GSM 구독
- **`FDreamState`** struct — 이 GDD 소유, Save/Load 직렬화
- **`UDreamDataAsset` 필드 확장** — Data Pipeline forward-declare를 이 GDD가 확장 (source 이전 필요)

### 레지스트리 업데이트 확인 (B-3)

다음 레지스트리 변경 사항은 이 GDD에서 결정이 확정되었으며, `design/registry/entities.yaml`에 반영되어야 한다:

1. **`FOnDreamTriggered` delegate 신규 등록** — source: `dream-trigger-system.md`, referenced_by: `game-state-machine.md`
2. **`FDreamState` struct 신규 등록** — source: `dream-trigger-system.md`, referenced_by: `save-load-persistence.md`
3. **`UDreamDataAsset` source 이전** — `data-pipeline.md` → `dream-trigger-system.md`로 변경, CR-7에서 확정된 필드 목록(DreamId, BodyText, TriggerTagWeights, TriggerThreshold, RequiredStage, EarliestDay) 반영
4. **`FOnGameStateChanged` referenced_by 갱신** — `dream-trigger-system.md` 추가 (Interactions §5, PendingDreamId 클리어 구독)

이 항목들은 GDD 승인 후 `/consistency-check` 또는 수동으로 레지스트리에 기입할 것.

---

## 7. Tuning Knobs

| Knob | 타입 | Full 기본값 | MVP 기본값 | 안전 범위 | 게임플레이 영향 |
|---|---|---|---|---|---|
| `MinDreamIntervalDays` | int32 | 4 | 2 | [3, 7] (Full) / [1, 3] (MVP) | **Pillar 3 일차 제어**. 낮출수록 꿈이 잦아져 희소성 파괴 위험. 3 미만은 Pillar 3 위반 경고. 7 초과 시 21일에 3회 최소 보장 수학적 달성 불가 |
| `MaxDreamsPerPlaythrough` | int32 | 5 | 2 | [3, 7] (Full) / [1, 3] (MVP) | 플레이스루당 최대 꿈 횟수. 3 미만이면 내러티브 아크 구성 불가. 7 초과 시 Pillar 3 파괴 |
| `DefaultDreamId` | FName | "" | "" | — | 태그 조건 충족 후보 없을 때 폴백. 비어있으면 폴백 없음 (꿈 없음). EC-1·EC-3 대비 기본 꿈 1개 등록 권장 |
| `MinimumGuaranteedDreams` | int32 | 1 | 1 | [1, 3] | 플레이스루 최소 보장 꿈 수. EC-1 강제 트리거 임계. 1 이상이면 꿈 없는 플레이스루 방지 |
| `EarliestDreamDay` | int32 | 2 | 2 | [2, 5] | 꿈이 나올 수 있는 최초 일차. Day 1은 항상 제외(CR-1 Guard). 5 초과 시 초기 탐색기 꿈 완전 배제 |
| `TagScoreEvaluationMode` | enum | `NormalizedDot` | `NormalizedDot` | {NormalizedDot, RawDot} | 태그 점수 계산 방식. NormalizedDot(기본)=V_norm 사용. RawDot=V 직접 사용(누적 절대값 반영). 디버그·프로토타입용 |

**외부화**: 모든 Knob은 `UDreamConfigAsset : UPrimaryDataAsset`에 `UPROPERTY(EditAnywhere)` 필드로 노출. Data Pipeline 경유 로드. MVP에서는 별도 MVP 오버라이드 필드 (`MVPMaxDreams`, `MVPMinInterval`)를 함께 노출하여 `GameDurationDaysOverride == 7`이면 자동 적용.

**Pillar 3 안전 경계선**:

| 조합 | 위험도 | 판정 |
|---|---|---|
| `MinDreamIntervalDays` < 3 | 높음 | Pillar 3 경고 (에디터 자동 경고) |
| `MaxDreamsPerPlaythrough` > 7 | 높음 | Pillar 3 위반 |
| `MinDreamIntervalDays` = 4, `MaxDreamsPerPlaythrough` = 5 | 없음 | 목표 범위 ✓ |
| `MinDreamIntervalDays` = 7, `MaxDreamsPerPlaythrough` = 3 | 낮음 | F3 검증 필요 (3회 보장 수학적 위험) |

**Knob 상호작용**:
- `MinDreamIntervalDays`와 `MaxDreamsPerPlaythrough`는 F3 공식으로 상호작용. 두 값의 조합이 [3, 5] 범위를 보장하는지 에디터 자동 검증 추가 권고.
- `EarliestDreamDay`가 높을수록 F3의 `EarliestPossibleDreamDay`가 올라가 예상 꿈 횟수 감소. `EarliestDreamDay = 5` + `MinDreamIntervalDays = 7`이면 수학적으로 2회 이상 불가 → 최소 보장 달성 불가 에디터 경고.

---

## 8. Acceptance Criteria

### 희소성 강제 (Pillar 3 수호)

**AC-DTS-01** | `AUTOMATED` | BLOCKING
- **GIVEN** `FDreamState.LastDreamDay = 5`, `CurrentDayIndex = 8`, `MinDreamIntervalDays = 4`, **WHEN** `FOnCardOffered` 수신, **THEN** `DaysSinceLastDream == 3` (< MinDreamIntervalDays) → 꿈 트리거 없음, `FOnDreamTriggered` 미발행, `DreamsUnlockedCount` 불변

**AC-DTS-02** | `AUTOMATED` | BLOCKING
- **GIVEN** `FDreamState.LastDreamDay = 4`, `CurrentDayIndex = 8`, `MinDreamIntervalDays = 4`, **WHEN** `FOnCardOffered` 수신 + TagScore 조건 충족 후보 존재, **THEN** `DaysSinceLastDream == 4` (== MinDreamIntervalDays) → 게이트 A 통과 → 꿈 트리거 가능

**AC-DTS-03** | `AUTOMATED` | BLOCKING
- **GIVEN** `DreamsUnlockedCount = 5`, `MaxDreamsPerPlaythrough = 5`, **WHEN** `FOnCardOffered` 수신 + TagScore 조건 충족 후보 존재, **THEN** 게이트 B 실패 → `FOnDreamTriggered` 미발행, `DreamsUnlockedCount` 불변 (5 초과 없음)

**AC-DTS-04** | `AUTOMATED` | BLOCKING
- **GIVEN** `DreamsUnlockedCount = 0`, `MinimumGuaranteedDreams = 3`, `MaxDreamsPerPlaythrough = 5`, `CurrentDayIndex = GameDurationDays(21)`, **WHEN** `FOnCardOffered` 수신, **THEN** `DreamsUnlockedCount(0) < MinimumGuaranteedDreams(3)` → 태그 조건·희소성 게이트·EarliestDay 무관하게 강제 트리거 1회 → `FOnDreamTriggered` 발행 1회, `DreamsUnlockedCount == 1` (EC-1)

### 트리거 조건 검증

**AC-DTS-05** | `AUTOMATED` | BLOCKING
- **GIVEN** `TagVector = {Season.Spring: 6, Emotion.Calm: 3, Element.Water: 1}` (TotalWeight=10), Dream "봄비" `TriggerTagWeights = {Season.Spring: 0.70, Emotion.Calm: 0.20, Element.Water: 0.10}`, `TriggerThreshold = 0.50`, **WHEN** TagScore 계산, **THEN** `TagScore ≈ 0.509` (±1e-3) ≥ 0.50 → 후보 등록

**AC-DTS-06** | `AUTOMATED` | BLOCKING
- **GIVEN** 동일 TagVector, Dream "어둠" `TriggerTagWeights = {Element.Earth: 0.80, Emotion.Melancholy: 0.20}`, `TriggerThreshold = 0.30`, 해당 태그들이 TagVector에 없음, **WHEN** TagScore 계산, **THEN** `TagScore == 0.0` < 0.30 → 후보 미등록

**AC-DTS-07** | `AUTOMATED` | BLOCKING
- **GIVEN** 후보 2개: Dream "A" TagScore = 0.55, Dream "B" TagScore = 0.72, 모두 미열람, **WHEN** CR-4 선택, **THEN** `SelectedDream.DreamId == "B"` (최고 점수 선택)

**AC-DTS-08** | `AUTOMATED` | BLOCKING
- **GIVEN** 후보 2개: Dream "A" TagScore = 0.60, Dream "B" TagScore = 0.60 (동점), **WHEN** CR-4 선택, **THEN** `SelectedDream.DreamId == "A"` ("A" < "B" FName 사전순, EC 동점 처리)

**AC-DTS-09** | `AUTOMATED` | BLOCKING
- **GIVEN** `EGrowthStage.Sprout`, Dream "성숙의 꿈" `RequiredStage = EGrowthStage.Mature`, TagScore 조건 충족, **WHEN** 후보 수집, **THEN** 해당 꿈 후보 미등록 (성장 단계 필터 CR-3 조건 2)

**AC-DTS-10** | `AUTOMATED` | BLOCKING
- **GIVEN** 후보 후보 목록에 이미 `UnlockedDreamIds`에 있는 꿈 포함, **WHEN** 후보 수집, **THEN** 해당 꿈 제외 (CR-3 조건 1 — 이미 본 꿈)

### 상태 저장 및 복원

**AC-DTS-11** | `AUTOMATED` | BLOCKING
- **GIVEN** 꿈 트리거 완료, **WHEN** CR-4 실행 후, **THEN** `FDreamState.UnlockedDreamIds` 갱신, `LastDreamDay == CurrentDayIndex`, `DreamsUnlockedCount` 이전값 + 1, `SaveAsync(EDreamReceived)` 호출 1회

**AC-DTS-12** | `CODE_REVIEW` | BLOCKING
- **GIVEN** CR-4 구현 소스, **WHEN** 정적 분석, **THEN** `FDreamState` 갱신 (항목 1-3) → `SaveAsync` 호출 → `FOnDreamTriggered` 발행 순서가 단일 call stack 내에서 보장됨 (`co_await`, `AsyncTask` 개입 없음)

**AC-DTS-13** | `INTEGRATION` | BLOCKING
- **GIVEN** 꿈 트리거 완료 후 `FDreamState = {UnlockedDreamIds: ["DreamA"], LastDreamDay: 5, DreamsUnlockedCount: 1}` 세이브, **WHEN** 앱 재시작 후 Load 완료, **THEN** 복원된 `FDreamState`가 저장 전 값과 동일, `DreamsUnlockedCount == 1`, 게이트 B 올바르게 4회 남음

**AC-DTS-14** | `INTEGRATION` | BLOCKING
- **GIVEN** `PendingDreamId = "DreamX"` (미표시 꿈), **WHEN** 앱 재시작 후 GSM이 `Waiting` 상태에서 안정화, **THEN** Dream Trigger가 `FOnDreamTriggered("DreamX")` 재발행 1회 (EC-5)

### 평가 무결성

**AC-DTS-15** | `AUTOMATED` | BLOCKING
- **GIVEN** `EGameState == Farewell`, **WHEN** `FOnCardOffered` 수신, **THEN** 평가 스킵 → `FOnDreamTriggered` 미발행 (EC-6)

**AC-DTS-16** | `AUTOMATED` | BLOCKING
- **GIVEN** `TotalWeight == 0` (GetTagVector 빈 맵), **WHEN** TagScore 계산, **THEN** `TagScore == 0.0` (TotalWeight = 0 가드, F1)

**AC-DTS-17** | `AUTOMATED` | BLOCKING
- **GIVEN** `GetAllDreamAssets()` 빈 배열 반환 (DataPipeline DegradedFallback), **WHEN** `FOnCardOffered` 수신, **THEN** `FOnDreamTriggered` 미발행, `LogDreamTrigger Warning` 1회 출력, `FDreamState` 불변 (EC-8)

**AC-DTS-18** | `AUTOMATED` | BLOCKING
- **GIVEN** `CurrentDayIndex - LastDreamDay` 가 음수를 반환하는 잘못된 저장 데이터, **WHEN** DaysSinceLastDream 계산, **THEN** `max(0, ...)` 가드 적용 → 음수 불가, 게이트 A 실패 (꿈 미트리거) (EC-9)

### 꿈 빈도 목표

**AC-DTS-19** | `AUTOMATED` | ADVISORY
- **GIVEN** Full 기본 파라미터 (`MinDreamIntervalDays=4`, `MaxDreamsPerPlaythrough=5`, `EarliestDreamDay=2`), **WHEN** F3 공식 실행, **THEN** `ExpectedDreamCount ∈ [3.0, 5.0]` (목표 3-5회/21일 범위 내) — 파라미터 변경 시 자동 회귀

**AC-DTS-20** | `MANUAL` | ADVISORY
- **GIVEN** 7일 MVP 플레이스루 (반복 5회), **WHEN** 각 플레이스루에서 꿈 수령 횟수 기록, **THEN** 최소 1회 이상 꿈 수령 (EC-1 보장), 7일 내 꿈 없는 플레이스루 0회. 수동 검증: 일기 화면 열람 후 꿈 텍스트가 올바르게 표시됨 확인

---

## Implementation Notes

### UE 5.6 구현 참고

**Subsystem 구조**: `UDreamTriggerSubsystem : UGameInstanceSubsystem`. `Initialize()` 내에서 `Collection.InitializeDependency<UDataPipelineSubsystem>()` 선언 필수 (Data Pipeline R2 BLOCKER 5).

**이벤트 구독 (ADR-0005 2026-04-19)**: `FOnCardProcessedByGrowth` 구독은 `Initialize()` 내 `GetGameInstance()->GetSubsystem<UGrowthAccumulationSubsystem>()->OnCardProcessedByGrowth.AddDynamic(this, &OnCardProcessedByGrowthHandler)` 패턴. `Deinitialize()`에서 `RemoveDynamic` 해제 필수. Card System `FOnCardOffered`는 **구독하지 않음** (Growth가 Stage 2 forwarder).

**FOnDreamTriggered 선언**:
```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDreamTriggered, FName, DreamId);
UPROPERTY(BlueprintAssignable)
FOnDreamTriggered OnDreamTriggered;
```

**UDreamDataAsset PrimaryAssetType 등록**: `DefaultEngine.ini`의 `[/Script/Engine.AssetManagerSettings]`에 `DreamData` PrimaryAssetType 등록 필수 (Data Pipeline R2 BLOCKER 4). 미등록 시 `GetAllDreamAssets()` 빈 결과.

**FDreamState 직렬화**: `UMossSaveData`에 `UPROPERTY() FDreamState DreamState` 필드 추가. `TArray<FName>`, `int32`, `FName` 모두 `FMossSaveSnapshot` POD-only whitelist 허용 타입.

**TagScore 계산 최적화**: 매 카드 건넴마다 전체 꿈 목록을 순회한다. Full Release 50개 꿈에서도 O(50 × 태그수)로 충분히 빠름 (프레임 예산 내). `UDataPipelineSubsystem` Ready 확인 후 캐싱된 자산 목록 사용.

### 프로토타입 우선 사항 (P2 Spike)

systems-index.md가 "Prototype Early"를 명시한 시스템이다. GDD 확정 전에 다음을 검증:
- `MinDreamIntervalDays = 4` 기본값이 "예고 없이 찾아오는 계절" 감각을 만드는지
- `TriggerThreshold = 0.50` 기본값이 "태그를 어느 정도 쌓아야 꿈이 온다"는 느낌을 주는지
- 7일 MVP에서 2회 꿈 수령이 충분한 내러티브 감각을 제공하는지

---

## Open Questions

| # | Question | Owner | Target | Status |
|---|---|---|---|---|
| OQ-DTS-1 | `FOnDreamTriggered` delegate 발행 직후 GSM이 연기할 때 `PendingDreamId` 기반 재시도 UX — 다음 세션에서 자동 표시 vs. 다음 `FOnCardOffered`까지 대기 중 어느 쪽이 더 자연스러운가? | Game Designer | GDD 승인 후 프로토타입 | Open |
| OQ-DTS-2 | Dream Journal UI가 꿈을 표시하는 순간(GSM Dream 상태) 이후 Waiting 복귀 시 `PendingDreamId`를 지워야 하는가, 아니면 Dream Trigger가 GSM `FOnGameStateChanged(Dream→Waiting)` 구독으로 직접 처리하는가? | Systems Designer + GSM | Dream Journal UI GDD | **RESOLVED** (2026-04-18): Dream Trigger가 `FOnGameStateChanged` 구독, `OldState == Dream`이면 무조건 `PendingDreamId = NAME_None` 클리어 + SaveAsync. §3 Interaction 5 참조. |
| OQ-DTS-3 | Full Release 8-12종 최종 형태별로 특화된 꿈이 필요한가? 현재 GDD는 형태-꿈 연결이 없음 (`RequiredStage`만 있음). 형태 ID를 `UDreamDataAsset`에 추가할지 여부 | Narrative Director + Game Designer | Full Scope | Open |
| OQ-DTS-4 | MVP 5개 꿈 콘텐츠 작성 일정 — game-concept.md가 "GDD 작성과 병행이 아닌 사전 작업"으로 권고. 꿈 텍스트 작가 또는 LLM 생성 + 편집 방식? | Producer + Narrative Director | MVP 구현 전 | Open |
| OQ-DTS-5 | TagScore 임계(`TriggerThreshold`)의 적정값 — 0.50이 너무 높아 꿈이 잘 안 나오는가, 너무 낮아 희소성이 태그 다양성 없이도 달성되는가? P2 Spike에서 측정 필요 | Systems Designer | P2 Prototype | Open |

---

## 레지스트리 등록 요청

이 GDD에서 정의된 다음 크로스-시스템 사실을 `design/registry/entities.yaml`에 추가할 것을 권고합니다:

**신규 Delegate**:
- `FOnDreamTriggered` — source: `dream-trigger-system.md`, referenced_by: GSM, Dream Journal UI

**신규 Struct**:
- `FDreamState` — source: `dream-trigger-system.md`, referenced_by: Save/Load

**기존 항목 갱신**:
- `UDreamDataAsset` — source를 `data-pipeline.md` → `dream-trigger-system.md`로 이전, 필드 목록 확장
- `FOnDreamTriggered`를 GSM `referenced_by`에 추가
- `ESaveReason.EDreamReceived`는 이미 레지스트리에 등록됨 (confirmed)
