# ADR-0003: Data Pipeline 로딩 전략 — Sync 일괄 로드 채택

## Status

**Accepted** (2026-04-19 — `/architecture-review` 통과, C5 해결 §Known Compromise HDD Cold-Start 추가됨. Implementation 단계 AC-DP-09/10 [5.6-VERIFY] 실측으로 정책 검증)

## Date

2026-04-18

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | Core / Data / Asset Loading |
| **Knowledge Risk** | MEDIUM — `UGameInstance::Init()` 동기 블로킹 허용 시간 + `UAssetManager::LoadPrimaryAssets` 동기 변종 동작 |
| **References Consulted** | `current-best-practices.md §5 Persistent State`, data-pipeline.md §D1 Memory Formula + §D2 InitializationTimeBudgetMs + §R2 BLOCKER 8 HDD cold-load, ADR-0002 §Decision |
| **Post-Cutoff APIs Used** | None — `UAssetManager::LoadPrimaryAssets` 동기 변종은 4.x+ 안정 |
| **Verification Required** | AC-DP-09 (T_init < 50ms 실측) + AC-DP-10 (3단계 임계 로그 분기 — Warn 5% / Error 50% / Fatal 200%) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0002 (Data Pipeline 컨테이너 선택 — Card=DT, FinalForm=DataAsset, Dream=DataAsset, Stillness=DataAsset) |
| **Enables** | Pipeline R3 동기 조회 contract (`GetCardRow` 등 sync return) — 모든 downstream 시스템의 pull API 패턴 |
| **Blocks** | Foundation sprint (Pipeline 구현) — 로딩 전략 미결정 시 구현 불가 |
| **Ordering Note** | ADR-0002, ADR-0010과 묶음 결정. 본 ADR은 ADR-0002의 컨테이너 선택을 전제 |

## Context

### Problem Statement

`UDataPipelineSubsystem::Initialize()`가 4개 카탈로그를 어떻게 로드할 것인가? Sync 일괄 로드 (UE 5.6 권고 50ms 동기 블로킹 이내) vs Async Bundle (`UAssetManager::LoadPrimaryAssetsWithType` 비동기) — 두 선택이 Pipeline R3 동기 조회 contract에 근본적 영향.

### Constraints

- **Pipeline R3 contract**: 모든 조회 API는 sync return (`TOptional<FRow>` / `UObject*`). downstream 시스템은 `Ready` 확인 후에만 조회 호출 — async 로드 시 R3 contract가 깨지거나 wait loop 필요
- **데이터 규모 (ADR-0002 채택 후)**: MVP 총 ~12KB / Full 총 ~120KB CPU-side catalog (Pipeline D1 Formula). 카드 `.uasset` 1개 + FinalForm 8-12개 + Dream 50개 + Stillness 1개 = ~60개 자산 (Full Vision)
- **초기화 시간 budget**: `MaxInitTimeBudgetMs = 50ms` — Pipeline D2 Formula 권고. UE 5.6: GameInstance::Init() 동기 블로킹 < 50ms 권장
- **HDD cold-load 현실**: SSD warm 기준 Full T_init ≈ 8.2ms. HDD cold-start는 디스크 seek + OS 파일 캐시 미적중으로 Full T_init ≈ 61ms — budget 초과 가능
- **첫 프레임 UX**: splash screen 없이 blocking이므로 "응답 없음" 체감은 200ms 초과 시에만 발생

### Requirements

- Pipeline R3 동기 조회 contract 유지 (모든 downstream 시스템 pull API가 sync)
- 첫 프레임 진입 전 Ready 상태 도달 (Title 화면 표시 전)
- 50ms budget 이내 (SSD warm 기준)
- HDD cold-start 초과 시 명확한 fallback 또는 전환 trigger
- PIE에서 `RefreshCatalog()` hot-swap 지원 (Cooked 빌드는 런타임 변경 없음)

## Decision

