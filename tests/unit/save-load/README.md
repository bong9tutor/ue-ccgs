# Save/Load Persistence Unit Tests

이 폴더는 `foundation-save-load` epic의 자동화 테스트 추적 문서를 보관합니다.
실제 테스트 코드는 UE Automation Framework 구조에 따라
`MadeByClaudeCode/Source/MadeByClaudeCode/Tests/` 에 위치합니다.

---

## Story 1-7: UMossSaveData + ESaveReason + UMossSaveLoadSettings

**구현 파일**

| 역할 | 경로 |
|------|------|
| 테스트 구현 | `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/MossSaveDataTests.cpp` |
| 데이터 스키마 | `MadeByClaudeCode/Source/MadeByClaudeCode/SaveLoad/MossSaveData.h` |
| 세팅 knobs | `MadeByClaudeCode/Source/MadeByClaudeCode/Settings/MossSaveLoadSettings.h` |

**테스트 카테고리**: `MossBaby.SaveLoad.Schema.*`

### 테스트 함수 목록

| 함수명 | 카테고리 | AC |
|--------|----------|----|
| `FMossSaveDataDefaultsTest` | `MossBaby.SaveLoad.Schema.Defaults` | AC-1 |
| `FMossSaveDataCurrentSchemaVersionConstTest` | `MossBaby.SaveLoad.Schema.CurrentSchemaVersionConst` | AC-3 |
| `FESaveReasonStableIntegersTest` | `MossBaby.SaveLoad.Schema.ESaveReasonStableIntegers` | AC-2 |
| `FESaveReasonNameTest` | `MossBaby.SaveLoad.Schema.ESaveReasonName` | AC-2 edge case |
| `FMossSaveLoadSettingsCDOTest` | `MossBaby.SaveLoad.Schema.SaveLoadSettingsCDO` | AC-4 |

### AC 매핑

| AC | 설명 | 검증 방식 | 테스트 함수 |
|----|------|-----------|-------------|
| AC-1 | `UMossSaveData : public USaveGame` — SaveSchemaVersion=1, WriteSeqNumber=0, LastSaveReason 빈 문자열, bSaveDegraded=false | AUTOMATED | `FMossSaveDataDefaultsTest` |
| AC-2 | `ESaveReason` 4값 stable integer (ECardOffered=0 ~ EFarewellReached=3) + 이름 문자열 round-trip | AUTOMATED | `FESaveReasonStableIntegersTest`, `FESaveReasonNameTest` |
| AC-3 | `CURRENT_SCHEMA_VERSION` 빌드 시간 상수 = 1 | AUTOMATED | `FMossSaveDataCurrentSchemaVersionConstTest` |
| AC-4 | `UMossSaveLoadSettings` 5 knob 기본값 검증 | AUTOMATED | `FMossSaveLoadSettingsCDOTest` |
| AC-5 | `FGrowthState`, `FDreamState` placeholder USTRUCT — `GENERATED_BODY()` 포함, 컴파일 통과 | CODE_REVIEW | `MossSaveData.h` 컴파일 확인 |

### 실행 명령 (headless)

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.SaveLoad.Schema.; Quit" \
  -nullrhi -nosound -log -unattended
```

전체 SaveLoad epic 테스트:

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.SaveLoad.; Quit" \
  -nullrhi -nosound -log -unattended
```

---

---

## Story 1-8 — Save/Load Subsystem lifecycle + coalesce

**구현 파일**

| 역할 | 경로 |
|------|------|
| 테스트 구현 | `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/MossSaveLoadSubsystemTests.cpp` |
| 서브시스템 헤더 | `MadeByClaudeCode/Source/MadeByClaudeCode/SaveLoad/MossSaveLoadSubsystem.h` |
| 서브시스템 구현 | `MadeByClaudeCode/Source/MadeByClaudeCode/SaveLoad/MossSaveLoadSubsystem.cpp` |

**테스트 카테고리**: `MossBaby.SaveLoad.Subsystem.*`

### 테스트 함수 목록

