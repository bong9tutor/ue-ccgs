// Copyright Moss Baby
//
// MossSaveMigrationTests.cpp — Migration chain + Sanity check 자동화 테스트
//
// Story 1-16: Formula 4 (Steps = CURRENT − From) + sanity check + rollback
// GDD: design/gdd/save-load-persistence.md §4 Schema 마이그레이션 + §Formula 4 + §E7
//
// AC 커버리지:
//   MIGRATION_SANITY_CHECK_POST → FMossMigrationSanityCheckRejectsInvalidDayIndexTest
//                                  FMossMigrationSanityCheckRejectsUpperBoundDayIndexTest
//                                  FMossMigrationSanityCheckAcceptsValidRangeTest
//                                  FMossMigrationSanityCheckRejectsNegativeNarrativeTest
//   Formula 4 no-op           → FMossMigrationNoOpWhenSameVersionTest
//   MIGRATION_EXCEPTION_ROLLBACK  → CURRENT_SCHEMA_VERSION=1 constexpr 고정으로 실경로 없음.
//                                   RunMigrationChain no-op 검증 + 미래 V2 bump 시 재검증 (TD-010).
//
// 설계 제약 (UE 5.6):
//   - try/catch 미사용 — UE는 C++ exceptions 기본 비활성.
//   - migrator: bool 반환 + FString& OutError (UE idiomatic).
//   - CURRENT_SCHEMA_VERSION = 1 constexpr 고정 → loop 미실행(no-op 경로만 커버).
//   - V1→V2 실경로 테스트: CURRENT_SCHEMA_VERSION bump 시 TD-010에서 활성화.
//
// headless 실행:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.SaveLoad.Migration.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "SaveLoad/MossSaveLoadSubsystem.h"
#include "SaveLoad/MossSaveData.h"
#include "Time/SessionRecord.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: RunMigrationChain — From == CURRENT (no-op)
//
// AC: Formula 4 no-op (AUTOMATED)
// Arrange: UMossSaveData.SaveSchemaVersion = CURRENT_SCHEMA_VERSION (=1)
// Act:     TestingRunMigrationChain
// Assert:  true 반환, SaveSchemaVersion 불변, SessionRecord 불변
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossMigrationNoOpWhenSameVersionTest,
    "MossBaby.SaveLoad.Migration.NoOpWhenSameVersion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossMigrationNoOpWhenSameVersionTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    UMossSaveData* Data = NewObject<UMossSaveData>();

    Data->SaveSchemaVersion     = UMossSaveData::CURRENT_SCHEMA_VERSION; // 현재 버전 = 1
    Data->SessionRecord.DayIndex        = 5;
    Data->SessionRecord.NarrativeCount  = 3;
    Data->WriteSeqNumber        = 42;
    Data->LastSaveReason        = TEXT("ECardOffered");

    const uint8  ExpectedVersion   = UMossSaveData::CURRENT_SCHEMA_VERSION;
    const int32  ExpectedDay       = 5;
    const int32  ExpectedNarrative = 3;
    const uint32 ExpectedWSN       = 42;

    // Act
    const bool bResult = Sub->TestingRunMigrationChain(Data);

    // Assert: no-op — 성공 반환 + 상태 불변
    TestTrue(TEXT("no-op 시 true 반환"), bResult);
    TestEqual(TEXT("SaveSchemaVersion 불변"), (int32)Data->SaveSchemaVersion, (int32)ExpectedVersion);
    TestEqual(TEXT("DayIndex 불변"),          Data->SessionRecord.DayIndex,        ExpectedDay);
    TestEqual(TEXT("NarrativeCount 불변"),    Data->SessionRecord.NarrativeCount,  ExpectedNarrative);
    TestEqual(TEXT("WriteSeqNumber 불변"),    Data->WriteSeqNumber,                ExpectedWSN);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: IsSemanticallySane — DayIndex = 0 (하한 위반) → false
