# Story 003: Header block + CRC32 + Formula 1-3 (Slot Selection, Validity, WSN) + Dual-slot Load

> **Epic**: Save/Load Persistence
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 0.5 days (~4 hours)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/save-load-persistence.md`
**Requirement**: Slot selection + validity predicate + WSN formula
**ADR Governing Implementation**: None (GDD contract 직접 구현)
**ADR Decision Summary**: 22-byte Header block (Magic 0x4D4F5353 + SchemaVersion + MinCompat + WSN + PayloadLen + CRC32). Formula 1 `TrustedSlot = argmax(WSN ∩ Valid)`, Formula 2 `NextWSN = max(A.WSN, B.WSN) + 1`, Formula 3 6-condition Validity (R3: EngineArchiveCompatible 제거).
**Engine**: UE 5.6 | **Risk**: LOW (FCrc + IFileManager 표준 API)
**Engine Notes**: `FCrc::MemCrc32(OutBytes.GetData(), OutBytes.Num(), 0)` — **seed = 0 고정** (R3 CRITICAL-4). `IFileManager::FileExists` 파일 부재 먼저 평가 (short-circuit).

**Control Manifest Rules (Foundation layer + Cross-cutting)**:
- Required: "Save file 위치: `FPlatformProcess::UserSettingsDir()` 하위"
- Required: "SaveSchemaVersion bump 정책: 필드 추가·삭제·재타입·의미 변경 시마다 +1"

---

## Acceptance Criteria

*From GDD §Acceptance Criteria (8개 로드·형식 AC):*

- [ ] `FMossSaveHeader` struct 정의 — 22 byte layout: `uint32 Magic`, `uint8 SaveSchemaVersion`, `uint8 MinCompatibleSchemaVersion`, `uint32 WriteSeqNumber`, `uint32 PayloadByteLength`, `uint32 PayloadCrc32`
- [ ] `MAGIC_NUMBER = 0x4D4F5353` ('MOSS') 상수
- [ ] `SLOT_SELECT_HIGHEST_WSN`: Slot A WSN=10, Slot B WSN=11 (모두 Valid) → Load 후 trusted = B + B의 payload 반환 (Formula 1)
- [ ] `WSN_FORMULA_MAX_PLUS_ONE`: A.WSN=7, B.WSN=0 (Invalid) → NextWSN = max(7, 0) + 1 = 8 (Formula 2)
- [ ] `CRC32_GENERATED_ON_WRITE`: 저장 후 헤더 `PayloadCrc32`가 payload에 대한 `FCrc::MemCrc32(seed=0)` 재계산값과 일치
- [ ] `VALIDITY_CRC_MISMATCH_FALLBACK`: A payload 1바이트 변조 (CRC 불일치) + B Valid → A Invalid + B로 fallback. 에러 다이얼로그 없음 (Formula 3)
- [ ] `FRESHSTART_FIRST_WRITE_TO_A`: 양 슬롯 부재 → 첫 SaveAsync가 슬롯 A에 WSN=1 기록, 두 번째가 슬롯 B WSN=2 (E1, E2)
- [ ] `FRESH_FIRST_RUN_SIGNALS`: 양 슬롯 부재 → `OnLoadComplete(bFreshStart=true, bHadPreviousData=false)`
- [ ] `FALLBACK_ONE_TIME_LIMIT`: 양 슬롯 Invalid → FreshStart 모드 + `OnLoadComplete(true, true)` + fallback 재귀 카운터 ≤ 1 (E10, E11)
- [ ] `SCHEMA_FUTURE_VERSION_REJECTED`: A.SaveSchemaVersion=255 + B Valid V=1 → A Invalid (V > CURRENT) + B load (E12)
- [ ] `SCHEMA_TOO_OLD_REJECTED`: `MinCompatibleSchemaVersion = 3` + A.V=1 → A Invalid (E13)

---

## Implementation Notes

*Derived from GDD §1 슬롯 아키텍처 + §3 로드 절차 + §Formula 1-3:*

```cpp
// Source/MadeByClaudeCode/SaveLoad/MossSaveHeader.h
struct FMossSaveHeader {
    static constexpr uint32 MAGIC_NUMBER = 0x4D4F5353;  // 'MOSS'
    static constexpr int32 HEADER_SIZE = 22;

    uint32 Magic = MAGIC_NUMBER;
    uint8 SaveSchemaVersion = UMossSaveData::CURRENT_SCHEMA_VERSION;
    uint8 MinCompatibleSchemaVersion = 1;
    uint32 WriteSeqNumber = 0;
    uint32 PayloadByteLength = 0;
    uint32 PayloadCrc32 = 0;

    bool Serialize(FArchive& Ar);  // 명시적 22 byte layout
    bool DeserializeFromBuffer(const TArray<uint8>& Bytes);
};

