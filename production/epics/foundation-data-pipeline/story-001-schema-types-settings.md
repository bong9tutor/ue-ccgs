# Story 001: Schema Types (FGiftCardRow, UMossFinalFormAsset, UDreamDataAsset, UStillnessBudgetAsset) + Developer Settings

> **Epic**: Data Pipeline
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 0.5 days (~3-4 hours)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/data-pipeline.md`
**Requirement**: `TR-dp-001` (OQ-ADR-1 container selection), `TR-dp-003` (UMossFinalFormAsset TMap editing)
**ADR Governing Implementation**: ADR-0002 (Container Selection), ADR-0010 (FFinalFormRow UPrimaryDataAsset 격상), ADR-0011 (UDeveloperSettings knob exposure)
**ADR Decision Summary**: Card = `UDataTable + FGiftCardRow` (C1 schema gate — 수치 필드 금지), FinalForm = `UMossFinalFormAsset : UPrimaryDataAsset` per-form (TMap 인라인 편집), Dream = `UDreamDataAsset : UPrimaryDataAsset` (C2 schema gate — 트리거 임계 외부 노출), Stillness = `UStillnessBudgetAsset : UPrimaryDataAsset` single.
**Engine**: UE 5.6 | **Risk**: LOW (타입 정의만)
**Engine Notes**: `GetPrimaryAssetId()` override 의무 (ADR-0002/0010). `UPROPERTY()` 매크로 GC 안전성 필수 (R2 BLOCKER 6).

**Control Manifest Rules (Foundation layer)**:
- Required: "Data Pipeline 컨테이너 선택" — Card=DataTable, FinalForm/Dream/Stillness=UPrimaryDataAsset (ADR-0002, ADR-0010)
- Required: "UPrimaryDataAsset 상속 클래스는 `GetPrimaryAssetId()` override"
- Forbidden: "`int32`/`float`/`double` 수치 효과 필드 on `FGiftCardRow`" — C1 schema gate
- Forbidden: "hardcode Dream trigger 임계 in code" — C2 schema gate (UPROPERTY 필드만)
- Forbidden: "`UDataRegistrySubsystem`" — 5.6 production 검증 부족

---

## Acceptance Criteria

*From GDD §Acceptance Criteria §Gap-1, Gap-2 + ADR-0002/0010 schema:*

- [ ] `FGiftCardRow : public FTableRowBase` 정의 — 필드: `FName CardId`, `TArray<FName> Tags`, `FText DisplayName`, `FText Description`, `FSoftObjectPath IconPath`. **수치 필드 없음** (C1 schema gate — ADR-0002)
- [ ] `UMossFinalFormAsset : UPrimaryDataAsset` 정의 — 필드: `FName FormId`, `TMap<FName, float> RequiredTagWeights`, `FText DisplayName`, `FSoftObjectPath MeshPath`, `FSoftObjectPath MaterialPresetPath`. `GetPrimaryAssetId()` → `FPrimaryAssetId("FinalForm", FormId)` (ADR-0010)
- [ ] `UDreamDataAsset : UPrimaryDataAsset` 정의 — 필드: `FName DreamId`, `FText BodyText`, `TMap<FName, float> TriggerTagWeights`, `float TriggerThreshold [0.0, 1.0]`, `EGrowthStage RequiredStage`, `int32 EarliestDay`. `GetPrimaryAssetId()` → `FPrimaryAssetId("DreamData", DreamId)` (C2 schema gate — 모든 임계 UPROPERTY)
- [ ] `UStillnessBudgetAsset : UPrimaryDataAsset` 정의 — 필드: `int32 MotionSlots [1,6]` (default 2), `int32 ParticleSlots [1,6]` (default 2), `int32 SoundSlots [1,8]` (default 3), `bool bReducedMotionEnabled` (default false)
- [ ] `UMossDataPipelineSettings : UDeveloperSettings` 정의 — knobs: `MaxInitTimeBudgetMs` (default 50.0), `MaxCatalogSizeCards` (default 200), `MaxCatalogSizeDreams` (default 100), `MaxCatalogSizeForms` (default 16), `CatalogOverflowWarnMultiplier` (1.05), `CatalogOverflowErrorMultiplier` (1.5), `CatalogOverflowFatalMultiplier` (2.0), `DuplicateCardIdPolicy` enum (WarnOnly / DegradedFallback, default WarnOnly), `bFTextEmptyGuard` (default true)
- [ ] C1 schema gate CI grep: `grep -rE "(int32|float|double) [A-Z][A-Za-z]*;?" Source/MadeByClaudeCode/.../FGiftCardRow*` 매치 0건 (CODE_REVIEW)

---

## Implementation Notes

*Derived from ADR-0002 §Key Interfaces + ADR-0010 §Class 정의 + ADR-0011 §Naming Convention:*

```cpp
// Source/MadeByClaudeCode/Data/GiftCardRow.h
USTRUCT(BlueprintType)
struct FGiftCardRow : public FTableRowBase {
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName CardId;
    UPROPERTY(EditAnywhere) TArray<FName> Tags;
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere) FText Description;
    UPROPERTY(EditAnywhere) FSoftObjectPath IconPath;
    // NO int32/float/double fields — C1 schema gate (ADR-0002)
};

