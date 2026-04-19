# Story 004: FMossSaveSnapshot POD + Atomic write (Step 2-10) + Ping-pong rename + E4 temp crash recovery

> **Epic**: Save/Load Persistence
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/save-load-persistence.md`
**Requirement**: Atomic write via dual-slot ping-pong
**ADR Governing Implementation**: None (GDD contract)
**ADR Decision Summary**: `FMossSaveSnapshot` POD-only USTRUCT (UObject* 금지) — worker thread GC 안전성. `IPlatformFile::MoveFile` atomic rename (Windows NTFS `MoveFileExW(MOVEFILE_REPLACE_EXISTING)`). Rename 후 `.tmp` cleanup best-effort.
**Engine**: UE 5.6 | **Risk**: LOW (표준 I/O)
**Engine Notes**: `UGameplayStatics::SaveGameToMemory` (memory-only — `SaveGameToSlot` 금지), `FFileHelper::SaveArrayToFile` temp 쓰기, `IPlatformFile::MoveFile` (not `IFileManager::Get().Move()` — atomic 깨짐).

**Control Manifest Rules (Foundation + Cross-cutting)**:
- Forbidden: `NO_SLOT_UTIL_SAVETOFILE` — `UGameplayStatics::SaveGameToSlot` / `LoadGameFromSlot` 호출 금지 (CODE_REVIEW)
- Forbidden: `FMossSaveSnapshot`에 `UObject*`, `TWeakObjectPtr`, `TSharedPtr` 등 GC 경합 타입 금지

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] `FMossSaveSnapshot` POD USTRUCT 정의 — 필드: `FSessionRecord SessionRecord`, `uint8 SaveSchemaVersion`, `FString LastSaveReason`, (GrowthState/DreamState POD flatten — Growth/Dream epic 완료 후 확장). `FromSaveData(const UMossSaveData&)` 정적 factory
- [ ] `ATOMIC_PINGPONG`: A.WSN=42, B.WSN=41 Idle → SaveAsync(ECardOffered) 완료 시 B.sav 갱신 + WSN=43 + A 무변경
- [ ] `ATOMIC_RENAME_NO_TMP_REMAINS`: 저장 완료 직후 `MossData_<target>.tmp` 부재 (rename 또는 cleanup 완료)
- [ ] `E4_TEMP_CRASH_RECOVERY`: `SaveArrayToFile` 직후 process kill → 재시작 시 `.sav`만 평가, `.tmp` 무시. 직전 정상 슬롯 로드 (Test Infrastructure BLOCKING 의존)
- [ ] `NO_SLOT_UTIL_SAVETOFILE` CODE_REVIEW: `grep -r "SaveGameToSlot\|LoadGameFromSlot" Source/MadeByClaudeCode/` = 0 매치
- [ ] POD-only 컴파일 타임 검증: `FMossSaveSnapshot` 헤더에 `UObject.h` 미포함 (`UObject*` 필드 선언 시 빌드 실패)

---

## Implementation Notes

*Derived from GDD §2 저장 절차 + §5 R2 BLOCKER 7:*

```cpp
// Source/MadeByClaudeCode/SaveLoad/MossSaveSnapshot.h
// NO #include "UObject/UObject.h"  — POD-only compile-time guard
USTRUCT()
struct FMossSaveSnapshot {
    GENERATED_BODY()
    UPROPERTY() FSessionRecord SessionRecord;
    UPROPERTY() uint8 SaveSchemaVersion = 0;
    UPROPERTY() FString LastSaveReason;
    // Growth/Dream state는 future flatten (FName IDs, not UObject*)

    static FMossSaveSnapshot FromSaveData(const UMossSaveData& Data) {
        FMossSaveSnapshot Out;
        Out.SessionRecord = Data.SessionRecord;
        Out.SaveSchemaVersion = Data.SaveSchemaVersion;
        Out.LastSaveReason = Data.LastSaveReason;
        return Out;
    }
};

