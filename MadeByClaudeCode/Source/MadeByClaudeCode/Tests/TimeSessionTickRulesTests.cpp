// Copyright Moss Baby
//
// TimeSessionTickRulesTests.cpp — In-session 1Hz Tick Rules 5-8 자동화 테스트
//
// Story 1-18: In-session 1Hz Tick + Rules 5-8 + FTSTicker
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// GDD: design/gdd/time-session-system.md §Core Rules §Rule 5-8 / §Formula 2/3
//
// AC 매핑 (9개 AC 중 7 AUTOMATED, 2 integration deferred):
//
//   MONOTONIC_DELTA_CLAMP_ZERO            → FTimeTickMonoDeltaClampZeroTest
//   NORMAL_TICK_IN_SESSION                → FTimeTickNormalTickInSessionTest
//   MINOR_SHIFT_NTP                       → FTimeTickMinorShiftNtpTest
//   MINOR_SHIFT_DST                       → FTimeTickMinorShiftDstTest
//   IN_SESSION_DISCREPANCY_LOG_ONLY       → FTimeTickInSessionDiscrepancyLogOnlyTest
//   RULE_PRECEDENCE_FIRST_MATCH_WINS      → FTimeTickRulePrecedenceFirstMatchWinsTest
//   TICK_HANDLE_NOT_REGISTERED_BEFORE_INIT → FTimeTickHandleNotRegisteredBeforeInitTest
//   SESSION_COUNT_TODAY_RESET_CONTRACT    → FTimeTickSessionCountTodayResetContractTest
//
//   Deferred (TD-012 — integration 분리 필요):
//   TICK_CONTINUES_WHEN_PAUSED   — SetGamePaused(true) + 실제 GameWorld 필요
//   FOCUS_LOSS_CONTINUES_TICK    — WM_ACTIVATE LOSS 시나리오 + 실제 GameWorld 필요
//
// Formula 2: MonoDelta = max(0, M₁ − M₀) — 부팅 리셋(MonoNow < Last) 시 clamp 0
// Formula 3: DaysElapsed = floor(WallDeltaSec / 86400)
// Rule 6 first-match-wins: MonoDelta < 5s AND WallDelta > 60s → SUSPEND_RESUME (Rule 8 우선 차단)
// ADR-0001 금지 패턴: tamper, cheat, bIsForward, bIsSuspicious 등 일체 미사용.
//
// 카테고리: MossBaby.Time.Tick.*
// 실행 방법 (헤드리스):
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject
//     -ExecCmds="Automation RunTests MossBaby.Time.Tick.; Quit"
//     -nullrhi -nosound -log -unattended

#if WITH_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Time/MossClockSource.h"
#include "Time/MossTimeSessionSubsystem.h"
#include "Time/MossTimeSessionTypes.h"
#include "Time/SessionRecord.h"
#include "Settings/MossTimeSessionSettings.h"

// ─────────────────────────────────────────────────────────────────────────────
// 공통 헬퍼 매크로
// ─────────────────────────────────────────────────────────────────────────────

/**
 * 테스트용 Subsystem + MockClock + 기본 레코드를 생성하는 매크로.
 * Sub, Mock, T0 변수를 선언한다.
 * Initialize()를 직접 호출하지 않으므로 FTSTicker 미등록 상태 — TestingTickInSession 직접 호출.
 */
#define MOSS_TICK_SETUP() \
    UMossTimeSessionSubsystem* Sub = NewObject<UMossTimeSessionSubsystem>(); \
    TSharedPtr<FMockClockSource> Mock = MakeShared<FMockClockSource>(); \
    Sub->SetClockSource(Mock); \
    const FDateTime T0(2026, 1, 1, 12, 0, 0);

/**
 * 기본 레코드를 주입하는 매크로.
 * DayIndex=5, SessionCountToday=3, LastWallUtc=T0, LastMonotonicSec=100.0.
 */
#define MOSS_TICK_INJECT_RECORD(Sub, T0) \
    { \
        FSessionRecord Prev; \
        Prev.DayIndex = 5; \
        Prev.SessionCountToday = 3; \
        Prev.LastWallUtc = (T0); \
        Prev.LastMonotonicSec = 100.0; \
        (Sub)->TestingInjectPrevRecord(Prev); \
    }

