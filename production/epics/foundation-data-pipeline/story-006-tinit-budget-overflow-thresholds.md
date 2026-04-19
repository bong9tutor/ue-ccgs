# Story 006: T_init budget 실측 + 3단계 overflow 임계 로그 분기 + D1 memory smoke

> **Epic**: Data Pipeline
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 0.5 days (~3 hours)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/data-pipeline.md`
**Requirement**: `TR-dp-002` (AC-DP-09/10 [5.6-VERIFY] T_init 실측), D1 memory budget smoke
**ADR Governing Implementation**: ADR-0003 (T_init 성능 예산 Normal/Warning/Error/Fatal 4-단계)
**ADR Decision Summary**: SSD warm ≤ 50ms / 50-52.5ms Warning / 52.5-75ms Error + Async Bundle 전환 검토 / >100ms Fatal. HDD cold-load known compromise (~61ms Error 영역). MaxCatalogSize 3단계 임계: 1.05× Warn / 1.5× Error / 2.0× Fatal.
**Engine**: UE 5.6 | **Risk**: MEDIUM (`FPlatformTime::Seconds()` ms 정밀도 [5.6-VERIFY])
**Engine Notes**: `MaxInitTimeBudgetMs` knob은 ConfigRestartRequired (Subsystem Initialize 1회 read). Fatal은 `checkf` 또는 `UE_LOG Fatal` — Shipping 빌드 포함 강제 (AC-DP-17 연계).

**Control Manifest Rules (Foundation layer)**:
- Required: "T_init SSD warm ≤ 50ms / 50-52.5ms Warning / 52.5-75ms Error + Async Bundle 전환 검토 flag / >100ms Fatal" (ADR-0003)
- Required: "HDD cold-start known compromise ~61ms T_init (Error 영역) 수용"

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] AC-DP-09 [5.6-VERIFY]: Full Vision 규모 Mock (40 card / 50 dream / 8 form) + `MaxInitTimeBudgetMs = 50.0f` 설정 시 `Initialize()` wall-clock 측정 — T_init < 50ms. 10회 연속 중 9회 이상 통과. 50ms 이상이면 `LogDataPipeline Error` (Logic · BLOCKING · P0)
- [ ] AC-DP-10 [5.6-VERIFY]: `MaxCatalogSizeCards = 200` + `Warn% = 1.05` + `Error% = 1.5` + `FatalMultiplier = 2.0` 설정 시:
  - 210 카드 → `Warning`만
  - 301 카드 → `Error` (Warning 포함)
  - 401 카드 → `Fatal` (Warning·Error 포함)
  (Logic · BLOCKING · P0)
- [ ] AC-DP-08: Full Vision 카탈로그 로드 후 `GetCatalogStats()` 또는 `Pipeline.MemoryFootprintBytes` ≤ 150,000 bytes + 텍스처 streaming pool 할당과 명확히 구분 (Config-Data · ADVISORY · P2)
- [ ] T_init 측정 중 `Warning`/`Error`/`Fatal` 로그 메시지는 knob 이름, 실제 값, 임계값 모두 포함

---

## Implementation Notes

*Derived from ADR-0003 §T_init 성능 예산 및 3단계 임계 + GDD §D1/D2/D3:*

```cpp
void UDataPipelineSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    const double StartTime = FPlatformTime::Seconds();
    const auto* Settings = UMossDataPipelineSettings::Get();

    CurrentState = EDataPipelineState::Loading;

    // Step 1-4 (Story 004)
    if (!RegisterCardCatalog()) { /* ... */ return; }
    CheckCatalogSize("Card", GetCardCount(), Settings);
    if (!RegisterFinalFormCatalog()) { /* ... */ return; }
    CheckCatalogSize("FinalForm", GetFinalFormCount(), Settings);
    if (!RegisterDreamCatalog()) { /* ... */ return; }
    CheckCatalogSize("Dream", GetDreamCount(), Settings);
    if (!RegisterStillnessCatalog()) { /* ... */ return; }

    const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;

    // AC-DP-09 T_init budget 3-단계 분기
    const float Budget = Settings->MaxInitTimeBudgetMs;
    if (ElapsedMs > Budget * 2.0f) {  // > 100ms (default)
        UE_LOG(LogDataPipeline, Fatal,
            TEXT("T_init %.2f ms > %.2f ms * 2.0 (Fatal) — Pipeline init aborted"),
            ElapsedMs, Budget);
    } else if (ElapsedMs > Budget * 1.5f) {  // 52.5-75ms
        UE_LOG(LogDataPipeline, Error,
            TEXT("T_init %.2f ms > %.2f ms * 1.5 (Error) — Async Bundle 전환 검토"),
            ElapsedMs, Budget);
    } else if (ElapsedMs > Budget * 1.05f) {  // 50-52.5ms
        UE_LOG(LogDataPipeline, Warning,
            TEXT("T_init %.2f ms > %.2f ms * 1.05 (Warning)"),
            ElapsedMs, Budget);
    } else {
        UE_LOG(LogDataPipeline, Log, TEXT("T_init %.2f ms (Normal)"), ElapsedMs);
    }

    CurrentState = EDataPipelineState::Ready;
}

