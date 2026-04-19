// Copyright Moss Baby
//
// InputAbstractionSubsystemTests.cpp — UMossInputAbstractionSubsystem 자동화 테스트
//
// Story-1-12: UMossInputAbstractionSubsystem + UMossInputAbstractionSettings + IMC 등록
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/input-abstraction-layer.md §Acceptance Criteria
//
// AC 커버리지:
//   AC-2 Subsystem 뼈대 → FInputSubsystemInitialStateTest
//   AC-1 Settings CDO  → FInputSubsystemSettingsCDOTest
//   AC-5 6 Action null → FInputSubsystemNullAssetsAllowedTest (Story-1-11 에셋 미생성 상태)
//   AC-5 Action 주입   → FInputSubsystemMockActionInjectionTest
//   AC-4 EInputMode    → FInputSubsystemModeEnumTest
//
// Initialize/Deinitialize 직접 호출 테스트 분류 (주의):
//   UGameInstanceSubsystem::Initialize는 FSubsystemCollectionBase& 인자 필요.
//   FSubsystemCollectionBase 직접 생성 불가 (UE 내부 구조 귀속).
//   따라서 Initialize/Deinitialize 생명주기 테스트는 integration test 또는
//   Game Instance가 살아있는 functional test에서 수행.
//   본 파일: NewObject<> 직접 생성 후 Initialize 미호출 경로만 테스트.
//
// Run headless (-nullrhi):
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Input.Subsystem.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Input/MossInputAbstractionSubsystem.h"
#include "Settings/MossInputAbstractionSettings.h"
#include "InputAction.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 1 — AC-2: Subsystem 초기 상태 검증
//
// GDD AC-2: NewObject 직후 (Initialize 미호출) 초기 상태 검증.
//   - CurrentMode == EInputMode::Mouse (기본값)
//   - bMappingContextRegistered == false (등록 전 상태)
//
// Story-1-12 뼈대 단계 — Initialize 없이 생성만으로 안전한 상태여야 함.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputSubsystemInitialStateTest,
    "MossBaby.Input.Subsystem.InitialState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputSubsystemInitialStateTest::RunTest(const FString& Parameters)
{
    // Arrange: Subsystem 인스턴스 직접 생성 (Initialize 미호출)
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // Assert: 초기 입력 모드 == Mouse
    TestEqual(
        TEXT("초기 CurrentMode == EInputMode::Mouse"),
        Sub->GetCurrentInputMode(),
        EInputMode::Mouse);

    // Assert: IMC 등록 플래그 == false (Initialize 미호출)
    TestFalse(
        TEXT("초기 bMappingContextRegistered == false (Initialize 미호출)"),
        Sub->IsMappingContextRegistered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 2 — AC-1: UMossInputAbstractionSettings CDO 기본값 3 knobs 검증
//
// GDD AC-1 (Story-1-12 §Settings):
//   - InputModeMouseThreshold == 3.0f (픽셀)
//   - OfferDragThresholdSec == 0.15f (초) — TestNearlyEqual 사용 (float)
//   - OfferHoldDurationSec == 0.5f (초) — TestNearlyEqual 사용 (float)
//
// GetDefault<UMossInputAbstractionSettings>() == UMossInputAbstractionSettings::Get().
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputSubsystemSettingsCDOTest,
    "MossBaby.Input.Subsystem.SettingsCDODefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputSubsystemSettingsCDOTest::RunTest(const FString& Parameters)
{
    // Arrange: CDO accessor
    const UMossInputAbstractionSettings* Settings = UMossInputAbstractionSettings::Get();
    TestNotNull(TEXT("UMossInputAbstractionSettings CDO 포인터 유효"), Settings);
    if (!Settings)
    {
        return false;
    }

    // Assert: InputModeMouseThreshold 기본값 == 3.0f
    // TestEqual은 float에서 bit-exact 비교. 3.0f는 안전한 float 리터럴.
    TestEqual(
        TEXT("InputModeMouseThreshold 기본값 == 3.0f"),
        Settings->InputModeMouseThreshold,
        3.0f);

    // Assert: OfferDragThresholdSec 기본값 == 0.15f
    // TestNearlyEqual: KINDA_SMALL_NUMBER (1e-4) 허용오차
    TestNearlyEqual(
        TEXT("OfferDragThresholdSec 기본값 ≈ 0.15f"),
        Settings->OfferDragThresholdSec,
        0.15f,
        KINDA_SMALL_NUMBER);

    // Assert: OfferHoldDurationSec 기본값 == 0.5f
    TestNearlyEqual(
        TEXT("OfferHoldDurationSec 기본값 ≈ 0.5f"),
        Settings->OfferHoldDurationSec,
        0.5f,
        KINDA_SMALL_NUMBER);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 3 — AC-5: Story-1-11 에셋 미생성 상태 — 모든 포인터 null 허용
//
// GDD §Story-1-11 에셋 미생성 정책:
//   NewObject 직후 (Initialize 미호출):
//     모든 IA_* + IMC_* 포인터 == nullptr.
//     TestingCountLoadedActions() == 0.
//
// 이 테스트는 Story-1-11 에셋이 없는 환경(CI headless 포함)에서도 통과해야 함.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputSubsystemNullAssetsAllowedTest,
    "MossBaby.Input.Subsystem.NullAssetsAllowed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputSubsystemNullAssetsAllowedTest::RunTest(const FString& Parameters)
{
    // Arrange: Initialize 미호출 — 에셋 로드 없는 상태
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // Assert: 6 Action 포인터 모두 nullptr (Initialize 미호출)
    TestNull(TEXT("IA_PointerMove == nullptr (Initialize 미호출)"), Sub->IA_PointerMove.Get());
    TestNull(TEXT("IA_Select == nullptr"),                          Sub->IA_Select.Get());
    TestNull(TEXT("IA_OfferCard == nullptr"),                       Sub->IA_OfferCard.Get());
    TestNull(TEXT("IA_NavigateUI == nullptr"),                      Sub->IA_NavigateUI.Get());
    TestNull(TEXT("IA_OpenJournal == nullptr"),                     Sub->IA_OpenJournal.Get());
    TestNull(TEXT("IA_Back == nullptr"),                            Sub->IA_Back.Get());

    // Assert: 2 IMC 포인터 모두 nullptr
    TestNull(TEXT("IMC_MouseKeyboard == nullptr"),                  Sub->IMC_MouseKeyboard.Get());
    TestNull(TEXT("IMC_Gamepad == nullptr"),                        Sub->IMC_Gamepad.Get());

    // Assert: TestingCountLoadedActions == 0
    TestEqual(
        TEXT("TestingCountLoadedActions() == 0 (에셋 null 상태)"),
        Sub->TestingCountLoadedActions(),
        0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 4 — AC-5: Mock Action 주입 테스트
//
// TestingInjectAction(FName, UInputAction*) 동작 검증:
//   1. NewObject<UInputAction>() Mock 생성
//   2. TestingInjectAction(FName("IA_Select"), MockAction) 호출
//   3. IA_Select != nullptr 확인
//   4. TestingCountLoadedActions() == 1 (나머지 5개는 null)
//
// Story-1-11 에셋 없는 환경에서 테스트 가능한 injection seam 검증.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputSubsystemMockActionInjectionTest,
    "MossBaby.Input.Subsystem.MockActionInjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputSubsystemMockActionInjectionTest::RunTest(const FString& Parameters)
{
    // Arrange: Subsystem 생성 + Mock Action 생성
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // Mock UInputAction: GC가 회수하지 않도록 Outer를 Sub로 설정
    UInputAction* MockAction = NewObject<UInputAction>(Sub);
    TestNotNull(TEXT("Mock UInputAction 생성 성공"), MockAction);
    if (!MockAction)
    {
        return false;
    }

    // Act: IA_Select 주입
    Sub->TestingInjectAction(FName(TEXT("IA_Select")), MockAction);

    // Assert: IA_Select 포인터가 MockAction과 동일
    TestNotNull(TEXT("IA_Select != nullptr after injection"), Sub->IA_Select.Get());
    TestEqual(
        TEXT("IA_Select == MockAction"),
        Sub->IA_Select.Get(),
        MockAction);

    // Assert: TestingCountLoadedActions == 1 (IA_Select만 주입됨, 나머지 5개 null)
    TestEqual(
        TEXT("TestingCountLoadedActions() == 1 (IA_Select 주입 후)"),
        Sub->TestingCountLoadedActions(),
        1);

    // Assert: 나머지 5개 Action은 여전히 null
    TestNull(TEXT("IA_PointerMove 여전히 nullptr"), Sub->IA_PointerMove.Get());
    TestNull(TEXT("IA_OfferCard 여전히 nullptr"),   Sub->IA_OfferCard.Get());
    TestNull(TEXT("IA_NavigateUI 여전히 nullptr"),  Sub->IA_NavigateUI.Get());
    TestNull(TEXT("IA_OpenJournal 여전히 nullptr"), Sub->IA_OpenJournal.Get());
    TestNull(TEXT("IA_Back 여전히 nullptr"),        Sub->IA_Back.Get());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 5 — AC-4: EInputMode enum 정수 안정성 검증
//
// EInputMode::Mouse == 0, EInputMode::Gamepad == 1.
// uint8 기반 — stable integer 보장.
//
// 이 테스트는 UENUM 순서 변경 없음을 확인하는 회귀 방지 테스트.
// Story-1-13에서 auto-detect 구현 시 enum 값이 바뀌면 이 테스트가 즉시 감지.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputSubsystemModeEnumTest,
    "MossBaby.Input.Subsystem.ModeEnumStability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputSubsystemModeEnumTest::RunTest(const FString& Parameters)
{
    // Assert: EInputMode::Mouse == 0 (uint8 정수값 안정성)
    TestEqual(
        TEXT("EInputMode::Mouse == 0 (uint8)"),
        static_cast<uint8>(EInputMode::Mouse),
        static_cast<uint8>(0));

    // Assert: EInputMode::Gamepad == 1 (uint8 정수값 안정성)
    TestEqual(
        TEXT("EInputMode::Gamepad == 1 (uint8)"),
        static_cast<uint8>(EInputMode::Gamepad),
        static_cast<uint8>(1));

    // Assert: Mouse != Gamepad (두 값이 구별됨)
    TestNotEqual(
        TEXT("EInputMode::Mouse != EInputMode::Gamepad"),
        static_cast<uint8>(EInputMode::Mouse),
        static_cast<uint8>(EInputMode::Gamepad));

    return true;
}

#endif // WITH_AUTOMATION_TESTS
