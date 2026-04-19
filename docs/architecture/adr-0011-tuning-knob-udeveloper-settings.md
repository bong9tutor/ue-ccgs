# ADR-0011: Tuning Knob 노출 방식 — `UDeveloperSettings` 정식 채택

## Status

**Accepted** (2026-04-19 — C6 해결 ConfigRestartRequired convention 정식화. Growth/Card/Dream Trigger §Tuning Knobs 분리는 각 sprint 진입 시 GDD 갱신)

## Date

2026-04-18

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 (hotfix 5.6.1 권장) |
| **Domain** | Core / Configuration |
| **Knowledge Risk** | LOW — `UDeveloperSettings`는 UE 5.0+ 안정 API, 5.6 변경 없음 (`docs/engine-reference/unreal/breaking-changes.md` 검증) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `breaking-changes.md`, `current-best-practices.md`, `docs/architecture/architecture.md §Architecture Principles §Principle 5` |
| **Post-Cutoff APIs Used** | None — `UDeveloperSettings`는 4.x 시대부터 안정 API |
| **Verification Required** | 각 시스템의 Tuning Knob class가 `UDeveloperSettings` 상속 + `UCLASS(Config=Game, DefaultConfig)` 매크로 사용 (CI grep) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None |
| **Enables** | 14개 시스템의 일관된 Tuning Knob 노출. Project Settings → "Moss Baby" 카테고리 자동 통합. ini 외부화 + hot reload. ADR-0002/0003 (Pipeline) 결정 시 Pipeline Tuning Knob도 본 패턴 채택 |
| **Blocks** | None — 다른 ADR/Epic 작성을 막지 않음. 단, Foundation sprint 시작 전 Accepted 권장 (Tuning Knob 일관성 강제) |
| **Ordering Note** | ADR-0001과 묶어서 즉시 작성 가능 (결정 사항 명확). Foundation/Core/Feature/Presentation/Meta 모든 layer의 Tuning Knob class 작성 시 본 ADR 참조 |

## Context

### Problem Statement

Moss Baby의 14개 MVP 시스템 모두 Tuning Knob 섹션을 보유하며 (각 GDD §Tuning Knobs), 각 시스템이 자체적으로 노출 방식을 결정할 경우 일관성이 깨진다. 노출 방식 후보:

- `UDeveloperSettings` 서브클래스 (Project Settings 자동 통합 + ini 외부화 + hot reload)
- `UDataAsset` / `UPrimaryDataAsset` (per-instance 자산, Content Browser 편집)
- ini 파일 직접 + `GConfig` 읽기 (타입 안전성 부재)
- 코드 `const` 또는 `static constexpr` (재빌드 필수)

각 시스템이 임의로 선택할 경우:
- 디자이너가 카드 가중치는 DataAsset에서, NarrativeCap은 ini에서, MPC blend duration은 DataAsset에서 변경해야 함 → 컨텍스트 스위칭 비용
- Project Settings 통합 부재 → 디자이너가 어떤 knob이 어디 있는지 알기 어려움
- Hot reload 부재 → 모든 변경이 PIE 재시작 필요

Time GDD OQ-5에서 `UDeveloperSettings`를 명시적으로 권장 — 본 ADR이 그 권장을 모든 시스템에 정식 채택으로 확장.

### Constraints

- **타입 안전성 필수**: 디자이너가 잘못된 타입 입력 시 Project Settings UI에서 차단되어야 함 (raw ini 편집 회피)
- **Project Settings 통합**: UE 표준 워크플로우에 부합 — 디자이너가 익숙한 UI에서 모든 게임 설정 접근
- **Hot reload 지원**: 일부 knob (특히 게임플레이 튜닝)은 변경 즉시 반영되어야 함 (Editor + PIE)
- **ini 외부화**: 빌드 후 패치 가능성 + version control 친화 + 빌드 머신 환경 차이 흡수
- **Per-instance 자산 예외 허용**: `UDreamDataAsset` 트리거 임계는 *각 자산별로 다름* — `UDeveloperSettings`로 표현 불가

### Requirements

