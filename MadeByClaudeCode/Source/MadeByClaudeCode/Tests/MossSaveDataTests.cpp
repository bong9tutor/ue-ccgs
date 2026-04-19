// Copyright Moss Baby
//
// Story-001: UMossSaveData + ESaveReason enum + UMossSaveLoadSettings — 스키마 검증 테스트
// GDD: design/gdd/save-load-persistence.md §Core Rules + §ESaveReason enum contract
//
// AC 커버리지:
//   AC-1 (UCLASS_USAVEGAME):
//       FMossSaveDataDefaultsTest — NewObject<UMossSaveData>() 기본값 검증
//   AC-2 (ESAVEREASON_STABLE_INTEGERS):
//       FESaveReasonStableIntegersTest — 4값 stable integer 검증 (0/1/2/3)
//       FESaveReasonNameTest           — enum 이름 문자열 round-trip 검증
//   AC-3 (CURRENT_SCHEMA_VERSION_CONSTANT):
//       FMossSaveDataCurrentSchemaVersionConstTest — 컴파일 시간 상수 = 1 검증
//   AC-4 (UMOSS_SAVE_LOAD_SETTINGS):
//       FMossSaveLoadSettingsCDOTest — 5 knob 기본값 검증
//
// Run headless:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.SaveLoad.Schema.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "SaveLoad/MossSaveData.h"
#include "Settings/MossSaveLoadSettings.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// AC-1 (UCLASS_USAVEGAME): UMossSaveData 기본값 검증
//
// QA 케이스 (Story-001 §QA Test Cases):
//   Given:  UMossSaveData CDO
//   When:   NewObject<UMossSaveData>() 생성
//   Then:   SaveSchemaVersion == 1, WriteSeqNumber == 0,
//           LastSaveReason.IsEmpty(), bSaveDegraded == false
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveDataDefaultsTest,
    "MossBaby.SaveLoad.Schema.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveDataDefaultsTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveData* SaveData = NewObject<UMossSaveData>();

    // Assert: 스키마 버전 초기값
    TestEqual(TEXT("SaveSchemaVersion 기본값은 1"), SaveData->SaveSchemaVersion, (uint8)1);

    // Assert: 쓰기 시퀀스 번호 초기값
    TestEqual(TEXT("WriteSeqNumber 기본값은 0"), SaveData->WriteSeqNumber, (uint32)0);

    // Assert: 마지막 저장 이유 초기값 — 빈 문자열
    TestTrue(TEXT("LastSaveReason 기본값은 빈 문자열"), SaveData->LastSaveReason.IsEmpty());

    // Assert: 열화 플래그 초기값
    TestFalse(TEXT("bSaveDegraded 기본값은 false"), SaveData->bSaveDegraded);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-3 (CURRENT_SCHEMA_VERSION_CONSTANT): 컴파일 시간 상수 검증
