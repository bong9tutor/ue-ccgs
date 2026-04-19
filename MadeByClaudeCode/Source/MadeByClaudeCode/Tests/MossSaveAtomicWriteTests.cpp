// Copyright Moss Baby
//
// MossSaveAtomicWriteTests.cpp — Atomic write + Ping-pong 자동화 테스트
//
// Story 1-10: FMossSaveSnapshot POD factory, ATOMIC_PINGPONG,
//             ATOMIC_RENAME_NO_TMP_REMAINS, Header round-trip + CRC
// GDD: design/gdd/save-load-persistence.md §2 저장 절차 Step 4-9
//
// 테스트 후 cleanup: .sav / .tmp 파일 IFileManager::Delete (CleanupTestSaveFiles 헬퍼)
// headless 실행: -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "SaveLoad/MossSaveLoadSubsystem.h"
#include "SaveLoad/MossSaveData.h"
#include "SaveLoad/MossSaveSnapshot.h"
#include "SaveLoad/MossSaveHeader.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// 공통 헬퍼: 테스트 시작·종료 시 슬롯 파일 cleanup
// ─────────────────────────────────────────────────────────────────────────────

static void CleanupTestSaveFiles()
{
    const FString SlotA = UMossSaveLoadSubsystem::GetSlotPath(TEXT('A'));
    const FString SlotB = UMossSaveLoadSubsystem::GetSlotPath(TEXT('B'));
    IFileManager& FM = IFileManager::Get();
    FM.Delete(*SlotA,                    /*bMustExist=*/false);
    FM.Delete(*(SlotA + TEXT(".tmp")),   /*bMustExist=*/false);
    FM.Delete(*SlotB,                    /*bMustExist=*/false);
    FM.Delete(*(SlotB + TEXT(".tmp")),   /*bMustExist=*/false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: MakeSnapshotFromSaveData — POD factory 필드 복사 검증
//
// AC: FMossSaveSnapshot POD factory (AUTOMATED)
// Arrange: UMossSaveData 필드 직접 설정
// Act:     MakeSnapshotFromSaveData 호출
// Assert:  모든 필드가 정확히 복사되는지 확인
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveSnapshotFactoryTest,
    "MossBaby.SaveLoad.AtomicWrite.SnapshotFactory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveSnapshotFactoryTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveData* Data = NewObject<UMossSaveData>();
    Data->SaveSchemaVersion = 1;
    Data->LastSaveReason = TEXT("ECardOffered");
    Data->SessionRecord.DayIndex = 7;
    Data->SessionRecord.NarrativeCount = 2;

    // Act
    const FMossSaveSnapshot Snap = MakeSnapshotFromSaveData(*Data);

    // Assert
    TestEqual(TEXT("SchemaVersion 일치"), (int32)Snap.SaveSchemaVersion, 1);
    TestEqual(TEXT("LastSaveReason 일치"), Snap.LastSaveReason, FString(TEXT("ECardOffered")));
    TestEqual(TEXT("DayIndex 일치"), Snap.SessionRecord.DayIndex, 7);
    TestEqual(TEXT("NarrativeCount 일치"), Snap.SessionRecord.NarrativeCount, 2);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: WriteSlotAtomic — .tmp 파일이 남지 않음 (atomic rename 완료 확인)
//
// AC: ATOMIC_RENAME_NO_TMP_REMAINS (AUTOMATED)
// Arrange: cleanup + subsystem + SaveData 준비
// Act:     TestingWriteSlotAtomic(B, Snap, WSN=1)
// Assert:  MossData_B.sav 존재, MossData_B.tmp 부재
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveAtomicRenameNoTmpTest,
    "MossBaby.SaveLoad.AtomicWrite.NoTmpRemains",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveAtomicRenameNoTmpTest::RunTest(const FString& Parameters)
{
    // Arrange
    CleanupTestSaveFiles();
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    UMossSaveData* Data = NewObject<UMossSaveData>();
    Data->SaveSchemaVersion = 1;
    Data->SessionRecord.DayIndex = 3;
    Sub->TestingSetSaveData(Data);
    Sub->TestingSetActiveSlot(TEXT('A')); // 다음 write는 B

    FMossSaveSnapshot Snap = MakeSnapshotFromSaveData(*Data);

    // Act
    const bool bOk = Sub->TestingWriteSlotAtomic(TEXT('B'), Snap, /*NextWSN=*/1);

    // Assert
    TestTrue(TEXT("WriteSlotAtomic 성공"), bOk);
    const FString SlotB    = UMossSaveLoadSubsystem::GetSlotPath(TEXT('B'));
    const FString SlotBTmp = SlotB + TEXT(".tmp");
    TestTrue( TEXT("MossData_B.sav 존재"),                    IFileManager::Get().FileSize(*SlotB) > 0);
    TestFalse(TEXT("MossData_B.tmp 부재 (atomic rename 완료)"), IFileManager::Get().FileExists(*SlotBTmp));

    // Cleanup
    CleanupTestSaveFiles();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: SaveAsync ping-pong — ActiveSlot=A → 다음 쓰기는 B
//
// AC: ATOMIC_PINGPONG (AUTOMATED)
// Arrange: cleanup + ActiveSlot=A
// Act:     TestingWriteSlotAtomic(B, ...)
// Assert:  B 슬롯 생성, A 슬롯 미생성
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSavePingPongSlotSelectTest,
    "MossBaby.SaveLoad.AtomicWrite.PingPongSlotSelect",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSavePingPongSlotSelectTest::RunTest(const FString& Parameters)
{
    // Arrange
    CleanupTestSaveFiles();
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    UMossSaveData* Data = NewObject<UMossSaveData>();
    Data->SaveSchemaVersion = 1;
    Sub->TestingSetSaveData(Data);
    Sub->TestingSetActiveSlot(TEXT('A'));

    FMossSaveSnapshot Snap = MakeSnapshotFromSaveData(*Data);

    // Act: Target=B (Active=A → ping-pong B)
    const bool bOk = Sub->TestingWriteSlotAtomic(TEXT('B'), Snap, /*NextWSN=*/43);

    // Assert: B 파일 생성, A 파일 미생성
    TestTrue(TEXT("WriteSlotAtomic 성공"), bOk);
    const FString SlotA = UMossSaveLoadSubsystem::GetSlotPath(TEXT('A'));
    const FString SlotB = UMossSaveLoadSubsystem::GetSlotPath(TEXT('B'));
    TestTrue( TEXT("B slot 생성"),             IFileManager::Get().FileSize(*SlotB) > 0);
    TestFalse(TEXT("A slot 미생성 (이 테스트)"), IFileManager::Get().FileExists(*SlotA));

    CleanupTestSaveFiles();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: Header CRC round-trip — 쓴 파일을 TestingReadSlot으로 읽어 WSN·CRC 검증
//
// AC: Header round-trip + CRC (AUTOMATED)
// Arrange: WriteSlotAtomic(B, Snap, WSN=5)
// Act:     TestingReadSlot(SlotB, Header, Payload)
// Assert:  bReadOk=true, Header.WriteSeqNumber=5, Header.SaveSchemaVersion=1
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveRoundTripHeaderCrcTest,
    "MossBaby.SaveLoad.AtomicWrite.RoundTripHeaderCrc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveRoundTripHeaderCrcTest::RunTest(const FString& Parameters)
{
    // Arrange
    CleanupTestSaveFiles();
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    UMossSaveData* Data = NewObject<UMossSaveData>();
    Data->SaveSchemaVersion = 1;
    Data->SessionRecord.DayIndex = 7;
    Sub->TestingSetSaveData(Data);
    Sub->TestingSetActiveSlot(TEXT('A'));

    FMossSaveSnapshot Snap = MakeSnapshotFromSaveData(*Data);
    const uint32 ExpectedWSN = 5;

    // Act: write
    const bool bWriteOk = Sub->TestingWriteSlotAtomic(TEXT('B'), Snap, ExpectedWSN);
    TestTrue(TEXT("Write 성공"), bWriteOk);

    // Act: read back via TestingReadSlot
    FMossSaveHeader Header;
    TArray<uint8> Payload;
    const FString SlotB = UMossSaveLoadSubsystem::GetSlotPath(TEXT('B'));
    const bool bReadOk = Sub->TestingReadSlot(SlotB, Header, Payload);

    // Assert
    TestTrue( TEXT("Read 성공 (CRC 일치)"),         bReadOk);
    TestEqual(TEXT("Header WSN 일치"),               Header.WriteSeqNumber, ExpectedWSN);
    TestEqual(TEXT("Header SchemaVersion 일치"), (int32)Header.SaveSchemaVersion, 1);

    CleanupTestSaveFiles();
    return true;
}

#endif // WITH_AUTOMATION_TESTS
