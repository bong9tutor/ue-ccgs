// Copyright Moss Baby
//
// Story-002: FSessionRecord 구조체 — double precision round-trip 검증 테스트
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// GDD: design/gdd/time-session-system.md §Formula 2 precision note
//
// AC 커버리지:
//   AC-1 (SESSION_RECORD_DOUBLE_TYPE_DECLARATION):
//       static_assert 컴파일 타임 강제 (CODE_REVIEW).
//       런타임 테스트 불필요 — static_assert가 실패하면 이 파일 자체가 컴파일되지 않음.
//       아래 FSessionRecordAcOneCodeReviewNoteTest 는 그 사실을 문서화하는 주석 역할만 함.
//   AC-2 (SESSION_RECORD_DOUBLE_PRECISION_RUNTIME):
//       FSessionRecordDoublePrecisionRoundTripTest — 1814400.123 round-trip ≤0.001 오차
//       FSessionRecordExtremePrecisionTest         — 1e-9 / 1e9 극단값 round-trip
//
// Round-trip 방식:
//   FMemoryWriter + FMemoryReader + FObjectAndNameAsStringProxyArchive 로
//   FSessionRecord를 바이트 배열에 직렬화 → 역직렬화.
//   UMossSaveData / Save-Load Subsystem 미사용 (Story 1-7 이후 구현 — Out of Scope).
//
// Run headless:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Time.SessionRecord.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Time/SessionRecord.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// AC-1 (SESSION_RECORD_DOUBLE_TYPE_DECLARATION) — CODE_REVIEW 검증 문서화
//
// static_assert(std::is_same_v<decltype(FSessionRecord::LastMonotonicSec), double>)
// 는 SessionRecord.h 에 선언되어 있다.
// 이 파일이 컴파일되면 static_assert 통과가 보장된다.
// 런타임 assert 불필요 — 단, 아래 테스트는 그 사실을 Automation 로그에 기록한다.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSessionRecordAcOneCodeReviewNoteTest,
    "MossBaby.Time.SessionRecord.AcOneCodeReviewNote",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSessionRecordAcOneCodeReviewNoteTest::RunTest(const FString& Parameters)
{
    // AC-1은 static_assert 컴파일로 CODE_REVIEW 검증 — 런타임 테스트 불필요.
    // 이 함수가 실행된다는 사실 자체가 SessionRecord.h static_assert 통과를 증명한다.
    AddInfo(TEXT("AC-1 (SESSION_RECORD_DOUBLE_TYPE_DECLARATION): "
                 "static_assert(is_same_v<decltype(LastMonotonicSec), double>) 컴파일 통과 확인. "
                 "CODE_REVIEW 검증 — 런타임 실행 불필요."));
    TestTrue(TEXT("AC-1 static_assert 컴파일 통과 — 이 라인 도달 = 검증 완료"), true);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FSessionRecord 기본값 검증
//
// 스토리 QA: 기본 생성 시 DayIndex=1, LastMonotonicSec=0.0 등 default 초기화 확인
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSessionRecordDefaultInitTest,
    "MossBaby.Time.SessionRecord.DefaultInit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSessionRecordDefaultInitTest::RunTest(const FString& Parameters)
{
    // Arrange: 기본 생성
    FSessionRecord Record;

    // Assert: 헤더에 명시된 default 초기값 검증
    TestEqual(TEXT("DayIndex 기본값은 1"), Record.DayIndex, 1);
    TestEqual(TEXT("LastMonotonicSec 기본값은 0.0"), Record.LastMonotonicSec, 0.0);
    TestEqual(TEXT("NarrativeCount 기본값은 0"), Record.NarrativeCount, 0);
    TestEqual(TEXT("SessionCountToday 기본값은 0"), Record.SessionCountToday, 0);
    TestTrue(TEXT("LastSaveReason 기본값은 빈 문자열"), Record.LastSaveReason.IsEmpty());

    // FGuid 기본값은 유효하지 않은(all-zero) GUID
    TestFalse(TEXT("SessionUuid 기본값은 유효하지 않은 GUID"), Record.SessionUuid.IsValid());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 헬퍼: FSessionRecord → TArray<uint8> 직렬화 → 역직렬화
//
// FObjectAndNameAsStringProxyArchive는 UPROPERTY 기반 리플렉션 직렬화를 활용.
// UScriptStruct::SerializeItem 과 달리 UHT 생성 코드 없이도 동작하며,
// USaveGame 기반 직렬화와 동일한 경로를 사용한다.
// ─────────────────────────────────────────────────────────────────────────────
static bool RoundTripSessionRecord(
    const FSessionRecord& Source,
    FSessionRecord& OutLoaded)
{
    // 직렬화: Source → Bytes
    TArray<uint8> Bytes;
    FMemoryWriter MemWriter(Bytes, /*bIsPersistent=*/true);
    FObjectAndNameAsStringProxyArchive WriteAr(MemWriter, /*bInLoadIfFindFails=*/false);

    // ArIsSaveGame 선택 사유:
    // UE는 ArIsSaveGame=true 시 UPROPERTY(SaveGame) specifier 마킹된 필드만 직렬화한다.
    // 이 Story 범위에서 FSessionRecord는 단순 UPROPERTY()만 사용하므로(Story 1-7에서 SaveGame
    // specifier 추가 여부 결정 예정), false로 두어 모든 UPROPERTY 필드를 직렬화한다.
    WriteAr.ArIsSaveGame = false;

    FSessionRecord MutableSource = Source;
    UScriptStruct* Struct = FSessionRecord::StaticStruct();
    Struct->SerializeItem(WriteAr, &MutableSource, nullptr);

    if (WriteAr.IsError())
    {
        return false;
    }

    // 역직렬화: Bytes → OutLoaded
    FMemoryReader MemReader(Bytes, /*bIsPersistent=*/true);
    FObjectAndNameAsStringProxyArchive ReadAr(MemReader, /*bInLoadIfFindFails=*/true);
    ReadAr.ArIsSaveGame = false;

    Struct->SerializeItem(ReadAr, &OutLoaded, nullptr);

    return !ReadAr.IsError();
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-2 (SESSION_RECORD_DOUBLE_PRECISION_RUNTIME) — 핵심 round-trip 테스트
//
// GDD AC SESSION_RECORD_DOUBLE_PRECISION_RUNTIME:
//   LastMonotonicSec = 1814400.123 (21일 + 0.123초) 대입
//   → serialize → deserialize round-trip
//   → TestNearlyEqual(loaded.LastMonotonicSec, 1814400.123, 0.001) 통과
//
// float(32-bit)이었다면 0.25초 오차로 이 테스트가 실패한다.
// double(64-bit)에서만 ≤0.001초 오차 보장.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSessionRecordDoublePrecisionRoundTripTest,
    "MossBaby.Time.SessionRecord.DoublePrecisionRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSessionRecordDoublePrecisionRoundTripTest::RunTest(const FString& Parameters)
{
    // Arrange: 21일 + 0.123초 — float이었다면 0.25초 오차로 실패할 값
    FSessionRecord Source;
    Source.SessionUuid    = FGuid::NewGuid();
    Source.DayIndex       = 21;
    Source.LastWallUtc    = FDateTime(2026, 4, 19, 12, 0, 0);
    Source.LastMonotonicSec = 1814400.123; // GDD AC 기준값
    Source.NarrativeCount = 2;
    Source.SessionCountToday = 3;
    Source.LastSaveReason = TEXT("EFarewellReached");

    // Act: round-trip
    FSessionRecord Loaded;
    const bool bSuccess = RoundTripSessionRecord(Source, Loaded);

    // Assert
    TestTrue(TEXT("Round-trip 직렬화 오류 없음"), bSuccess);
    TestNearlyEqual(
        TEXT("LastMonotonicSec 21일 기준값 round-trip ≤0.001 오차"),
        Loaded.LastMonotonicSec,
        1814400.123,
        0.001);

    // 다른 필드도 손상 없이 복원되었는지 확인 (qa-tester GAP #1 보완)
    TestEqual(TEXT("DayIndex round-trip 일치"), Loaded.DayIndex, Source.DayIndex);
    TestEqual(TEXT("LastWallUtc round-trip 일치 (ticks bit-exact)"),
              Loaded.LastWallUtc.GetTicks(), Source.LastWallUtc.GetTicks());
    TestEqual(TEXT("NarrativeCount round-trip 일치"), Loaded.NarrativeCount, Source.NarrativeCount);
    TestEqual(TEXT("SessionCountToday round-trip 일치"), Loaded.SessionCountToday, Source.SessionCountToday);
    TestEqual(TEXT("LastSaveReason round-trip 일치"), Loaded.LastSaveReason, Source.LastSaveReason);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-2 edge case: 극소값(1e-9) + 극대값(1e9) round-trip
//
// GDD QA Edge cases: "1e-9 극소값과 1e9 극대값 round-trip"
// double은 두 범위 모두 정밀하게 표현 가능.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSessionRecordExtremePrecisionTest,
    "MossBaby.Time.SessionRecord.ExtremePrecision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSessionRecordExtremePrecisionTest::RunTest(const FString& Parameters)
{
    // --- 극소값 (1e-9초 = 1나노초) ---
    {
        FSessionRecord Source;
        Source.LastMonotonicSec = 1e-9;

        FSessionRecord Loaded;
        const bool bSuccess = RoundTripSessionRecord(Source, Loaded);

        TestTrue(TEXT("[극소] Round-trip 직렬화 오류 없음"), bSuccess);
        // 극소값은 절대 오차보다 상대 오차로 검증
        // TestNearlyEqual은 절대 허용 오차를 사용하므로 직접 비교
        TestEqual(
            TEXT("[극소] LastMonotonicSec 1e-9 round-trip bit-exact"),
            Loaded.LastMonotonicSec,
            1e-9);
    }

    // --- 극대값 (1e9초 ≈ 31.7년) ---
    {
        FSessionRecord Source;
        Source.LastMonotonicSec = 1e9;

        FSessionRecord Loaded;
        const bool bSuccess = RoundTripSessionRecord(Source, Loaded);

        TestTrue(TEXT("[극대] Round-trip 직렬화 오류 없음"), bSuccess);
        TestNearlyEqual(
            TEXT("[극대] LastMonotonicSec 1e9 round-trip ≤0.001 오차"),
            Loaded.LastMonotonicSec,
            1e9,
            0.001);
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS
