// Copyright Moss Baby
//
// MossSaveHeaderTests.cpp — FMossSaveHeader 단위 테스트
//
// Story 1-9: FMossSaveHeader struct + MAGIC + Serialize/Deserialize
// GDD: design/gdd/save-load-persistence.md §1 슬롯 아키텍처 + §Formula 3
//
// AC 커버리지:
//   FMossSaveHeaderDefaultsTest        — 기본값 검증 (Magic/Version/WSN/Len/Crc)
//   FMossSaveHeaderSerializeRoundTripTest — Serialize → Deserialize bit-exact 일치
//   FMossSaveHeaderSizeTest             — 직렬화 후 버퍼 크기 정확히 22 bytes
//   FMossSaveHeaderMagicVerifyTest      — MAGIC_NUMBER == 0x4D4F5353 상수 검증
//
// Run headless:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.SaveLoad.Header.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "SaveLoad/MossSaveHeader.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// FMossSaveHeaderDefaultsTest
//
// FMossSaveHeader{} 기본값이 GDD §1 헤더 스펙과 일치하는지 검증.
//
// Arrange: FMossSaveHeader 기본 생성
// Act:     각 필드 직접 읽기
// Assert:  Magic=0x4D4F5353, SchemaVersion=CURRENT(1), MinCompatible=1,
//          WSN=0, PayloadLen=0, PayloadCrc32=0
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveHeaderDefaultsTest,
    "MossBaby.SaveLoad.Header.Defaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveHeaderDefaultsTest::RunTest(const FString& Parameters)
{
    // Arrange
    const FMossSaveHeader Header;

    // Assert: Magic 기본값 — MAGIC_NUMBER 상수
    TestEqual(
        TEXT("Magic 기본값 = 0x4D4F5353 (MAGIC_NUMBER)"),
        Header.Magic,
        FMossSaveHeader::MAGIC_NUMBER);

    // Assert: SaveSchemaVersion 기본값 — CURRENT_SCHEMA_VERSION(1)
    TestEqual(
        TEXT("SaveSchemaVersion 기본값 = UMossSaveData::CURRENT_SCHEMA_VERSION (1)"),
        Header.SaveSchemaVersion,
        UMossSaveData::CURRENT_SCHEMA_VERSION);

    // Assert: MinCompatibleSchemaVersion 기본값
    TestEqual(
        TEXT("MinCompatibleSchemaVersion 기본값 = 1"),
        Header.MinCompatibleSchemaVersion,
        (uint8)1);

    // Assert: WriteSeqNumber 기본값
    TestEqual(
        TEXT("WriteSeqNumber 기본값 = 0"),
        Header.WriteSeqNumber,
        (uint32)0);

    // Assert: PayloadByteLength 기본값
    TestEqual(
        TEXT("PayloadByteLength 기본값 = 0"),
        Header.PayloadByteLength,
        (uint32)0);

    // Assert: PayloadCrc32 기본값
    TestEqual(
        TEXT("PayloadCrc32 기본값 = 0"),
        Header.PayloadCrc32,
        (uint32)0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FMossSaveHeaderSerializeRoundTripTest
//
// FMossSaveHeader → SerializeToBuffer → DeserializeFromBuffer → bit-exact 일치.
//
// Arrange: Header A — 특정 값으로 초기화
// Act:     SerializeToBuffer → 새 Header B DeserializeFromBuffer
// Assert:  A와 B의 모든 필드 bit-exact 일치
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveHeaderSerializeRoundTripTest,
    "MossBaby.SaveLoad.Header.SerializeRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveHeaderSerializeRoundTripTest::RunTest(const FString& Parameters)
{
    // Arrange: Header A — 알려진 값으로 초기화
    FMossSaveHeader HeaderA;
    HeaderA.Magic = FMossSaveHeader::MAGIC_NUMBER;
    HeaderA.SaveSchemaVersion = UMossSaveData::CURRENT_SCHEMA_VERSION;
    HeaderA.MinCompatibleSchemaVersion = 1;
    HeaderA.WriteSeqNumber = 42;
    HeaderA.PayloadByteLength = 100;
    HeaderA.PayloadCrc32 = 0xDEADBEEF;

    // Act: 직렬화
    TArray<uint8> Buffer;
    HeaderA.SerializeToBuffer(Buffer);

    // Act: 역직렬화
    FMossSaveHeader HeaderB;
    const bool bDeserializeOk = HeaderB.DeserializeFromBuffer(Buffer);

    // Assert: Deserialize 성공
    TestTrue(TEXT("DeserializeFromBuffer 성공"), bDeserializeOk);

    // Assert: 모든 필드 bit-exact 일치
    TestEqual(
        TEXT("Magic round-trip 일치"),
        HeaderB.Magic,
        HeaderA.Magic);

    TestEqual(
        TEXT("SaveSchemaVersion round-trip 일치"),
        HeaderB.SaveSchemaVersion,
        HeaderA.SaveSchemaVersion);

    TestEqual(
        TEXT("MinCompatibleSchemaVersion round-trip 일치"),
        HeaderB.MinCompatibleSchemaVersion,
        HeaderA.MinCompatibleSchemaVersion);

    TestEqual(
        TEXT("WriteSeqNumber round-trip 일치 (42)"),
        HeaderB.WriteSeqNumber,
        HeaderA.WriteSeqNumber);

    TestEqual(
        TEXT("PayloadByteLength round-trip 일치 (100)"),
        HeaderB.PayloadByteLength,
        HeaderA.PayloadByteLength);

    TestEqual(
        TEXT("PayloadCrc32 round-trip 일치 (0xDEADBEEF)"),
        HeaderB.PayloadCrc32,
        HeaderA.PayloadCrc32);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FMossSaveHeaderSizeTest
//
// FMossSaveHeader::SerializeToBuffer 후 버퍼 크기가 정확히 HEADER_SIZE(22) bytes.
//
// Arrange: 기본 FMossSaveHeader
// Act:     SerializeToBuffer → 버퍼 크기 확인
// Assert:  버퍼 크기 == HEADER_SIZE == 22
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveHeaderSizeTest,
    "MossBaby.SaveLoad.Header.Size22Bytes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveHeaderSizeTest::RunTest(const FString& Parameters)
{
    // Arrange
    FMossSaveHeader Header;

    // Act
    TArray<uint8> Buffer;
    Header.SerializeToBuffer(Buffer);

    // Assert: 22 bytes 고정 크기
    TestEqual(
        TEXT("직렬화 후 버퍼 크기 = HEADER_SIZE (22 bytes)"),
        Buffer.Num(),
        FMossSaveHeader::HEADER_SIZE);

    // Assert: HEADER_SIZE 상수 자체도 22
    TestEqual(
        TEXT("HEADER_SIZE 상수 = 22"),
        FMossSaveHeader::HEADER_SIZE,
        22);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// FMossSaveHeaderMagicVerifyTest
//
// MAGIC_NUMBER 상수 == 0x4D4F5353 ('MOSS') 검증.
//
// Arrange: 상수 직접 참조
// Assert:  MAGIC_NUMBER == 0x4D4F5353
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveHeaderMagicVerifyTest,
    "MossBaby.SaveLoad.Header.MagicConstant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveHeaderMagicVerifyTest::RunTest(const FString& Parameters)
{
    // Assert: MAGIC_NUMBER 상수 값 == 0x4D4F5353
    TestEqual(
        TEXT("MAGIC_NUMBER == 0x4D4F5353 ('MOSS')"),
        FMossSaveHeader::MAGIC_NUMBER,
        (uint32)0x4D4F5353);

    // Assert: 기본 생성 헤더의 Magic 필드도 MAGIC_NUMBER
    const FMossSaveHeader Header;
    TestEqual(
        TEXT("기본 생성 헤더 Magic == MAGIC_NUMBER"),
        Header.Magic,
        FMossSaveHeader::MAGIC_NUMBER);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
