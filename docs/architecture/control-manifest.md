# Control Manifest

> **Engine**: Unreal Engine 5.6 (hotfix 5.6.1 권장)
> **Last Updated**: 2026-04-19
> **Manifest Version**: 2026-04-19
> **ADRs Covered**: ADR-0001, ADR-0002, ADR-0003, ADR-0004, ADR-0005, ADR-0006, ADR-0007, ADR-0008, ADR-0009, ADR-0010, ADR-0011, ADR-0012, ADR-0013
> **Status**: Active — regenerate with `/create-control-manifest` when ADRs change
> **Review mode**: lean (TD-MANIFEST skipped — Lean mode)

`Manifest Version`은 이 manifest가 생성된 날짜이다. Story 파일은 생성 시 이 날짜를 embed하며, `/story-readiness`가 story의 embedded version과 비교하여 stale rules 대비 작성된 story를 감지한다. `Last Updated`와 동일하되 다른 소비자를 위한 별도 필드.

이 manifest는 **모든 Accepted ADR + technical-preferences + engine-reference**에서 추출한 프로그래머 quick-reference다. 각 rule의 근거(rationale)는 참조된 ADR을 확인한다.

---

## Foundation Layer Rules

*Applies to: Time & Session System (#1), Data Pipeline (#2), Save/Load Persistence (#3), Input Abstraction Layer (#4)*

### Required Patterns

- **Forward time tamper는 정상 경과와 bit-exact 동일 처리** — 분기·탐지·로깅 일체 없음 — source: ADR-0001
- **Time & Session 8-Rule Classifier는 `IMossClockSource`만 통해 시간 입력** — `FDateTime::UtcNow()` / `FPlatformTime::Seconds()` 직접 호출은 `IMossClockSource` 구현체 내부에만 허용 — source: ADR-0001
- **Data Pipeline 컨테이너 선택** — Card = `UDataTable` + `FGiftCardRow` USTRUCT, FinalForm = `UMossFinalFormAsset : UPrimaryDataAsset` (per-form asset), Dream = `UDreamDataAsset : UPrimaryDataAsset` (per-dream), Stillness = `UStillnessBudgetAsset : UPrimaryDataAsset` (single instance) — source: ADR-0002, ADR-0010
- **`UDataPipelineSubsystem::Initialize()`는 sync 순차 로드** — 4단계 순서 고정: Card DataTable → FinalForm DataAsset bucket → Dream DataAsset bucket → Stillness single DataAsset — source: ADR-0003
- **Pipeline pull API는 전부 sync return** — `TOptional<FRow>` / `UObject*` / `TArray<T>` — downstream은 Ready 상태 확인 후에만 조회 — source: ADR-0003
- **DefaultEngine.ini `[/Script/Engine.AssetManagerSettings]`에 `PrimaryAssetTypesToScan` 3종 등록** — DreamData / FinalForm / StillnessBudget — AC-DP-18 [5.6-VERIFY] 대상 — source: ADR-0002
- **`UPrimaryDataAsset` 상속 클래스는 `GetPrimaryAssetId()` override** — `FPrimaryAssetId("[Type]", [Id])` 반환 — source: ADR-0002, ADR-0010
- **Downstream Subsystem Initialize는 `InitializeDependency<UDataPipelineSubsystem>()` 호출** — 생성자·다른 멤버 함수에서 호출 금지 (Initialize override 내부 전용) — source: ADR-0003
- **Save/Load는 per-trigger atomicity만 보장** — in-memory mutation + `SaveAsync(reason)`는 동일 game-thread 함수 — source: ADR-0009, Principle 3
- **Compound 이벤트는 `GSM->BeginCompoundEvent(PrimaryReason)` / `EndCompoundEvent()` boundary로 batch** — Save/Load coalesce 정책 활용으로 단일 commit — source: ADR-0009
- **PIE hot-swap `RefreshCatalog()`는 `Loading` 상태 재진입 차단** — AC-DP-14 — source: ADR-0003

### Forbidden Approaches

- **Never use 변수·함수 이름에 `tamper`, `cheat`, `bIsForward`, `bIsSuspicious`, `DetectClockSkew`, `IsSuspiciousAdvance` 패턴** — Pillar 1 위반 + AC `NO_FORWARD_TAMPER_DETECTION` grep 차단 — source: ADR-0001
- **Never compare `WallDelta > expected_max` for suspicion** — algorithmically indistinguishable from 정상 경과 — source: ADR-0001
- **Never add anti-tamper hash·signature·file integrity 검증** — `USaveGame` CRC32 외 추가 금지. CRC32는 무결성만, "치트 감지"에 연결 안 됨 — source: ADR-0001
- **Never use `UDataRegistrySubsystem`** — 5.6 production 검증 부족 (MEDIUM-HIGH risk). 100행 이하 규모에서 이점 없음 — source: ADR-0002
- **Never define `int32`/`float`/`double` 수치 효과 필드 on `FGiftCardRow` USTRUCT** — C1 schema gate (Pillar 2 보호). 허용 타입: `FName`, `TArray<FName>`, `FText`, `FSoftObjectPath` — source: ADR-0002
- **Never hardcode Dream trigger 임계 in code** — `UDreamDataAsset`의 `UPROPERTY(EditAnywhere)` 필드(`TriggerThreshold`, `TriggerTagWeights`)만 사용 — C2 schema gate (Pillar 3 보호) — source: ADR-0002
- **Never use async bundle loading for all catalogs** — R3 sync contract 깨짐. async는 조건부 fallback (T_init > 75ms 시 별도 ADR로 전환) — source: ADR-0003
- **Never use lazy load (첫 조회 시 로드)** — 첫 프레임 이후 지연 스파이크 발생 — source: ADR-0003
- **Never write sequence-level atomicity API in Save/Load layer** — Compound atomicity는 GSM boundary 책임 (Principle 3) — source: ADR-0009

### Performance Guardrails

- **`UDataPipelineSubsystem::Initialize()` T_init** — SSD warm ≤ 50ms (Full Vision ~8.2ms 예상) / 50-52.5ms Warning / 52.5-75ms Error + Async Bundle 전환 검토 flag / >100ms Fatal (checkf/UE_LOG Fatal로 등록 차단) — source: ADR-0003
- **Data Pipeline CPU-side catalog memory** — Full Vision ~120KB (1.5GB ceiling의 0.01%) — source: ADR-0002, ADR-0003
- **HDD cold-start known compromise** — ~61ms T_init (Error 영역) 수용. Steam Hardware Survey 2025 90%+ SSD 근거 — source: ADR-0003

---

## Core Layer Rules

*Applies to: Game State Machine (#5), Moss Baby Character System (#6), Lumen Dusk Scene (#11)*

### Required Patterns

- **GSM `TickMPC()` 내 순차 갱신**: (1) MPC scalar 8개 + AmbientRGB vector → (2) DirectionalLight Temperature/Intensity/Rotation + SkyLight Intensity 직접 set → (3) 단일 함수 내 완료 — source: ADR-0004
- **`UMossGameStateSubsystem::RegisterSceneLights(ADirectionalLight*, ASkyLight*)` API 노출** — Lumen Dusk Scene Actor BeginPlay에서 호출 — source: ADR-0004
- **DirectionalLight Mobility = `Stationary`** (Lumen 직접광 + 부분 Shadow baking), **SkyLight Mobility = `Movable`** (실시간 앰비언트) — source: ADR-0004
- **`PP_MossDusk` Post Process Volume이 Vignette/DoF/Contrast 소유** — `FOnGameStateChanged` 구독 + 자체 Lerp. 씬 머티리얼은 MPC scalar read-only 참조 — source: ADR-0004
- **PSO Precaching 3-Phase 전략** — (1) Build: `r.ShaderPipelineCache.Enabled 1` + `PSO_MossDusk.bin` 번들 생성, (2) Runtime: Pipeline Ready 후 비동기 로드 + `FShaderPipelineCache::SetBatchMode(BatchMode::Fast)`, (3) Dawn Entry: `ALumenDuskSceneActor::IsPSOReady()` 확인, 10초 타임아웃 후 DegradedFallback — source: ADR-0013
- **PSO 번들 부재·손상 시 silent DegradedFallback** — `UE_LOG(Warning)` 1회 + 점진 컴파일 정상 진행, 에러 다이얼로그 금지 — source: ADR-0013
- **GSM이 Offering→Waiting / Farewell P0 전환은 `FOnCardProcessedByGrowth` (Stage 2)만 트리거** — `bIsDay21Complete` flag 분기 — source: ADR-0005

### Forbidden Approaches

- **Never call MPC `SetScalarParameterValue` / `SetVectorParameterValue` outside GSM `TickMPC`** — GSM은 MPC의 유일 writer. AC-LDS-06 grep = 0건 — source: ADR-0004
- **Never use Blueprint Construction Script / Tick으로 MPC → Light Actor 복사** — tick priority 비결정성, debug 어려움, cycle 리스크 — source: ADR-0004
- **Never use splash screen to precompile all PSOs** — Pillar 1 위반 — source: ADR-0013
- **Never enable PSO precaching in Editor/PIE** — Shipping/Cooked 빌드만 적용, Editor/PIE는 점진 컴파일이 정상 — source: ADR-0013
- **Never use Gameplay Camera System auto-spawn path** — UE 5.6 제거됨. 명시적 Camera Component 배치 + Sequencer 사용 — source: deprecated-apis.md
- **Never subscribe GSM to Stage 1 `FOnCardOffered`** — Stage 2 `FOnCardProcessedByGrowth` 전용 (ADR-0005 Migration 완료) — source: ADR-0005

### Performance Guardrails

- **Lumen HWRT GPU budget ≤ 5ms** (`LumenGPUBudgetMs`) — 6 GameState 각각 측정. 초과 시 SWRT 자동 폴백 분기 (Lumen Dusk §E1) — AC-LDS-04 [5.6-VERIFY] — source: ADR-0004
- **GSM `TickMPC` CPU cost** — per-frame 8 MPC set + 3-4 Light Actor property set + 1 SkyLight set ≈ 50-100μs (16.6ms budget의 0.5%) — source: ADR-0004
- **PSO Warmup timeout** — `PSOWarmupTimeoutSec` 기본 10.0초 (Dawn 최소 체류 3초 내 대부분 완료 목표) — source: ADR-0013
- **Waiting→Dream Temperature 전환 frame delta ≤ 100K/frame @60fps** — AC-LDS-08 — source: ADR-0004

---

## Feature Layer Rules

*Applies to: Growth Accumulation Engine (#7), Card System (#8), Dream Trigger System (#9), Stillness Budget (#10)*

### Required Patterns

- **2단계 Delegate 패턴**:
  - **Stage 1** `FOnCardOffered(FName CardId)` — Card System publisher. **Subscribers**: Growth (CR-1 태그 가산) + MBC (Reacting 즉시 진입 — 시각 즉시성)
  - **Stage 2** `FOnCardProcessedByGrowth(const FGrowthProcessResult&)` — Growth publisher. **Subscribers**: GSM (Offering→Waiting / Day 21 Farewell P0) + Dream Trigger (CR-1 평가 — 최신 TagVector 보장)
  - source: ADR-0005
- **Growth `OnCardOfferedHandler`의 마지막 statement는 `OnCardProcessedByGrowth.Broadcast(Result)`** — C++ call stack 순서로 Growth → GSM/Dream 전개 강제 — source: ADR-0005
- **`FGrowthProcessResult` struct payload**: `FName CardId` + `bool bIsDay21Complete` + `FName FinalFormId` — Stage 2 delegate — source: ADR-0005
- **Card 풀 샘플링은 Eager** — `FOnDayAdvanced(NewDayIndex)` 수신 즉시 `RegenerateDailyHand` 호출 + Ready 상태 진입 — source: ADR-0006
- **`FRandomStream` 외부 주입** — `PlaythroughSeed + DayIndex` HashCombine으로 결정적 시드 — source: ADR-0006, card-system.md
- **Stillness Budget ↔ Engine concurrency Hybrid**:
  - 단일 `USoundConcurrency SC_MossBaby_Stillness` (`MaxCount = 32`, `Resolution = StopOldest`)를 모든 게임 SFX/ambient에 적용
  - Niagara Effect Type 3종: `EFT_MossBaby_Background`, `EFT_MossBaby_Standard`, `EFT_MossBaby_Narrative`
  - Sound request 순서: (1) Stillness Budget `Request(Channel, Priority)` → (2) Engine spawn (SoundConcurrency 자동 적용) → (3) RAII Release
  - source: ADR-0007
- **Growth `int32 GetLastCardDay() const` API**: return 0 = 미건넴 (sentinel), > 0 = 해당 day — source: ADR-0012
- **`FFinalFormRow` 격상**: `UMossFinalFormAsset : UPrimaryDataAsset` per-form. `Content/Data/FinalForms/` 폴더에 배치. `RequiredTagWeights: TMap<FName, float>` 필드 포함 — source: ADR-0010
- **Dream Trigger는 Stage 2 구독 only** — `FOnCardProcessedByGrowth` 수신 시 CR-1 평가 시작 — source: ADR-0005
- **카드 반응성 즉시 보장** — MBC는 Stage 1 `FOnCardOffered` 구독으로 Growth 처리 대기 없이 즉시 Reacting 진입 — source: ADR-0005

### Forbidden Approaches

- **Never subscribe MBC to Stage 2** — 시각 즉시성 우선 (Pillar 2 "즉시 피드백") — source: ADR-0005
- **Never broadcast `FOnCardOffered` outside Card System** — Card가 유일 writer. `grep "OnCardOffered.Broadcast" Source/MadeByClaudeCode/` = Card System 파일만 매치 — source: ADR-0005
- **Never call Growth pull API from MBC inside Growth Handler window** — Layer Boundary 위반 위험. `GetMaterialHints()`는 Reacting 후 별도 이벤트에서 pull — source: ADR-0005
- **Never use UE Multicast Delegate `AddDynamicWithPriority`** — UE 공식 API 아님. 비공개 계약 의존 회피 — source: ADR-0005
- **Never lazy-sample card pool on Offering entry** — Dawn 체류 시간 증가 + Full 규모에서 timeout 마진 감소 — source: ADR-0006
- **Never bypass Stillness Budget for game events** — 엔진 concurrency만 거쳐 Stillness Budget Request 스킵하는 경로 금지 (Pillar 1/3 보호) — source: ADR-0007
- **Never use `TOptional<int32>` or `bool TryGetLastCardDay(int32&)` for `GetLastCardDay()`** — int32 sentinel 0 채택 (단순성) — source: ADR-0012
- **Never store `FFinalFormRow` as `UDataTable` rows** — TMap 인라인 편집 미지원 (CR-8 문제). `UMossFinalFormAsset` per-form 자산 사용 — source: ADR-0010

### Performance Guardrails

- **Card 샘플링 CPU cost** — MVP ≈ 0.1ms, Full 40장 < 1ms (FRandomStream 3회 × 40장 가중 탐색). GameInstance::Init 50ms budget의 ≤ 1% — source: ADR-0006
- **Stage 1 + Stage 2 Multicast broadcast cost** — 4 subscriber 기준 ≈ 4-20μs 추가 (16.6ms budget의 0.1%) — source: ADR-0005
- **Engine voice limit** — `SC_MossBaby_Stillness MaxCount = 32` (게임 레이어 3 슬롯의 ~10× 여유). VS playtest 시 `stat audio` 측정, >10 voice 시 디자인 검토 — source: ADR-0007
- **Niagara 파티클 수 상한 per 씬** — 200개 (데스크톱 메모리 제약) — source: current-best-practices.md §3

---

## Presentation Layer Rules

*Applies to: Card Hand UI (#12), Dream Journal UI (#13)*

### Required Patterns

- **Card Hand UI 드래그는 Enhanced Input Subscription**:
  - `IA_OfferCard` Hold `ETriggerEvent::Triggered` → `OnOfferCardHoldStarted` (드래그 시작 + Stillness Request)
  - `IA_OfferCard` `ETriggerEvent::Completed` → `OnOfferCardReleased` (확정/취소 분기)
  - `IA_PointerMove` Axis2D `ETriggerEvent::Triggered` → `OnPointerMove` (드래그 중 위치 갱신)
  - source: ADR-0008
- **Card Hand UI `NativeConstruct` override에서 Enhanced Input Component 바인딩** — `Cast<UEnhancedInputComponent>(PC->InputComponent)->BindAction(...)` — source: ADR-0008
- **`NativeOnMouseEnter` / `NativeOnMouseLeave`는 시각 highlight 전용** — 게임 상태 변경·드래그 시작 금지 (Pattern 2 Hover-only 금지 준수) — source: ADR-0008
- **Drag state: `Idle → Dragging → (Offering | Idle)`** — `Dragging` 중 다른 `OnOfferCardHoldStarted` 무시 (EC-CHU-8) — source: ADR-0008
- **Dream Journal UI는 `SaveAsync()` 직접 호출 금지** — `bHasUnseenDream = false`는 in-memory 변경만, 다음 저장 이유에 포함 — AC-DJ-17 — source: dream-journal-ui.md

### Forbidden Approaches

- **Never override `NativeOnMouseButtonDown` / `NativeOnMouseMove` / `NativeOnMouseButtonUp`** — raw event 처리 = Input Abstraction Rule 1 위반 — source: ADR-0008
- **Never use `UDragDropOperation`** — Enhanced Input과 통합 어려움 + Gamepad alternative 분기 코드 폭증 — source: ADR-0008
- **Never make drag start on hover-only** — Steam Deck 컨트롤러 모드에서 hover 개념 없음 (Pattern 2) — source: ADR-0008, ux/interaction-patterns.md
- **Never call `SaveAsync()` from Dream Journal UI source** — `grep "SaveAsync" Source/MadeByClaudeCode/UI/Dream*` = 0건 — source: dream-journal-ui.md AC-DJ-17

### Performance Guardrails

- **Card Hand UI `OnPointerMove` cost** — 드래그 중에만 실행, Idle/Hidden 상태 cost 0 — source: ADR-0008
- **Hold threshold** — Mouse 0.15s / Gamepad 0.20s (Formula 2 Input Abstraction) — source: ADR-0008, input-abstraction-layer.md

---

## Meta Layer Rules

*Applies to: Window & Display Management (#14)*

### Required Patterns

- **`SWindow::FArguments::MinWidth/MinHeight` = 800/600** — OS 수준 최소 크기 강제 (EC-08) — source: window-display-management.md
- **Per-Monitor DPI Awareness V2** — 모니터 이동 시 `WM_DPICHANGED` 자동 재계산 — source: window-display-management.md
- **Window 포커스 상실 시 `bDisableWorldRendering = true`** — CPU/GPU 절약 (최소화 시) — source: window-display-management.md
- **Window 위치가 화면 밖이면 기본 모니터 중앙 이동 + 50% 가시성 보장** — EC-07 — source: window-display-management.md

### Forbidden Approaches

- **Never create modal dialog** — Rule 10, AC-WD-13, Pillar 1 위반 — source: window-display-management.md
- **Never auto-pause game on focus loss** — Rule 6, AC-WD-06 — source: window-display-management.md
- **Never auto-mute audio on focus loss** — Rule 6, AC-WD-06 — source: window-display-management.md
- **Never steal focus on always-on-top activation** — Rule 7, AC-WD-10 — source: window-display-management.md

---

## Cross-Layer Rules

### Required Patterns (Tuning Knob)

- **시스템 전역 상수 Tuning Knob은 `U[SystemName]Settings : UDeveloperSettings`**:
  - `UCLASS(Config=Game, DefaultConfig)` 매크로
  - `UPROPERTY(Config, EditAnywhere, Category="...")` 필드
  - Project Settings → Game → Moss Baby → [System] 카테고리 통합
  - 클래스 명명: `UMossTimeSessionSettings`, `UMossDataPipelineSettings`, `UMossSaveLoadSettings`, `UMossInputAbstractionSettings`, `UMossGameStateSettings`, `UMossCardSystemSettings`, `UMossDreamTriggerSettings`, `UMossWindowDisplaySettings` (기타 시스템별)
  - source: ADR-0011
- **Hot reload 불가 knob에는 `meta=(ConfigRestartRequired=true)` 명시** — PIE/Editor 재시작 필요 필드 식별 — source: ADR-0011
- **ini 파일 위치**: `Config/DefaultGame.ini` (game-wide) + `Saved/Config/WindowsNoEditor/Game.ini` (런타임 변경) — source: ADR-0011

### Forbidden Approaches (Tuning Knob)

- **Never read ini with `GConfig` directly for tuning knobs** — 타입 안전성 부재 — source: ADR-0011
- **Never use `const` / `static constexpr` for system-wide tuning values** — 재빌드 필수, 디자이너 변경 불가 — source: ADR-0011
- **Never use `UDataAsset` for system-wide constants** — per-instance 콘텐츠 데이터가 아니면 `UDeveloperSettings` 사용 — source: ADR-0011

### Exceptions (UPrimaryDataAsset 유지)

- **Per-instance 콘텐츠**: `UDreamDataAsset` (각 꿈별 트리거 임계), `UMossBabyPresetAsset` (성장 단계별 머티리얼 프리셋), `UCardSystemConfigAsset` 시즌 가중치
- **에디터 친화 다중 인스턴스**: `UStillnessBudgetAsset`, `UGameStateMPCAsset`, `ULumenDuskAsset` (단일 인스턴스지만 에디터 편집 워크플로우 유지)
- source: ADR-0011

---

## Global Rules (All Layers)

### Naming Conventions (UE Standard)

| Element | Convention | Example |
|---------|-----------|---------|
| Actor classes | `A` prefix + PascalCase | `AMossBaby`, `ALumenDuskSceneActor` |
| UObject classes | `U` prefix + PascalCase | `UGrowthAccumulationSubsystem`, `UMossFinalFormAsset` |
| Struct types | `F` prefix + PascalCase | `FGiftCardRow`, `FGrowthProcessResult`, `FSessionRecord` |
| Enum types | `E` prefix + PascalCase | `EGameState`, `EStillnessChannel` |
| Interface types | `I` prefix + PascalCase | `IMossClockSource` |
| Template types | `T` prefix + PascalCase | `TOptional<T>`, `TWeakObjectPtr<T>` |
| Variables | PascalCase | `GrowthStage`, `CurrentDay` |
| Functions | PascalCase | `OfferCard()`, `TriggerDream()` |
| Booleans | `b` prefix + PascalCase | `bHasDreamToday`, `bIsSleeping`, `bWithered` |
| Files | 클래스에서 prefix 제거 | `MossBaby.h`, `GiftCardComponent.h` |
| Folders | PascalCase 카테고리 | `Source/MadeByClaudeCode/Gameplay/`, `Source/MadeByClaudeCode/UI/` |
| Delegates | `F` prefix + `On` 패턴 | `FOnCardOffered`, `FOnDayAdvanced`, `FOnGameStateChanged` |

source: technical-preferences.md

### Performance Budgets

| Target | Value | Source |
|--------|-------|--------|
| Framerate | 60 fps locked | technical-preferences.md |
| Frame budget | 16.6 ms/frame | technical-preferences.md |
| Draw calls | < 500 | technical-preferences.md |
| Memory ceiling | 1.5 GB | technical-preferences.md |
| Particle count per scene | ≤ 200 | current-best-practices.md §3 |
| Window minimum size | 800×600 | window-display-management.md |

### Approved Libraries / Addons

- **UE 내장만 사용** (이 프로젝트는 외부 라이브러리 미사용)
- UE 5.6 Standard: Enhanced Input, UMG, Niagara, Lumen, MPC, USaveGame, UAssetManager, FTSTicker, UPrimaryDataAsset, UDeveloperSettings
- source: technical-preferences.md (`Allowed Libraries / Addons: None configured yet`)

### Forbidden APIs (UE 5.6)

Deprecated / 검증되지 않은 APIs — 사용 금지:

| API | Reason | Use Instead |
|---|---|---|
| Gameplay Camera System auto-spawn | 5.6에서 제거됨 | 명시적 Camera Component 배치 + Sequencer |
| 일부 Blueprint Camera Directors | Deprecated 노드 표시 | 대체 노드 안내 참조 |
| 일부 Blueprint Camera Node Evaluators | Deprecated 노드 표시 | 상동 |
| `TSubobjectPtr` | UE4 잔재 (Legacy) | `TObjectPtr<>` (UE 5.0+) |
| Blueprint `Delay` 노드 (장기 대기) | 세이브 로드 시 손실 | Timer Handle + 명시적 세이브 통합 |
| `FTimerManager` for 21일 실시간 추적 | 앱 종료 시 손실 | `FDateTime::UtcNow()` 기반 경과 계산 + 세이브 |
| `UDataRegistrySubsystem` | 5.6 production 검증 부족 | `UDataTable` + `UPrimaryDataAsset` (ADR-0002) |
| `ensure*` macros in Shipping critical paths | Shipping에서 strip됨 | `checkf` 또는 명시적 `if + UE_LOG Fatal` 패턴 |
| `NativeOnMouseButtonDown/Move/Up` override | Input Abstraction Rule 1 위반 | Enhanced Input `IA_*` subscription (ADR-0008) |
| `UDragDropOperation` | Enhanced Input 통합 어려움 | `IA_OfferCard` Hold + `IA_PointerMove` 구독 (ADR-0008) |

source: deprecated-apis.md, ADR-0002, ADR-0008

### Post-Cutoff API Verification Targets (5.6-VERIFY)

Implementation 단계 실기 검증 필수 4건:

| API | AC | ADR |
|---|---|---|
| `UAssetManager` `PrimaryAssetTypesToScan` cooked build 동작 | AC-DP-18 | ADR-0002, ADR-0010 |
| `UGameInstance::Init()` 동기 블로킹 timing + `UAssetManager::LoadPrimaryAssets` 동기 변종 | AC-DP-09/10 | ADR-0003 |
| UE 5.6 Lumen HWRT `r.Lumen.HardwareRayTracing` + `r.Lumen.SurfaceCacheResolution` | AC-LDS-04 | ADR-0004 |
| UE 5.6 `FShaderPipelineCache::SetBatchMode(BatchMode::Fast)` + `NumPrecompilesRemaining()` | AC-LDS-11 | ADR-0013 |

### Cross-Cutting Constraints

- **Save/Load modal UI · "저장 중..." 인디케이터 금지** — AC `NO_SAVE_INDICATOR_UI`, Pillar 1 — source: save-load-persistence.md
- **Save file 위치**: `FPlatformProcess::UserSettingsDir()` 하위 — source: current-best-practices.md §5
- **Day counter 영구 미표시** — systems-index.md R2 CLOSED, Pillar 1 — source: systems-index.md
- **수치·확률·태그 레이블 UI 표시 금지** — AC `NO_HARDCODED_STATS_UI`, Pillar 2 — source: card-system.md, dream-journal-ui.md
- **`Farewell → Any` 상태 전환 분기 금지** — AC-GSM-05 grep 0건, Pillar 4 — source: game-state-machine.md
- **UI 알림·팝업·모달·요구사항 알림 금지** — Pillar 1 전역 — source: game-concept.md
- **Input 장치 전환 알림 금지** — Pillar 1 — source: input-abstraction-layer.md
- **Hover-only 상호작용 금지** (Steam Deck 컨트롤러 모드 대응) — Pattern 2 — source: ux/interaction-patterns.md
- **SaveSchemaVersion bump 정책**: 필드 추가·삭제·재타입·의미 변경 시마다 +1 — source: save-load-persistence.md
- **USaveGame 필드는 전부 `UPROPERTY()` 표기** — GC 안전성 — source: current-best-practices.md §10
- **Raw pointer 대신 `TObjectPtr<>` 사용** (UE 5.0+) — source: current-best-practices.md §10
- **Tick 함수 남용 금지** — 이벤트/타이머 기반 우선, Tick은 렌더 관련에만 — source: current-best-practices.md §10
- **Blueprint에서 무거운 로직 금지** (카드 태그 누적·꿈 판정은 C++ 필수) — source: current-best-practices.md §10
- **Hardcoded 문자열 경로 금지** — `FSoftObjectPath` + DataTable 참조 — source: current-best-practices.md §10
- **Editor-only 기능 런타임 호출 금지** — `#if WITH_EDITOR` 가드 필수 — source: current-best-practices.md §10

---

## Architecture Principles Summary (Cross-Reference)

모든 rule은 5가지 Architecture Principles에 귀속 — [architecture.md §Architecture Principles](architecture.md) 참조:

1. **Pillar-First Engineering** — 성능·생산성 trade-off 시 Pillar 보호 우선
2. **Schema as Pillar Guard** — compile-time + editor save + CI grep 4-layer 강제
3. **Per-Trigger Atomicity, Sequence by GSM** — Save/Load = storage durability, GSM = game logic ordering
4. **Foundation Independence + Strict Layer Boundaries** — 단방향 의존 (상위 → 하위), Foundation 4 시스템 간 의존 0, Engine API 직접 호출은 Foundation/Meta + ADR 명시 예외(GSM Light Actor, Lumen Dusk PSO/HWRT)
5. **Engine-Aware Design with Explicit Mitigation** — HIGH/MEDIUM risk 도메인을 GDD 단계에서 [5.6-VERIFY] AC로 명시

---

## Regeneration

이 manifest는 **ADR이 새로 Accepted되거나 기존 ADR이 revised**되면 재생성한다:

```
/create-control-manifest
```

재생성 시:
- Story 파일의 `Manifest Version` embed와 이 파일의 `Manifest Version` 비교
- 불일치 story는 `/story-readiness`에서 STALE RULES 마킹
