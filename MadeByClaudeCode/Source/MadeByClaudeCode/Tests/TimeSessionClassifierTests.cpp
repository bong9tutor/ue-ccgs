// Copyright Moss Baby
//
// TimeSessionClassifierTests.cpp — Between-session Classifier 자동화 테스트
//
// Story 1-14: Between-session Classifier (Rules 1-4 + Formula 4/5)
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// GDD: design/gdd/time-session-system.md §Core Rules §Rule 1~4 / §Formula 4/5
//
// AC 매핑 (12개 AC → 12개 AUTOMATED 테스트 1:1):
//
//   FIRST_RUN                          → FTimeClassifierFirstRunTest
//   BACKWARD_GAP_REJECT                → FTimeClassifierBackwardGapRejectTest
//   BACKWARD_GAP_REPEATED              → FTimeClassifierBackwardGapRepeatedTest
//   LONG_GAP_SILENT_BOUNDARY_OVER      → FTimeClassifierLongGapSilentBoundaryOverTest
//   LONG_GAP_BOUNDARY_EXACT            → FTimeClassifierLongGapBoundaryExactTest
//   ACCEPTED_GAP_SILENT                → FTimeClassifierAcceptedGapSilentTest
//   ACCEPTED_GAP_NARRATIVE_THRESHOLD   → FTimeClassifierAcceptedGapNarrativeThresholdTest
//   NARRATIVE_THRESHOLD_EXACT_BOUNDARY → FTimeClassifierNarrativeThresholdExactBoundaryTest
//   NARRATIVE_CAP_ENFORCED             → FTimeClassifierNarrativeCapEnforcedTest
//   DAYINDEX_FORMULA_FLOOR             → FTimeClassifierDayIndexFormulaFloorTest
//   DAYINDEX_CLAMP_UPPER               → FTimeClassifierDayIndexClampUpperTest
//   DAYINDEX_CORRUPTION_CLAMP          → FTimeClassifierDayIndexCorruptionClampTest
//
// Formula 4: DayIndex 전진 = FMath::FloorToInt32(WallDeltaSec / 86400.0)
// Formula 5: WallDeltaSec > NarrativeThresholdHours * 3600.0 (strict `>`)
//            + NarrativeCount < NarrativeCapPerPlaythrough
// ADR-0001 금지 패턴: tamper, cheat, bIsForward, bIsSuspicious 등 일체 미사용.
//
// 카테고리: MossBaby.Time.Classifier.*
// 실행 방법 (헤드리스):
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject
//     -ExecCmds="Automation RunTests MossBaby.Time.Classifier.; Quit"
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

/** 테스트용 Subsystem 인스턴스와 Mock 클락을 생성하는 매크로 */
#define MOSS_CLASSIFIER_SETUP() \
    UMossTimeSessionSubsystem* Sub = NewObject<UMossTimeSessionSubsystem>(); \
    TSharedPtr<FMockClockSource> Mock = MakeShared<FMockClockSource>(); \
    Sub->SetClockSource(Mock); \
    const FDateTime T0(2026, 1, 1, 12, 0, 0);

// ─────────────────────────────────────────────────────────────────────────────
// AC: FIRST_RUN
//   PrevRecord == nullptr 최초 실행 → START_DAY_ONE 반환,
//   CurrentRecord.DayIndex == 1, delegate 1회 broadcast.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierFirstRunTest,
    "MossBaby.Time.Classifier.FirstRun",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierFirstRunTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();
    Mock->SetUtcNow(T0);
    Mock->SetMonotonicSec(0.0);

    int32 BroadcastCount = 0;
    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastCount, &BroadcastedAction](ETimeAction Action)
    {
        BroadcastCount++;
        BroadcastedAction = Action;
    });

    // Act — PrevRecord nullptr → FIRST_RUN
    const ETimeAction Result = Sub->ClassifyOnStartup(nullptr);

    // Assert
    TestEqual(TEXT("START_DAY_ONE 반환"), (int32)Result, (int32)ETimeAction::START_DAY_ONE);
    TestEqual(TEXT("DayIndex == 1"), Sub->TestingGetCurrentRecord().DayIndex, 1);
    TestEqual(TEXT("delegate 1회 broadcast"), BroadcastCount, 1);
    TestEqual(TEXT("delegate 값 START_DAY_ONE"), (int32)BroadcastedAction, (int32)ETimeAction::START_DAY_ONE);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: BACKWARD_GAP_REJECT