- 모든 시스템 수준 Tuning Knob (시스템 전역 상수 — 예: NarrativeCap, GameDurationDays, MaxInitTimeBudgetMs)은 `UDeveloperSettings` 노출
- Per-instance 자산 (각 카드/꿈/형태별 다른 값)은 `UPrimaryDataAsset` 유지 — 본 ADR의 예외
- 일관된 카테고리 분류 (Project Settings → "Moss Baby" → 시스템별 sub-category)
- ini 파일 위치: `Config/DefaultGame.ini` (game-wide config) + `Saved/Config/WindowsNoEditor/Game.ini` (런타임 변경)

## Decision

**모든 시스템의 Tuning Knob을 `UDeveloperSettings` 서브클래스로 노출한다.** 각 시스템은 자체 `U[SystemName]Settings : UDeveloperSettings` 클래스를 정의하며, `UCLASS(Config=Game, DefaultConfig)` 매크로 + `UPROPERTY(Config, EditAnywhere, Category=...)` 필드로 knob를 노출한다.

**예외 (`UPrimaryDataAsset` 유지)**:
1. **Per-instance 콘텐츠 데이터** — `UDreamDataAsset` (각 꿈별 트리거 임계), `UMossBabyPresetAsset` (성장 단계별 머티리얼 프리셋), `UCardSystemConfigAsset` 시즌 가중치 (DataTable이 아닌 단일 자산은 `UPrimaryDataAsset`)
2. **에디터 친화적 다중 인스턴스 자산** — `UStillnessBudgetAsset`, `UGameStateMPCAsset`, `ULumenDuskAsset` (단, 이들은 단일 인스턴스이므로 `UDeveloperSettings`로 전환 검토 가능 — 본 ADR은 GDD가 명시한 자산 형태 유지)

### Naming Convention

```cpp
// 시스템별 Settings class 명명 규칙
U[SystemName]Settings : public UDeveloperSettings

// 예시:
UMossTimeSessionSettings        // Time & Session System (#1)
UMossDataPipelineSettings       // Data Pipeline (#2)
UMossSaveLoadSettings           // Save/Load Persistence (#3)
UMossInputAbstractionSettings   // Input Abstraction Layer (#4)
UMossGameStateSettings          // Game State Machine (#5) — MPC 자산은 별도 UPrimaryDataAsset
UMossCardSystemSettings         // Card System (#8) — 단, ConfigAsset은 별도 UPrimaryDataAsset
UMossDreamTriggerSettings       // Dream Trigger (#9) — 단, DreamAsset은 별도 UPrimaryDataAsset
UMossWindowDisplaySettings      // Window & Display Management (#14)
```

### Project Settings Category 매핑

```text
Project Settings
└── Game
    └── Moss Baby
        ├── Time & Session
        ├── Data Pipeline
        ├── Save/Load Persistence
        ├── Input Abstraction
        ├── Game State Machine
        ├── Card System
        ├── Dream Trigger
        ├── Stillness Budget
        ├── Lumen Dusk Scene
        └── Window & Display
```

각 `U[SystemName]Settings` 클래스는 `GetCategoryName()` override로 "Moss Baby"를 반환, `GetSectionName()` 으로 시스템 sub-category 분류.

### Key Interfaces

```cpp
// 표준 패턴 — Time & Session 예시
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Time & Session"))
class UMossTimeSessionSettings : public UDeveloperSettings {
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Day Structure",
              meta=(ClampMin="7", ClampMax="60"))
    int32 GameDurationDays = 21;

    UPROPERTY(Config, EditAnywhere, Category="Classification",
              meta=(ClampMin="1.0", ClampMax="30.0", Units="Seconds"))
    float DefaultEpsilonSec = 5.0f;

    UPROPERTY(Config, EditAnywhere, Category="Narrative Cap",
              meta=(ClampMin="0", ClampMax="10"))
    int32 NarrativeCapPerPlaythrough = 3;

    UPROPERTY(Config, EditAnywhere, Category="Narrative Cap",
              meta=(ClampMin="6", ClampMax="72", Units="Hours"))
    int32 NarrativeThresholdHours = 24;

    // ... 나머지 4 knobs (InSessionTolerance, SuspendThresholds 등)

    // UDeveloperSettings overrides
    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }
    virtual FName GetSectionName() const override { return TEXT("Time & Session"); }

    // Static accessor (런타임 + 에디터 어디서나)
    static const UMossTimeSessionSettings* Get() {
        return GetDefault<UMossTimeSessionSettings>();
    }
};

// 사용 예시 (런타임)
const auto* Settings = UMossTimeSessionSettings::Get();
int32 NarrativeCap = Settings->NarrativeCapPerPlaythrough;
```

