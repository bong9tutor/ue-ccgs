// Copyright Moss Baby
//
// MossSaveHeader.h — 22-byte 세이브 파일 헤더 구조체
//
// Story 1-9: FMossSaveHeader struct + MAGIC + Serialize/Deserialize
// GDD: design/gdd/save-load-persistence.md §1 슬롯 아키텍처 + §Formula 3
//
// 헤더 Layout (little-endian, 22 bytes 고정):
//   Offset  Size  Field
//   0       4     Magic            uint32  0x4D4F5353 ('MOSS')
//   4       1     SaveSchemaVersion uint8
//   5       1     MinCompatibleSchemaVersion uint8
//   6       4     WriteSeqNumber   uint32
//   10      4     PayloadByteLength uint32
//   14      4     PayloadCrc32     uint32  FCrc::MemCrc32(seed=0)
//   ──────────────────────────────────────────
//   Total   22
//
// Non-USTRUCT: UE 리플렉션 없이 raw binary 직렬화.
// Serialize(FArchive&) 하나로 read/write 방향 자동 전환 (FArchive 관용).
//
// Story 1-9 Out of Scope:
//   - Atomic write + FMossSaveSnapshot (Story 1-10)
//   - Migration chain (Story 1-16)

#pragma once
#include "CoreMinimal.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "SaveLoad/MossSaveData.h"

/**
 * FMossSaveHeader — 22-byte 세이브 파일 헤더.
 *
 * Layout (little-endian):
 *   Magic(4) + SchemaVersion(1) + MinCompatible(1) + WSN(4) + PayloadLen(4) + PayloadCrc32(4)
 *
 * CRC32 계산: FCrc::MemCrc32(PayloadData, PayloadLen, 0) — seed=0 고정 (R3 CRITICAL-4).
 * Non-USTRUCT — UE GC 밖. UMossSaveLoadSubsystem 스택 로컬로만 사용.
 */
struct FMossSaveHeader
{
    /** 파일 식별 매직 넘버 — 'MOSS' (0x4D4F5353) */
    static constexpr uint32 MAGIC_NUMBER = 0x4D4F5353;

    /** 헤더 고정 크기 (bytes). 직렬화 후 버퍼가 이 크기를 충족해야 유효. */
    static constexpr int32 HEADER_SIZE = 22;

    /** 파일 식별 매직. 읽기 후 MAGIC_NUMBER 와 비교하여 유효성 확인. */
    uint32 Magic = MAGIC_NUMBER;

    /**
     * 이 파일을 기록한 빌드의 스키마 버전.
     * UMossSaveData::CURRENT_SCHEMA_VERSION 과 동기화하여 초기화.
     * ReadSlot에서 CURRENT_SCHEMA_VERSION 초과 시 거부 (E12).
     */
    uint8 SaveSchemaVersion = UMossSaveData::CURRENT_SCHEMA_VERSION;

    /**
     * 이 파일을 읽을 수 있는 최소 스키마 버전.
     * UMossSaveLoadSettings::MinCompatibleSchemaVersion 미만 시 거부 (E13).
     */
    uint8 MinCompatibleSchemaVersion = 1;

    /**
     * 단조 증가 쓰기 시퀀스 번호.
     * 저장마다 +1. dual-slot 중 최신본 선택(Formula 1)에 사용.
     */
    uint32 WriteSeqNumber = 0;

    /**
     * 헤더 뒤에 이어지는 payload(직렬화된 USaveGame) 바이트 수.
     * ReadSlot에서 실제 버퍼 크기와 비교하여 일치 여부 확인.
     */
    uint32 PayloadByteLength = 0;

    /**
     * Payload 전체에 대한 CRC32.
     * FCrc::MemCrc32(PayloadData, PayloadLen, /*seed=*/0) 로 계산.
     * ReadSlot에서 재계산 값과 비교하여 데이터 무결성 확인 (Formula 3 조건 6).
     */
    uint32 PayloadCrc32 = 0;

    /**
     * FArchive 직렬화 — reader/writer 양방향.
     *
     * FArchive::IsLoading() / IsSaving() 에 따라 read/write 방향 자동 전환.
     * 22-byte 명시 레이아웃 순서로 직렬화.
     *
     * @param Ar  FMemoryReader (읽기) 또는 FMemoryWriter (쓰기)
     */
    void Serialize(FArchive& Ar)
    {
        Ar << Magic;
        Ar << SaveSchemaVersion;
        Ar << MinCompatibleSchemaVersion;
        Ar << WriteSeqNumber;
        Ar << PayloadByteLength;
        Ar << PayloadCrc32;
    }

    /**
     * 바이트 버퍼에서 헤더를 역직렬화.
     *
     * @param Bytes  원본 파일 바이트 배열. 최소 HEADER_SIZE(22) bytes 필요.
     * @return       성공 시 true. 버퍼 크기 부족 또는 Archive 오류 시 false.
     */
    bool DeserializeFromBuffer(const TArray<uint8>& Bytes)
    {
        if (Bytes.Num() < HEADER_SIZE)
        {
            return false;
        }
        FMemoryReader Reader(Bytes, /*bIsPersistent=*/true);
        Serialize(Reader);
        return !Reader.IsError();
    }

    /**
     * 헤더를 바이트 버퍼 앞에 직렬화하여 추가.
     *
     * OutBytes에 HEADER_SIZE(22) bytes를 append.
     * 기존 내용이 있으면 뒤에 추가된다 — 필요 시 호출 전 Reset.
     *
     * @param OutBytes  출력 버퍼. 헤더 22 bytes가 추가됨.
     */
    void SerializeToBuffer(TArray<uint8>& OutBytes) const
    {
        FMemoryWriter Writer(OutBytes, /*bIsPersistent=*/true);
        // FArchive::operator<< 는 non-const 참조를 요구하므로 const_cast 사용.
        // FMemoryWriter IsSaving()=true 이므로 실제로는 쓰기 경로만 실행.
        const_cast<FMossSaveHeader*>(this)->Serialize(Writer);
    }
};
