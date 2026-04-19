// Copyright Moss Baby
//
// DataPipelineSchemaTests.cpp — Data Pipeline Schema Types 자동화 테스트
//
// Story-1-4: Data Pipeline Schema Types
// AC 커버리지:
//   AC-1 (FGiftCardRow) — C1 schema gate: CODE_REVIEW 검증 (grep 기반, 런타임 불필요)
//   AC-2 (MossFinalFormAsset) — GetPrimaryAssetId() 반환 검증: AUTOMATED
//   AC-3 (DreamDataAsset) — GetPrimaryAssetId() 반환 검증: AUTOMATED
//   AC-4 (StillnessBudgetAsset) — 기본값 검증: AUTOMATED
//   AC-5 (MossDataPipelineSettings) — 9개 CDO 기본값 검증: AUTOMATED
//   AC-6 (C1 schema gate CI grep) — CODE_REVIEW (tests/unit/data-pipeline/README.md 참조)
//
// 테스트 카테고리: MossBaby.Data.Schema.*
//
// Run headless:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Data.Schema.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Data/MossFinalFormAsset.h"
#include "Data/DreamDataAsset.h"
#include "Data/StillnessBudgetAsset.h"
#include "Settings/MossDataPipelineSettings.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// AC-5: UMossDataPipelineSettings CDO 기본값 검증
//
// GDD §G.2 Performance Budget + §G.3 Behavior Policy + §G.4 Threshold Scaling
// 9개 knob의 CDO 기본값이 GDD/ADR에 명시된 값과 일치하는지 검증.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineSettingsCDOTest,
    "MossBaby.Data.Schema.DataPipelineSettingsCDO",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineSettingsCDOTest::RunTest(const FString& Parameters)
{
    // Arrange: CDO 접근
    const UMossDataPipelineSettings* Settings = UMossDataPipelineSettings::Get();

    // Assert: Get() 반환값 비-null
    if (!TestNotNull(TEXT("UMossDataPipelineSettings::Get() 반환값이 nullptr이 아님"), Settings))
    {
        return false;
    }

    // Assert: Performance Budget — MaxInitTimeBudgetMs
    TestEqual(
        TEXT("MaxInitTimeBudgetMs 기본값 == 50.0f (GDD §D2 Formula 기준)"),
        Settings->MaxInitTimeBudgetMs,
        50.0f);

    // Assert: Performance Budget — MaxCatalogSizeCards
    TestEqual(
        TEXT("MaxCatalogSizeCards 기본값 == 200 (Full Vision 5배 여유)"),
        Settings->MaxCatalogSizeCards,
        200);

    // Assert: Performance Budget — MaxCatalogSizeDreams
    TestEqual(
        TEXT("MaxCatalogSizeDreams 기본값 == 100 (Full Vision 2배 여유)"),
        Settings->MaxCatalogSizeDreams,
        100);

    // Assert: Performance Budget — MaxCatalogSizeForms
    TestEqual(
        TEXT("MaxCatalogSizeForms 기본값 == 16 (Full Vision 여유)"),
        Settings->MaxCatalogSizeForms,
        16);

    // Assert: Threshold Scaling — CatalogOverflowWarnMultiplier
    TestEqual(
        TEXT("CatalogOverflowWarnMultiplier 기본값 == 1.05f"),
        Settings->CatalogOverflowWarnMultiplier,
        1.05f);

    // Assert: Threshold Scaling — CatalogOverflowErrorMultiplier
    TestEqual(
        TEXT("CatalogOverflowErrorMultiplier 기본값 == 1.5f"),
        Settings->CatalogOverflowErrorMultiplier,
        1.5f);

    // Assert: Threshold Scaling — CatalogOverflowFatalMultiplier
    TestEqual(
        TEXT("CatalogOverflowFatalMultiplier 기본값 == 2.0f"),
        Settings->CatalogOverflowFatalMultiplier,
        2.0f);

    // Assert: Behavior Policy — DuplicateCardIdPolicy
    TestEqual(
        TEXT("DuplicateCardIdPolicy 기본값 == WarnOnly"),
        Settings->DuplicateCardIdPolicy,
        EDuplicateCardIdPolicy::WarnOnly);

    // Assert: Behavior Policy — bFTextEmptyGuard
    TestTrue(
        TEXT("bFTextEmptyGuard 기본값 == true"),
        Settings->bFTextEmptyGuard);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-2: UMossFinalFormAsset GetPrimaryAssetId() 검증
//
// ADR-0010: GetPrimaryAssetId() → FPrimaryAssetId("FinalForm", FormId)
// NewObject로 인스턴스 생성 후 FormId 설정 → GetPrimaryAssetId() 반환값 검증.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFinalFormAssetGetPrimaryAssetIdTest,
    "MossBaby.Data.Schema.FinalFormAssetGetPrimaryAssetId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFinalFormAssetGetPrimaryAssetIdTest::RunTest(const FString& Parameters)
{
    // Arrange: NewObject로 인스턴스 생성 (GC 관리 — GetTransientPackage() 사용)
    UMossFinalFormAsset* Asset = NewObject<UMossFinalFormAsset>(GetTransientPackage());
    if (!TestNotNull(TEXT("UMossFinalFormAsset NewObject 생성 성공"), Asset))
    {
        return false;
    }

    // Arrange: FormId 설정
    Asset->FormId = FName("test_sprout");

    // Act: GetPrimaryAssetId() 호출
    const FPrimaryAssetId AssetId = Asset->GetPrimaryAssetId();

    // Assert: PrimaryAssetType == "FinalForm"
    TestEqual(
        TEXT("FinalForm PrimaryAssetType == \"FinalForm\""),
        AssetId.PrimaryAssetType.ToString(),
        FString("FinalForm"));

    // Assert: PrimaryAssetName == "test_sprout"
    TestEqual(
        TEXT("FinalForm PrimaryAssetName == \"test_sprout\""),
        AssetId.PrimaryAssetName.ToString(),
        FString("test_sprout"));

    // Assert: IsValid()
    TestTrue(
        TEXT("FinalForm AssetId.IsValid() == true"),
        AssetId.IsValid());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-3: UDreamDataAsset GetPrimaryAssetId() 검증
//
// ADR-0002: GetPrimaryAssetId() → FPrimaryAssetId("DreamData", DreamId)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDreamDataAssetGetPrimaryAssetIdTest,
    "MossBaby.Data.Schema.DreamDataAssetGetPrimaryAssetId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamDataAssetGetPrimaryAssetIdTest::RunTest(const FString& Parameters)
{
    // Arrange: NewObject로 인스턴스 생성
    UDreamDataAsset* Asset = NewObject<UDreamDataAsset>(GetTransientPackage());
    if (!TestNotNull(TEXT("UDreamDataAsset NewObject 생성 성공"), Asset))
    {
        return false;
    }

    // Arrange: DreamId 설정
    Asset->DreamId = FName("dream_001");

    // Act: GetPrimaryAssetId() 호출
    const FPrimaryAssetId AssetId = Asset->GetPrimaryAssetId();

    // Assert: PrimaryAssetType == "DreamData"
    TestEqual(
        TEXT("DreamData PrimaryAssetType == \"DreamData\""),
        AssetId.PrimaryAssetType.ToString(),
        FString("DreamData"));

    // Assert: PrimaryAssetName == "dream_001"
    TestEqual(
        TEXT("DreamData PrimaryAssetName == \"dream_001\""),
        AssetId.PrimaryAssetName.ToString(),
        FString("dream_001"));

    // Assert: IsValid()
    TestTrue(
        TEXT("DreamData AssetId.IsValid() == true"),
        AssetId.IsValid());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-3 (추가): UDreamDataAsset 기본값 검증
//
// C2 schema gate: TriggerThreshold 기본값 0.6f, EarliestDay 기본값 2,
// TriggerTagWeights 초기화 빈 TMap 확인.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDreamDataAssetDefaultsTest,
    "MossBaby.Data.Schema.DreamDataAssetDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDreamDataAssetDefaultsTest::RunTest(const FString& Parameters)
{
    // Arrange: NewObject로 인스턴스 생성 (기본값 초기화 상태)
    UDreamDataAsset* Asset = NewObject<UDreamDataAsset>(GetTransientPackage());
    if (!TestNotNull(TEXT("UDreamDataAsset NewObject 생성 성공"), Asset))
    {
        return false;
    }

    // Assert: TriggerThreshold 기본값 (C2 schema gate)
    TestEqual(
        TEXT("TriggerThreshold 기본값 == 0.6f (C2 schema gate)"),
        Asset->TriggerThreshold,
        0.6f);

    // Assert: EarliestDay 기본값
    TestEqual(
        TEXT("EarliestDay 기본값 == 2"),
        Asset->EarliestDay,
        2);

    // Assert: TriggerTagWeights 초기화 빈 TMap
    TestTrue(
        TEXT("TriggerTagWeights 초기화 후 빈 TMap"),
        Asset->TriggerTagWeights.IsEmpty());

    // Assert: RequiredStage 기본값 — enum class 0번째 값 (Sprout)
    // (qa-tester GAP-2 보완: EGrowthStage 첫 값이 의도된 기본 상태인지 확인)
    TestEqual(
        TEXT("RequiredStage 기본값 == EGrowthStage::Sprout (enum 0번째)"),
        static_cast<uint8>(Asset->RequiredStage),
        static_cast<uint8>(EGrowthStage::Sprout));

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-4: UStillnessBudgetAsset 기본값 검증
//
// ADR-0002: 단일 인스턴스. 기본값: MotionSlots=2, ParticleSlots=2,
// SoundSlots=3, bReducedMotionEnabled=false.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStillnessBudgetAssetDefaultsTest,
    "MossBaby.Data.Schema.StillnessBudgetAssetDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStillnessBudgetAssetDefaultsTest::RunTest(const FString& Parameters)
{
    // Arrange: NewObject로 인스턴스 생성 (기본값 초기화 상태)
    UStillnessBudgetAsset* Asset = NewObject<UStillnessBudgetAsset>(GetTransientPackage());
    if (!TestNotNull(TEXT("UStillnessBudgetAsset NewObject 생성 성공"), Asset))
    {
        return false;
    }

    // Assert: MotionSlots 기본값
    TestEqual(
        TEXT("MotionSlots 기본값 == 2"),
        Asset->MotionSlots,
        2);

    // Assert: ParticleSlots 기본값
    TestEqual(
        TEXT("ParticleSlots 기본값 == 2"),
        Asset->ParticleSlots,
        2);

    // Assert: SoundSlots 기본값
    TestEqual(
        TEXT("SoundSlots 기본값 == 3"),
        Asset->SoundSlots,
        3);

    // Assert: bReducedMotionEnabled 기본값
    TestFalse(
        TEXT("bReducedMotionEnabled 기본값 == false"),
        Asset->bReducedMotionEnabled);

    // Assert: GetPrimaryAssetId() PrimaryAssetType 검증 (qa-tester GAP-1 보완)
    // StillnessBudget은 ADR-0002 단일 인스턴스 패턴 — PrimaryAssetType="StillnessBudget" 고정
    const FPrimaryAssetId AssetId = Asset->GetPrimaryAssetId();
    TestEqual(
        TEXT("StillnessBudget PrimaryAssetType == \"StillnessBudget\""),
        AssetId.PrimaryAssetType.ToString(),
        FString("StillnessBudget"));

    return true;
}

#endif // WITH_AUTOMATION_TESTS