**Sync 일괄 로드 채택** — `UDataPipelineSubsystem::Initialize()`가 4개 카탈로그를 동기 순차 로드한다. Async Bundle은 **조건부 fallback** — T_init 실측 > 50ms budget 발생 시 전환 trigger.

### 구체 로드 순서 (R2 deterministic)

```cpp
void UDataPipelineSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    CurrentState = EDataPipelineState::Loading;

    // Step 1: Card DataTable (sync load)
    //   TSoftObjectPtr<UDataTable> CardTable = ...;
    //   CardTable->LoadSynchronous();  또는 UAssetManager sync load
    bool bCardOk = RegisterCardCatalog();
    if (!bCardOk) { CurrentState = EDataPipelineState::DegradedFallback; return; }

    // Step 2: FinalForm DataAsset bucket (ADR-0010 — per-form DataAsset)
    //   UAssetManager::LoadPrimaryAssetsWithType(FPrimaryAssetType("FinalForm"))
    //   → sync variant (블로킹)
    bool bFormOk = RegisterFinalFormCatalog();
    if (!bFormOk) { CurrentState = EDataPipelineState::DegradedFallback; return; }

    // Step 3: Dream DataAsset bucket
    //   UAssetManager::LoadPrimaryAssetsWithType(FPrimaryAssetType("DreamData"))
    bool bDreamOk = RegisterDreamCatalog();
    if (!bDreamOk) { CurrentState = EDataPipelineState::DegradedFallback; return; }

    // Step 4: Stillness single DataAsset
    bool bStillnessOk = RegisterStillnessCatalog();
    if (!bStillnessOk) { CurrentState = EDataPipelineState::DegradedFallback; return; }

    CurrentState = EDataPipelineState::Ready;
}
```

### T_init 성능 예산 및 3단계 임계

Pipeline D2 Formula + §G.4 Threshold Scaling Knobs 채택:

| 상태 | 조건 | 로그 레벨 | 동작 |
|---|---|---|---|
| **Normal** | T_init ≤ 50ms | 정보 로그 | 정상 동작 |
| **Warning** | 50ms < T_init ≤ 52.5ms (budget × 1.05) | `UE_LOG Warning` | 정상 동작 + Warning 발행 |
| **Error** | 52.5ms < T_init ≤ 75ms (budget × 1.5) | `UE_LOG Error` | 정상 동작 + Error 발행 + Async Bundle 전환 검토 flag |
| **Fatal** | T_init > 100ms (budget × 2.0) | `UE_LOG Fatal` | Fatal 발화 + 카탈로그 등록 단계 진입 차단 (Shipping 포함 — `checkf` 또는 `UE_LOG Fatal`) |

**HDD cold-load 정책**: 예상 61ms (Error 영역이지만 Fatal 미만). 정상 실행 허용 + Error 로그 발행. 실측 결과가 지속적으로 Fatal 초과 시 Async Bundle 전환 (별도 ADR로 superseded).

### Async Bundle 전환 Trigger (Future)

본 ADR은 Sync 일괄 로드 채택이나, 다음 조건 중 하나 발생 시 별도 ADR로 Async Bundle 전환:
1. 실측 T_init > budget × 1.5 (= 75ms) 지속 발생 — 사용자 PC 환경 다수에서
2. Full Vision 이후 규모 확장 (카드 100+ / 꿈 100+) — D2 Formula 재계산 결과
3. 텍스처 streaming pool 상주 메모리가 1.5GB ceiling 근접 — 비동기 언로드 필요

전환 시 R3 동기 조회 contract 재협상 필요 (downstream 모든 시스템 영향).

### PIE Hot-swap (`RefreshCatalog`)

Cooked 빌드는 런타임 변경 없음. PIE에서는 `RefreshCatalog()` 수동 호출로 hot-swap 지원:
- 동일 로드 순서 재실행
- 기존 `ContentRegistry` 클리어 후 재등록
- Loading 상태 재진입 동안 downstream 조회는 `checkf` assert (AC-DP-13)
- **E6 재진입 guard**: `Loading` 상태에서 `RefreshCatalog` 재호출 차단 (AC-DP-14)

