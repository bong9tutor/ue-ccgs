// Copyright Moss Baby
//
// InputModeDetectionTests.cpp — Input Mode Auto-Detect + Hysteresis 자동화 테스트
//
// Story-1-13: Input Mode Auto-Detection + Hysteresis (Formula 1) + OnInputModeChanged delegate
// GDD: design/gdd/input-abstraction-layer.md §Formula 1 / §Rule 3
//
// AC 커버리지:
//   INPUT_MODE_AUTO_DETECT  → FInputModeAutoDetectMouseToGamepadTest
//   INPUT_MODE_HYSTERESIS   → FInputModeHysteresisBelowThresholdTest
//   Boundary strict `>` (3.0 정확 — 전환 미발생) → FInputModeBoundaryExactThresholdTest
//   Boundary 3.1 초과 (전환 발생) → FInputModeBoundaryAboveThresholdTest
//   Same mode no double broadcast → FInputModeSameModeNoDelegateTest
//   Multi-subscriber → FInputModeDelegateSubscriptionCountTest
//
// Formula 1: ShouldSwitchToMouse = (MouseDelta > InputModeMouseThreshold) AND (CurrentMode == Gamepad)
//   strict `>` — 3.0px 정확히 이동 시 전환 미발생, 3.001px 초과 시 전환 발생.
//
// APlayerController SetShowMouseCursor:
//   단위 테스트 환경(NewObject 직접 생성, GetGameInstance() == null)에서 PC == nullptr.
//   TransitionToMode 내 null guard 처리 → crash 없음.
//   cursor 가시성 실기 검증은 integration test 분리 (PC 실제 존재 환경).
//
// Pillar 1 준수: UI notification / modal 관련 API 호출 0건.
//
// Run headless (-nullrhi):
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Input.ModeDetection.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Input/MossInputAbstractionSubsystem.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// 공통 헬퍼: Subsystem 생성 + 마우스 위치 리셋
//
// 각 테스트는 TestingResetMousePosition()을 통해 bMousePositionInitialized = false로
// 리셋하여 테스트 간 독립성을 보장한다.
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 1 — AC: INPUT_MODE_AUTO_DETECT
//
// GDD AC INPUT_MODE_AUTO_DETECT:
//   Mouse 모드 활성 + OnGamepadInputReceived() → OnInputModeChanged(Gamepad) 1회 + CurrentMode==Gamepad
//
// 검증:
//   - OnGamepadInputReceived() 호출 후 CurrentMode == Gamepad
//   - delegate 람다가 정확히 1회 호출됨
//   - 람다 인자 NewMode == Gamepad
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputModeAutoDetectMouseToGamepadTest,
    "MossBaby.Input.ModeDetection.AutoDetectMouseToGamepad",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputModeAutoDetectMouseToGamepadTest::RunTest(const FString& Parameters)
{
    // Arrange: Subsystem 생성 + 초기화 리셋
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetMousePosition();
    Sub->TestingSetCurrentMode(EInputMode::Mouse);

    // Arrange: delegate 구독 (호출 횟수 + 마지막 인자 추적)
    int32 CallCount = 0;
    EInputMode ReceivedMode = EInputMode::Mouse; // 초기값 — Gamepad로 바뀌어야 함
    Sub->OnInputModeChanged.AddLambda([&CallCount, &ReceivedMode](EInputMode NewMode)
    {
        ++CallCount;
        ReceivedMode = NewMode;
    });

    // Act: 게임패드 입력 수신
    Sub->OnGamepadInputReceived();

    // Assert: 모드 전환 완료
    TestEqual(
        TEXT("OnGamepadInputReceived() 후 CurrentMode == Gamepad"),
        Sub->GetCurrentInputMode(),
        EInputMode::Gamepad);

    // Assert: delegate 1회 발행
    TestEqual(
        TEXT("OnInputModeChanged delegate 1회 발행"),
        CallCount,
        1);

    // Assert: delegate 인자 == Gamepad
    TestEqual(
        TEXT("delegate 인자 == EInputMode::Gamepad"),
        ReceivedMode,
        EInputMode::Gamepad);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 2 — AC: INPUT_MODE_HYSTERESIS (2px < threshold 3.0)
//
// GDD AC INPUT_MODE_HYSTERESIS:
//   Gamepad 모드 + InputModeMouseThreshold=3.0 + delta=2.0px → 전환 미발생
//
// 검증:
//   - CurrentMode == Gamepad 유지
//   - delegate 미발행 (CallCount == 0)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputModeHysteresisBelowThresholdTest,
    "MossBaby.Input.ModeDetection.HysteresisBelowThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputModeHysteresisBelowThresholdTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetMousePosition();
    Sub->TestingSetCurrentMode(EInputMode::Gamepad);

    int32 CallCount = 0;
    Sub->OnInputModeChanged.AddLambda([&CallCount](EInputMode) { ++CallCount; });

    // Act: 최초 호출(기준 위치 확립) + 2px 이동 (threshold 3.0 미만)
    Sub->OnMouseMoved(FVector2D(0.0f, 0.0f));  // 초기화 — 전환 없음
    Sub->OnMouseMoved(FVector2D(2.0f, 0.0f));  // delta = 2.0 < 3.0 → 전환 미발생

    // Assert: 모드 유지
    TestEqual(
        TEXT("delta 2.0px — CurrentMode == Gamepad 유지 (strict >, threshold 3.0 미만)"),
        Sub->GetCurrentInputMode(),
        EInputMode::Gamepad);

    // Assert: delegate 미발행
    TestEqual(
        TEXT("delta 2.0px — OnInputModeChanged 미발행 (CallCount == 0)"),
        CallCount,
        0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 3 — Boundary strict `>`: delta == 3.0 정확 → 전환 미발생
//
// Formula 1 strict `>`: 3.0 == threshold → 전환 미발생.
// ('>=' 이었다면 전환 발생 — strict `>` 구현 검증의 핵심 경계값 테스트)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputModeBoundaryExactThresholdTest,
    "MossBaby.Input.ModeDetection.BoundaryExactThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputModeBoundaryExactThresholdTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetMousePosition();
    Sub->TestingSetCurrentMode(EInputMode::Gamepad);

    int32 CallCount = 0;
    Sub->OnInputModeChanged.AddLambda([&CallCount](EInputMode) { ++CallCount; });

    // Act: (0,0) 기준 → (3,0) 이동 = delta 정확히 3.0px
    Sub->OnMouseMoved(FVector2D(0.0f, 0.0f));
    Sub->OnMouseMoved(FVector2D(3.0f, 0.0f));

    // Assert: strict `>` — 3.0 정확 시 전환 미발생
    TestEqual(
        TEXT("delta == 3.0px (경계값 정확) — CurrentMode == Gamepad 유지 (strict `>`)"),
        Sub->GetCurrentInputMode(),
        EInputMode::Gamepad);

    TestEqual(
        TEXT("delta == 3.0px — delegate 미발행 (CallCount == 0)"),
        CallCount,
        0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 4 — Boundary: delta > 3.0 (3.1) → 전환 발생
//
// delta = 3.1 > threshold 3.0 → strict `>` 충족 → Mouse 모드 전환.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputModeBoundaryAboveThresholdTest,
    "MossBaby.Input.ModeDetection.BoundaryAboveThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputModeBoundaryAboveThresholdTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetMousePosition();
    Sub->TestingSetCurrentMode(EInputMode::Gamepad);

    int32 CallCount = 0;
    EInputMode ReceivedMode = EInputMode::Gamepad;
    Sub->OnInputModeChanged.AddLambda([&CallCount, &ReceivedMode](EInputMode NewMode)
    {
        ++CallCount;
        ReceivedMode = NewMode;
    });

    // Act: (0,0) 기준 → (3.1,0) 이동 = delta 3.1 > 3.0
    Sub->OnMouseMoved(FVector2D(0.0f, 0.0f));
    Sub->OnMouseMoved(FVector2D(3.1f, 0.0f));

    // Assert: 전환 발생
    TestEqual(
        TEXT("delta 3.1 > threshold 3.0 — CurrentMode == Mouse (전환 발생)"),
        Sub->GetCurrentInputMode(),
        EInputMode::Mouse);

    TestEqual(
        TEXT("delta 3.1 — OnInputModeChanged 1회 발행"),
        CallCount,
        1);

    TestEqual(
        TEXT("delta 3.1 — delegate 인자 == EInputMode::Mouse"),
        ReceivedMode,
        EInputMode::Mouse);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 5 — Same Mode: 동일 모드 재진입 시 delegate 추가 발행 없음
//
// GDD TransitionToMode: `if (NewMode == CurrentMode) return` — delegate 미발행.
//
// 검증 흐름:
//   1. Mouse → OnGamepadInputReceived() → Gamepad 전환 (delegate 1회)
//   2. Gamepad → OnGamepadInputReceived() 재호출 → 동일 모드, no-op (delegate 추가 없음)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputModeSameModeNoDelegateTest,
    "MossBaby.Input.ModeDetection.SameModeNoDelegate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputModeSameModeNoDelegateTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetMousePosition();
    Sub->TestingSetCurrentMode(EInputMode::Mouse);

    int32 CallCount = 0;
    Sub->OnInputModeChanged.AddLambda([&CallCount](EInputMode) { ++CallCount; });

    // Act 1: 첫 번째 OnGamepadInputReceived → Mouse → Gamepad 전환
    Sub->OnGamepadInputReceived();

    TestEqual(
        TEXT("첫 번째 OnGamepadInputReceived() — CurrentMode == Gamepad"),
        Sub->GetCurrentInputMode(),
        EInputMode::Gamepad);

    TestEqual(
        TEXT("첫 번째 전환 — delegate 1회 발행"),
        CallCount,
        1);

    // Act 2: 두 번째 OnGamepadInputReceived → 이미 Gamepad — no-op
    Sub->OnGamepadInputReceived();

    // Assert: 모드 변화 없음 + delegate 추가 발행 없음
    TestEqual(
        TEXT("두 번째 OnGamepadInputReceived() — CurrentMode 여전히 Gamepad"),
        Sub->GetCurrentInputMode(),
        EInputMode::Gamepad);

    TestEqual(
        TEXT("두 번째 호출 — delegate 추가 발행 없음 (CallCount 여전히 1)"),
        CallCount,
        1);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 6 — Multi-Subscriber: 여러 람다 구독 시 모두 호출됨
//
// FOnInputModeChanged는 Multicast — Broadcast 시 등록된 모든 구독자가 호출되어야 함.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputModeDelegateSubscriptionCountTest,
    "MossBaby.Input.ModeDetection.DelegateSubscriptionCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputModeDelegateSubscriptionCountTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetMousePosition();
    Sub->TestingSetCurrentMode(EInputMode::Mouse);

    // Arrange: 3개 독립 구독자 등록
    int32 SubscriberA = 0;
    int32 SubscriberB = 0;
    int32 SubscriberC = 0;

    Sub->OnInputModeChanged.AddLambda([&SubscriberA](EInputMode) { ++SubscriberA; });
    Sub->OnInputModeChanged.AddLambda([&SubscriberB](EInputMode) { ++SubscriberB; });
    Sub->OnInputModeChanged.AddLambda([&SubscriberC](EInputMode) { ++SubscriberC; });

    // Act: 모드 전환 1회
    Sub->OnGamepadInputReceived();

    // Assert: 3개 구독자 모두 1회씩 호출됨
    TestEqual(TEXT("구독자 A — 1회 호출"), SubscriberA, 1);
    TestEqual(TEXT("구독자 B — 1회 호출"), SubscriberB, 1);
    TestEqual(TEXT("구독자 C — 1회 호출"), SubscriberC, 1);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