//   Now < PrevRecord.LastWallUtc → HOLD_LAST_TIME 반환,
//   LastWallUtc 갱신 없음 (이전 값 유지).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierBackwardGapRejectTest,
    "MossBaby.Time.Classifier.BackwardGapReject",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierBackwardGapRejectTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    // T0 기준으로 PrevRecord 구성
    FSessionRecord Prev;
    Prev.DayIndex = 3;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    // 현재 시각을 T0보다 1초 이전으로 설정 (역행)
    Mock->SetUtcNow(T0 - FTimespan::FromSeconds(1.0));

    int32 BroadcastCount = 0;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastCount](ETimeAction) { BroadcastCount++; });

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert
    TestEqual(TEXT("HOLD_LAST_TIME 반환"), (int32)Result, (int32)ETimeAction::HOLD_LAST_TIME);
    // LastWallUtc 갱신 없음 — PrevRecord의 T0 값이 CurrentRecord에 복사된 채로 유지
    TestEqual(TEXT("LastWallUtc 갱신 없음 — T0 유지"), Sub->TestingGetCurrentRecord().LastWallUtc, T0);
    TestEqual(TEXT("delegate 1회 broadcast"), BroadcastCount, 1);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: BACKWARD_GAP_REPEATED
//   같은 Subsystem 인스턴스에 역행 UTC로 ClassifyOnStartup 2회 연속 호출.
//   두 번 모두 HOLD_LAST_TIME 반환.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierBackwardGapRepeatedTest,
    "MossBaby.Time.Classifier.BackwardGapRepeated",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierBackwardGapRepeatedTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 5;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    // Act 1 — T0 - 1시간 (역행)
    Mock->SetUtcNow(T0 - FTimespan::FromHours(1.0));
    const ETimeAction Result1 = Sub->ClassifyOnStartup(&Prev);

    // Act 2 — T0 - 2시간 (더 역행. 동일 인스턴스 재사용)
    Mock->SetUtcNow(T0 - FTimespan::FromHours(2.0));
    const ETimeAction Result2 = Sub->ClassifyOnStartup(&Prev);

    // Assert
    TestEqual(TEXT("1회 HOLD_LAST_TIME"), (int32)Result1, (int32)ETimeAction::HOLD_LAST_TIME);
    TestEqual(TEXT("2회 HOLD_LAST_TIME"), (int32)Result2, (int32)ETimeAction::HOLD_LAST_TIME);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: LONG_GAP_SILENT_BOUNDARY_OVER