// ─────────────────────────────────────────────────────────────────────────────
// AC: MONOTONIC_DELTA_CLAMP_ZERO
//
// Formula 2 clamp: LastMonotonicSec=600.0, MockMonoNow=0.5 (부팅 리셋 시뮬레이션)
// MonoDelta = max(0, 0.5 - 600.0) = 0 → clamp 보장 + 크래시 없음.
// WallDelta=1s, MonoDelta=0 → DiscrepancyAbs=1 ≤ DefaultEpsilonSec(5) → Rule 5 ADVANCE_SILENT.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeTickMonoDeltaClampZeroTest,
    "MossBaby.Time.Tick.MonoDeltaClampZero",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeTickMonoDeltaClampZeroTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_TICK_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 3;
    Prev.SessionCountToday = 1;
    Prev.LastWallUtc = T0;
    Prev.LastMonotonicSec = 600.0; // 이전 session의 monotonic
    Sub->TestingInjectPrevRecord(Prev);

    // WallDelta = 1s, MonoNow = 0.5 (부팅 리셋 — MonoNow < LastMonotonicSec)
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(1.0));
    Mock->SetMonotonicSec(0.5);

    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastedAction](ETimeAction Action)
    {
        BroadcastedAction = Action;
    });

    // Act — Formula 2 clamp 검증 (크래시 없음 + 정상 분류)
    const bool bContinue = Sub->TestingTickInSession(1.0f);

    // Assert
    // MonoDelta = max(0, 0.5 - 600.0) = 0.0 → DiscrepancyAbs = |1.0 - 0.0| = 1.0 ≤ 5.0 (Epsilon)
    TestTrue(TEXT("ticker continues (true 반환)"), bContinue);
    TestEqual(TEXT("Rule 5 ADVANCE_SILENT: WallDelta=1s, MonoDelta=0 (clamped)"),
              BroadcastedAction, ETimeAction::ADVANCE_SILENT);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: NORMAL_TICK_IN_SESSION (Rule 5)
//
// WallDelta=1s, MonoDelta=1s → DiscrepancyAbs=0 ≤ DefaultEpsilonSec(5)
// → ADVANCE_SILENT. DayIndex 불변.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeTickNormalTickInSessionTest,
    "MossBaby.Time.Tick.NormalTickInSession",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeTickNormalTickInSessionTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_TICK_SETUP();
    MOSS_TICK_INJECT_RECORD(Sub, T0);

    // WallDelta=1s, MonoDelta=1s → 정상 시계 동기
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(1.0));
    Mock->SetMonotonicSec(101.0); // LastMonotonicSec=100.0 → MonoDelta=1.0

    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    int32 BroadcastCount = 0;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastedAction, &BroadcastCount](ETimeAction Action)
    {
        BroadcastedAction = Action;
        BroadcastCount++;
    });

    const int32 DayIndexBefore = Sub->TestingGetCurrentRecord().DayIndex;

    // Act
    const bool bContinue = Sub->TestingTickInSession(1.0f);

    // Assert
    TestTrue(TEXT("ticker continues"), bContinue);
    TestEqual(TEXT("Rule 5: ADVANCE_SILENT"), BroadcastedAction, ETimeAction::ADVANCE_SILENT);
    TestEqual(TEXT("delegate 1회 broadcast"), BroadcastCount, 1);
    TestEqual(TEXT("DayIndex 불변 (1초 경과 → 전진 없음)"),
              Sub->TestingGetCurrentRecord().DayIndex, DayIndexBefore);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: MINOR_SHIFT_NTP (Rule 7)
//
// |Δ| = 15s (DefaultEpsilonSec=5 < 15 ≤ InSessionToleranceMinutes×60=5400)
// → ADVANCE_SILENT (NTP 재동기 흡수). DayIndex 불변.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeTickMinorShiftNtpTest,
    "MossBaby.Time.Tick.MinorShiftNtp",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeTickMinorShiftNtpTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_TICK_SETUP();
    MOSS_TICK_INJECT_RECORD(Sub, T0);

    // WallDelta=16s, MonoDelta=1s → DiscrepancyAbs=15s (5 < 15 ≤ 5400)
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(16.0));
    Mock->SetMonotonicSec(101.0); // MonoDelta = 101.0 - 100.0 = 1.0

    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastedAction](ETimeAction Action)
    {
        BroadcastedAction = Action;
    });

    const int32 DayIndexBefore = Sub->TestingGetCurrentRecord().DayIndex;

    // Act
    const bool bContinue = Sub->TestingTickInSession(1.0f);

    // Assert
    TestTrue(TEXT("ticker continues"), bContinue);
    TestEqual(TEXT("Rule 7 MINOR_SHIFT: ADVANCE_SILENT (NTP 흡수)"),
              BroadcastedAction, ETimeAction::ADVANCE_SILENT);
    TestEqual(TEXT("DayIndex 불변"), Sub->TestingGetCurrentRecord().DayIndex, DayIndexBefore);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: MINOR_SHIFT_DST (Rule 7)