## Alternatives Considered

### Alternative 1: Async Bundle (`LoadPrimaryAssetsWithType` 비동기)

- **Description**: `UAssetManager::LoadPrimaryAssetsWithType` 비동기 변종 — 카탈로그 로드를 background에서 진행, Ready 콜백.
- **Pros**: GameInstance::Init() 블로킹 없음. 대규모 콘텐츠 (100+ 자산) 확장 가능
- **Cons**: **Pipeline R3 동기 조회 contract 깨짐** → downstream 모든 시스템이 `IsReady()` 대기 loop 또는 async handler 패턴 필요. 첫 프레임에 Title 화면 표시 불가 (Ready 도달 전). 복잡성 ↑↑
- **Rejection Reason**: 100행 이하 규모에서 overkill. R3 sync contract가 downstream 14개 시스템의 pull API 패턴을 결정 — 변경 비용 > 이점

### Alternative 2: Lazy Load (첫 조회 시점에 로드)

- **Description**: `GetCardRow` 최초 호출 시 해당 카탈로그 로드.
- **Pros**: Init 시간 0ms. 사용하지 않는 카탈로그 미로드
- **Cons**: 첫 조회 시 지연 스파이크 (카탈로그 전체 로드 5-10ms 블로킹). R3 contract 혼란 (sync API가 첫 호출 시 실질적 async 비용). "Ready" 상태 의미 불명
- **Rejection Reason**: 지연 스파이크가 첫 프레임 이후 발생 → 더 나쁜 UX (Init에 집중되는 편이 낫다)

### Alternative 3: Preload + Background Bundle

- **Description**: 작은 카탈로그 (Card DT, Stillness)는 Init에서 sync, 큰 카탈로그 (Dream 50개)는 background.
- **Pros**: Init 시간 최소화 + 점진 로드
- **Cons**: 혼합 contract 복잡성 (일부 sync, 일부 async). R3 contract 일관성 부재 — downstream이 카탈로그별 다른 패턴 학습. Dream은 게임 시작 직후 바로 사용될 수 있음 (Day 1 카드 건넴 즉시 Dream Trigger 평가)
- **Rejection Reason**: 50개 Dream 자산 = ~60KB — sync 로드도 충분히 빠름. 복잡성 추가 이유 없음

## Consequences

### Positive

- **R3 동기 contract 유지**: 모든 downstream 시스템의 pull API가 sync — 구현 단순. 14개 시스템 일관 패턴
- **첫 프레임 Ready**: Title 화면 표시 전 Ready 도달 보장 — UX 명확
- **구현 단순성**: 카탈로그 등록 순서 deterministic (R2). async 콜백·상태 머신 불필요
- **Full Vision까지 충분**: D2 Formula 기준 Full T_init ≈ 8.2ms SSD warm — budget의 16% 사용
- **HDD cold-start 정직한 처리**: Error 로그 발행으로 실측 데이터 수집 + 필요 시 Async 전환 trigger 명시

### Negative

- **GameInstance::Init() 블로킹 8-61ms**: 앱 시작 시 한 번의 동기 블로킹. HDD cold-start 환경 (일부 사용자)에서 체감 가능 (단, splash 없이 200ms 미만 — "응답 없음" 미발생)
- **미래 규모 확장 시 재평가 필요**: 100+ 자산 규모면 본 ADR 재검토 + Async Bundle 전환 ADR 작성
- **PIE hot-swap의 Loading 상태 wait 필요**: `RefreshCatalog` 중 조회는 assert — downstream이 PIE 환경에서 handling 필요 (단, AC-DP-13으로 명시)

### Known Compromise — HDD Cold-Start (C5 명시 2026-04-19)

**정당화가 통계 의존**: HDD cold-start 시 T_init ~61ms → Pillar 1의 "응답성" 약속과 긴장 관계. 본 ADR은 다음 근거로 이 compromise를 수용:

