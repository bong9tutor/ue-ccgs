// Copyright Moss Baby
//
// Story-1-17: Offer Hold Formula 2 + ApplySettingsToMappingContext 테스트
// AC: ApplySettingsToMappingContext null-safe, IMC Hold threshold 반영, multi-trigger count, wrong-action skip
//
// Run headless:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Input.OfferHold.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"
#include "Input/MossInputAbstractionSubsystem.h"
#include "Settings/MossInputAbstractionSettings.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputOfferHoldApplyNoOpOnNullAssetTest,
    "MossBaby.Input.OfferHold.ApplyNoOpOnNullAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputOfferHoldApplyNoOpOnNullAssetTest::RunTest(const FString& Parameters)
{
    // Arrange: Subsystem NewObject (LoadInputAssets 호출 없음 — 모든 에셋 null)
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    if (!TestNotNull(TEXT("Subsystem 생성"), Sub)) { return false; }

    // Act: crash 없이 완료되어야 함 (IA_OfferCard null → early return)
    Sub->ApplySettingsToMappingContext();

    // Assert: 크래시 없이 여기 도달 = PASS
    TestTrue(TEXT("Null 에셋 상태에서 ApplySettingsToMappingContext 완료"), true);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputOfferHoldApplyToMockIMCTest,
    "MossBaby.Input.OfferHold.ApplyToMockIMC",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputOfferHoldApplyToMockIMCTest::RunTest(const FString& Parameters)
{
    // Arrange: Mock IMC + Action + HoldTrigger (초기값 99.0f)
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    UInputMappingContext* MockIMC = NewObject<UInputMappingContext>();
    UInputAction* MockAction = NewObject<UInputAction>();
    UInputTriggerHold* HoldTrigger = NewObject<UInputTriggerHold>();
    HoldTrigger->HoldTimeThreshold = 99.0f;

    FEnhancedActionKeyMapping& Mapping = MockIMC->MapKey(MockAction, EKeys::LeftMouseButton);
    Mapping.Triggers.Add(HoldTrigger);

    // Act: apply 0.15f
    const int32 UpdatedCount = Sub->TestingApplySettingsToSingleIMC(MockIMC, MockAction, 0.15f);

    // Assert
    TestEqual(TEXT("Hold trigger 1개 업데이트"), UpdatedCount, 1);
    TestEqual(TEXT("HoldTimeThreshold == 0.15f"), HoldTrigger->HoldTimeThreshold, 0.15f);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputOfferHoldApplyMultipleHoldTriggersTest,
    "MossBaby.Input.OfferHold.ApplyMultipleHoldTriggers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputOfferHoldApplyMultipleHoldTriggersTest::RunTest(const FString& Parameters)
{
    // Arrange: Action 1개, Mapping 1개에 HoldTrigger 2개 + PressedTrigger 1개 (non-Hold)
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    UInputMappingContext* MockIMC = NewObject<UInputMappingContext>();
    UInputAction* MockAction = NewObject<UInputAction>();

    UInputTriggerHold* Hold1 = NewObject<UInputTriggerHold>();
    UInputTriggerHold* Hold2 = NewObject<UInputTriggerHold>();
    UInputTriggerPressed* PressedTrigger = NewObject<UInputTriggerPressed>();
    Hold1->HoldTimeThreshold = 99.0f;
    Hold2->HoldTimeThreshold = 88.0f;

    FEnhancedActionKeyMapping& Mapping = MockIMC->MapKey(MockAction, EKeys::LeftMouseButton);
    Mapping.Triggers.Add(Hold1);
    Mapping.Triggers.Add(Hold2);
    Mapping.Triggers.Add(PressedTrigger);

    // Act
    const int32 UpdatedCount = Sub->TestingApplySettingsToSingleIMC(MockIMC, MockAction, 0.5f);

    // Assert: Hold 2개만 업데이트, Pressed는 영향 없음
    TestEqual(TEXT("Hold trigger 2개 업데이트"), UpdatedCount, 2);
    TestEqual(TEXT("Hold1 == 0.5f"), Hold1->HoldTimeThreshold, 0.5f);
    TestEqual(TEXT("Hold2 == 0.5f"), Hold2->HoldTimeThreshold, 0.5f);
    // Pressed는 HoldTimeThreshold 멤버 없음 → 검증 불필요 (다른 Trigger type)
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInputOfferHoldApplyIgnoresWrongActionTest,
    "MossBaby.Input.OfferHold.ApplyIgnoresWrongAction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInputOfferHoldApplyIgnoresWrongActionTest::RunTest(const FString& Parameters)
{
    // Arrange: Action A + Action B 각각 Hold trigger. TargetAction=A만 업데이트 기대
    UMossInputAbstractionSubsystem* Sub = NewObject<UMossInputAbstractionSubsystem>();
    UInputMappingContext* MockIMC = NewObject<UInputMappingContext>();
    UInputAction* ActionA = NewObject<UInputAction>();
    UInputAction* ActionB = NewObject<UInputAction>();
    UInputTriggerHold* HoldA = NewObject<UInputTriggerHold>();
    UInputTriggerHold* HoldB = NewObject<UInputTriggerHold>();
    HoldA->HoldTimeThreshold = 99.0f;
    HoldB->HoldTimeThreshold = 77.0f;

    MockIMC->MapKey(ActionA, EKeys::LeftMouseButton).Triggers.Add(HoldA);
    MockIMC->MapKey(ActionB, EKeys::RightMouseButton).Triggers.Add(HoldB);

    // Act: TargetAction=A만
    const int32 UpdatedCount = Sub->TestingApplySettingsToSingleIMC(MockIMC, ActionA, 0.15f);

    // Assert: A만 1개 업데이트, B 불변
    TestEqual(TEXT("Action A만 1개 업데이트"), UpdatedCount, 1);
    TestEqual(TEXT("HoldA == 0.15f"), HoldA->HoldTimeThreshold, 0.15f);
    TestEqual(TEXT("HoldB 불변 77.0f"), HoldB->HoldTimeThreshold, 77.0f);
    return true;
}

#endif // WITH_AUTOMATION_TESTS