//
// |Δ| = 3600s (1h DST spring-forward). 5 < 3600 ≤ 5400 → ADVANCE_SILENT.
// 플레이어 대면 알림 없음 — delegate ADVANCE_SILENT만 발행.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeTickMinorShiftDstTest,
    "MossBaby.Time.Tick.MinorShiftDst",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeTickMinorShiftDstTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_TICK_SETUP();
    MOSS_TICK_INJECT_RECORD(Sub, T0);

    // WallDelta = 3601s, MonoDelta = 1s → DiscrepancyAbs = 3600s (1h)
    // InSessionToleranceMinutes=90 → ToleranceSec=5400. 3600 ≤ 5400 → Rule 7.
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(3601.0));
    Mock->SetMonotonicSec(101.0); // MonoDelta=1.0

    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastedAction](ETimeAction Action)
    {
        BroadcastedAction = Action;
    });

    // Act
    const bool bContinue = Sub->TestingTickInSession(1.0f);

    // Assert
    TestTrue(TEXT("ticker continues"), bContinue);
    TestEqual(TEXT("Rule 7 MINOR_SHIFT DST: ADVANCE_SILENT"),
              BroadcastedAction, ETimeAction::ADVANCE_SILENT);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: IN_SESSION_DISCREPANCY_LOG_ONLY (Rule 8)
//
// |Δ| = 4h58min = 17880s > InSessionToleranceMinutes×60 = 5400s
// → LOG_ONLY. DayIndex 불변. LastWallUtc / LastMonotonicSec 갱신 없음.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeTickInSessionDiscrepancyLogOnlyTest,
    "MossBaby.Time.Tick.InSessionDiscrepancyLogOnly",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeTickInSessionDiscrepancyLogOnlyTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_TICK_SETUP();
    MOSS_TICK_INJECT_RECORD(Sub, T0);

    // WallDelta = 17882s (≈ 4h58min + 2s), MonoDelta = 2s → DiscrepancyAbs = 17880s > 5400s
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(17882.0));
    Mock->SetMonotonicSec(102.0); // MonoDelta=2.0

    ETimeAction BroadcastedAction = ETimeAction::ADVANCE_SILENT;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastedAction](ETimeAction Action)
    {
        BroadcastedAction = Action;
    });

    const int32 DayIndexBefore = Sub->TestingGetCurrentRecord().DayIndex;
    const FDateTime LastWallBefore = Sub->TestingGetCurrentRecord().LastWallUtc;
    const double LastMonoBefore = Sub->TestingGetCurrentRecord().LastMonotonicSec;

    // Act
    const bool bContinue = Sub->TestingTickInSession(1.0f);

    // Assert
    TestTrue(TEXT("ticker continues"), bContinue);
    TestEqual(TEXT("Rule 8 IN_SESSION_DISCREPANCY: LOG_ONLY"),
              BroadcastedAction, ETimeAction::LOG_ONLY);
    TestEqual(TEXT("DayIndex 불변"), Sub->TestingGetCurrentRecord().DayIndex, DayIndexBefore);
    // Rule 8: LastWallUtc / LastMonotonicSec 갱신 없음 — 다음 tick에서 재평가
    TestEqual(TEXT("LastWallUtc 갱신 없음"),
              Sub->TestingGetCurrentRecord().LastWallUtc, LastWallBefore);
    TestEqual(TEXT("LastMonotonicSec 갱신 없음"),
              Sub->TestingGetCurrentRecord().LastMonotonicSec, LastMonoBefore);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: RULE_PRECEDENCE_FIRST_MATCH_WINS (Rule 6 vs Rule 8)