//
// AC: MIGRATION_SANITY_CHECK_POST (AUTOMATED — 직접 검증)
//
// 설계 노트:
//   CURRENT_SCHEMA_VERSION = 1 constexpr 고정이라 RunMigrationChain 내부의
//   loop가 실행되지 않는다 (From == To → early return).
//   따라서 sanity check post-migration 경로를 직접 테스트하는 것이 가장 현실적.
//   TestingIsSane()으로 IsSemanticallySane 직접 호출.
//
// Arrange: DayIndex = 0 (범위 외 — 하한 위반)
// Act:     TestingIsSane
// Assert:  false 반환
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossMigrationSanityCheckRejectsInvalidDayIndexTest,
    "MossBaby.SaveLoad.Migration.SanityCheck.RejectsZeroDayIndex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossMigrationSanityCheckRejectsInvalidDayIndexTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    UMossSaveData* Data = NewObject<UMossSaveData>();

    Data->SessionRecord.DayIndex       = 0;   // 범위 외 — 하한 위반 (유효범위 [1, 21])
    Data->SessionRecord.NarrativeCount = 0;   // 정상

    // Act
    const bool bSane = Sub->TestingIsSane(*Data);

    // Assert
    TestFalse(TEXT("DayIndex=0 → sane=false (하한 위반)"), bSane);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: IsSemanticallySane — DayIndex = 22 (상한 위반) → false
//
// AC: MIGRATION_SANITY_CHECK_POST (AUTOMATED — 직접 검증, 상한 경계)
// Arrange: DayIndex = 22 (범위 외 — 상한 위반)
// Act:     TestingIsSane
// Assert:  false 반환
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossMigrationSanityCheckRejectsUpperBoundDayIndexTest,
    "MossBaby.SaveLoad.Migration.SanityCheck.RejectsUpperBoundDayIndex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossMigrationSanityCheckRejectsUpperBoundDayIndexTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    UMossSaveData* Data = NewObject<UMossSaveData>();

    Data->SessionRecord.DayIndex       = 22;  // 범위 외 — 상한 위반 (유효범위 [1, 21])
    Data->SessionRecord.NarrativeCount = 0;   // 정상

    // Act
    const bool bSane = Sub->TestingIsSane(*Data);

    // Assert
    TestFalse(TEXT("DayIndex=22 → sane=false (상한 위반)"), bSane);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: IsSemanticallySane — DayIndex ∈ [1, 21] 유효 범위 경계값 → true