//
// CURRENT_SCHEMA_VERSION 은 static constexpr — 런타임에도 값 확인 가능.
// 이 테스트가 실행된다는 사실 자체가 컴파일 통과를 증명.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveDataCurrentSchemaVersionConstTest,
    "MossBaby.SaveLoad.Schema.CurrentSchemaVersionConst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveDataCurrentSchemaVersionConstTest::RunTest(const FString& Parameters)
{
    // Arrange + Assert: static constexpr 값 검증
    // constexpr이므로 컴파일 타임에 1로 고정 — TestEqual로 런타임 값 기록
    TestEqual(
        TEXT("CURRENT_SCHEMA_VERSION 컴파일 시간 상수 = 1"),
        (uint8)UMossSaveData::CURRENT_SCHEMA_VERSION,
        (uint8)1);

    // SaveSchemaVersion 기본값이 CURRENT_SCHEMA_VERSION 과 동기화됨을 추가 확인
    UMossSaveData* SaveData = NewObject<UMossSaveData>();
    TestEqual(
        TEXT("SaveSchemaVersion 기본값 == CURRENT_SCHEMA_VERSION"),
        SaveData->SaveSchemaVersion,
        (uint8)UMossSaveData::CURRENT_SCHEMA_VERSION);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-2 (ESAVEREASON_STABLE_INTEGERS): enum 정수 값 stable integer 계약 검증
//
// GDD §6 stable integer 약속:
//   ECardOffered=0, EDreamReceived=1, ENarrativeEmitted=2, EFarewellReached=3
//   직렬화 파일에서 정수로 역직렬화 — 중간 삽입·재배치 금지
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FESaveReasonStableIntegersTest,
    "MossBaby.SaveLoad.Schema.ESaveReasonStableIntegers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FESaveReasonStableIntegersTest::RunTest(const FString& Parameters)
{
    // Assert: 각 enum 값의 정수 표현 — GDD §6 stable integer 계약
    TestEqual(
        TEXT("ECardOffered == 0 (stable integer)"),
        (uint8)ESaveReason::ECardOffered,
        (uint8)0);

    TestEqual(
        TEXT("EDreamReceived == 1 (stable integer)"),
        (uint8)ESaveReason::EDreamReceived,
        (uint8)1);

    TestEqual(
        TEXT("ENarrativeEmitted == 2 (stable integer)"),
        (uint8)ESaveReason::ENarrativeEmitted,
        (uint8)2);

    TestEqual(
        TEXT("EFarewellReached == 3 (stable integer)"),
        (uint8)ESaveReason::EFarewellReached,
        (uint8)3);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-2 edge case: ESaveReason 이름 문자열 round-trip
//
// QA edge case (Story-001 §QA Test Cases):
//   UEnum::GetValueAsString(ESaveReason::EDreamReceived) 에 "EDreamReceived" 포함
//   UMossSaveData::LastSaveReason 필드에 저장되는 문자열 계약 검증
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FESaveReasonNameTest,
    "MossBaby.SaveLoad.Schema.ESaveReasonName",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FESaveReasonNameTest::RunTest(const FString& Parameters)
{
    // Act: UEnum 리플렉션으로 enum 이름 문자열 획득
    const FString DreamReceivedName =
        UEnum::GetValueAsString(ESaveReason::EDreamReceived);

    // Assert: enum 이름에 "EDreamReceived" 포함
    // GetValueAsString 반환값 형식: "ESaveReason::EDreamReceived" 또는 "EDreamReceived"
    TestTrue(
        TEXT("ESaveReason::EDreamReceived 이름 문자열에 \"EDreamReceived\" 포함"),
        DreamReceivedName.Contains(TEXT("EDreamReceived")));

    // 나머지 값도 이름 문자열 존재 확인 (리플렉션 등록 검증)
    TestFalse(
        TEXT("ECardOffered 이름 문자열 비어있지 않음"),
        UEnum::GetValueAsString(ESaveReason::ECardOffered).IsEmpty());

    TestFalse(
        TEXT("ENarrativeEmitted 이름 문자열 비어있지 않음"),
        UEnum::GetValueAsString(ESaveReason::ENarrativeEmitted).IsEmpty());

    TestFalse(
        TEXT("EFarewellReached 이름 문자열 비어있지 않음"),
        UEnum::GetValueAsString(ESaveReason::EFarewellReached).IsEmpty());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// AC-4 (UMOSS_SAVE_LOAD_SETTINGS): UMossSaveLoadSettings CDO 5 knob 기본값 검증
//
// QA 케이스 (Story-001 §QA Test Cases):
//   Given:  UMossSaveLoadSettings CDO
//   When:   UMossSaveLoadSettings::Get() 호출
//   Then:   5 knob 기본값 각각 검증
//           MinCompatibleSchemaVersion=1, MaxPayloadBytes=10000000,
//           SaveFailureRetryThreshold=3, DeinitFlushTimeoutSec=2.0f,
//           bDevDiagnosticsEnabled=false
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveLoadSettingsCDOTest,
    "MossBaby.SaveLoad.Schema.SaveLoadSettingsCDO",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveLoadSettingsCDOTest::RunTest(const FString& Parameters)
{
    // Arrange: CDO 획득
    const UMossSaveLoadSettings* Settings = UMossSaveLoadSettings::Get();
    TestNotNull(TEXT("UMossSaveLoadSettings::Get() CDO 반환 비어있지 않음"), Settings);
    if (!Settings) { return false; }

    // Assert: MinCompatibleSchemaVersion 기본값
    TestEqual(
        TEXT("MinCompatibleSchemaVersion 기본값 = 1"),
        Settings->MinCompatibleSchemaVersion,
        (uint8)1);

    // Assert: MaxPayloadBytes 기본값 — 10MB
    TestEqual(
        TEXT("MaxPayloadBytes 기본값 = 10000000 (10MB)"),
        Settings->MaxPayloadBytes,
        10000000);

    // Assert: SaveFailureRetryThreshold 기본값
    TestEqual(
        TEXT("SaveFailureRetryThreshold 기본값 = 3"),
        Settings->SaveFailureRetryThreshold,
        3);

    // Assert: DeinitFlushTimeoutSec 기본값 — 2.0초
    TestNearlyEqual(
        TEXT("DeinitFlushTimeoutSec 기본값 = 2.0f"),
        (double)Settings->DeinitFlushTimeoutSec,
        2.0,
        0.001);

    // Assert: bDevDiagnosticsEnabled 기본값 — 출시 빌드 대비 false
    TestFalse(
        TEXT("bDevDiagnosticsEnabled 기본값 = false"),
        Settings->bDevDiagnosticsEnabled);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