//
// MonoDelta=0s (< SuspendMonotonicThresholdSec=5), WallDelta=7200s (> SuspendWallThresholdSec=60)
// → Rule 6 SUSPEND_RESUME 매치. Rule 8도 논리상 match(|Δ|=7200>5400)이지만
//   first-match-wins로 Rule 6 우선.
// → ADVANCE_SILENT (NOT LOG_ONLY). 로그 "SUSPEND_RESUME".
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeTickRulePrecedenceFirstMatchWinsTest,
    "MossBaby.Time.Tick.RulePrecedenceFirstMatchWins",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeTickRulePrecedenceFirstMatchWinsTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_TICK_SETUP();
    MOSS_TICK_INJECT_RECORD(Sub, T0);

    // MonoDelta=0 (MonoNow = LastMonotonicSec = 100.0 → delta=0)
    // WallDelta=7200s (2h)
    // Rule 6: MonoDelta=0 < 5.0 AND WallDelta=7200 > 60.0 → match
    // Rule 8: DiscrepancyAbs=7200 > 5400 → 논리상 match, but Rule 6 먼저
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(7200.0));
    Mock->SetMonotonicSec(100.0); // MonoDelta = 100.0 - 100.0 = 0.0

    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastedAction](ETimeAction Action)
    {
        BroadcastedAction = Action;
    });

    // Act
    const bool bContinue = Sub->TestingTickInSession(1.0f);

    // Assert
    TestTrue(TEXT("ticker continues"), bContinue);
    TestEqual(TEXT("Rule 6 first-match-wins: ADVANCE_SILENT (NOT LOG_ONLY)"),
              BroadcastedAction, ETimeAction::ADVANCE_SILENT);
    // Rule 6 → LastWallUtc / LastMonotonicSec 갱신됨
    TestEqual(TEXT("LastWallUtc 갱신됨"),
              Sub->TestingGetCurrentRecord().LastWallUtc,
              T0 + FTimespan::FromSeconds(7200.0));
    TestEqual(TEXT("LastMonotonicSec 갱신됨"),
              Sub->TestingGetCurrentRecord().LastMonotonicSec, 100.0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: TICK_HANDLE_NOT_REGISTERED_BEFORE_INIT
//
// NewObject 후 Initialize() 미호출 상태에서 TestingIsTickRegistered() == false.
// TestingUnregisterTick() 이중 호출도 크래시 없음.
//
// NOTE: Initialize() 후 TickHandle 등록 검증은 실제 GameWorld가 필요한
// integration test로 분리 (TD-012).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeTickHandleNotRegisteredBeforeInitTest,
    "MossBaby.Time.Tick.HandleNotRegisteredBeforeInit",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeTickHandleNotRegisteredBeforeInitTest::RunTest(const FString& Parameters)
{
    // Arrange — Initialize() 미호출 (NewObject만)
    UMossTimeSessionSubsystem* Sub = NewObject<UMossTimeSessionSubsystem>();

    // Assert: Initialize 미호출 → TickHandle 미등록
    TestFalse(TEXT("Initialize 미호출 → TickHandle.IsValid() == false"),
              Sub->TestingIsTickRegistered());

    // TestingUnregisterTick 이중 호출 — 크래시 없음 검증
    Sub->TestingUnregisterTick();
    Sub->TestingUnregisterTick(); // idempotent
    TestFalse(TEXT("이중 UnregisterTick 후에도 TickHandle invalid"),
              Sub->TestingIsTickRegistered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: SESSION_COUNT_TODAY_RESET_CONTRACT
//
// Prev: DayIndex=5, SessionCountToday=3.
// WallDelta=25h (90000s), MonoDelta=0 → Rule 6 SUSPEND_RESUME
// AdvanceDayIfNeeded: DaysElapsed=floor(90000/86400)=1 → NewDay=6
// SessionCountToday=0 리셋. OnDayAdvanced(6) 1회 broadcast.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeTickSessionCountTodayResetContractTest,
    "MossBaby.Time.Tick.SessionCountTodayResetContract",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeTickSessionCountTodayResetContractTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_TICK_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 5;
    Prev.SessionCountToday = 3;
    Prev.LastWallUtc = T0;
    Prev.LastMonotonicSec = 100.0;
    Sub->TestingInjectPrevRecord(Prev);

    // WallDelta=90000s (25h), MonoDelta=0 → Rule 6 SUSPEND_RESUME
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(90000.0));
    Mock->SetMonotonicSec(100.0); // MonoDelta=0

    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastedAction](ETimeAction Action)
    {
        BroadcastedAction = Action;
    });

    int32 DayAdvancedBroadcastCount = 0;
    int32 DayAdvancedValue = 0;
    Sub->OnDayAdvanced.AddLambda([&DayAdvancedBroadcastCount, &DayAdvancedValue](int32 NewDay)
    {
        DayAdvancedBroadcastCount++;
        DayAdvancedValue = NewDay;
    });

    // Act
    const bool bContinue = Sub->TestingTickInSession(1.0f);

    // Assert
    TestTrue(TEXT("ticker continues"), bContinue);
    TestEqual(TEXT("Rule 6: ADVANCE_SILENT"), BroadcastedAction, ETimeAction::ADVANCE_SILENT);

    // AdvanceDayIfNeeded: floor(90000/86400)=1 → NewDay=6
    TestEqual(TEXT("DayIndex 전진: 5 → 6"),
              Sub->TestingGetCurrentRecord().DayIndex, 6);

    // SESSION_COUNT_TODAY_RESET_CONTRACT
    TestEqual(TEXT("SessionCountToday=0 리셋"),
              Sub->TestingGetCurrentRecord().SessionCountToday, 0);

    // OnDayAdvanced 1회 broadcast, 값=6
    TestEqual(TEXT("OnDayAdvanced 1회 broadcast"), DayAdvancedBroadcastCount, 1);
    TestEqual(TEXT("OnDayAdvanced 값=6"), DayAdvancedValue, 6);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