### `UPrimaryDataAsset` 예외 케이스 (본 ADR이 *변경하지 않는* 자산)

```cpp
// 유지: per-instance 트리거 임계 — UDeveloperSettings로 표현 불가
UCLASS()
class UDreamDataAsset : public UPrimaryDataAsset {
    UPROPERTY(EditAnywhere) FName DreamId;
    UPROPERTY(EditAnywhere) FText BodyText;
    UPROPERTY(EditAnywhere) TMap<FName, float> TriggerTagWeights;  // 꿈별 다름
    UPROPERTY(EditAnywhere) float TriggerThreshold;                // 꿈별 다름
    // ...
};

// 유지: 성장 단계별 프리셋 — DataAsset 형태가 자연스러움
UCLASS()
class UMossBabyPresetAsset : public UPrimaryDataAsset {
    UPROPERTY(EditAnywhere) TMap<EGrowthStage, FMossBabyMaterialPreset> StagePresets;
    UPROPERTY(EditAnywhere) FMossBabyMaterialPreset DryingPreset;
    // ...
};

// 유지: 단일 인스턴스이지만 GDD가 PrimaryDataAsset으로 명시
// (본 ADR은 변경 강요하지 않음 — 미래 revision에서 UDeveloperSettings 전환 검토 가능)
UCLASS()
class UStillnessBudgetAsset : public UPrimaryDataAsset { ... };

UCLASS()
class UGameStateMPCAsset : public UPrimaryDataAsset { ... };

UCLASS()
class ULumenDuskAsset : public UPrimaryDataAsset { ... };
```

## Alternatives Considered

### Alternative 1: 모든 Tuning Knob을 `UDataAsset` / `UPrimaryDataAsset`으로 통일

- **Description**: `UDeveloperSettings` 사용 금지, 모든 knob을 `U[SystemName]ConfigAsset : UPrimaryDataAsset`으로 노출. Content Browser 편집.
- **Pros**: 단일 노출 메커니즘 — 디자이너가 한 곳만 학습. 자산 이름·검색·import/export 표준화
- **Cons**: Project Settings 통합 부재 → "Moss Baby" 카테고리에 게임 설정이 노출되지 않음. ini 외부화 부재 — 빌드 후 패치 어려움. Hot reload 제한적 (자산 변경 후 Subsystem 재초기화 필요). 디자이너가 Project Settings UI 대신 Content Browser 탐색해야 함 (시스템 전역 상수에 부적합)
- **Rejection Reason**: Project Settings 통합 + ini 외부화 + hot reload는 시스템 전역 상수에 더 적합. Per-instance 자산은 이미 DataAsset 사용 중 — 두 메커니즘 공존이 더 자연스러움

### Alternative 2: ini 파일 + `GConfig` 직접 읽기

- **Description**: `DefaultGame.ini`에 직접 knob 작성, 런타임에 `GConfig->GetInt(TEXT("[/Script/MossBaby.MossTimeSession]"), TEXT("NarrativeCap"), Value, GGameIni)` 같은 직접 읽기.
- **Pros**: ini 외부화 + 빌드 후 패치 가능
- **Cons**: 타입 안전성 부재 (raw string key) — typo 시 silent default. 에디터 통합 부재 → 디자이너가 ini 파일 직접 편집 (위험). UE 표준 워크플로우 우회. Project Settings UI에 노출되지 않음
- **Rejection Reason**: 타입 안전성 부재 + 디자이너 친화 부재. `UDeveloperSettings`는 내부적으로 ini 파일에 저장하면서도 타입 안전 + 에디터 통합 제공

### Alternative 3: 코드 `static constexpr` 또는 `const`

- **Description**: 모든 knob을 C++ `const` / `static constexpr`로 정의. 변경 시 재빌드.
- **Pros**: 컴파일 타임 보장 + 런타임 비용 0
- **Cons**: **모든 변경 재빌드 필수** — 디자이너 친화 부재 + 빠른 튜닝 사이클 불가. 빌드 후 패치 불가
- **Rejection Reason**: 디자이너 튜닝 사이클이 무너짐 (Tuning Knob의 본질적 이유와 모순)

### Alternative 4: 시스템별 자유 선택 (현재 GDD 상태)

- **Description**: 각 GDD가 자체 노출 방식 선택 — Time은 `UDeveloperSettings`, Card는 `UCardSystemConfigAsset`, Save는 `UMossSaveLoadConfig`, Stillness는 `UStillnessBudgetAsset` ...
- **Pros**: 각 시스템의 특성에 최적
- **Cons**: 디자이너가 14개 시스템 각각 다른 노출 방식 학습. Project Settings UI에 일부만 노출 → 일관성 부재. ADR 부재 시 자연스럽게 발생할 시나리오 — 사실 현재 GDD 상태가 이미 부분적으로 이 분기
- **Rejection Reason**: Architecture Principle (일관성) 위반. 본 ADR이 정확히 이 fragmentation을 통일

## Consequences

### Positive

- **디자이너 워크플로우 통일**: 시스템 전역 상수는 모두 Project Settings → "Moss Baby"에서 검색 + 편집. 학습 비용 1회
- **Project Settings 자동 통합**: UE 표준 워크플로우에 부합 — 새 contributor도 즉시 위치 파악 가능
- **ini 외부화**: `Config/DefaultGame.ini`에 자동 저장 → version control 친화 + 빌드 후 패치 가능
- **Hot reload**: 에디터에서 knob 변경 시 즉시 반영 (PIE 재시작 일부만 필요 — Subsystem 캐싱 정책에 따라)
- **타입 안전성**: `UPROPERTY(Config, EditAnywhere)` + `meta=(ClampMin/Max)` 메타데이터로 raw ini 편집 위험 차단
- **Architecture Principle 강화**: Principle 4 (Foundation Independence + Strict Layer Boundaries)와 일관 — 시스템별 Settings class는 layer 경계와 무관하게 일관

### Negative

- **약 10개 Settings class 작성 필요**: 각 시스템별 `U[SystemName]Settings` 클래스 작성 (이미 GDD가 knob 정의 → 단순 매핑)
- **Per-instance vs 전역 상수 구분 필요**: 디자이너가 "이 knob이 어디 있는지" 결정할 때 두 메커니즘 (Project Settings vs Content Browser) 학습. 단, 명확한 규칙 (전역 상수 = Settings, per-instance = DataAsset)으로 혼동 최소화
- **Project Settings UI 비대화 가능성**: 14개 시스템 모두 sub-category로 노출 → "Moss Baby" 섹션이 길어짐. **Mitigation**: `meta=(DisplayName=...)` 으로 명확한 이름 + Category 분류 + Subsection (예: Time & Session → Day Structure / Classification / Narrative Cap)

### Risks

- **`UDeveloperSettings`의 Subsystem-time accessor 호출 timing**: `GetDefault<>()` 호출 시점이 Subsystem `Initialize()` 전이면 ini 미로드 가능성. **Mitigation**: `GetDefault<>()`는 CDO 반환 — UE engine이 Subsystem보다 먼저 ini 로드 보장. 표준 패턴이며 안전
- **Hot reload 한계**: 일부 knob 변경 (예: `MaxInitTimeBudgetMs`)은 Subsystem 재초기화 필요. **Mitigation (C6 2026-04-19 해결)**: 본 ADR이 다음 convention을 정식 채택:
  - `UPROPERTY(Config, EditAnywhere, meta=(ConfigRestartRequired="true"))` — UE 표준 메타데이터. 에디터에서 "Restart Required" 표시 + 런타임 변경 시 경고
  - Convention 1: Subsystem `Initialize()`에서 한 번 읽고 캐싱하는 knob은 모두 `ConfigRestartRequired="true"` (예: `MaxInitTimeBudgetMs`, `GameDurationDays`, `MinCompatibleSchemaVersion`)
  - Convention 2: 매 프레임 read 또는 이벤트 시점에 read하는 knob은 hot reload 자연 지원 (meta 불필요) — 예: `BlendDurationBias` (GSM Tick), `bReducedMotionEnabled` (Stillness Budget 매 Request)
  - 각 시스템 `UMossXxxSettings` 클래스의 각 UPROPERTY에 이 convention을 반드시 적용. 위반 시 PR 코드 리뷰에서 차단