// MossSaveLoadSubsystem.cpp — Load 절차
void UMossSaveLoadSubsystem::LoadInitial() {
    State = ESaveLoadState::Loading;
    const FString SlotA = GetSlotPath("A");
    const FString SlotB = GetSlotPath("B");

    FMossSaveHeader HeaderA, HeaderB;
    TArray<uint8> PayloadA, PayloadB;
    const bool bValidA = ReadSlot(SlotA, HeaderA, PayloadA);  // Formula 3
    const bool bValidB = ReadSlot(SlotB, HeaderB, PayloadB);

    // Formula 1 — argmax(WSN ∩ Valid)
    const uint32 WsnA = bValidA ? HeaderA.WriteSeqNumber : 0;
    const uint32 WsnB = bValidB ? HeaderB.WriteSeqNumber : 0;

    if (!bValidA && !bValidB) {
        // FreshStart + fallback 재귀 방지 (E10/E11)
        const bool bHadPreviousData = FPaths::FileExists(SlotA) || FPaths::FileExists(SlotB);
        SaveData = NewObject<UMossSaveData>();
        ActiveSlot = 'A';  // 다음 SaveAsync가 A에 WSN=1
        State = ESaveLoadState::FreshStart;
        OnLoadComplete.Broadcast(/*bFreshStart=*/true, bHadPreviousData);
        return;
    }

    const bool bUseA = bValidA && (!bValidB || WsnA >= WsnB);  // Tiebreaker: A
    const TArray<uint8>& TrustedPayload = bUseA ? PayloadA : PayloadB;
    ActiveSlot = bUseA ? 'A' : 'B';

    // Load 절차 Step 6
    UMossSaveData* Loaded = Cast<UMossSaveData>(
        UGameplayStatics::LoadGameFromMemory(TrustedPayload));
    if (!Loaded) {
        // Fallback (1회 한정 — R3 archive mismatch)
        const TArray<uint8>& OtherPayload = bUseA ? PayloadB : PayloadA;
        Loaded = Cast<UMossSaveData>(UGameplayStatics::LoadGameFromMemory(OtherPayload));
        if (!Loaded) {
            // 재귀 차단 — FreshStart (E10/E11)
            SaveData = NewObject<UMossSaveData>();
            State = ESaveLoadState::FreshStart;
            OnLoadComplete.Broadcast(true, true);
            return;
        }
        ActiveSlot = bUseA ? 'B' : 'A';
    }
    SaveData = Loaded;
    State = (SaveData->SaveSchemaVersion < UMossSaveData::CURRENT_SCHEMA_VERSION)
        ? ESaveLoadState::Migrating
        : ESaveLoadState::Idle;
    OnLoadComplete.Broadcast(/*bFreshStart=*/false, true);
}

bool UMossSaveLoadSubsystem::ReadSlot(const FString& Path, FMossSaveHeader& OutHeader,
                                       TArray<uint8>& OutPayload) {
    // Short-circuit order: IOError → Exists → Magic → Len → Schema → CRC
    if (!FPaths::FileExists(Path)) return false;
    TArray<uint8> Buffer;
    if (!FFileHelper::LoadFileToArray(Buffer, *Path)) return false;
    if (Buffer.Num() < FMossSaveHeader::HEADER_SIZE) return false;
    if (!OutHeader.DeserializeFromBuffer(Buffer)) return false;
    if (OutHeader.Magic != FMossSaveHeader::MAGIC_NUMBER) return false;

    const auto* Settings = UMossSaveLoadSettings::Get();
    if (OutHeader.SaveSchemaVersion > UMossSaveData::CURRENT_SCHEMA_VERSION) return false;  // future
    if (OutHeader.SaveSchemaVersion < Settings->MinCompatibleSchemaVersion) return false;  // too old
    if (OutHeader.PayloadByteLength != uint32(Buffer.Num() - FMossSaveHeader::HEADER_SIZE)) return false;

    OutPayload.Reset();
    OutPayload.Append(Buffer.GetData() + FMossSaveHeader::HEADER_SIZE,
                      Buffer.Num() - FMossSaveHeader::HEADER_SIZE);
    const uint32 CalcCrc = FCrc::MemCrc32(OutPayload.GetData(), OutPayload.Num(), 0);  // seed=0
    if (CalcCrc != OutHeader.PayloadCrc32) return false;
    return true;
}