//   WallDeltaSec > GameDurationDays * 86400 → LONG_GAP_SILENT 반환,
//   DayIndex == GameDurationDays, OnFarewellReached broadcast.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierLongGapSilentBoundaryOverTest,
    "MossBaby.Time.Classifier.LongGapSilentBoundaryOver",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierLongGapSilentBoundaryOverTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 2;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    // GameDurationDays * 86400 + 1초 초과
    const double OverLimitSec = Settings->GameDurationDays * 86400.0 + 1.0;
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(OverLimitSec));

    int32 FarewellCount = 0;
    EFarewellReason FarewellReason = EFarewellReason::NormalCompletion;
    Sub->OnFarewellReached.AddLambda([&FarewellCount, &FarewellReason](EFarewellReason Reason)
    {
        FarewellCount++;
        FarewellReason = Reason;
    });

    int32 ActionCount = 0;
    Sub->OnTimeActionResolved.AddLambda([&ActionCount](ETimeAction) { ActionCount++; });

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert
    TestEqual(TEXT("LONG_GAP_SILENT 반환"), (int32)Result, (int32)ETimeAction::LONG_GAP_SILENT);
    TestEqual(TEXT("DayIndex == GameDurationDays"), Sub->TestingGetCurrentRecord().DayIndex, Settings->GameDurationDays);
    TestEqual(TEXT("OnFarewellReached 1회"), FarewellCount, 1);
    TestEqual(TEXT("LongGapAutoFarewell 이유"), (int32)FarewellReason, (int32)EFarewellReason::LongGapAutoFarewell);
    TestEqual(TEXT("OnTimeActionResolved 1회"), ActionCount, 1);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: LONG_GAP_BOUNDARY_EXACT
//   WallDeltaSec == GameDurationDays * 86400 정확히 경계값.
//   경계값은 strict `>` 이므로 LONG_GAP_SILENT 아님 → ADVANCE_* 계열.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierLongGapBoundaryExactTest,
    "MossBaby.Time.Classifier.LongGapBoundaryExact",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierLongGapBoundaryExactTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 1;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    // 정확히 GameDurationDays * 86400초 — strict `>` 이므로 LONG_GAP 미해당
    const double ExactLimitSec = Settings->GameDurationDays * 86400.0;
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(ExactLimitSec));

    int32 FarewellCount = 0;
    Sub->OnFarewellReached.AddLambda([&FarewellCount](EFarewellReason) { FarewellCount++; });

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert — 경계값은 LONG_GAP_SILENT 아님 (Rule 3: strict `>`)
    TestNotEqual(TEXT("LONG_GAP_SILENT 아님 (경계 정확)"), (int32)Result, (int32)ETimeAction::LONG_GAP_SILENT);
    TestEqual(TEXT("Farewell broadcast 없음"), FarewellCount, 0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: ACCEPTED_GAP_SILENT
//   WallDeltaSec ∈ (0, NarrativeThreshold] 또는 NarrativeCount >= Cap
//   → ADVANCE_SILENT 반환, DayIndex 전진.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierAcceptedGapSilentTest,
    "MossBaby.Time.Classifier.AcceptedGapSilent",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierAcceptedGapSilentTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 2;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    // NarrativeThresholdHours = 24h → 24h 정각 = 86400초. strict `>` 기준 경계값 미달.
    // Formula 5: WallDeltaSec > NarrativeThresholdSec 이어야 NARRATIVE 발화.
    // 86400초 정확히 → threshold와 같으므로 ADVANCE_SILENT.
    const double ThresholdExactSec = Settings->NarrativeThresholdHours * 3600.0;
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(ThresholdExactSec));

    int32 BroadcastCount = 0;
    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastCount, &BroadcastedAction](ETimeAction Action)
    {
        BroadcastCount++;
        BroadcastedAction = Action;
    });

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert
    TestEqual(TEXT("ADVANCE_SILENT 반환"), (int32)Result, (int32)ETimeAction::ADVANCE_SILENT);
    TestEqual(TEXT("delegate ADVANCE_SILENT"), (int32)BroadcastedAction, (int32)ETimeAction::ADVANCE_SILENT);
    TestEqual(TEXT("delegate 1회"), BroadcastCount, 1);
    // DayIndex: Prev=2 + floor(86400/86400)=1 → 3
    TestEqual(TEXT("DayIndex == 3"), Sub->TestingGetCurrentRecord().DayIndex, 3);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: ACCEPTED_GAP_NARRATIVE_THRESHOLD