- **DataAsset 예외와의 일관성 혼동**: 디자이너가 "왜 어떤 knob은 Settings에 있고 어떤 건 DataAsset에 있는지" 혼동 가능. **Mitigation**: 본 ADR §Decision의 "예외" 섹션을 onboarding 문서에 인용

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| `time-session-system.md` | OQ-5 — "Tuning Knob 노출 방식 — `UDataAsset` vs `UDeveloperSettings`(권장 — UE 5.6 idiomatic, Project Settings 자동 통합, Hot reload 지원, 런타임 전역 튜닝에 적합) vs ini 파일?" | **본 ADR이 OQ-5의 권장을 모든 시스템에 정식 채택으로 확장** |
| `time-session-system.md` | §Tuning Knobs (7 knobs: GameDurationDays, DefaultEpsilonSec, InSessionToleranceMinutes, NarrativeThresholdHours, NarrativeCapPerPlaythrough, SuspendMonotonicThresholdSec, SuspendWallThresholdSec) | `UMossTimeSessionSettings` 클래스로 노출 |
| `data-pipeline.md` | §G.2 카테고리 A — Performance Budget Knobs (P0): MaxInitTimeBudgetMs, MaxCatalogSizeCards/Dreams/Forms. §G.3 Behavior Policy Knobs: DuplicateCardIdPolicy, bFTextEmptyGuard, bSoftRefMissingError. §G.4 Threshold Scaling: CatalogOverflowWarn/Error/FatalMultiplier | `UMossDataPipelineSettings` 클래스 (10+ knobs) |
| `save-load-persistence.md` | §Tuning Knobs (5 knobs: MinCompatibleSchemaVersion, MaxPayloadBytes, SaveFailureRetryThreshold, DeinitFlushTimeoutSec, bDevDiagnosticsEnabled). 명시: "노브는 `UMossSaveLoadConfig : UDeveloperSettings`에 노출되어 Project Settings에서 조정 가능 (Time GDD OQ-5 결정 — `UDeveloperSettings` 채택)" | `UMossSaveLoadSettings` 클래스 — 이미 GDD가 본 ADR과 일관 |
| `input-abstraction-layer.md` | §Tuning Knobs (3 knobs: InputModeMouseThreshold, OfferDragThresholdSec, OfferHoldDurationSec) | `UMossInputAbstractionSettings` 클래스 |
| `game-state-machine.md` | §Tuning Knobs (10+ knobs: BlendDurationBias, D_min/D_max[transition], DawnMinDwellSec, CardSystemReadyTimeoutSec, DreamDeferMaxSec, WitheredThresholdDays + MPC parameters는 별도 UGameStateMPCAsset) | `UMossGameStateSettings` 클래스 (MPC asset은 예외 유지) |
| `moss-baby-character-system.md` | §Tuning Knobs (10+ knobs: GrowthTransitionDurationSec, FinalRevealDurationSec, CardReactionEmissionBoost, CardReactionTauEmission/SSS, BreathingAmplitude/Period, DryingThreshold/FullDays + 프리셋은 별도 UMossBabyPresetAsset) | `UMossCharacterSettings` 클래스 (Preset asset은 예외 유지) |
| `growth-accumulation-engine.md` | §Tuning Knobs (8 knobs: GrowingDayThreshold, MatureDayThreshold, GameDurationDaysOverride, DefaultFormId, CategoryWeightMap, TagCategories, HueShiftOffsetMap, SSSOffsetMap). 명시: "모든 Knob은 `UGrowthConfigAsset : UPrimaryDataAsset`에 ... 노출" | **GDD 변경 권장**: 시스템 전역 상수 (GrowingDayThreshold, MatureDayThreshold 등)는 `UMossGrowthSettings : UDeveloperSettings`로 이전. 카테고리·오프셋 맵은 DataAsset 유지 검토 (per-content 데이터) |
| `card-system.md` | §Tuning Knobs (6 knobs: InSeasonWeight, OffSeasonWeight, ConsecutiveDaySuppression, HandSize, SeasonDayRanges, GameDurationDays). 명시: "ConfigAsset 위치: `UCardSystemConfigAsset` (`UPrimaryDataAsset`)" | **GDD 변경 권장**: 시스템 전역 상수는 `UMossCardSystemSettings : UDeveloperSettings`로 이전. ConfigAsset의 다른 데이터(시즌 가중치 테이블 등)는 DataAsset 유지 검토 |
| `dream-trigger-system.md` | §Tuning Knobs (6 knobs: MinDreamIntervalDays, MaxDreamsPerPlaythrough, DefaultDreamId, MinimumGuaranteedDreams, EarliestDreamDay, TagScoreEvaluationMode). 명시: "외부화: 모든 Knob은 `UDreamConfigAsset : UPrimaryDataAsset`에 노출" | **GDD 변경 권장**: 시스템 전역 상수는 `UMossDreamTriggerSettings : UDeveloperSettings`로 이전. UDreamDataAsset (per-dream 트리거 임계)은 본 ADR 예외 — 유지 |
| `stillness-budget.md` | §Tuning Knobs (4 knobs: MotionSlots, ParticleSlots, SoundSlots, bReducedMotionEnabled) | UStillnessBudgetAsset (단일 인스턴스 PrimaryDataAsset) — GDD가 명시 → 본 ADR 예외 유지 (단, 미래 revision에서 UDeveloperSettings 전환 검토 가능) |
| `lumen-dusk-scene.md` | §Tuning Knobs (12 knobs: CameraFOV, BaseFStop, DreamFStopDelta, ... LumenGPUBudgetMs, PSOWarmupTimeoutSec) | ULumenDuskAsset (PrimaryDataAsset) — GDD가 명시 → 본 ADR 예외 유지 |
| `card-hand-ui.md` | §Tuning Knobs (10 knobs: CardWidthPx, CardHeightPx, CardSpacingPx, ... CardOfferDurationSec). UCardHandUIConfigAsset | UPrimaryDataAsset — GDD 명시 → 본 ADR 예외 유지 (UI 시각 튜닝은 디자이너가 Content Browser에서 자주 편집) |
| `dream-journal-ui.md` | §Tuning Knobs (10 knobs: FontSizeBody, ... PaddingHorizontal). UDreamJournalConfig | UPrimaryDataAsset — GDD 명시 → 본 ADR 예외 유지 |
| `window-display-management.md` | §G Tuning Knobs (10 knobs: MinWindowWidth/Height, DefaultWindowWidth/Height, bAlwaysOnTopDefault, DPIScaleOverride, FocusEventDebounceMsec, ...) — 명시: "모든 튜닝 노브는 `assets/data/window_display_settings.ini` (또는 UE `UDeveloperSettings` 서브클래스)에 정의" | `UMossWindowDisplaySettings : UDeveloperSettings` 채택 (이미 GDD가 옵션 명시) |