//
// AC: MIGRATION_SANITY_CHECK_POST (AUTOMATED — 유효 범위 검증)
// Arrange: DayIndex = 1 (최솟값), NarrativeCount = 0
//          DayIndex = 21 (최댓값), NarrativeCount = 100
// Act:     TestingIsSane
// Assert:  true 반환 (양 경계 모두)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossMigrationSanityCheckAcceptsValidRangeTest,
    "MossBaby.SaveLoad.Migration.SanityCheck.AcceptsValidDayRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossMigrationSanityCheckAcceptsValidRangeTest::RunTest(const FString& Parameters)
{
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();

    // Arrange + Act + Assert: DayIndex = 1 (하한 경계)
    {
        UMossSaveData* Data = NewObject<UMossSaveData>();
        Data->SessionRecord.DayIndex       = 1;
        Data->SessionRecord.NarrativeCount = 0;
        const bool bSane = Sub->TestingIsSane(*Data);
        TestTrue(TEXT("DayIndex=1 → sane=true (하한 경계)"), bSane);
    }

    // Arrange + Act + Assert: DayIndex = 21 (상한 경계)
    {
        UMossSaveData* Data = NewObject<UMossSaveData>();
        Data->SessionRecord.DayIndex       = 21;
        Data->SessionRecord.NarrativeCount = 100; // 정상 양수
        const bool bSane = Sub->TestingIsSane(*Data);
        TestTrue(TEXT("DayIndex=21 NarrativeCount=100 → sane=true (상한 경계)"), bSane);
    }

    // Arrange + Act + Assert: 중간값 DayIndex = 10
    {
        UMossSaveData* Data = NewObject<UMossSaveData>();
        Data->SessionRecord.DayIndex       = 10;
        Data->SessionRecord.NarrativeCount = 5;
        const bool bSane = Sub->TestingIsSane(*Data);
        TestTrue(TEXT("DayIndex=10 NarrativeCount=5 → sane=true (중간값)"), bSane);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5: IsSemanticallySane — NarrativeCount = -1 → false
//
// AC: MIGRATION_SANITY_CHECK_POST (AUTOMATED — NarrativeCount 음수 거부)
// Arrange: DayIndex = 5 (정상), NarrativeCount = -1 (비정상)
// Act:     TestingIsSane
// Assert:  false 반환
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossMigrationSanityCheckRejectsNegativeNarrativeTest,
    "MossBaby.SaveLoad.Migration.SanityCheck.RejectsNegativeNarrativeCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossMigrationSanityCheckRejectsNegativeNarrativeTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    UMossSaveData* Data = NewObject<UMossSaveData>();

    Data->SessionRecord.DayIndex       = 5;   // 정상 범위
    Data->SessionRecord.NarrativeCount = -1;  // 비정상 — 음수 금지

    // Act
    const bool bSane = Sub->TestingIsSane(*Data);

    // Assert
    TestFalse(TEXT("NarrativeCount=-1 → sane=false (음수 금지)"), bSane);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6: RunMigrationChain — rollback stub (CURRENT=1 constexpr 제약 명시)
//
// AC: MIGRATION_EXCEPTION_ROLLBACK (stub — 실경로 없음)
//
// 설계 노트:
//   CURRENT_SCHEMA_VERSION = 1 constexpr 고정으로 RunMigrationChain loop가
//   실행되지 않는다 (From == To → no-op early return).
//   따라서 실제 migrator 실패 → rollback 경로는 현재 커버 불가.
//
//   실경로 테스트 조건: CURRENT_SCHEMA_VERSION >= 2 (V2 bump 시).
//   CURRENT_SCHEMA_VERSION bump 시 이 테스트를 실제 rollback 검증으로 교체
//   → tech-debt TD-010 참조.
//
//   현재 이 테스트는:
//     1. CURRENT_SCHEMA_VERSION = 1 constexpr임을 런타임에서 확인.
//     2. RunMigrationChain(From=1)이 no-op true를 반환함을 재확인.
//     3. 향후 rollback 테스트의 placeholder로서 존재.
//
// Arrange: SaveSchemaVersion = CURRENT (1)
// Act:     TestingRunMigrationChain
// Assert:  true + version 불변 (no-op 재확인)
//          + 주석: V2 bump 시 mock migrator로 rollback 검증 필요
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossMigrationRollbackStubTest,
    "MossBaby.SaveLoad.Migration.RollbackStub.CurrentV1NoopConfirm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossMigrationRollbackStubTest::RunTest(const FString& Parameters)
{
    // CURRENT_SCHEMA_VERSION = 1 constexpr 런타임 확인.
    // 이 값이 변경되면 이 테스트는 아래 assert로 실패 → TD-010 작업 신호.
    const uint8 Current = UMossSaveData::CURRENT_SCHEMA_VERSION;
    TestEqual(TEXT("CURRENT_SCHEMA_VERSION = 1 (rollback 실경로는 V2 bump 후 TD-010)"),
              (int32)Current, 1);

    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    UMossSaveData* Data = NewObject<UMossSaveData>();

    Data->SaveSchemaVersion            = UMossSaveData::CURRENT_SCHEMA_VERSION;
    Data->SessionRecord.DayIndex       = 7;
    Data->SessionRecord.NarrativeCount = 2;

    // Act: CURRENT=1, From=1 → no-op → loop 미실행 → rollback 경로 없음.
    const bool bResult = Sub->TestingRunMigrationChain(Data);

    // Assert: no-op 재확인.
    TestTrue(TEXT("CURRENT=1 no-op → true (rollback 실경로 없음)"), bResult);
    TestEqual(TEXT("SaveSchemaVersion 불변 (rollback 없음)"),
              (int32)Data->SaveSchemaVersion, (int32)UMossSaveData::CURRENT_SCHEMA_VERSION);

    // TODO TD-010: CURRENT_SCHEMA_VERSION >= 2 bump 시 이 테스트를 아래 패턴으로 교체:
    //   Sub->TestingSetV1ToV2Migrator([](UMossSaveData* D, FString& Err) -> bool {
    //       Err = TEXT("mock failure"); return false; });
    //   const bool bFail = Sub->TestingRunMigrationChain(Data);
    //   TestFalse("migrator 실패 → rollback → false", bFail);
    //   TestEqual("rollback 후 SaveSchemaVersion 원복", Data->SaveSchemaVersion, OriginalVersion);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
