# Story 005: Edge case policies (DuplicateCardId, FTextEmpty guard, RefreshCatalog 재진입, GetFailedCatalogs)

> **Epic**: Data Pipeline
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/data-pipeline.md`
**Requirement**: `TR-dp-001` (OQ-E-1, OQ-E-3 resolved), policy knobs
**ADR Governing Implementation**: ADR-0011 (Policy knob exposure — DuplicateCardIdPolicy, bFTextEmptyGuard)
**ADR Decision Summary**: Policy knob 기반 행위 — `DuplicateCardIdPolicy` (WarnOnly default / DegradedFallback), `bFTextEmptyGuard = true` default. PIE `RefreshCatalog()` 재진입 guard.
**Engine**: UE 5.6 | **Risk**: LOW
**Engine Notes**: GDD §Open Questions E-1, E-3, E-9가 Resolved. Settings에서 정책 전환 런타임 가능 (ConfigRestartRequired 없는 knob).

**Control Manifest Rules (Foundation layer)**:
- Required: "PIE hot-swap `RefreshCatalog()`는 `Loading` 상태 재진입 차단" — AC-DP-14 (ADR-0003)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] AC-DP-11: `DuplicateCardIdPolicy = WarnOnly` + 동일 CardId "dup_id" 2행 시 `Initialize()` → 카탈로그 **Ready 유지** + `GetCardRow("dup_id")` **첫 row** 반환 + `LogDataPipeline Warning` 중복 ID + 무시된 row 수 포함. `DegradedFallback` 변경 시 카탈로그 전체 DegradedFallback 전환 + 빈 `TOptional` 반환 (Logic · BLOCKING · P0)
- [ ] AC-DP-12: `bFTextEmptyGuard = true` + 카드 5개 중 1개의 DisplayName FText 빈 경우 → 해당 row 1개만 등록 거부 + 4개 정상 등록 + 카탈로그 Ready. `GetAllCardIds()` 크기 = 4. `false` 변경 시 5개 모두 등록 (Logic · BLOCKING · P0)
- [ ] AC-DP-14: `Loading` 상태 중 `RefreshCatalog()` 재진입 호출 → 즉시 return + `LogDataPipeline Warning` "RefreshCatalog already in progress" + 등록 순서 깨지지 않음 (Logic · BLOCKING · P1)
- [ ] AC-DP-15: Card만 DegradedFallback + Dream/FinalForm/Stillness Ready 시 `GetFailedCatalogs() → TArray<FName>` 반환에 Card만 포함 + 나머지 미포함. `GetAllDreamAssets()`, `GetGrowthFormRow()` 정상 동작 (Integration · ADVISORY · P1)
- [ ] AC-DP-07 [5.6-VERIFY]: PIE 세션 + 카드 10개 등록 후 Row 1개 추가 → `RefreshCatalog()` 호출 → `GetAllCardIds()` 크기 11 + 새 CardId 포함 + 완료 후 Ready (Integration · BLOCKING · P1)

---

## Implementation Notes

*Derived from GDD §Core Rules E1/E4/E6 + ADR-0003 §PIE Hot-swap:*

```cpp
// DataPipelineSubsystem.cpp
bool UDataPipelineSubsystem::RegisterCardCatalog() {
    // ... CardTable load ...
    TSet<FName> SeenIds;
    int32 DupCount = 0;
    int32 EmptyFTextCount = 0;
    const auto* Settings = UMossDataPipelineSettings::Get();

    // DataTable row 순회 + policy 적용
    for (auto It = CardTable->GetRowMap().CreateConstIterator(); It; ++It) {
        FGiftCardRow* Row = reinterpret_cast<FGiftCardRow*>(It.Value());

        // AC-DP-11 DuplicateCardIdPolicy
        if (SeenIds.Contains(Row->CardId)) {
            if (Settings->DuplicateCardIdPolicy == EDuplicateCardIdPolicy::DegradedFallback) {
                UE_LOG(LogDataPipeline, Error,
                    TEXT("Duplicate CardId %s — policy=DegradedFallback"), *Row->CardId.ToString());
                return false;
            } else {
                DupCount++;
                continue;  // 첫 row만 유지 (WarnOnly)
            }
        }
        // AC-DP-12 bFTextEmptyGuard
        if (Settings->bFTextEmptyGuard && Row->DisplayName.IsEmpty()) {
            EmptyFTextCount++;
            continue;  // row 단위 거부
        }
        SeenIds.Add(Row->CardId);
    }
    if (DupCount > 0) {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("Found %d duplicate CardId row(s) — first row wins (WarnOnly policy)"), DupCount);
    }
    if (EmptyFTextCount > 0) {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("Rejected %d row(s) with empty DisplayName (bFTextEmptyGuard=true)"), EmptyFTextCount);
    }
    return true;
}

