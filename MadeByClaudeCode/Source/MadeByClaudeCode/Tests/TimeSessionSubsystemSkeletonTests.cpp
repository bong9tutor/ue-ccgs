// Copyright Moss Baby
//
// TimeSessionSubsystemSkeletonTests.cpp — UMossTimeSessionSubsystem 뼈대 자동화 테스트
//
// Story-003: UMossTimeSessionSubsystem 뼈대 + enum + Settings
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/time-session-system.md §Acceptance Criteria (Story 003)
//
// AC 커버리지:
//   AC-1 Subsystem 뼈대 → FTimeSessionSubsystemDelegateBindTest (delegate 바인딩 + Broadcast)
//   AC-2~4 enum 정의   → CODE_REVIEW (헤더 컴파일 — 이 파일이 컴파일되면 enum 존재 증명)
//   AC-5 Settings 7 knobs → FTimeSessionSettingsCDODefaultsTest
//   AC-6 3 Delegates  → FTimeSessionSubsystemDelegateBindTest
//
// Initialize/Deinitialize 직접 호출 테스트 분류 (주의):
//   UGameInstanceSubsystem::Initialize는 FSubsystemCollectionBase&를 인자로 받으며,
//   FSubsystemCollectionBase 직접 생성이 불가능하다 (UE 내부 구조 귀속).
//   따라서 Initialize/Deinitialize 실제 생명주기 테스트는 integration test 또는
//   Game Instance가 살아있는 functional test에서 수행해야 한다.
//   본 파일에서는 NewObject<> 직접 생성 후 Initialize 미호출 경로만 테스트한다.
//
// Run headless:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Time.Subsystem.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Time/MossTimeSessionSubsystem.h"
#include "Time/MossClockSource.h"
#include "Time/SessionRecord.h"
#include "Time/MossTimeSessionTypes.h"
#include "Settings/MossTimeSessionSettings.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 1 — AC-1 / AC-6: Delegate 바인딩 + Broadcast
//
// GDD AC-1: Subsystem 인스턴스 생성 후 delegates 접근 가능.
// GDD AC-6: 3 delegates(OnTimeActionResolved, OnDayAdvanced, OnFarewellReached) 존재.
//
// NewObject<UMossTimeSessionSubsystem>() 직접 생성 (Initialize 미호출).
// Initialize/Deinitialize integration은 integration test 또는 playtest로 분리.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeSessionSubsystemDelegateBindTest,
    "MossBaby.Time.Subsystem.DelegateBind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTimeSessionSubsystemDelegateBindTest::RunTest(const FString& Parameters)
{
    // Arrange: Subsystem 인스턴스 생성 (Initialize 미호출 — integration 분류)
    UMossTimeSessionSubsystem* Sub = NewObject<UMossTimeSessionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // ── OnDayAdvanced 바인딩 + Broadcast ───────────────────────────────────
    // Act
    bool bDayAdvancedCalled = false;
    int32 ReceivedDay = -1;
    Sub->OnDayAdvanced.AddLambda([&bDayAdvancedCalled, &ReceivedDay](int32 NewDay)
    {
        bDayAdvancedCalled = true;
        ReceivedDay = NewDay;
    });
    Sub->OnDayAdvanced.Broadcast(7);

    // Assert
    TestTrue(TEXT("OnDayAdvanced 람다 호출됨"), bDayAdvancedCalled);
    TestEqual(TEXT("OnDayAdvanced Broadcast 값 수신"), ReceivedDay, 7);

    // ── OnTimeActionResolved 바인딩 + Broadcast ────────────────────────────
    bool bActionCalled = false;
    ETimeAction ReceivedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&bActionCalled, &ReceivedAction](ETimeAction Action)
    {
        bActionCalled = true;
        ReceivedAction = Action;
    });
    Sub->OnTimeActionResolved.Broadcast(ETimeAction::START_DAY_ONE);

    TestTrue(TEXT("OnTimeActionResolved 람다 호출됨"), bActionCalled);
    TestEqual(TEXT("OnTimeActionResolved Broadcast 값 수신"),
              ReceivedAction, ETimeAction::START_DAY_ONE);

    // ── OnFarewellReached 바인딩 + Broadcast ──────────────────────────────
    bool bFarewellCalled = false;
    EFarewellReason ReceivedReason = EFarewellReason::NormalCompletion;
    Sub->OnFarewellReached.AddLambda([&bFarewellCalled, &ReceivedReason](EFarewellReason Reason)
    {
        bFarewellCalled = true;
        ReceivedReason = Reason;
    });
    Sub->OnFarewellReached.Broadcast(EFarewellReason::LongGapAutoFarewell);

    TestTrue(TEXT("OnFarewellReached 람다 호출됨"), bFarewellCalled);
    TestEqual(TEXT("OnFarewellReached Broadcast 값 수신"),
              ReceivedReason, EFarewellReason::LongGapAutoFarewell);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 2 — AC-1: Clock Source 주입 + ClassifyOnStartup(nullptr) 스텁 검증
