// Copyright Moss Baby
//
// DataPipelineCatalogTests.cpp — Catalog Registration + DegradedFallback 단위 테스트
//
// Story-1-6: Initialize 4단계 실제 구현 + DegradedFallback
// ADR-0003: Sync 일괄 로드 채택 (4단계 순서 고정 + 즉시 return)
// ADR-0002: 컨테이너 선택 (Card=DT, FinalForm=DataAsset, Dream=DataAsset, Stillness=DataAsset)
//
// AC 커버리지:
//   AC-DP-02 (등록 순서 Card→FinalForm→Dream→Stillness): Initialize 구현 확인
//            → Initialize 직접 호출은 FSubsystemCollectionBase 귀속으로 불가.
//              실제 순서 검증은 integration test 분류 (Out of Scope).
//              본 파일은 레지스트리 직접 주입 경로로 각 카탈로그 동작을 단위 검증.
//   AC-DP-03 (DegradedFallback 진입): FDataPipelineDegradedFallbackStateTest
//   AC-DP-04 (fail-close Card): Story 1-5 FDataPipelinePullAPIReturnsEmptyOnReadyTest 커버
//   AC-DP-05 (fail-close Dream/Growth): FDataPipelineDreamRegistrationFlowTest +
//            FDataPipelineFinalFormRegistrationFlowTest (empty fallback 포함)
//
// 테스트 전략:
//   - Initialize() 직접 호출 불가 (FSubsystemCollectionBase UE 내부 귀속)
//   - TestingSetState(Ready) + TestingAddXxx() 훅으로 레지스트리 직접 설정
//   - 각 pull API의 정상/빈 반환을 검증
//   - DegradedFallback 상태는 TestingSetState(DegradedFallback)으로 직접 진입 검증
//
// 헤드리스 실행:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Data.Pipeline.Catalog.; Quit" \
//     -nullrhi -nosound -log -unattended
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#include "Misc/AutomationTest.h"
#include "Data/DataPipelineSubsystem.h"
#include "Data/MossFinalFormData.h"
#include "Data/GiftCardRow.h"
#include "Data/DreamDataAsset.h"
#include "Data/StillnessBudgetAsset.h"
#include "Data/MossFinalFormAsset.h"
#include "Engine/DataTable.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 1 — TestingClearAll 후 레지스트리 초기화 확인
//
// TestingClearAll() 호출 시 모든 레지스트리와 FailedCatalogs가 비워지는지 검증.
// 상태 변경(Uninitialized 복귀)은 TestingClearAll 책임이 아님 —
// 상태는 TestingSetState로 별도 제어. 레지스트리 클리어만 검증.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineClearAllTest,
    "MossBaby.Data.Pipeline.Catalog.ClearAll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineClearAllTest::RunTest(const FString& Parameters)
{
    // Arrange: 인스턴스 생성 + Ready 설정 + 데이터 주입
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingSetState(EDataPipelineState::Ready);

    // 꿈 자산 주입
    UDreamDataAsset* DreamAsset = NewObject<UDreamDataAsset>();
    TestNotNull(TEXT("UDreamDataAsset 인스턴스 생성 성공"), DreamAsset);
    if (!DreamAsset) { return false; }
    DreamAsset->DreamId = FName("dream_clear_test");
    Sub->TestingAddDreamAsset(FName("dream_clear_test"), DreamAsset);

    // 주입 후 조회 성공 확인
    TestNotNull(
        TEXT("TestingAddDreamAsset 후 GetDreamAsset 성공"),
        Sub->GetDreamAsset(FName("dream_clear_test")));

    // Act: TestingClearAll 호출 후 상태 Ready 재설정 (ClearAll은 레지스트리만 클리어)
    Sub->TestingClearAll();
    Sub->TestingSetState(EDataPipelineState::Ready);

    // Assert: 꿈 레지스트리 클리어됨
    TestNull(
        TEXT("TestingClearAll 후 GetDreamAsset → nullptr (레지스트리 클리어)"),
        Sub->GetDreamAsset(FName("dream_clear_test")));

    // Assert: GetAllDreamAssets() → 빈 배열
    TestEqual(
        TEXT("TestingClearAll 후 GetAllDreamAssets() → 빈 TArray"),
        Sub->GetAllDreamAssets().Num(),
        0);

    // Assert: FailedCatalogs 클리어됨
    TestEqual(
        TEXT("TestingClearAll 후 GetFailedCatalogs() → 빈 TArray"),
        Sub->GetFailedCatalogs().Num(),
        0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 2 — Card DataTable + 유효한 RowStruct 주입 후 pull API 동작 검증
//
// TestingSetCardTable에 유효한 UDataTable(FGiftCardRow RowStruct) 주입 →
// GetAllCardIds() 빈 배열 (row 없음), GetCardRow 빈 TOptional 확인.
// RowStruct mismatch 시 RegisterCardCatalog가 false 반환하는 로직은
// cpp 구현 코드 리뷰로 검증 (CODE_REVIEW AC).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineCardTableInjectionTest,
    "MossBaby.Data.Pipeline.Catalog.CardTableInjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineCardTableInjectionTest::RunTest(const FString& Parameters)
{
    // Arrange: 인스턴스 생성
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    // DataTable 생성 + FGiftCardRow RowStruct 지정
    UDataTable* CardTable = NewObject<UDataTable>();
    TestNotNull(TEXT("UDataTable 인스턴스 생성 성공"), CardTable);
    if (!CardTable) { return false; }
    CardTable->RowStruct = FGiftCardRow::StaticStruct();

    // TestingSetCardTable 주입
    Sub->TestingSetCardTable(CardTable);

    // Ready 상태 강제 설정 (checkf 우회)
    Sub->TestingSetState(EDataPipelineState::Ready);
    TestTrue(TEXT("TestingSetState(Ready) 후 IsReady() == true"), Sub->IsReady());

    // Act + Assert: GetCardRow(NAME_None) → 빈 TOptional (Fail-close, AC-DP-04)
    const TOptional<FGiftCardRow> CardNone = Sub->GetCardRow(NAME_None);
    TestFalse(
        TEXT("GetCardRow(NAME_None) → 빈 TOptional (AC-DP-04 Fail-close)"),
        CardNone.IsSet());

    // Act + Assert: GetCardRow("nonexistent") → 빈 TOptional
    const TOptional<FGiftCardRow> CardNonexistent = Sub->GetCardRow(FName("nonexistent_card"));
    TestFalse(
        TEXT("GetCardRow('nonexistent_card') → 빈 TOptional (AC-DP-04 Fail-close)"),
        CardNonexistent.IsSet());

    // Act + Assert: GetAllCardIds() → 빈 배열 (DataTable에 Row 없음)
    const TArray<FName> AllIds = Sub->GetAllCardIds();
    TestEqual(
        TEXT("GetAllCardIds() → 빈 TArray (CardTable에 row 없음)"),
        AllIds.Num(),
        0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 3 — Dream 자산 레지스트리 주입 후 pull API 동작 검증
//
// TestingAddDreamAsset으로 UDreamDataAsset 주입 →
// GetDreamAsset("dream_test") → not null,
// GetAllDreamAssets() → 1개 원소 확인.
// AC-DP-05: Dream empty fallback은 DegradedFallback 테스트에서 별도 검증.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineDreamRegistrationFlowTest,
    "MossBaby.Data.Pipeline.Catalog.DreamRegistrationFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineDreamRegistrationFlowTest::RunTest(const FString& Parameters)
{
    // Arrange: 인스턴스 생성 + UDreamDataAsset 인스턴스 생성
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    UDreamDataAsset* DreamAsset = NewObject<UDreamDataAsset>();
    TestNotNull(TEXT("UDreamDataAsset 인스턴스 생성 성공"), DreamAsset);
    if (!DreamAsset) { return false; }

    // Dream 자산 속성 설정
    DreamAsset->DreamId = FName("dream_test");
    DreamAsset->BodyText = FText::FromString(TEXT("테스트 꿈 텍스트"));
    DreamAsset->TriggerThreshold = 0.5f;
    DreamAsset->EarliestDay = 3;

    // TestingAddDreamAsset으로 레지스트리에 직접 주입
    Sub->TestingAddDreamAsset(FName("dream_test"), DreamAsset);

    // Ready 상태 강제 설정
    Sub->TestingSetState(EDataPipelineState::Ready);
    TestTrue(TEXT("TestingSetState(Ready) 후 IsReady() == true"), Sub->IsReady());

    // Act + Assert: GetDreamAsset("dream_test") → not null
    UDreamDataAsset* Result = Sub->GetDreamAsset(FName("dream_test"));
    TestNotNull(
        TEXT("GetDreamAsset('dream_test') → not null (등록된 자산)"),
        Result);

    // Assert: 반환된 자산의 DreamId 일치 확인
    if (Result)
    {
        TestEqual(
            TEXT("반환 DreamAsset.DreamId == 'dream_test'"),
            Result->DreamId,
            FName("dream_test"));
    }

    // Act + Assert: GetAllDreamAssets() → 1개 원소
    const TArray<UDreamDataAsset*> AllDreams = Sub->GetAllDreamAssets();
    TestEqual(
        TEXT("GetAllDreamAssets() → 1개 원소 (1개 등록)"),
        AllDreams.Num(),
        1);

    // Assert: GetAllDreamAssets() 결과에 nullptr 없음 (AC-DP-05: nullptr 포함 금지)
    for (UDreamDataAsset* Asset : AllDreams)
    {
        TestNotNull(TEXT("GetAllDreamAssets() 원소 — nullptr 포함 금지 (AC-DP-05)"), Asset);
    }

    // Act + Assert: GetDreamBodyText("dream_test") → empty 아님
    const FText BodyText = Sub->GetDreamBodyText(FName("dream_test"));
    TestFalse(
        TEXT("GetDreamBodyText('dream_test') → empty 아님 (텍스트 존재)"),
        BodyText.IsEmpty());

    // Act + Assert: 없는 ID → nullptr + GetAllDreamAssets 영향 없음
    UDreamDataAsset* NonExistent = Sub->GetDreamAsset(FName("nonexistent_dream"));
    TestNull(
        TEXT("GetDreamAsset('nonexistent_dream') → nullptr (Fail-close, AC-DP-05)"),
        NonExistent);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 4 — FinalForm 자산 레지스트리 주입 후 pull API 동작 검증
//
// TestingAddFormAsset으로 UMossFinalFormAsset 주입 →
// GetGrowthFormRow(FormId) → FMossFinalFormData not empty 확인.
// AC-DP-05: GetGrowthFormRow("nonexistent") → 빈 TOptional 확인.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineFinalFormRegistrationFlowTest,
    "MossBaby.Data.Pipeline.Catalog.FinalFormRegistrationFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineFinalFormRegistrationFlowTest::RunTest(const FString& Parameters)
{
    // Arrange: 인스턴스 생성 + UMossFinalFormAsset 생성
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    UMossFinalFormAsset* FormAsset = NewObject<UMossFinalFormAsset>();
    TestNotNull(TEXT("UMossFinalFormAsset 인스턴스 생성 성공"), FormAsset);
    if (!FormAsset) { return false; }

    // FinalForm 자산 속성 설정
    FormAsset->FormId = FName("form_mossy");
    FormAsset->DisplayName = FText::FromString(TEXT("이끼 가득 형태"));
    FormAsset->RequiredTagWeights.Add(FName("tag_gentle"), 0.6f);
    FormAsset->RequiredTagWeights.Add(FName("tag_curious"), 0.4f);

    // TestingAddFormAsset으로 레지스트리에 직접 주입
    Sub->TestingAddFormAsset(FName("form_mossy"), FormAsset);

    // Ready 상태 강제 설정
    Sub->TestingSetState(EDataPipelineState::Ready);
    TestTrue(TEXT("TestingSetState(Ready) 후 IsReady() == true"), Sub->IsReady());

    // Act + Assert: GetGrowthFormRow("form_mossy") → IsSet()
    const TOptional<FMossFinalFormData> FormResult = Sub->GetGrowthFormRow(FName("form_mossy"));
    TestTrue(
        TEXT("GetGrowthFormRow('form_mossy') → IsSet() (등록된 형태)"),
        FormResult.IsSet());

    // Assert: 반환된 view의 FormId 일치 확인
    if (FormResult.IsSet())
    {
        TestEqual(
            TEXT("반환 FMossFinalFormData.FormId == 'form_mossy'"),
            FormResult->FormId,
            FName("form_mossy"));

        // Assert: RequiredTagWeights 포함 확인 (ADR-0010 FromAsset() 변환 검증)
        TestTrue(
            TEXT("반환 view.RequiredTagWeights에 'tag_gentle' 포함"),
            FormResult->RequiredTagWeights.Contains(FName("tag_gentle")));
    }

    // Act + Assert: GetAllGrowthFormIds() → 1개
    const TArray<FName> AllFormIds = Sub->GetAllGrowthFormIds();
    TestEqual(
        TEXT("GetAllGrowthFormIds() → 1개 원소 (1개 등록)"),
        AllFormIds.Num(),
        1);

    // Act + Assert: GetGrowthFormRow("nonexistent") → 빈 TOptional (AC-DP-05 Fail-close)
    const TOptional<FMossFinalFormData> NonExistent = Sub->GetGrowthFormRow(FName("nonexistent_form"));
    TestFalse(
        TEXT("GetGrowthFormRow('nonexistent_form') → 빈 TOptional (AC-DP-05 Fail-close)"),
        NonExistent.IsSet());

    // Act + Assert: GetGrowthFormRow(NAME_None) → 빈 TOptional
    const TOptional<FMossFinalFormData> NoneResult = Sub->GetGrowthFormRow(NAME_None);
    TestFalse(
        TEXT("GetGrowthFormRow(NAME_None) → 빈 TOptional (Fail-close)"),
        NoneResult.IsSet());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 5 — Stillness 자산 주입 후 GetStillnessBudgetAsset 반환 검증
//
// TestingSetStillnessAsset으로 UStillnessBudgetAsset 주입 →
// GetStillnessBudgetAsset() → not null 확인.
// Stillness 미주입 시 nullptr 반환 (consumer가 default 사용) 확인.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineStillnessAssetTest,
    "MossBaby.Data.Pipeline.Catalog.StillnessAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineStillnessAssetTest::RunTest(const FString& Parameters)
{
    // Arrange: 인스턴스 생성
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    // Ready 상태 설정 (자산 미주입 상태)
    Sub->TestingSetState(EDataPipelineState::Ready);

    // Act + Assert: Stillness 미주입 시 nullptr 반환 (consumer가 default 사용)
    UStillnessBudgetAsset* NullResult = Sub->GetStillnessBudgetAsset();
    TestNull(
        TEXT("Stillness 미주입 상태 GetStillnessBudgetAsset() → nullptr (empty OK)"),
        NullResult);

    // Arrange: UStillnessBudgetAsset 인스턴스 생성 + 기본값 설정
    UStillnessBudgetAsset* StillnessAsset = NewObject<UStillnessBudgetAsset>();
    TestNotNull(TEXT("UStillnessBudgetAsset 인스턴스 생성 성공"), StillnessAsset);
    if (!StillnessAsset) { return false; }

    // CDO 기본값 확인 (ADR-0002 §UStillnessBudgetAsset 기본값)
    TestEqual(TEXT("MotionSlots 기본값 == 2"), StillnessAsset->MotionSlots, 2);
    TestEqual(TEXT("ParticleSlots 기본값 == 2"), StillnessAsset->ParticleSlots, 2);
    TestEqual(TEXT("SoundSlots 기본값 == 3"), StillnessAsset->SoundSlots, 3);
    TestFalse(TEXT("bReducedMotionEnabled 기본값 == false"), StillnessAsset->bReducedMotionEnabled);

    // TestingSetStillnessAsset 주입
    Sub->TestingSetStillnessAsset(StillnessAsset);

    // Act + Assert: 주입 후 GetStillnessBudgetAsset() → not null
    UStillnessBudgetAsset* Result = Sub->GetStillnessBudgetAsset();
    TestNotNull(
        TEXT("TestingSetStillnessAsset 후 GetStillnessBudgetAsset() → not null"),
        Result);

    // Assert: 반환 자산의 슬롯 값 일치 확인
    if (Result)
    {
        TestEqual(
            TEXT("반환 자산 MotionSlots == 2 (CDO 기본값)"),
            Result->MotionSlots,
            2);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 6 — DegradedFallback 상태 직접 진입 + pull API 동작 검증
//
// AC-DP-03: TestingSetState(DegradedFallback) → IsReady() == false.
// DegradedFallback 상태에서 pull API(checkf 포함)는 assert 발동 → 직접 호출 불가.
// GetFailedCatalogs() 빈 배열 확인 (TestingSetState만 호출 — FailedCatalogs 미설정).
//
// NOTE: DegradedFallback 상태에서 pull API 호출 시 checkf(IsReady()) 발동으로
//       process abort 발생 (Debug/Development 빌드). 자동화 테스트에서 직접
//       발동 불가. 상태 확인 + IsReady() == false 검증으로 AC-DP-03 부분 커버.
//       EnterDegradedFallback 동작의 완전한 검증은 Initialize() integration test에서 수행.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineDegradedFallbackStateTest,
    "MossBaby.Data.Pipeline.Catalog.DegradedFallbackState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineDegradedFallbackStateTest::RunTest(const FString& Parameters)
{
    // Arrange: 인스턴스 생성
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    // Act: TestingSetState(DegradedFallback) — 상태 직접 설정
    Sub->TestingSetState(EDataPipelineState::DegradedFallback);

    // Assert: IsReady() == false (AC-DP-03: DegradedFallback 시 IsReady false)
    TestFalse(
        TEXT("DegradedFallback 상태에서 IsReady() == false (AC-DP-03)"),
        Sub->IsReady());

    // Assert: GetState() == DegradedFallback
    TestEqual(
        TEXT("GetState() == DegradedFallback"),
        Sub->GetState(),
        EDataPipelineState::DegradedFallback);

    // Assert: GetFailedCatalogs() — TestingSetState만 호출했으므로 빈 배열
    // (실제 EnterDegradedFallback 호출 시에는 값이 추가됨 — Integration test에서 검증)
    const TArray<FName> FailedCatalogs = Sub->GetFailedCatalogs();
    TestEqual(
        TEXT("TestingSetState만으로는 GetFailedCatalogs() 빈 TArray (FailedCatalogs 미설정)"),
        FailedCatalogs.Num(),
        0);

    // NOTE: DegradedFallback 상태에서 pull API 호출 시 checkf(IsReady()) assertion 발동.
    //       GetCardRow / GetDreamAsset / GetGrowthFormRow 등은 호출 불가 (process abort).
    //       아래 pull API 시나리오는 DegradedFallback에서 직접 테스트 불가:
    //         - GetAllDreamAssets() → 빈 TArray (AC-DP-05 DegradedFallback 시)
    //         - GetGrowthFormRow("x") → 빈 TOptional (AC-DP-05 DegradedFallback 시)
    //       위 시나리오는 Initialize() integration test에서 EnterDegradedFallback 호출 후
    //       상태 전환 전에 레지스트리 상태를 검증하는 방식으로 대체 가능.

    // Assert: DegradedFallback에서도 OnLoadComplete Broadcast 가능 확인
    // (EnterDegradedFallback은 Broadcast를 포함 — 여기서는 직접 Broadcast 검증)
    bool bDelegateCalled = false;
    Sub->OnLoadComplete.AddLambda(
        [&bDelegateCalled](bool /*bFreshStart*/, bool /*bHadPreviousData*/)
        {
            bDelegateCalled = true;
        });
    Sub->OnLoadComplete.Broadcast(false, false);
    TestTrue(
        TEXT("DegradedFallback 상태에서도 OnLoadComplete Broadcast 가능"),
        bDelegateCalled);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