uint32 UMossSaveLoadSubsystem::ComputeNextWSN() const {
    FMossSaveHeader A, B;
    TArray<uint8> _;
    const uint32 WsnA = ReadSlot(GetSlotPath("A"), A, _) ? A.WriteSeqNumber : 0;
    const uint32 WsnB = ReadSlot(GetSlotPath("B"), B, _) ? B.WriteSeqNumber : 0;
    // Wrap detection (R2 BLOCKER 5)
    const int64 Candidate = int64(FMath::Max(WsnA, WsnB)) + 1;
    if (Candidate > UINT32_MAX) {
        TriggerWSNReset();
        return 1;
    }
    return uint32(Candidate);
}
```

- Slot 경로: `FPlatformProcess::UserSettingsDir() / MossBaby/SaveGames/MossData_A.sav`

---

## Out of Scope

- Story 004: Atomic write (Step 2-10) + FMossSaveSnapshot
- Story 005: Migration chain (Formula 4)
- Story 006: NarrativeCount atomic commit + coalesce negative AC

---

## QA Test Cases

**For Logic story:**
- **SLOT_SELECT_HIGHEST_WSN**
  - Given: A WSN=10 Valid, B WSN=11 Valid (서로 다른 NarrativeCount 주입)
  - When: `LoadInitial()`
  - Then: `GetSaveData()->SessionRecord.NarrativeCount` = B의 값
- **CRC32_GENERATED_ON_WRITE**
  - Given: Write 완료
  - When: 파일 헤더 hex 덤프 + 독립 CRC32 재계산
  - Then: 헤더 `PayloadCrc32`와 일치 (seed=0)
- **VALIDITY_CRC_MISMATCH_FALLBACK**
  - Given: A payload 1바이트 = 0xFF 변조, B Valid
  - When: `LoadInitial()`
  - Then: A Invalid → B payload 로드 + 에러 다이얼로그 없음
- **FRESHSTART_FIRST_WRITE_TO_A**
  - Given: 슬롯 파일 완전 부재
  - When: LoadInitial → FreshStart; SaveAsync 1회
  - Then: `MossData_A.sav` 생성 + WSN=1; 2회차 → B WSN=2
- **SCHEMA_FUTURE_VERSION_REJECTED**
  - Given: A.SaveSchemaVersion = 255, B Valid V=1
  - When: LoadInitial (CURRENT=1)
  - Then: A Invalid + B load + `OnLoadComplete(false, _)`
- **SCHEMA_TOO_OLD_REJECTED**
  - Given: `MinCompatibleSchemaVersion = 3`, A.V=1
  - When: LoadInitial
  - Then: A Invalid → 둘 다 too-old → FreshStart + bHadPreviousData=true
- **FALLBACK_ONE_TIME_LIMIT**
  - Given: 양쪽 Invalid (CRC corrupt)
  - When: LoadInitial
  - Then: `OnLoadComplete(true, true)` + fallback 카운터 ≤ 1 (무한 재귀 차단)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/save-load/slot_selection_test.cpp` (Formula 1)
- `tests/unit/save-load/wsn_formula_test.cpp` (Formula 2)
- `tests/unit/save-load/validity_predicate_test.cpp` (Formula 3 - 6 conditions)
- `tests/unit/save-load/crc32_test.cpp` (CRC seed=0)
- `tests/unit/save-load/freshstart_signals_test.cpp`

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (UMossSaveData), Story 002 (Subsystem lifecycle)
- Unlocks: Story 004

---

## Completion Notes

**Completed**: 2026-04-19
**Criteria**: Header/Formula 구현 완료, 실제 파일 I/O AC는 Integration 분리
**Files delivered**:
- `SaveLoad/MossSaveHeader.h` (신규) — 22-byte layout + Serialize/DeserializeFromBuffer/SerializeToBuffer
- `Tests/MossSaveHeaderTests.cpp` (신규, 4 tests)
- `SaveLoad/MossSaveLoadSubsystem.h/.cpp` (수정) — LoadInitial/ReadSlot/ComputeNextWSN/GetSlotPath 추가 + Initialize에서 LoadInitial 호출 + FallbackRecursionCount 추가
- `tests/unit/save-load/README.md` (Story 1-9 섹션 append)

**Test Evidence**: 4 UE Automation — `MossBaby.SaveLoad.Header.{Defaults/SerializeRoundTrip/Size/MagicVerify}`

**AC 커버**:
- [x] FMossSaveHeader 22-byte layout: SerializeRoundTripTest + SizeTest
- [x] MAGIC_NUMBER 0x4D4F5353: MagicVerifyTest
- [x] Formula 1/2/3 구현: LoadInitial/ComputeNextWSN/ReadSlot (subsystem integration test로 분리)
- [x] FallbackRecursionCount ≤ 1: 코드 구현 (integration 검증)

**ADR/Rule 준수**:
- ADR-0001 grep: 0 매치
- FCrc::MemCrc32(seed=0) CRITICAL-4 준수
- Short-circuit validity 순서 준수

**Deferred (Story 1-10 또는 integration)**:
- SLOT_SELECT_HIGHEST_WSN / VALIDITY_CRC_MISMATCH_FALLBACK / FRESHSTART_FIRST_WRITE_TO_A / SCHEMA_FUTURE/TOO_OLD_REJECTED / FALLBACK_ONE_TIME_LIMIT — 실제 `.sav` 파일 생성·변조 필요 → Story 1-10 write 경로 완성 또는 integration story에서 커버
- CRC32_GENERATED_ON_WRITE — Story 1-10 (write side 구현 후)

**Integration test TD-005 lifecycle에 포함**: Initialize→LoadInitial→Subsystem 전체 생명주기 실제 검증