//
// SetClockSource로 FMockClockSource 주입 후 ClassifyOnStartup(nullptr) 호출.
// Story 003 스텁: nullptr → START_DAY_ONE 반환.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeSessionSubsystemClockSourceInjectionTest,
    "MossBaby.Time.Subsystem.ClockSourceInjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTimeSessionSubsystemClockSourceInjectionTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossTimeSessionSubsystem* Sub = NewObject<UMossTimeSessionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // Act: FMockClockSource 주입
    TSharedPtr<FMockClockSource> MockClock = MakeShared<FMockClockSource>();
    MockClock->SetUtcNow(FDateTime(2026, 4, 19, 12, 0, 0));
    MockClock->SetMonotonicSec(100.0);
    Sub->SetClockSource(MockClock);

    // Act: ClassifyOnStartup(nullptr) — 최초 실행 경로
    const ETimeAction Action = Sub->ClassifyOnStartup(nullptr);

    // Assert: Story 003 스텁은 nullptr → START_DAY_ONE
    TestEqual(
        TEXT("ClassifyOnStartup(nullptr) → START_DAY_ONE (Story 003 스텁)"),
        Action,
        ETimeAction::START_DAY_ONE);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 3 — AC-5: UMossTimeSessionSettings CDO 기본값 7 knobs 전체 검증
//
// GetDefault<UMossTimeSessionSettings>() == UMossTimeSessionSettings::Get()
// CDO 기본값: GDD §Tuning Knobs 표와 1:1 대응.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeSessionSettingsCDODefaultsTest,
    "MossBaby.Time.Subsystem.SettingsCDODefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTimeSessionSettingsCDODefaultsTest::RunTest(const FString& Parameters)
{
    // Arrange: CDO accessor
    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    TestNotNull(TEXT("UMossTimeSessionSettings CDO 포인터 유효"), Settings);
    if (!Settings)
    {
        return false;
    }

    // Assert: 7 knobs 기본값 전체 검증 (GDD §Tuning Knobs 표 기준)

    // Day Structure
    TestEqual(TEXT("GameDurationDays 기본값 == 21"),
              Settings->GameDurationDays, 21);

    // Classification Thresholds
    TestEqual(TEXT("DefaultEpsilonSec 기본값 == 5.0f"),
              Settings->DefaultEpsilonSec, 5.0f);
    TestEqual(TEXT("InSessionToleranceMinutes 기본값 == 90"),
              Settings->InSessionToleranceMinutes, 90);

    // Narrative Cap
    TestEqual(TEXT("NarrativeThresholdHours 기본값 == 24"),
              Settings->NarrativeThresholdHours, 24);
    TestEqual(TEXT("NarrativeCapPerPlaythrough 기본값 == 3"),
              Settings->NarrativeCapPerPlaythrough, 3);

    // Suspend Detection
    TestEqual(TEXT("SuspendMonotonicThresholdSec 기본값 == 5.0f"),
              Settings->SuspendMonotonicThresholdSec, 5.0f);
    TestEqual(TEXT("SuspendWallThresholdSec 기본값 == 60.0f"),
              Settings->SuspendWallThresholdSec, 60.0f);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 4 — AC-1: ClassifyOnStartup 스텁 동작 2가지 경로 검증
//
// nullptr → START_DAY_ONE
// 유효 FSessionRecord → HOLD_LAST_TIME (Story 004 Classifier 구현 전 안전 스텁)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeSessionSubsystemStubClassifyTest,
    "MossBaby.Time.Subsystem.StubClassify",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTimeSessionSubsystemStubClassifyTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossTimeSessionSubsystem* Sub = NewObject<UMossTimeSessionSubsystem>();
    TestNotNull(TEXT("Subsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // ── nullptr 경로: START_DAY_ONE ────────────────────────────────────────
    // Act
    const ETimeAction ActionNull = Sub->ClassifyOnStartup(nullptr);

    // Assert
    TestEqual(
        TEXT("ClassifyOnStartup(nullptr) → START_DAY_ONE"),
        ActionNull,
        ETimeAction::START_DAY_ONE);

    // ── 유효 레코드 경로: HOLD_LAST_TIME ──────────────────────────────────
    // Arrange: 임의 유효 레코드
    FSessionRecord ValidRecord;
    ValidRecord.SessionUuid = FGuid::NewGuid();
    ValidRecord.DayIndex = 3;
    ValidRecord.LastWallUtc = FDateTime(2026, 4, 10, 9, 0, 0);
    ValidRecord.LastMonotonicSec = 3600.0;
    ValidRecord.NarrativeCount = 1;

    // Act
    const ETimeAction ActionValid = Sub->ClassifyOnStartup(&ValidRecord);

    // Assert: Story 003 스텁은 유효 레코드 → HOLD_LAST_TIME
    // Story 004에서 실제 Rule 2~4 분류로 교체 예정.
    TestEqual(
        TEXT("ClassifyOnStartup(유효 레코드) → HOLD_LAST_TIME (Story 003 스텁)"),
        ActionValid,
        ETimeAction::HOLD_LAST_TIME);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
