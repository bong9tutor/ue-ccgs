# Story 004: Catalog Registration Loading (Card → FinalForm → Dream → Stillness) + DegradedFallback

> **Epic**: Data Pipeline
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/data-pipeline.md`
**Requirement**: `TR-dp-002` (Sync 일괄 로드)
**ADR Governing Implementation**: ADR-0003 (Step 1-4 순차 순서 고정)
**ADR Decision Summary**: Card DataTable → FinalForm DataAsset bucket → Dream DataAsset bucket → Stillness single. 한 단계 실패 시 즉시 `DegradedFallback` 전환. `UAssetManager::LoadPrimaryAssets` sync variant 사용.
**Engine**: UE 5.6 | **Risk**: MEDIUM (GameInstance::Init timing)
**Engine Notes**: `UDataTable::LoadSynchronous` 또는 `UAssetManager::LoadPrimaryAssets` sync 변종. `UEditorValidator` 저장 시점 작동 [5.6-VERIFY] AC-DP-06.

**Control Manifest Rules (Foundation layer)**:
- Required: "4단계 순서 고정: Card DataTable → FinalForm DataAsset bucket → Dream DataAsset bucket → Stillness single DataAsset" (ADR-0003)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] AC-DP-02: 4개 카탈로그 Mock이 모두 정상 로드 가능 시 `Initialize()` 실행 → 등록 로그 순서 `Card → FinalForm → Dream → Stillness` + `IsReady() == true` (Logic · BLOCKING · P0)
- [ ] AC-DP-03: Card 정상 + FinalForm DT 로드 실패 설정 시 `Initialize()` → 상태 `DegradedFallback` + Dream·Stillness 등록 시도되지 않음 + `LogDataPipeline Error` 메시지 정확히 1회 (Logic · BLOCKING · P0)
- [ ] AC-DP-04: Ready 상태에서 `GetCardRow(NAME_None)`, `GetCardRow("nonexistent")`, `GetCardRow("삭제된 옛 ID")` 모두 빈 `TOptional` 반환 (default row 반환 금지) (Logic · BLOCKING · P0)
- [ ] AC-DP-05: Dream 카탈로그 DegradedFallback 시 `GetAllDreamAssets()` 빈 `TArray`, `GetGrowthFormRow("nonexistent")` 빈 `TOptional` (Logic · BLOCKING · P0)
- [ ] AC-DP-06 [5.6-VERIFY]: `UEditorValidatorBase` 서브클래스 구현. `FGiftCardRow`에 수치 stat 필드 추가 시도 저장 시 `EDataValidationResult::Invalid` 반환 + Row ID + 필드명 로그 포함 (Integration · BLOCKING · P0)
- [ ] AC-DP-16: 존재 카탈로그 "mossy_dawn" + 조회 "mossy_dawn_v1" (옛 ID) → `TOptional` 빈 + `LogDataPipeline Warning` "Unknown CardId: mossy_dawn_v1 — rename 또는 migration 누락 의심" (Integration · BLOCKING · P0)

---

## Implementation Notes

*Derived from ADR-0003 §구체 로드 순서 + ADR-0010 §Pipeline 통합:*

```cpp
// Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp
void UDataPipelineSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    CurrentState = EDataPipelineState::Loading;

    const double StartTime = FPlatformTime::Seconds();

    // Step 1: Card DataTable (sync load)
    if (!RegisterCardCatalog()) {
        CurrentState = EDataPipelineState::DegradedFallback;
        FailedCatalogs.Add("Card");
        UE_LOG(LogDataPipeline, Error, TEXT("Card catalog registration failed"));
        return;  // 이후 단계 시도하지 않음 (AC-DP-03)
    }

    // Step 2: FinalForm DataAsset bucket
    if (!RegisterFinalFormCatalog()) {
        CurrentState = EDataPipelineState::DegradedFallback;
        FailedCatalogs.Add("FinalForm");
        UE_LOG(LogDataPipeline, Error, TEXT("FinalForm catalog registration failed"));
        return;
    }

    // Step 3: Dream DataAsset bucket
    if (!RegisterDreamCatalog()) {
        CurrentState = EDataPipelineState::DegradedFallback;
        FailedCatalogs.Add("Dream");
        UE_LOG(LogDataPipeline, Error, TEXT("Dream catalog registration failed"));
        return;
    }

    // Step 4: Stillness single DataAsset
    if (!RegisterStillnessCatalog()) {
        CurrentState = EDataPipelineState::DegradedFallback;
        FailedCatalogs.Add("Stillness");
        UE_LOG(LogDataPipeline, Error, TEXT("Stillness catalog registration failed"));
        return;
    }

    const double Elapsed = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    UE_LOG(LogDataPipeline, Log, TEXT("Pipeline Ready — T_init = %.2f ms"), Elapsed);

    CurrentState = EDataPipelineState::Ready;
}