//   WallDeltaSec > NarrativeThresholdHours * 3600 (strict >) + NarrativeCount < Cap
//   → ADVANCE_WITH_NARRATIVE 반환, NarrativeCount 1 증가.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierAcceptedGapNarrativeThresholdTest,
    "MossBaby.Time.Classifier.AcceptedGapNarrativeThreshold",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierAcceptedGapNarrativeThresholdTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 3;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    // threshold 정확히 1초 초과 → strict `>` 조건 충족
    const double OverThresholdSec = Settings->NarrativeThresholdHours * 3600.0 + 1.0;
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(OverThresholdSec));

    int32 BroadcastCount = 0;
    ETimeAction BroadcastedAction = ETimeAction::LOG_ONLY;
    Sub->OnTimeActionResolved.AddLambda([&BroadcastCount, &BroadcastedAction](ETimeAction Action)
    {
        BroadcastCount++;
        BroadcastedAction = Action;
    });

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert
    TestEqual(TEXT("ADVANCE_WITH_NARRATIVE 반환"), (int32)Result, (int32)ETimeAction::ADVANCE_WITH_NARRATIVE);
    TestEqual(TEXT("delegate ADVANCE_WITH_NARRATIVE"), (int32)BroadcastedAction, (int32)ETimeAction::ADVANCE_WITH_NARRATIVE);
    TestEqual(TEXT("NarrativeCount 1 증가"), Sub->TestingGetCurrentRecord().NarrativeCount, 1);
    TestEqual(TEXT("delegate 1회"), BroadcastCount, 1);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: NARRATIVE_THRESHOLD_EXACT_BOUNDARY
//   WallDeltaSec == NarrativeThresholdHours * 3600 정확히 경계값.
//   strict `>` 이므로 ADVANCE_SILENT (내러티브 미발화).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierNarrativeThresholdExactBoundaryTest,
    "MossBaby.Time.Classifier.NarrativeThresholdExactBoundary",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierNarrativeThresholdExactBoundaryTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 2;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    // 정확히 NarrativeThresholdHours * 3600초 — strict `>` 이므로 미달
    const double ExactThresholdSec = Settings->NarrativeThresholdHours * 3600.0;
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(ExactThresholdSec));

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert — 경계 정확값은 ADVANCE_SILENT (내러티브 없음)
    TestEqual(TEXT("ADVANCE_SILENT (경계 정확, strict >)"), (int32)Result, (int32)ETimeAction::ADVANCE_SILENT);
    TestEqual(TEXT("NarrativeCount 불변"), Sub->TestingGetCurrentRecord().NarrativeCount, 0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: NARRATIVE_CAP_ENFORCED
//   NarrativeCount >= NarrativeCapPerPlaythrough 이면 threshold 초과해도
//   ADVANCE_SILENT (cap 강제, 내러티브 미발화).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierNarrativeCapEnforcedTest,
    "MossBaby.Time.Classifier.NarrativeCapEnforced",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierNarrativeCapEnforcedTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();

    FSessionRecord Prev;
    Prev.DayIndex = 5;
    // Cap에 이미 도달한 상태 (NarrativeCount == NarrativeCapPerPlaythrough)
    Prev.NarrativeCount = Settings->NarrativeCapPerPlaythrough;
    Prev.LastWallUtc = T0;

    // threshold 충분히 초과
    const double OverThresholdSec = Settings->NarrativeThresholdHours * 3600.0 + 3600.0;
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(OverThresholdSec));

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert — cap 도달 → ADVANCE_SILENT
    TestEqual(TEXT("ADVANCE_SILENT (cap 도달)"), (int32)Result, (int32)ETimeAction::ADVANCE_SILENT);
    // NarrativeCount 불변 (cap 이상으로 증가 금지)
    TestEqual(TEXT("NarrativeCount cap 초과 없음"), Sub->TestingGetCurrentRecord().NarrativeCount, Settings->NarrativeCapPerPlaythrough);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: DAYINDEX_FORMULA_FLOOR
