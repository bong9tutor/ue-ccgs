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

## 미구현 스토리 (Out of Scope)

| Story | 내용 | 상태 |
|-------|------|------|
| Story 1-8 | USaveLoadSubsystem 뼈대 + 4-state machine | 미시작 |
| Story 1-9 | Header block struct + CRC32 + Formula 1-3 | 미시작 |
| Story 1-10 | Atomic write + dual-slot | 미시작 |

---

*최종 업데이트: 2026-04-19 — Story 1-7 완료*
