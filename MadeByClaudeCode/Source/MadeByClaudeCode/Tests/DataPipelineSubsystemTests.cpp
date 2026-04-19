// Copyright Moss Baby
//
// DataPipelineSubsystemTests.cpp — UDataPipelineSubsystem 자동화 테스트
//
// Story-1-5: Pipeline Subsystem 뼈대 + 4-state machine + pull API 스텁
// ADR-0003: Sync 일괄 로드 채택
// ADR-0002: 컨테이너 선택
//
// AC 커버리지:
//   AC-DP-01 (skeleton): Initialize에서 Ready 전환 →
//     FDataPipelineInitialStateTest (Uninitialized 초기 상태) +
//     Integration test (실제 Init lifecycle — Out of Scope, Story 1-7 또는 별도 integration)
//   EDataPipelineState enum: 헤더 컴파일 증명 (이 파일 컴파일 = enum 존재)
//   GetState/IsReady: FDataPipelineInitialStateTest
//   OnLoadComplete delegate: FDataPipelineDelegateBindTest
//   AC-DP-13 checkf (Pre-init 조회 차단): 주의 사항 섹션 참조 (아래)
//   AC-DP-04 skeleton (GetCardRow NAME_None): FDataPipelinePullAPIReturnsEmptyOnReadyTest
//   Pull API 8개 선언: 모두 뼈대 스텁, Story 1-6에서 실제 catalog 연동
//
// ─── AC-DP-13 checkf 테스트 전략 (IMPORTANT) ──────────────────────────────────
//
// AC-DP-13: Uninitialized/Loading 상태에서 pull API 호출 시 checkf assertion 발동.
// checkf 실패 시 Debug/Development 빌드에서 process abort 발생.
// UE Automation Framework은 process abort를 catch할 수 없으므로
// checkf 직접 발동 테스트는 자동화로 수행 불가 (의도적 crash 위험).
//
// CI 검증 전략:
//   1. 소스 grep: checkf(IsReady() ... "AC-DP-13" 문자열 존재 확인
//      grep -r "AC-DP-13" MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp
//      → 8개 pull API 함수 모두 checkf 존재 확인 (CODE_REVIEW / BLOCKING)
//   2. Shipping 빌드: check/checkf는 Shipping에서도 유지 (ADR-0003 §AC-DP-13 권고)
//      → UE_BUILD_SHIPPING에서도 checkf 제거 여부를 컴파일러 optimizer가 영향 줄 수 없음
//         (DO_CHECK 매크로는 Shipping에서 DO_GUARD_SLOW=0이지만 checkf는 항상 활성)
//   3. TestingSetState(Ready) 후 NAME_None/없는 ID 조회 → 빈 반환 확인 (이 파일)
//
// Initialize/Deinitialize 직접 호출 테스트 분류 (주의):
//   UGameInstanceSubsystem::Initialize는 FSubsystemCollectionBase& 인자를 받으며,
//   FSubsystemCollectionBase 직접 생성 불가 (UE 내부 구조 귀속).
//   따라서 생명주기 통합 테스트는 integration test 또는 Game Instance가 살아있는
//   functional test에서 수행해야 한다.
//   본 파일에서는 NewObject<> 직접 생성 후 Initialize 미호출 경로(Uninitialized 상태)와
//   TestingSetState()를 통한 상태 강제 설정 경로만 테스트한다.
//
// 헤드리스 실행:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Data.Pipeline.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Data/DataPipelineSubsystem.h"
#include "Data/MossFinalFormData.h"
#include "Data/GiftCardRow.h"
#include "Data/DreamDataAsset.h"
#include "Data/StillnessBudgetAsset.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 1 — EDataPipelineState 초기 상태 검증
//
// AC-DP-01 (skeleton 파트): NewObject 직후 GetState() == Uninitialized.
// GetState/IsReady API 기본 동작 확인.
//
// NOTE: 실제 Initialize() 호출 후 Ready 전환은 Integration test 범위
//       (FSubsystemCollectionBase 직접 생성 불가 — UE 내부 귀속).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineInitialStateTest,
    "MossBaby.Data.Pipeline.InitialState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineInitialStateTest::RunTest(const FString& Parameters)
{
    // Arrange: Subsystem 인스턴스 직접 생성 (Initialize 미호출 — Uninitialized 상태)
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // Act: 초기 상태 조회 (Initialize 미호출)

    // Assert: GetState() == Uninitialized
    TestEqual(
        TEXT("초기 GetState() == Uninitialized"),
        Sub->GetState(),
        EDataPipelineState::Uninitialized);

    // Assert: IsReady() == false (Uninitialized 상태)
    TestFalse(
        TEXT("초기 IsReady() == false"),
        Sub->IsReady());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 2 — Ready 상태에서 pull API Fail-close 빈 반환 검증
//
// AC-DP-04 skeleton: GetCardRow(NAME_None) → 빈 TOptional (default row 반환 금지).
// TestingSetState(Ready) 훅으로 checkf 우회 (WITH_AUTOMATION_TESTS 가드).
//
// 검증 항목:
//   - GetCardRow(NAME_None) → 빈 TOptional
//   - GetAllCardIds() → 빈 TArray (카탈로그 미등록 상태)
//   - GetDreamAsset(NAME_None) → nullptr
//   - GetDreamBodyText(NAME_None) → empty FText
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelinePullAPIReturnsEmptyOnReadyTest,
    "MossBaby.Data.Pipeline.PullAPIReturnsEmptyOnReady",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelinePullAPIReturnsEmptyOnReadyTest::RunTest(const FString& Parameters)
{
    // Arrange: 인스턴스 생성 후 TestingSetState(Ready)로 상태 강제 설정
    // (Initialize() 직접 호출 불가 — FSubsystemCollectionBase 귀속)
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }
    Sub->TestingSetState(EDataPipelineState::Ready);
    TestTrue(TEXT("TestingSetState(Ready) 후 IsReady() == true"), Sub->IsReady());

    // ── GetCardRow(NAME_None) — Fail-close ────────────────────────────────
    // Act
    const TOptional<FGiftCardRow> CardResult = Sub->GetCardRow(NAME_None);

    // Assert: NAME_None → 빈 TOptional (default-constructed row 반환 금지, AC-DP-04)
    TestFalse(
        TEXT("GetCardRow(NAME_None) → 빈 TOptional (AC-DP-04 Fail-close)"),
        CardResult.IsSet());

    // ── GetAllCardIds() — 빈 카탈로그 ────────────────────────────────────
    // Act
    const TArray<FName> AllCardIds = Sub->GetAllCardIds();

    // Assert: 카탈로그 미등록 상태(뼈대) → 빈 TArray
    TestEqual(
        TEXT("GetAllCardIds() — 뼈대 상태 빈 배열"),
        AllCardIds.Num(),
        0);

    // ── GetDreamAsset(NAME_None) — Fail-close ─────────────────────────────
    // Act
    UDreamDataAsset* DreamResult = Sub->GetDreamAsset(NAME_None);

    // Assert: NAME_None → nullptr
    TestNull(
        TEXT("GetDreamAsset(NAME_None) → nullptr (Fail-close)"),
        DreamResult);

    // ── GetDreamBodyText(NAME_None) — Fail-close ──────────────────────────
    // Act
    const FText BodyText = Sub->GetDreamBodyText(NAME_None);

    // Assert: NAME_None → empty FText
    TestTrue(
        TEXT("GetDreamBodyText(NAME_None) → empty FText (Fail-close)"),
        BodyText.IsEmpty());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 3 — OnLoadComplete Delegate 바인딩 + Broadcast 검증
//
// OnLoadComplete.AddLambda 바인딩 후 Broadcast(true, false) →
//   람다 호출 확인 + bFreshStart / bHadPreviousData 인자 전달 확인.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineDelegateBindTest,
    "MossBaby.Data.Pipeline.DelegateBind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineDelegateBindTest::RunTest(const FString& Parameters)
{
    // Arrange: Subsystem 인스턴스 생성
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // Arrange: 람다 바인딩 + 수신 값 추적 변수
    bool bDelegateCalled = false;
    bool bReceivedFreshStart = false;
    bool bReceivedHadPreviousData = true; // 기대값과 반대로 초기화 (검증 정확성)

    Sub->OnLoadComplete.AddLambda(
        [&bDelegateCalled, &bReceivedFreshStart, &bReceivedHadPreviousData]
        (bool bFreshStart, bool bHadPreviousData)
        {
            bDelegateCalled        = true;
            bReceivedFreshStart    = bFreshStart;
            bReceivedHadPreviousData = bHadPreviousData;
        });

    // Act: Broadcast (bFreshStart=true, bHadPreviousData=false)
    Sub->OnLoadComplete.Broadcast(true, false);

    // Assert: 람다 호출 여부
    TestTrue(
        TEXT("OnLoadComplete 람다 호출됨"),
        bDelegateCalled);

    // Assert: bFreshStart 인자 전달 확인
    TestTrue(
        TEXT("OnLoadComplete bFreshStart == true 수신"),
        bReceivedFreshStart);

    // Assert: bHadPreviousData 인자 전달 확인
    TestFalse(
        TEXT("OnLoadComplete bHadPreviousData == false 수신"),
        bReceivedHadPreviousData);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 4 — Deinitialize 후 IsReady() == false 검증
//
// TestingSetState(Ready) → Deinitialize() 직접 호출 → IsReady() == false 확인.
//
// NOTE: GetAllCardIds() 등 pull API는 Deinitialize 후 checkf(IsReady())에 걸리므로
//       해당 API 호출은 불가 (assert 발동). IsReady() == false 단일 assertion만 수행.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDataPipelineDeinitClearsRegistriesTest,
    "MossBaby.Data.Pipeline.DeinitClearsRegistries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDataPipelineDeinitClearsRegistriesTest::RunTest(const FString& Parameters)
{
    // Arrange: 인스턴스 생성 후 Ready 강제 설정
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }
    Sub->TestingSetState(EDataPipelineState::Ready);
    TestTrue(TEXT("Deinitialize 전 IsReady() == true"), Sub->IsReady());

    // Act: Deinitialize() 직접 호출
    // NOTE: UGameInstanceSubsystem::Deinitialize()는 직접 호출 가능 (public override)
    //       실제 GameInstance lifecycle과 다르지만 레지스트리 클리어 로직 검증 목적으로 허용
    Sub->Deinitialize();

    // Assert: Deinitialize 후 IsReady() == false (Uninitialized 복귀)
    TestFalse(
        TEXT("Deinitialize 후 IsReady() == false (Uninitialized 복귀)"),
        Sub->IsReady());

    // Assert: GetState() == Uninitialized
    TestEqual(
        TEXT("Deinitialize 후 GetState() == Uninitialized"),
        Sub->GetState(),
        EDataPipelineState::Uninitialized);

    // NOTE: GetAllCardIds(), GetAllDreamAssets() 등은 IsReady() == false 상태에서
    //       checkf(IsReady()) assertion이 발동되므로 이 시점에서 호출 불가.
    //       레지스트리 클리어는 내부 구현 세부사항 — IsReady() == false로 간접 검증.

    return true;
}

#endif // WITH_AUTOMATION_TESTS