//   Formula 4: DaysElapsed = FMath::FloorToInt32(WallDeltaSec / 86400.0)
//   23h59m59s 경과 → DaysElapsed=0 → DayIndex 불변.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierDayIndexFormulaFloorTest,
    "MossBaby.Time.Classifier.DayIndexFormulaFloor",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierDayIndexFormulaFloorTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    FSessionRecord Prev;
    Prev.DayIndex = 4;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    // 23시간 59분 59초 경과 — floor(86399/86400) = 0
    const double UnderOneDaySec = 23.0 * 3600.0 + 59.0 * 60.0 + 59.0;
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(UnderOneDaySec));

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert — DaysElapsed=0 이므로 DayIndex 불변(4)
    TestNotEqual(TEXT("HOLD 아님 (시간 진행)"), (int32)Result, (int32)ETimeAction::HOLD_LAST_TIME);
    TestEqual(TEXT("DayIndex 불변 (floor=0)"), Sub->TestingGetCurrentRecord().DayIndex, 4);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: DAYINDEX_CLAMP_UPPER
//   DayIndex 전진 결과가 GameDurationDays 초과 시 GameDurationDays로 clamp.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierDayIndexClampUpperTest,
    "MossBaby.Time.Classifier.DayIndexClampUpper",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierDayIndexClampUpperTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();

    FSessionRecord Prev;
    // 이미 끝 가까이에 있는 상태
    Prev.DayIndex = Settings->GameDurationDays - 1;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    // 5일 경과 → DayIndex가 GameDurationDays + 4가 되려 하지만 clamp
    const double FiveDaysSec = 5.0 * 86400.0 + 1.0;
    Mock->SetUtcNow(T0 + FTimespan::FromSeconds(FiveDaysSec));

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert — clamp 상한 = GameDurationDays
    TestEqual(TEXT("DayIndex == GameDurationDays (clamp 상한)"),
        Sub->TestingGetCurrentRecord().DayIndex, Settings->GameDurationDays);
    // LONG_GAP_SILENT 가 아닌 ADVANCE 계열 (5일 < 21일)
    TestNotEqual(TEXT("LONG_GAP_SILENT 아님"), (int32)Result, (int32)ETimeAction::LONG_GAP_SILENT);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC: DAYINDEX_CORRUPTION_CLAMP
//   E13 corruption: PrevRecord.DayIndex가 [1, GameDurationDays] 범위 밖이어도
//   ClassifyOnStartup 초입에서 clamp하여 정상 처리.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTimeClassifierDayIndexCorruptionClampTest,
    "MossBaby.Time.Classifier.DayIndexCorruptionClamp",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)
bool FTimeClassifierDayIndexCorruptionClampTest::RunTest(const FString& Parameters)
{
    // Arrange
    MOSS_CLASSIFIER_SETUP();

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();

    FSessionRecord Prev;
    // 세이브 파일 손상 시뮬레이션 — DayIndex가 최대값 훨씬 초과
    Prev.DayIndex = 9999;
    Prev.NarrativeCount = 0;
    Prev.LastWallUtc = T0;

    // 1시간 경과 (정상 범위)
    Mock->SetUtcNow(T0 + FTimespan::FromHours(1.0));

    // Act
    const ETimeAction Result = Sub->ClassifyOnStartup(&Prev);

    // Assert — 분류 후 DayIndex가 [1, GameDurationDays] 범위 내 보장
    const int32 FinalDay = Sub->TestingGetCurrentRecord().DayIndex;
    TestTrue(TEXT("DayIndex >= 1 (corruption clamp)"), FinalDay >= 1);
    TestTrue(TEXT("DayIndex <= GameDurationDays (corruption clamp)"), FinalDay <= Settings->GameDurationDays);
    // HOLD_LAST_TIME이나 LONG_GAP_SILENT 아님 (1시간 경과, 정상 경로)
    TestNotEqual(TEXT("HOLD_LAST_TIME 아님"), (int32)Result, (int32)ETimeAction::HOLD_LAST_TIME);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