// Source/MadeByClaudeCode/Data/MossFinalFormAsset.h
UCLASS(BlueprintType)
class UMossFinalFormAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, AssetRegistrySearchable) FName FormId;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="1.0"))
    TMap<FName, float> RequiredTagWeights;
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere, meta=(AllowedClasses="StaticMesh")) FSoftObjectPath MeshPath;
    UPROPERTY(EditAnywhere, meta=(AllowedClasses="MaterialInstance")) FSoftObjectPath MaterialPresetPath;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override {
        return FPrimaryAssetId("FinalForm", FormId);
    }
};

// Source/MadeByClaudeCode/Data/DreamDataAsset.h
UCLASS(BlueprintType)
class UDreamDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) FName DreamId;
    UPROPERTY(EditAnywhere) FText BodyText;
    UPROPERTY(EditAnywhere) TMap<FName, float> TriggerTagWeights;
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="1.0"))
    float TriggerThreshold = 0.6f;  // C2 schema gate — UPROPERTY only
    UPROPERTY(EditAnywhere) EGrowthStage RequiredStage;
    UPROPERTY(EditAnywhere) int32 EarliestDay = 2;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override {
        return FPrimaryAssetId("DreamData", DreamId);
    }
};

// Source/MadeByClaudeCode/Data/StillnessBudgetAsset.h
UCLASS(BlueprintType)
class UStillnessBudgetAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, meta=(ClampMin="1", ClampMax="6")) int32 MotionSlots = 2;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1", ClampMax="6")) int32 ParticleSlots = 2;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1", ClampMax="8")) int32 SoundSlots = 3;
    UPROPERTY(EditAnywhere) bool bReducedMotionEnabled = false;
};

// Source/MadeByClaudeCode/Settings/MossDataPipelineSettings.h
UENUM()
enum class EDuplicateCardIdPolicy : uint8 { WarnOnly, DegradedFallback };

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Data Pipeline"))
class UMossDataPipelineSettings : public UDeveloperSettings {
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Performance Budget",
              meta=(ClampMin="10.0", ClampMax="200.0", Units="Milliseconds", ConfigRestartRequired="true"))
    float MaxInitTimeBudgetMs = 50.0f;

    UPROPERTY(Config, EditAnywhere, Category="Performance Budget",
              meta=(ClampMin="10", ClampMax="1000")) int32 MaxCatalogSizeCards = 200;
    UPROPERTY(Config, EditAnywhere, Category="Performance Budget") int32 MaxCatalogSizeDreams = 100;
    UPROPERTY(Config, EditAnywhere, Category="Performance Budget") int32 MaxCatalogSizeForms = 16;

    UPROPERTY(Config, EditAnywhere, Category="Threshold Scaling",
              meta=(ClampMin="1.0", ClampMax="1.2")) float CatalogOverflowWarnMultiplier = 1.05f;
    UPROPERTY(Config, EditAnywhere, Category="Threshold Scaling",
              meta=(ClampMin="1.2", ClampMax="1.8")) float CatalogOverflowErrorMultiplier = 1.5f;
    UPROPERTY(Config, EditAnywhere, Category="Threshold Scaling",
              meta=(ClampMin="1.5", ClampMax="3.0")) float CatalogOverflowFatalMultiplier = 2.0f;

    UPROPERTY(Config, EditAnywhere, Category="Behavior Policy")
    EDuplicateCardIdPolicy DuplicateCardIdPolicy = EDuplicateCardIdPolicy::WarnOnly;
    UPROPERTY(Config, EditAnywhere, Category="Behavior Policy") bool bFTextEmptyGuard = true;

    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }
    virtual FName GetSectionName() const override { return TEXT("Data Pipeline"); }
    static const UMossDataPipelineSettings* Get() { return GetDefault<UMossDataPipelineSettings>(); }
};
```

- `EGrowthStage`는 `core-moss-baby-character` epic 또는 공용 enums 헤더에 정의 (이 story는 forward declare + include)
- `Source/MadeByClaudeCode/Data/` 디렉터리 + `Source/MadeByClaudeCode/Settings/` 분리

---

## Out of Scope

- Story 002: DefaultEngine.ini `PrimaryAssetTypesToScan` 등록
- Story 003: UDataPipelineSubsystem 뼈대 + 4-state machine
- Story 004: Catalog registration + loading order (Rule 2)

---

## QA Test Cases

**For Logic story:**
- **Schema type definitions**
  - Given: 각 헤더 파일 컴파일
  - When: UE Editor 로드 + Content Browser에서 각 타입으로 자산 생성 시도
  - Then: `UMossFinalFormAsset`, `UDreamDataAsset`, `UStillnessBudgetAsset` 각각 생성 가능 + DataTable에서 `FGiftCardRow` row 타입 선택 가능
- **C1 schema gate (CODE_REVIEW)**
  - Setup: `FGiftCardRow` struct
  - Verify: `grep -rE "int32 [A-Z]|float [A-Z]|double [A-Z]" Source/MadeByClaudeCode/Data/GiftCardRow.h` (UPROPERTY 주변)
  - Pass: 0 매치 + 허용 타입(`FName`, `TArray<FName>`, `FText`, `FSoftObjectPath`)만 존재
- **UDeveloperSettings CDO access**
  - Given: `UMossDataPipelineSettings` 정의
  - When: `UMossDataPipelineSettings::Get()->MaxInitTimeBudgetMs`
  - Then: 50.0 반환 (default); Project Settings → Game → Moss Baby → "Data Pipeline" 카테고리 수동 확인
- **GetPrimaryAssetId override**
  - Given: Test DreamDataAsset with DreamId = "dream_001"
  - When: `GetPrimaryAssetId()`
  - Then: `FPrimaryAssetId("DreamData", "dream_001")` 반환

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/data-pipeline/schema_types_test.cpp` (AUTOMATED: GetPrimaryAssetId + Settings CDO)
- C1 schema gate CI grep script: `.claude/hooks/data-pipeline-c1-grep.sh` (CODE_REVIEW)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (Foundation 독립 — Growth epic `EGrowthStage` forward declare 가능)
- Unlocks: Story 002, 003