| 함수명 | 카테고리 | AC |
|--------|----------|----|
| `FMossSaveLoadInitialStateTest` | `MossBaby.SaveLoad.Subsystem.InitialState` | 뼈대 + ESaveLoadState |
| `FMossSaveLoadSaveAsyncIdleToSavingTest` | `MossBaby.SaveLoad.Subsystem.SaveAsyncIdleToSaving` | SaveAsync API |
| `FMossSaveLoadLoadingStateDropTest` | `MossBaby.SaveLoad.Subsystem.LoadingStateDrop` | AC E19 |
| `FMossSaveLoadMigratingCoalesceTest` | `MossBaby.SaveLoad.Subsystem.MigratingCoalesce` | AC E20 |
| `FMossSaveLoadHundredXCoalesceTest` | `MossBaby.SaveLoad.Subsystem.HundredXCoalesce` | AC E22 (뼈대 레벨) |
| `FMossSaveLoadPendingConsumeOnIdleTest` | `MossBaby.SaveLoad.Subsystem.PendingConsumeOnIdle` | Coalesce 큐 소비 |
| `FMossSaveLoadDeinitFlushReentryTest` | `MossBaby.SaveLoad.Subsystem.DeinitFlushReentry` | Deinit 재진입 차단 |

### AC 매핑

| AC | 설명 | 검증 방식 | 테스트 함수 |
|----|------|-----------|-------------|
| 뼈대 + ESaveLoadState | `UMossSaveLoadSubsystem : UGameInstanceSubsystem`, 5-state enum, FOnLoadComplete delegate | AUTOMATED | `FMossSaveLoadInitialStateTest` |
| SaveAsync(ESaveReason) API | Idle→Saving→Idle 전이 + IOCommitCount 증가 | AUTOMATED | `FMossSaveLoadSaveAsyncIdleToSavingTest` |
| FlushOnly (T2) | Idle 시 SaveAsync 1회 트리거, bDeinitFlushInProgress 변경 안 함 | CODE_REVIEW + Integration | 소스 grep + TD-005 |
| FlushAndDeinit (T1/T3/T4) + 재진입 차단 | FThreadSafeBool 첫 호출 set, 이후 재진입 차단 | AUTOMATED | `FMossSaveLoadDeinitFlushReentryTest` |
| T2 activation delegate | FSlateApplication::IsInitialized() 가드 + FDelegateHandle Remove | CODE_REVIEW | 소스 grep |
| T3 CoreDelegates::OnExit | FDelegateHandle 정확한 Remove (RemoveAll 대신) | CODE_REVIEW | 소스 grep |
| T4 Deinitialize flush | bDeinitFlushInProgress 미설정 시 FlushAndDeinit 호출 | AUTOMATED | `FMossSaveLoadDeinitFlushReentryTest` |
| Coalesce 큐 최대 1 | 최신 reason 승리, 최대 1개 유지 | AUTOMATED | `FMossSaveLoadHundredXCoalesceTest` |
| AC E19 Loading drop | Loading 상태 SaveAsync → silent drop + 진단 로그 "drop" | AUTOMATED | `FMossSaveLoadLoadingStateDropTest` |
| AC E20 Migrating coalesce | Migrating 3회 → coalesce → Idle 후 1회 commit | AUTOMATED | `FMossSaveLoadMigratingCoalesceTest` |
| AC E22 100X (뼈대 레벨) | Saving 중 100회 → coalesce 큐 최대 1 증명 | AUTOMATED | `FMossSaveLoadHundredXCoalesceTest` |
| ADR-0009 준수 | sequence-level atomicity API 없음 — GSM 책임 | CODE_REVIEW | 소스 grep (API 부재 확인) |

### Out of Scope

- **T1 viewport close 실제 World* 바인딩**: Story 1-20 또는 게임 코드 (UMossBabyGameInstance::Init)
- **AC E22 실제 I/O ≤ 2회 증명**: Story 1-10 TFuture 실제 구현 이후 (뼈대에서는 coalesce 큐 최대 1만 증명)
- **AC E23 Deinit timeout 실측**: Story 1-10 TFuture WaitFor 구현 이후 (뼈대에서는 timeout 로그 스텁만)
- **Integration test**: 4-trigger 실제 Windows lifecycle → TD-005 lifecycle story 포함
- **Story 1-9**: Header block + CRC32 + dual-slot Load
- **Story 1-10**: FMossSaveSnapshot POD + Atomic write

### 실행 명령 (headless)

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.SaveLoad.Subsystem.; Quit" \
  -nullrhi -nosound -log -unattended
```

전체 SaveLoad epic 테스트:

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.SaveLoad.; Quit" \
  -nullrhi -nosound -log -unattended
```

---

## 미구현 스토리 (Out of Scope)

| Story | 내용 | 상태 |
|-------|------|------|
| Story 1-9 | Header block struct + CRC32 + Formula 1-3 | 미시작 |
| Story 1-10 | Atomic write + dual-slot | 미시작 |

---

*최종 업데이트: 2026-04-20 — Story 1-8 완료*