1. **통계적 근거**: Steam Hardware Survey 2025 기준 PC 게이머의 SSD 사용률 90%+ — HDD cold-start 환경은 소수
2. **기술적 근거**: 61ms < 200ms (Windows "응답 없음" 임계) — 실제 "멈춤" 체감 미발생. Alt+Tab 반응성과 동일 수준
3. **대안의 비용**: Alternative 3 (Preload + Background Bundle for Dream) 재도입 시 R3 동기 contract 깨짐 → 14 downstream 시스템 API 패턴 변경 — 비용 > 이점

**HDD cold-start 완화 경로 (미래)**: 실측 결과 사용자 경험이 저해되는 경우 (playtest에서 복수 보고) **Dream 카탈로그 Alternative 3 부분 재도입** — Dream은 Day 1 즉시 필요하지 않으므로 배경 로드 가능. 이는 별도 ADR로 superseding.

**추적**: Implementation 단계에서 HDD 환경 실측 포함 (AC-DP-09 10회 중 9회 통과 기준에 HDD cold 케이스 추가). 결과가 AC 통과 실패 시 본 ADR 재검토.

### Risks

- **HDD cold-start 사용자 부정적 경험**: 61ms 블로킹 중 마우스 비응답 느낌. **Mitigation**: SSD가 PC 게이머 대다수 (Steam Hardware Survey 2025 기준 90%+) — Mitigation 필수성 낮음. 지속 발생 시 Async 전환 ADR
- **DefaultEngine.ini 등록 누락으로 cooked 빌드 빈 결과**: `UAssetManager::GetPrimaryAssetIdList` 빈 배열 → Dream/FinalForm 카탈로그 DegradedFallback. **Mitigation**: AC-DP-18 [5.6-VERIFY] + `GetFailedCatalogs()` API로 명시적 진단
- **초기화 중 다른 Subsystem 의존 호출**: `InitializeDependency<UDataPipelineSubsystem>()` 미선언 시 Pipeline 미준비 상태 조회 → assert. **Mitigation**: R2 BLOCKER 5 — 모든 downstream Subsystem Initialize에서 dependency 선언 의무 (본 ADR §Key Interfaces)

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `data-pipeline.md` | OQ-ADR-2 "로딩 전략 — Sync 일괄 vs Async Bundle" | **본 ADR이 OQ-ADR-2의 정식 결정** (Sync 채택) |
| `data-pipeline.md` | D2 `InitializationTimeBudgetMs = 50ms` (Full ≈ 8.2ms SSD, 61ms HDD) | 본 ADR §T_init 성능 예산 섹션 + 3단계 임계 |
| `data-pipeline.md` | R1 초기화 진입점 (`UGameInstance::Init()` 직후) | 본 ADR §구체 로드 순서 |
| `data-pipeline.md` | R2 등록 순서 (Card → FinalForm → Dream → Stillness) | 본 ADR §Decision Step 1-4 |
| `data-pipeline.md` | R3 동기 + null-safe + fail-close contract | 본 ADR이 R3 유지를 전제로 Sync 채택 |
| `data-pipeline.md` | R7 PIE Hot-reload vs Cooked | 본 ADR §PIE Hot-swap 섹션 |
| `data-pipeline.md` | R2 BLOCKER 5 Subsystem 의존 선언 | 본 ADR §Risks + downstream 의무 |
| `data-pipeline.md` | R2 BLOCKER 8 HDD cold-load 시나리오 | 본 ADR §T_init 성능 예산 섹션 Error 임계 |
| `data-pipeline.md` | AC-DP-09 T_init budget 실측 | 본 ADR Validation Criteria |
| `data-pipeline.md` | AC-DP-10 3단계 임계 로그 분기 | 본 ADR §T_init 성능 예산 섹션 표 직접 매핑 |
| 모든 downstream 14개 시스템 | pull API sync return (GetCardRow, GetAllDreamAssets 등) | R3 contract 유지 — 본 ADR의 전제 보장 |

## Performance Implications