bool UDataPipelineSubsystem::RegisterDreamCatalog() {
    UAssetManager& AssetMgr = UAssetManager::Get();
    TArray<FPrimaryAssetId> DreamIds;
    AssetMgr.GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"), DreamIds);
    if (DreamIds.Num() == 0) {
        UE_LOG(LogDataPipeline, Error,
            TEXT("DreamData PrimaryAssetType empty — DefaultEngine.ini 등록 누락 의심"));
        return false;
    }
    TArray<FName> Bundles;
    AssetMgr.LoadPrimaryAssets(DreamIds, Bundles);  // sync 변종
    for (const FPrimaryAssetId& Id : DreamIds) {
        UDreamDataAsset* Asset = Cast<UDreamDataAsset>(AssetMgr.GetPrimaryAssetObject(Id));
        if (!Asset) continue;
        DreamRegistry.Add(Asset->DreamId, Asset);
    }
    return DreamRegistry.Num() > 0;
}
// FinalForm/Card/Stillness 등록도 동일 패턴

// UEditorValidatorBase 구현 (AC-DP-06)
UCLASS()
class UMossCardValidator : public UEditorValidatorBase {
    GENERATED_BODY()
public:
    virtual bool CanValidateAsset_Implementation(UObject* Asset) const override {
        return Cast<UDataTable>(Asset) && Cast<UDataTable>(Asset)->RowStruct == FGiftCardRow::StaticStruct();
    }
    virtual EDataValidationResult ValidateLoadedAsset_Implementation(
        UObject* Asset, TArray<FText>& Errors) override {
        // C1 schema gate — int32/float/double 필드 검출 시 Invalid
        // (구체 reflection scan은 ADR-0002 §R4 참조)
        return EDataValidationResult::Valid;
    }
};
```

- `UEditorValidator`는 Editor 모듈 한정 (`#if WITH_EDITOR` 가드 또는 별도 Editor module)
- `FailedCatalogs` 배열은 AC-DP-15 `GetFailedCatalogs()` API 기반

---

## Out of Scope

- Story 005: 에디터 E 케이스 (DuplicateCardId, FTextEmpty, RefreshCatalog 재진입)
- Story 006: T_init 측정 + 3단계 임계 로그
- Story 007: 단조성 guard + AC-DP-17

---

## QA Test Cases

**For Logic story:**
- **AC-DP-02 (등록 순서)**
  - Given: 4 catalog Mock all-ok
  - When: `Initialize()`
  - Then: 로그 순서 Card → FinalForm → Dream → Stillness + `IsReady()`
- **AC-DP-03 (DegradedFallback 진입)**
  - Given: Card ok, FinalForm DT load 실패 mock
  - When: `Initialize()`
  - Then: 상태 `DegradedFallback` + Dream·Stillness register 미호출 + Error 로그 1회
- **AC-DP-04 (fail-close Card)**
  - Given: Ready 상태 + Card catalog 정상
  - When: `GetCardRow(NAME_None)` / `GetCardRow("nonexistent")` / `GetCardRow("old_id")`
  - Then: 3개 모두 빈 `TOptional`
  - Edge cases: `GetCardRow("")` (empty string) 동일
- **AC-DP-05 (fail-close Dream/Growth)**
  - Given: Dream DegradedFallback
  - When: `GetAllDreamAssets()` + `GetGrowthFormRow("x")`
  - Then: 빈 TArray + 빈 TOptional (TArray는 nullptr 포함 금지)
- **AC-DP-06 [5.6-VERIFY]**
  - Setup: Editor with UMossCardValidator registered
  - Verify: DataTable Row에 `int32 GrowthBonus = 10` 저장 시도 → Validator 호출됨 → `Invalid` 반환 → 저장 차단
  - Pass: Row ID + 필드명 로그 포함

---

## Test Evidence

**Story Type**: Logic (Integration for AC-DP-06)
**Required evidence**:
- `tests/unit/data-pipeline/catalog_registration_order_test.cpp` (AC-DP-02)
- `tests/unit/data-pipeline/degraded_fallback_transition_test.cpp` (AC-DP-03)
- `tests/unit/data-pipeline/fail_close_contract_test.cpp` (AC-DP-04, 05)
- `tests/integration/data-pipeline/schema_guard_validator_test.cpp` (AC-DP-06 [5.6-VERIFY])
- `tests/integration/data-pipeline/unknown_card_id_warning_test.cpp` (AC-DP-16)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 003 (Subsystem 뼈대), Story 002 (ini 등록)
- Unlocks: Story 005, 006