**Note**: GDD 명시적 변경이 필요한 시스템은 Growth, Card, Dream Trigger 3건. 본 ADR Accepted 후 해당 GDD § Tuning Knobs 섹션 갱신 권장 (시스템 전역 상수 vs per-content 데이터 분리). 단, GDD Acceptance Criteria는 영향 없음 (knob 노출 메커니즘은 GDD 행위 정의와 독립).

## Performance Implications

- **CPU**: 영향 없음 — `GetDefault<UDeveloperSettings>()`는 CDO 반환 (포인터 lookup, 무료)
- **Memory**: 시스템당 ~수십 바이트 (각 Settings class CDO) — 무시 가능
- **Load Time**: 영향 없음 — ini 로드는 UE engine이 startup 단계에서 1회 처리, Subsystem과 독립
- **Network**: N/A

## Migration Plan

신규 결정. 기존 코드 없음 (architecture 단계). Implementation 단계 진입 시:

### Phase A: Settings Class 작성 (Foundation sprint)

1. `UMossTimeSessionSettings`, `UMossDataPipelineSettings`, `UMossSaveLoadSettings`, `UMossInputAbstractionSettings` 작성 — 각 시스템 GDD §Tuning Knobs 섹션 매핑
2. `Source/MadeByClaudeCode/Settings/` 디렉터리에 배치
3. `DefaultGame.ini` 자동 생성 (UE 첫 빌드 시) — 기본값으로 채워짐