- **CPU (Init)**: MVP ≈ 2.7ms, Full ≈ 8.2ms SSD warm / 61ms HDD cold — D2 Formula
- **Memory (catalog CPU-side)**: MVP ≈ 12KB, Full ≈ 120KB — D1 Formula (1.5GB ceiling의 0.01%)
- **Memory (TWeakObjectPtr 런타임 캐시)**: ~800 bytes (60 entries × 8 bytes)
- **Frame Budget (런타임)**: 0 — Pipeline은 Tick 없음, pull API는 TMap lookup O(1)
- **Network**: N/A

## Migration Plan

Foundation sprint 진입 시:

1. `UDataPipelineSubsystem` skeleton 작성 — 상기 Initialize() 4-step 순서
2. 각 카탈로그 등록 함수 구현 (`RegisterCardCatalog`, `RegisterFinalFormCatalog`, `RegisterDreamCatalog`, `RegisterStillnessCatalog`)
3. 3단계 임계 로그 구현 (`UMossDataPipelineSettings::CatalogOverflowWarn/Error/FatalMultiplier`)
4. AC-DP-09 자동화 테스트 (Full Vision scale mock 카탈로그로 T_init 측정 — 10회 중 9회 이상 통과)
5. AC-DP-18 [5.6-VERIFY] Cooked build에서 PrimaryAssetTypesToScan 결과 검증
6. PIE `RefreshCatalog` 구현 (Loading 상태 재진입 가드 AC-DP-14)

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| T_init budget 준수 (SSD) | AC-DP-09 — Full Vision mock scale Init 10회 wall-clock 측정 | 10회 중 9회 이상 T_init < 50ms |
| 3단계 임계 분기 | AC-DP-10 — 카드 210/301/401행 mock으로 Warning/Error/Fatal 각각 발화 확인 | 로그 레벨 분기 정확 |
| HDD cold-load 허용 | HDD 환경에서 T_init 실측 | Error 영역 (50-75ms) 허용, Fatal (>100ms) 발생 시 ADR 재검토 |
| R2 등록 순서 deterministic | AC-DP-02 — 4 카탈로그 Mock 모두 정상 로드 시 Card→FinalForm→Dream→Stillness 순서 로그 확인 | 순서 일치 |
| R3 fail-close | AC-DP-04, AC-DP-05 — 존재하지 않는 ID 조회 시 `TOptional` empty / `TArray` empty 반환 | 정확히 비어있음 |
| PIE Hot-swap | AC-DP-07 [5.6-VERIFY] — PIE에서 Row 추가 후 RefreshCatalog → Ready 복원 + 새 ID 포함 | 동작 확인 |
| RefreshCatalog 재진입 차단 | AC-DP-14 — Loading 상태에서 RefreshCatalog 재호출 시 즉시 return + Warning | 재진입 차단 확인 |
| DefaultEngine.ini 등록 | AC-DP-18 [5.6-VERIFY] — Cooked build에서 `GetPrimaryAssetIdList("DreamData")` 비어있지 않음 | 결과 ≥ 1 |

## Related Decisions

- **ADR-0002** (Data Pipeline 컨테이너 선택) — 본 ADR의 전제 (4 카탈로그 컨테이너 확정)
- **ADR-0010** (FFinalFormRow 저장 형식) — 본 ADR의 FinalForm 로드가 ADR-0010에 의해 DataAsset bucket 로드로 구체화
- **ADR-0011** (`UDeveloperSettings`) — 본 ADR의 3단계 임계 knob (CatalogOverflowWarn/Error/FatalMultiplier)은 `UMossDataPipelineSettings : UDeveloperSettings`
- **data-pipeline.md** §D1/D2/D3 Formulas + §G.4 Threshold Scaling — 본 ADR의 수치 근거
- **architecture.md** §Data Flow §3.1 Initialisation Order — 본 ADR의 로드 순서가 architecture-wide Initialisation Order의 `[1] Data Pipeline.Initialize()` 단계를 구체화