// AC-DP-14 RefreshCatalog 재진입 차단
void UDataPipelineSubsystem::RefreshCatalog() {
    if (bRefreshInProgress || CurrentState == EDataPipelineState::Loading) {
        UE_LOG(LogDataPipeline, Warning, TEXT("RefreshCatalog already in progress — ignoring re-entry"));
        return;
    }
    bRefreshInProgress = true;
    ON_SCOPE_EXIT { bRefreshInProgress = false; };

    // 기존 등록 클리어 후 재실행
    DreamRegistry.Empty();
    FormRegistry.Empty();
    CardTable = nullptr;
    StillnessAsset = nullptr;
    FailedCatalogs.Empty();

    // Initialize와 동일 순서 재실행 (Step 1-4)
    CurrentState = EDataPipelineState::Loading;
    // ... Step 1-4 (Story 004 로직 재사용) ...
}
```

- `GetFailedCatalogs()`는 Story 003에서 이미 선언 — 본 story는 실제 내용 채움

---

## Out of Scope

- Story 006: T_init budget + 3단계 임계
- Story 007: 단조성 guard

---

## QA Test Cases

**For Logic story:**
- **AC-DP-11 (DuplicateCardIdPolicy WarnOnly)**
  - Given: `WarnOnly` policy + DT에 "dup_id" 2행 (첫 번째 "A", 두 번째 "B")
  - When: `Initialize()`
  - Then: Ready + `GetCardRow("dup_id")` = 첫 row (A) + Warning 로그 "1 duplicate"
  - Edge cases: `DegradedFallback` policy → `GetState() == DegradedFallback` + 빈 TOptional
- **AC-DP-12 (bFTextEmptyGuard)**
  - Given: 5 cards, 1개 DisplayName 빈 FText
  - When: `bFTextEmptyGuard = true` + `Initialize()`
  - Then: `GetAllCardIds().Num() == 4` + 빈 row 제외 + Warning 로그 "1 rejected"
  - Edge cases: `false` → 5 모두 등록
- **AC-DP-14 (RefreshCatalog 재진입)**
  - Given: Loading 상태 중
  - When: `RefreshCatalog()` 재호출
  - Then: 즉시 return + Warning 로그 + 원래 Initialize 계속 진행
- **AC-DP-15 (GetFailedCatalogs API)**
  - Given: Card Fail + Dream/Form/Stillness Ready
  - When: `GetFailedCatalogs()`
  - Then: `TArray<FName>` = [Card]
  - Edge cases: 모두 Ready → 빈 배열
- **AC-DP-07 [5.6-VERIFY] (PIE RefreshCatalog)**
  - Setup: PIE session + 10 cards
  - Verify: 1개 row 추가 → `RefreshCatalog()` → `GetAllCardIds().Num() == 11`
  - Pass: Ready 복귀 + 새 CardId 포함

---

## Test Evidence

**Story Type**: Logic (Integration for AC-DP-07, AC-DP-15)
**Required evidence**:
- `tests/unit/data-pipeline/duplicate_card_id_policy_test.cpp` (AC-DP-11)
- `tests/unit/data-pipeline/ftext_empty_guard_test.cpp` (AC-DP-12)
- `tests/unit/data-pipeline/refresh_reentry_guard_test.cpp` (AC-DP-14)
- `tests/integration/data-pipeline/degraded_fallback_api_test.cpp` (AC-DP-15)
- `tests/integration/data-pipeline/pie_refresh_catalog_test.cpp` (AC-DP-07 [5.6-VERIFY])

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 004 (catalog registration 기초)
- Unlocks: Story 006