### Phase B: Subsystem 통합 (각 시스템 sprint)

```cpp
// 표준 패턴 — 각 Subsystem.Initialize()에서
void UMossTimeSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    const auto* Settings = UMossTimeSessionSettings::Get();
    NarrativeCap = Settings->NarrativeCapPerPlaythrough;
    GameDuration = Settings->GameDurationDays;
    // ...
}
```

### Phase C: GDD 갱신 (Growth, Card, Dream Trigger 3건)

각 GDD §Tuning Knobs 섹션에 다음 분리 명시:
- "시스템 전역 상수 (`UMossXxxSettings : UDeveloperSettings`)": 시스템 wide 값
- "Per-content 데이터 (`UXxxDataAsset : UPrimaryDataAsset`)": 자산별 다른 값

### Phase D: CI 정적 분석 hook (선택)

```bash
# 모든 Settings class가 UDeveloperSettings 상속 강제
grep -rE "class U[A-Z][a-zA-Z]+Settings.*:" Source/MadeByClaudeCode/Settings/ | \
  grep -v "UDeveloperSettings" | \
  test ! -s  # 매치 0건 보장
```

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| 모든 시스템 전역 상수가 `UDeveloperSettings` 노출 | Project Settings → Game → "Moss Baby" 카테고리 — 14개 시스템 sub-category 표시 (DataAsset 예외는 명시) | 수동 QA 체크리스트 |
| Naming convention | `Source/MadeByClaudeCode/Settings/` 모든 class가 `U[SystemName]Settings : UDeveloperSettings` | CI grep |
| Hot reload 동작 | 에디터에서 knob 변경 후 PIE 진입 → 변경값 반영 확인 (예: `NarrativeCapPerPlaythrough` 3 → 1 변경 후 7일 MVP 플레이) | 수동 QA |
| ini 외부화 동작 | `Config/DefaultGame.ini`에 `[/Script/MossBaby.MossTimeSessionSettings]` 섹션 존재 + knob 값 평문 저장 | 빌드 후 ini 파일 확인 |
| Per-instance 자산 예외 일관성 | UDreamDataAsset, UMossBabyPresetAsset 등 5개 자산이 UPrimaryDataAsset 유지 | Code review |
| Time GDD OQ-5 RESOLVED 처리 | Time GDD §Open Questions의 OQ-5에 "**RESOLVED** by ADR-0011 (2026-04-XX)" 마킹 | GDD 갱신 PR |

## Related Decisions

- **time-session-system.md** §OQ-5 — 본 ADR의 직접 출처 ("`UDeveloperSettings` 권장")
- **architecture.md** §Architecture Principles §Principle 5 (Engine-Aware Design) — `UDeveloperSettings`는 UE 표준 패턴, 5.6 변경 없음 (LOW risk)
- **ADR-0001** (Forward Time Tamper) — 정책·컨벤션 ADR로 함께 묶음 (즉시 작성 가능 — 결정 명확)
- **ADR-0002 / ADR-0003 / ADR-0010** (Data Pipeline 컨테이너 / 로딩 / FFinalFormRow) — 본 ADR Accepted 후 Pipeline Settings class 작성 시 본 ADR 참조
- 향후 작성될 모든 시스템별 ADR — 본 ADR이 모든 시스템의 Tuning Knob 노출 메커니즘을 통일