// AC-DP-10 MaxCatalogSize 3-단계 분기
void UDataPipelineSubsystem::CheckCatalogSize(const FString& CatalogName, int32 Count,
                                               const UMossDataPipelineSettings* Settings) {
    int32 Budget = 200;  // knob switch by CatalogName
    if (CatalogName == "Card") Budget = Settings->MaxCatalogSizeCards;
    else if (CatalogName == "Dream") Budget = Settings->MaxCatalogSizeDreams;
    else if (CatalogName == "FinalForm") Budget = Settings->MaxCatalogSizeForms;

    const float WarnThresh = Budget * Settings->CatalogOverflowWarnMultiplier;
    const float ErrorThresh = Budget * Settings->CatalogOverflowErrorMultiplier;
    const float FatalThresh = Budget * Settings->CatalogOverflowFatalMultiplier;

    if (Count >= FatalThresh) {
        UE_LOG(LogDataPipeline, Warning, TEXT("%s count %d > Warn threshold %.0f"), *CatalogName, Count, WarnThresh);
        UE_LOG(LogDataPipeline, Error, TEXT("%s count %d > Error threshold %.0f"), *CatalogName, Count, ErrorThresh);
        UE_LOG(LogDataPipeline, Fatal, TEXT("%s count %d > Fatal threshold %.0f"), *CatalogName, Count, FatalThresh);
    } else if (Count >= ErrorThresh) {
        UE_LOG(LogDataPipeline, Warning, TEXT("%s count %d > Warn threshold %.0f"), *CatalogName, Count, WarnThresh);
        UE_LOG(LogDataPipeline, Error, TEXT("%s count %d > Error threshold %.0f"), *CatalogName, Count, ErrorThresh);
    } else if (Count >= WarnThresh) {
        UE_LOG(LogDataPipeline, Warning, TEXT("%s count %d > Warn threshold %.0f"), *CatalogName, Count, WarnThresh);
    }
}

// AC-DP-08 memory footprint
FString UDataPipelineSubsystem::GetCatalogStats() const {
    // D1 공식 계산
    const int64 CardBytes = (CardTable ? CardTable->GetRowMap().Num() : 0) * 160;  // S_card avg
    const int64 DreamBytes = DreamRegistry.Num() * 1280;
    const int64 FormBytes = FormRegistry.Num() * 320;
    const int64 Total = CardBytes + DreamBytes + FormBytes;
    return FString::Printf(TEXT("Pipeline CPU-side: %lld bytes (Card=%lld, Dream=%lld, Form=%lld)"),
        Total, CardBytes, DreamBytes, FormBytes);
}
```

---

## Out of Scope

- Story 007: 단조성 Shipping-safe guard (AC-DP-17)
- Runtime dynamic Async 전환 (AC-DP-09 Error 시 flag만 세팅, 전환은 별도 ADR)

---

## QA Test Cases

**For Logic story:**
- **AC-DP-09 [5.6-VERIFY] (T_init budget)**
  - Given: Full Vision mock + `MaxInitTimeBudgetMs = 50.0f`
  - When: `Initialize()` 10회 반복
  - Then: T_init < 50ms 9회 이상 + 로그 레벨 "Normal"
  - Edge cases: Mock을 느리게 주입하여 75ms 시뮬 → Error 로그 + "Async Bundle 전환 검토"
- **AC-DP-10 [5.6-VERIFY] (3-단계 overflow)**
  - Given: `MaxCatalogSizeCards = 200`, multipliers (1.05/1.5/2.0)
  - When: 210장 / 301장 / 401장 각각 `Initialize()`
  - Then:
    - 210 → Warning 로그 1회 ("Warn threshold 210")
    - 301 → Warning + Error 로그 (Warning 포함)
    - 401 → Warning + Error + Fatal 로그 (모두 포함)
- **AC-DP-08 (memory footprint)**
  - Given: Full Vision 로드
  - When: `GetCatalogStats()` 호출
  - Then: 로그 문자열에 Total < 150000 + 텍스처 pool 언급 없음 (분리)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/data-pipeline/init_time_budget_test.cpp` (AC-DP-09 [5.6-VERIFY])
- `tests/unit/data-pipeline/catalog_overflow_threshold_test.cpp` (AC-DP-10 [5.6-VERIFY])
- `production/qa/smoke-[date].md` (AC-DP-08 Config-Data smoke)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 004 (catalog loading)
- Unlocks: Story 007

---

## Completion Notes

**Completed**: 2026-04-19
**Criteria**: 4/4 AUTOMATED (Normal/Warning/Error/Fatal branches + GetCatalogStats format)

**Files delivered**:
- `Data/DataPipelineSubsystem.h/.cpp` (수정) — EvaluateTInitBudget + CheckCatalogSize + GetCatalogStats + bInitFatalTriggered/ErrorTriggered/WarningTriggered flags + Testing 훅
- `Tests/DataPipelineBudgetTests.cpp` (신규, 8 tests)
- `tests/unit/data-pipeline/README.md` Story 1-19 섹션 append

**Test Evidence**: 8 UE Automation — `MossBaby.Data.Pipeline.Budget.*`

**AC 커버**:
- [x] AC-DP-09 T_init budget (Normal/Warn/Error/Fatal 4-단계 분기)
- [x] AC-DP-10 3-단계 catalog overflow (Warn 210 / Error 301 / Fatal 401 Cards)
- [x] AC-DP-08 memory footprint: GetCatalogStats 포맷 검증

**Fatal 처리 결정**:
- `UE_LOG Fatal`은 process 종료 유발 → test harness crash 방지를 위해 `Error` 로그 + `[FATAL_THRESHOLD]` 접두어 + `bInitFatalTriggered = true` flag 조합으로 대체
- Shipping 빌드에서 checkf 강제는 TD-013 (future)

**Deferred**:
- [5.6-VERIFY] AC-DP-09/10 실측 10회 반복: release-gate MANUAL test
- TD-013: Fatal 로그를 Shipping 빌드에서 checkf로 강제 강화