---

## Completion Notes

**Completed**: 2026-04-19
**Criteria**: 6/6 passing (AC-1~6 모두 COVERED — 5 AUTOMATED + 1 CODE_REVIEW)
**Files delivered** (8건):
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/GiftCardRow.h` (56 lines, FTableRowBase)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/MossFinalFormAsset.h` (68 lines)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DreamDataAsset.h` (78 lines)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/StillnessBudgetAsset.h` (56 lines)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/GrowthStage.h` (43 lines, **임시 공용 enum**)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Settings/MossDataPipelineSettings.h` (152 lines, 9 knobs + EDuplicateCardIdPolicy)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineSchemaTests.cpp` (5 tests, `MossBaby.Data.Schema.*`)
- `tests/unit/data-pipeline/README.md` (evidence index + C1 grep 명령)

**C1 Schema Gate**: `grep -rE "(int32|float|double) [A-Z]" GiftCardRow.h` → **0 매치** ✅
**C2 Schema Gate**: DreamDataAsset의 TriggerThreshold/TriggerTagWeights/EarliestDay 모두 `UPROPERTY` 노출, 하드코딩 0건 ✅

**Test Evidence**: 5 tests (`FDataPipelineSettingsCDOTest` 9 knobs, `FFinalFormAssetGetPrimaryAssetIdTest`, `FDreamDataAssetGetPrimaryAssetIdTest`, `FDreamDataAssetDefaultsTest`, `FStillnessBudgetAssetDefaultsTest`). QA GAP 2건 인라인 보완 (RequiredStage 기본값 + StillnessBudget PrimaryAssetType 검증 추가).

**Code Review**: APPROVED (unreal-specialist) + GAPS→RESOLVED (qa-tester 2건 인라인 보완)
**ADR 준수**:
- ADR-0001: 금지 패턴 grep 0 매치 (주석 인용만)
- ADR-0002: Card=DataTable+FGiftCardRow, FinalForm/Dream/Stillness=UPrimaryDataAsset ✅
- ADR-0010: per-form 격상 + TMap 인라인 편집 ✅
- ADR-0011: UDeveloperSettings + GetCategoryName/SectionName + static Get() ✅

**Deviations / Notes**:
- **ADVISORY (TD-006)**: `Data/GrowthStage.h` 임시 공용 enum — `core-moss-baby-character` epic 진입 시 `Characters/` 폴더로 이동 예정. `DreamDataAsset.h`의 `#include` 경로 업데이트 + 다른 파일이 include 시작할 경우 리팩토링 범위 확장 위험.
- **긍정적 관찰**: `FGiftCardRow.IconPath`에 `AllowedClasses="Texture2D"` 에디터 가드 자발 추가 (Story 스펙 이상의 방어).

**제안 (unreal-specialist, 비블로킹)**:
- `TMap<FName, float>`의 `ClampMin/Max` 메타가 UE 5.6 에디터에서 실제 Value 필드에 적용되는지 Growth sprint 초기 수동 검증 권고. 미동작 시 `UEditorValidatorBase`로 보완.