// Source/MadeByClaudeCode/SaveLoad/MossSaveLoadSubsystem.cpp
void UMossSaveLoadSubsystem::SaveAsync(ESaveReason Reason) {
    // State transitions handled in Story 002
    // ... drop/coalesce 체크 후 Idle → Saving 전이

    // Step 1: Snapshot (game thread, ~1ms)
    const FMossSaveSnapshot Snapshot = FMossSaveSnapshot::FromSaveData(*SaveData);
    SaveData->LastSaveReason = UEnum::GetValueAsString(Reason);

    // Step 2: Target slot 결정 — ping-pong (혹은 FreshStart → A)
    const TCHAR TargetSlot = (ActiveSlot == 'A') ? 'B' : 'A';
    if (State == ESaveLoadState::FreshStart) { /* ActiveSlot==⊥ → A 고정 */ }

    // Step 3-5: Worker thread로 위임 — snapshot copy만 사용 (GC 안전)
    InFlightSave = Async(EAsyncExecution::ThreadPool, [this, Snapshot, TargetSlot]() {
        const uint32 NextWSN = ComputeNextWSN();  // Step 3 Formula 2

        // Step 4: Serialize (thread-safe — POD)
        TArray<uint8> Payload;
        // 실제 serialize는 UMossSaveData → SaveGameToMemory (game thread 사전 실행 or snapshot re-materialization)
        // 더 안전: snapshot 자체를 SerializeStruct로 write (UObject 경로 회피)

        // Step 5: Checksum (seed=0)
        const uint32 Crc = FCrc::MemCrc32(Payload.GetData(), Payload.Num(), 0);

        // Step 6: Header build
        FMossSaveHeader Header;
        Header.SaveSchemaVersion = Snapshot.SaveSchemaVersion;
        Header.WriteSeqNumber = NextWSN;
        Header.PayloadByteLength = Payload.Num();
        Header.PayloadCrc32 = Crc;

        // Step 7: Temp write
        TArray<uint8> FullBuffer;
        Header.SerializeToBuffer(FullBuffer);
        FullBuffer.Append(Payload);
        const FString TempPath = GetSlotPath(TargetSlot) + TEXT(".tmp");  // MossData_B.tmp
        if (!FFileHelper::SaveArrayToFile(FullBuffer, *TempPath)) {
            UE_LOG(LogMossSaveLoad, Warning, TEXT("SaveArrayToFile failed"));
            return;  // directly 실패 처리
        }

        // Step 8: Atomic rename (Windows NTFS atomic)
        const FString FinalPath = GetSlotPath(TargetSlot);
        IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
        if (!PF.MoveFile(*FinalPath, *TempPath)) {
            UE_LOG(LogMossSaveLoad, Warning, TEXT("MoveFile failed"));
            return;
        }

        // Step 9: In-memory active pointer (game thread post)
        AsyncTask(ENamedThreads::GameThread, [this, TargetSlot]() {
            ActiveSlot = TargetSlot;
            State = ESaveLoadState::Idle;
            // Step 10: Cleanup 반대 슬롯 .tmp (best-effort)
            const TCHAR OtherSlot = (TargetSlot == 'A') ? 'B' : 'A';
            const FString OtherTmp = GetSlotPath(OtherSlot) + TEXT(".tmp");
            IFileManager::Get().Delete(*OtherTmp, /*RequireExists=*/false);
            // Pending coalesced SaveAsync 처리 (Story 002 coalesce)
            if (PendingSaveReason.IsSet()) {
                const ESaveReason Next = PendingSaveReason.GetValue();
                PendingSaveReason.Reset();
                SaveAsync(Next);
            }
        });
    });
}

FString UMossSaveLoadSubsystem::GetSlotPath(TCHAR Slot) const {
    return FString::Printf(TEXT("%sMossBaby/SaveGames/MossData_%c.sav"),
        *FPlatformProcess::UserSettingsDir(), Slot);
}
```

- **`SaveGameToSlot` 절대 사용 금지** — `SaveGameToMemory` + `FFileHelper::SaveArrayToFile` + `IPlatformFile::MoveFile` 경로 필수 (GDD §Detailed Design Step 7-8)

---

## Out of Scope

- Story 005: Migration chain (Formula 4)
- Story 006: NarrativeCount atomic + compound negative AC
- Story 007: E14 disk full + E15 non-ASCII

---

## QA Test Cases

**For Logic story:**
- **ATOMIC_PINGPONG**
  - Given: A WSN=42, B WSN=41, Idle
  - When: `SaveAsync(ECardOffered)` 완료
  - Then: B.sav 갱신 + 헤더 WSN=43 + A 파일 mtime 불변
  - Edge cases: FreshStart 직후 → 첫 저장 A
- **ATOMIC_RENAME_NO_TMP_REMAINS**
  - Given: 저장 정상 완료
  - When: `IFileManager::FileExists("MossData_B.tmp")`
  - Then: false
- **E4_TEMP_CRASH_RECOVERY (Test Infra 의존)**
  - Given: `.sav` 정상 + `.tmp`만 존재 (crash 시뮬)
  - When: 재시작 `Initialize()`
  - Then: `.sav` 로드 (`.tmp` 무시) + NarrativeCount = kill 직전 commit 값 + `.tmp`는 다음 저장 Step 10에서 정리
- **NO_SLOT_UTIL_SAVETOFILE (CODE_REVIEW)**
  - Setup: 전체 Source
  - Verify: `grep -r "SaveGameToSlot\|LoadGameFromSlot" Source/MadeByClaudeCode/`
  - Pass: 0 매치
- **POD-only guard**
  - Given: `FMossSaveSnapshot` 파일
  - When: 가짜 `UPROPERTY() UObject* CurrentDream;` 추가 시도
  - Then: 헤더에 `UObject.h` 미include → 빌드 실패 확인

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/save-load/atomic_pingpong_test.cpp`
- `tests/unit/save-load/atomic_rename_test.cpp`
- `tests/unit/save-load/temp_crash_recovery_test.cpp` (Test Infra 의존 — BLOCKING if infra ready)
- `.claude/hooks/no-slot-util-grep.sh` (CODE_REVIEW)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 003 (Header + Formula 1-3)
- Unlocks: Story 005, 006
