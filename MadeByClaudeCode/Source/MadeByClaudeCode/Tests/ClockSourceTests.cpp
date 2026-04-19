// Copyright Moss Baby
//
// Story-001: IMossClockSource 인터페이스 검증 테스트
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
//
// AC 커버리지:
//   AC-001: IMossClockSource 4 virtual methods 정의 → (헤더 컴파일로 CODE_REVIEW 검증)
//   AC-002: FRealClockSource 구현 → FClockSourceRealUtcDiffNonNegativeTest
//                                   FClockSourceRealMonoMonotonicTest
//   AC-003: FMockClockSource setter/simulate → FClockSourceMockSetGetUtcTest
//                                              FClockSourceMockSetGetMonoTest
//                                              FClockSourceMockSuspendResumeTest
//
// Run headless:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Time.ClockSource.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Time/MossClockSource.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// AC-003: FMockClockSource — SetUtcNow / GetUtcNow bit-exact 일치 검증
// 스토리 QA: "SetUtcNow(FDateTime(2026,4,19)) 후 반환값이 주입값과 bit-exact 일치"
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClockSourceMockSetGetUtcTest,
    "MossBaby.Time.ClockSource.MockSetGetUtc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClockSourceMockSetGetUtcTest::RunTest(const FString& Parameters)
{
    // Arrange: 테스트 날짜 주입 (Story-001 QA 케이스 기준값)
    FMockClockSource Mock;
    const FDateTime Injected(2026, 4, 19);

    // Act
    Mock.SetUtcNow(Injected);
    const FDateTime Retrieved = Mock.GetUtcNow();

    // Assert: bit-exact 일치 (Ticks 비교)
    TestEqual(TEXT("GetUtcNow 반환값이 SetUtcNow 주입값과 bit-exact 일치"), Retrieved.GetTicks(), Injected.GetTicks());

    // 추가: 기본값(FDateTime(0))에서 주입 전 초기 상태 검증
    FMockClockSource Default;
    TestEqual(TEXT("초기 MockedUtc는 FDateTime(0)"), Default.GetUtcNow().GetTicks(), FDateTime(0).GetTicks());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-003: FMockClockSource — SetMonotonicSec / GetMonotonicSec bit-exact 일치 검증
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClockSourceMockSetGetMonoTest,
    "MossBaby.Time.ClockSource.MockSetGetMono",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClockSourceMockSetGetMonoTest::RunTest(const FString& Parameters)
{
    // Arrange
    FMockClockSource Mock;
    const double InjectedSec = 12345.678;

    // Act
    Mock.SetMonotonicSec(InjectedSec);
    const double Retrieved = Mock.GetMonotonicSec();

    // Assert: bit-exact 일치
    TestEqual(TEXT("GetMonotonicSec 반환값이 SetMonotonicSec 주입값과 bit-exact 일치"), Retrieved, InjectedSec);

    // 추가: 기본값 0.0 검증
    FMockClockSource Default;
    TestEqual(TEXT("초기 MockedMono는 0.0"), Default.GetMonotonicSec(), 0.0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-003: FMockClockSource — SimulateSuspend + SimulateResume 예외 없이 실행 검증
// 스토리 QA Edge case: "SimulateSuspend() 후 SimulateResume() 호출 시 예외 없음"
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClockSourceMockSuspendResumeTest,
    "MossBaby.Time.ClockSource.MockSuspendResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClockSourceMockSuspendResumeTest::RunTest(const FString& Parameters)
{
    // Arrange
    FMockClockSource Mock;
    Mock.SetUtcNow(FDateTime(2026, 4, 19));
    Mock.SetMonotonicSec(100.0);

    // Act: suspend → resume 시퀀스 (크래시·예외 없이 완료되어야 함)
    Mock.SimulateSuspend();
    Mock.SimulateResume();

    // Assert: suspend/resume 후 주입 값은 변경되지 않아야 함
    TestEqual(TEXT("Suspend/Resume 후 UTC 값 불변"), Mock.GetUtcNow().GetTicks(), FDateTime(2026, 4, 19).GetTicks());
    TestEqual(TEXT("Suspend/Resume 후 Mono 값 불변"), Mock.GetMonotonicSec(), 100.0);

    // 연속 호출도 문제없어야 함
    Mock.SimulateSuspend();
    Mock.SimulateSuspend(); // 중복 suspend
    Mock.SimulateResume();

    TestTrue(TEXT("중복 Suspend 후 Resume 완료 — 크래시 없음"), true);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-002: FRealClockSource — GetUtcNow 2회 호출, 두 값의 차이 ≥ 0 검증
// 스토리 QA: "GetUtcNow() 2회 호출 → 두 값의 차이 ≥ 0 (wall clock 단조 아님 가정 유지)"
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClockSourceRealUtcDiffNonNegativeTest,
    "MossBaby.Time.ClockSource.RealUtcDiffNonNegative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClockSourceRealUtcDiffNonNegativeTest::RunTest(const FString& Parameters)
{
    // Arrange
    FRealClockSource Real;

    // Act: 연속 2회 호출
    const FDateTime First = Real.GetUtcNow();
    const FDateTime Second = Real.GetUtcNow();

    // Assert: 차이 ≥ 0 (Second - First의 Ticks가 음수가 아니어야 함)
    // 주의: wall clock은 NTP 등으로 뒤로 갈 수 있으나, 즉각 연속 호출에서는 non-negative 기대
    const FTimespan Diff = Second - First;
    TestTrue(TEXT("FRealClockSource GetUtcNow 연속 호출 차이 ≥ 0"), Diff.GetTicks() >= 0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-002: FRealClockSource — GetMonotonicSec 2회 호출, 후속 값 ≥ 이전 값 검증
// 스토리 QA Edge case: "GetMonotonicSec() 연속 호출 시 후속 값 ≥ 이전 값 (monotonic 보장)"
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FClockSourceRealMonoMonotonicTest,
    "MossBaby.Time.ClockSource.RealMonoMonotonic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FClockSourceRealMonoMonotonicTest::RunTest(const FString& Parameters)
{
    // Arrange
    FRealClockSource Real;

    // Act: 연속 2회 호출
    const double First = Real.GetMonotonicSec();
    const double Second = Real.GetMonotonicSec();

    // Assert: 후속 ≥ 이전 (FPlatformTime::Seconds 단조 증가 보장)
    TestTrue(TEXT("FRealClockSource GetMonotonicSec 후속 값 ≥ 이전 값"), Second >= First);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
