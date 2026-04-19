# Time & Session — Unit Test Evidence

**Story**: Story-001 (IMossClockSource 인터페이스 + FRealClockSource + FMockClockSource)
**Epic**: Foundation Time & Session
**ADR**: ADR-0001 (Forward Time Tamper 명시적 수용 정책)

## 실제 테스트 파일 위치

UE Automation 테스트는 UE 모듈 빌드 시스템 상 UE 모듈 내부에 위치해야 한다.
이 README는 CCGS evidence 인덱스 역할을 한다.

실제 파일:
`MadeByClaudeCode/Source/MadeByClaudeCode/Tests/ClockSourceTests.cpp`

## Story-001 AC 매핑

| AC | 내용 | 검증 유형 | 테스트 함수 |
|---|---|---|---|
| AC-001 | `IMossClockSource` 4 virtual methods 정의 | CODE_REVIEW | 헤더 컴파일 성공 + grep 패턴 0 매치 |
| AC-002 | `FRealClockSource` 구현 (`FDateTime::UtcNow`, `FPlatformTime::Seconds` 래핑) | AUTOMATED | `FClockSourceRealUtcDiffNonNegativeTest`, `FClockSourceRealMonoMonotonicTest` |
| AC-003 | `FMockClockSource` setter/simulate 구현 | AUTOMATED | `FClockSourceMockSetGetUtcTest`, `FClockSourceMockSetGetMonoTest`, `FClockSourceMockSuspendResumeTest` |

## 테스트 함수 목록 (5개)

| 함수 | 카테고리 | 검증 내용 |
|---|---|---|
| `FClockSourceMockSetGetUtcTest` | `MossBaby.Time.ClockSource.MockSetGetUtc` | SetUtcNow / GetUtcNow bit-exact |
| `FClockSourceMockSetGetMonoTest` | `MossBaby.Time.ClockSource.MockSetGetMono` | SetMonotonicSec / GetMonotonicSec bit-exact |
| `FClockSourceMockSuspendResumeTest` | `MossBaby.Time.ClockSource.MockSuspendResume` | SimulateSuspend + SimulateResume 예외 없음 |
| `FClockSourceRealUtcDiffNonNegativeTest` | `MossBaby.Time.ClockSource.RealUtcDiffNonNegative` | Real GetUtcNow 2회 차이 ≥ 0 |
| `FClockSourceRealMonoMonotonicTest` | `MossBaby.Time.ClockSource.RealMonoMonotonic` | Real GetMonotonicSec 후속 ≥ 이전 |

## 실행 방법

### 에디터 (개발 중)

1. UnrealEditor 실행 → `Tools` → `Session Frontend` → `Automation` 탭
2. `MossBaby.Time.ClockSource` 카테고리 expand
3. `Start Tests` → 결과 확인

### 헤드리스 (-nullrhi)

```bash
"C:/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "MadeByClaudeCode/MadeByClaudeCode.uproject" \
  -ExecCmds="Automation RunTests MossBaby.Time.ClockSource.; Quit" \
  -nullrhi -nosound -log -unattended
```

---

## Story 1-2 — FSessionRecord

**Story**: Story-002 (FSessionRecord USTRUCT + double precision runtime round-trip)
**Epic**: Foundation Time & Session
**ADR**: ADR-0001 (Forward Time Tamper 명시적 수용 정책)

### 실제 테스트 파일 위치

실제 파일:
`MadeByClaudeCode/Source/MadeByClaudeCode/Tests/SessionRecordTests.cpp`

구조체 정의:
`MadeByClaudeCode/Source/MadeByClaudeCode/Time/SessionRecord.h`

### AC 매핑

| AC | 내용 | 검증 유형 | 테스트 함수 |
|---|---|---|---|
| AC-1 (SESSION_RECORD_DOUBLE_TYPE_DECLARATION) | `LastMonotonicSec` `double` 선언 + `static_assert` 컴파일 통과 | CODE_REVIEW | `SessionRecord.h:static_assert` 컴파일 성공 + `grep "float LastMonotonicSec"` 0건 |
| AC-2 (SESSION_RECORD_DOUBLE_PRECISION_RUNTIME) | `1814400.123` round-trip ≤0.001초 오차 | AUTOMATED | `FSessionRecordDoublePrecisionRoundTripTest` |

### 테스트 함수 목록 (4개)

| 함수 | 카테고리 | 검증 내용 |
|---|---|---|
| `FSessionRecordAcOneCodeReviewNoteTest` | `MossBaby.Time.SessionRecord.AcOneCodeReviewNote` | AC-1 CODE_REVIEW 문서화 (static_assert 컴파일 통과 증명) |
| `FSessionRecordDefaultInitTest` | `MossBaby.Time.SessionRecord.DefaultInit` | 기본값 검증 (DayIndex=1, LastMonotonicSec=0.0 등) |
| `FSessionRecordDoublePrecisionRoundTripTest` | `MossBaby.Time.SessionRecord.DoublePrecisionRoundTrip` | AC-2 핵심: `1814400.123` round-trip ≤0.001 오차 |
| `FSessionRecordExtremePrecisionTest` | `MossBaby.Time.SessionRecord.ExtremePrecision` | AC-2 edge case: `1e-9` 극소값 + `1e9` 극대값 round-trip |

### Round-trip 방식

`FMemoryWriter` + `FMemoryReader` + `FObjectAndNameAsStringProxyArchive` + `UScriptStruct::SerializeItem`.
`UMossSaveData` / Save-Load Subsystem 미사용 (Story 1-7 이후 구현 — Out of Scope).

### 실행 방법

#### 에디터 (개발 중)

1. UnrealEditor 실행 → `Tools` → `Session Frontend` → `Automation` 탭
2. `MossBaby.Time.SessionRecord` 카테고리 expand
3. `Start Tests` → 결과 확인

#### 헤드리스 (-nullrhi)

```bash
"C:/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "MadeByClaudeCode/MadeByClaudeCode.uproject" \
  -ExecCmds="Automation RunTests MossBaby.Time.SessionRecord.; Quit" \
  -nullrhi -nosound -log -unattended
```

---

## Story 1-3 — Subsystem Skeleton + Enums + Settings

**Story**: Story-003 (UMossTimeSessionSubsystem 뼈대 + enum 타입 정의 + Developer Settings)
**Epic**: Foundation Time & Session
**ADR**: ADR-0001 (Forward Time Tamper 명시적 수용 정책), ADR-0011 (Tuning Knob UDeveloperSettings 채택)

### 실제 파일 위치

- 테스트 파일: `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/TimeSessionSubsystemSkeletonTests.cpp`
- Subsystem 선언: `MadeByClaudeCode/Source/MadeByClaudeCode/Time/MossTimeSessionSubsystem.h`
- Subsystem 구현: `MadeByClaudeCode/Source/MadeByClaudeCode/Time/MossTimeSessionSubsystem.cpp`
- Enum 정의: `MadeByClaudeCode/Source/MadeByClaudeCode/Time/MossTimeSessionTypes.h`
- Settings: `MadeByClaudeCode/Source/MadeByClaudeCode/Settings/MossTimeSessionSettings.h`

### Settings CDO 기본값

| Knob | 기본값 | 카테고리 |
|---|---|---|
| GameDurationDays | 21 | Day Structure |
| DefaultEpsilonSec | 5.0f | Classification |
| InSessionToleranceMinutes | 90 | Classification |
| NarrativeThresholdHours | 24 | Narrative Cap |
| NarrativeCapPerPlaythrough | 3 | Narrative Cap |
| SuspendMonotonicThresholdSec | 5.0f | Suspend Detection |
| SuspendWallThresholdSec | 60.0f | Suspend Detection |

### AC 매핑

| AC | 내용 | 검증 유형 | 테스트 함수 |
|---|---|---|---|
| AC-1 Subsystem 뼈대 | Initialize/Deinitialize override, FSessionRecord 멤버, clock 주입 API | CODE_REVIEW + AUTOMATED | `FTimeSessionSubsystemDelegateBindTest`, `FTimeSessionSubsystemClockSourceInjectionTest`, `FTimeSessionSubsystemStubClassifyTest` |
| AC-2 ETimeAction enum | 6 값 정의 | CODE_REVIEW | 헤더 컴파일 성공 (이 파일 컴파일 = 증명) |
| AC-3 EFarewellReason enum | 2 값 정의 | CODE_REVIEW | 헤더 컴파일 성공 |
| AC-4 EBetweenSessionClass + EInSessionClass | 각 5·4 값 정의 | CODE_REVIEW | 헤더 컴파일 성공 |
| AC-5 Settings 7 knobs | CDO 기본값 전체 검증 | AUTOMATED | `FTimeSessionSettingsCDODefaultsTest` |
| AC-6 3 Delegates | FOnTimeActionResolved, FOnDayAdvanced, FOnFarewellReached 바인딩 + Broadcast | AUTOMATED | `FTimeSessionSubsystemDelegateBindTest` |

### 테스트 함수 목록 (4개)

| 함수 | 카테고리 | 검증 내용 |
|---|---|---|
| `FTimeSessionSubsystemDelegateBindTest` | `MossBaby.Time.Subsystem.DelegateBind` | AC-1/AC-6: 3 delegates 바인딩 + Broadcast 람다 호출 확인 |
| `FTimeSessionSubsystemClockSourceInjectionTest` | `MossBaby.Time.Subsystem.ClockSourceInjection` | AC-1: SetClockSource(FMockClockSource) 후 ClassifyOnStartup(nullptr) → START_DAY_ONE |
| `FTimeSessionSettingsCDODefaultsTest` | `MossBaby.Time.Subsystem.SettingsCDODefaults` | AC-5: 7 knob CDO 기본값 전체 검증 |
| `FTimeSessionSubsystemStubClassifyTest` | `MossBaby.Time.Subsystem.StubClassify` | AC-1: ClassifyOnStartup 스텁 동작 (nullptr → START_DAY_ONE, 유효 레코드 → HOLD_LAST_TIME) |

### Out of Scope

- **Initialize/Deinitialize 실제 생명주기 테스트**: `FSubsystemCollectionBase` 직접 생성 불가 (UE 내부 구조). Game Instance가 살아있는 integration test 또는 functional test에서 수행해야 함 — 본 Story 범위 밖.
- Story 004: 8-Rule Classifier 실제 로직 (Between-session Rule 1~4)
- Story 005: In-session Rule 5~8 + 1Hz tick
- Story 1-7: SaveSubsystem 연동 (TriggerSaveForNarrative 실구현)

### 실행 방법

#### 에디터 (개발 중)

1. UnrealEditor 실행 → `Tools` → `Session Frontend` → `Automation` 탭
2. `MossBaby.Time.Subsystem` 카테고리 expand
3. `Start Tests` → 결과 확인

#### 헤드리스 (-nullrhi)

```bash
"C:/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "MadeByClaudeCode/MadeByClaudeCode.uproject" \
  -ExecCmds="Automation RunTests MossBaby.Time.Subsystem.; Quit" \
  -nullrhi -nosound -log -unattended
```

---

## Story 1-14 — Between-session Classifier (Rules 1-4 + Formula 4/5)

- 실제 cpp: `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/TimeSessionClassifierTests.cpp`
- 카테고리: `MossBaby.Time.Classifier.*`
- AC 12개 → 12개 AUTOMATED 테스트 1:1 매핑
- ADR-0001 엄수: 금지 패턴 grep 0 매치, WallDelta > expected_max 분기 없음, Rule 4 silent 수용
- Formula 4: FMath::FloorToInt32 strict floor
- Formula 5: strict `>` (24h 정각 시 ADVANCE_SILENT, 24h+1s 시 ADVANCE_WITH_NARRATIVE)
- Clock null guard: SetClockSource 미호출 시 HOLD_LAST_TIME fallback

---

## Story 1-18 — In-session 1Hz Tick + Rules 5-8 + FTSTicker

- 실제 cpp: `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/TimeSessionTickRulesTests.cpp`
- 카테고리: `MossBaby.Time.Tick.*`
- AC 9개 중 7 AUTOMATED + 2 integration deferred (TD-012)

### AC 매핑

| AC | 테스트 함수 | 검증 내용 |
|---|---|---|
| MONOTONIC_DELTA_CLAMP_ZERO | `FTimeTickMonoDeltaClampZeroTest` | Formula 2: MonoNow=0.5 < LastMono=600.0 → MonoDelta=0 + 크래시 없음 |
| NORMAL_TICK_IN_SESSION | `FTimeTickNormalTickInSessionTest` | Rule 5: WallDelta=1s, MonoDelta=1s → ADVANCE_SILENT + DayIndex 불변 |
| MINOR_SHIFT_NTP | `FTimeTickMinorShiftNtpTest` | Rule 7: DiscrepancyAbs=15s (5<15≤5400) → ADVANCE_SILENT |
| MINOR_SHIFT_DST | `FTimeTickMinorShiftDstTest` | Rule 7: DiscrepancyAbs=3600s (1h, ≤5400) → ADVANCE_SILENT |
| IN_SESSION_DISCREPANCY_LOG_ONLY | `FTimeTickInSessionDiscrepancyLogOnlyTest` | Rule 8: DiscrepancyAbs=17880s >5400 → LOG_ONLY + DayIndex/LastWall/LastMono 불변 |
| RULE_PRECEDENCE_FIRST_MATCH_WINS | `FTimeTickRulePrecedenceFirstMatchWinsTest` | Rule 6 first-match-wins: MonoDelta=0 + WallDelta=7200s → ADVANCE_SILENT (not LOG_ONLY) |
| TICK_HANDLE_NOT_REGISTERED_BEFORE_INIT | `FTimeTickHandleNotRegisteredBeforeInitTest` | Initialize 미호출 → TickHandle.IsValid()=false + 이중 UnregisterTick 크래시 없음 |
| SESSION_COUNT_TODAY_RESET_CONTRACT | `FTimeTickSessionCountTodayResetContractTest` | Rule 6 + AdvanceDayIfNeeded: WallDelta=25h → DayIndex=6 + SessionCountToday=0 + OnDayAdvanced(6) |

### Deferred (TD-012 — integration 분리)

| AC | 이유 | 분리 위치 |
|---|---|---|
| TICK_CONTINUES_WHEN_PAUSED | SetGamePaused(true) + 실제 GameWorld + 5초 대기 필요 | `tests/integration/time-session/` |
| FOCUS_LOSS_CONTINUES_TICK | WM_ACTIVATE LOSS 시나리오 + OS 이벤트 주입 필요 | `tests/integration/time-session/` |

### 구현 세부

- FTSTicker engine-level: SetGamePaused/focus loss 영향 없음 (GDD §Window Focus vs OS Sleep 표)
- FTimerManager 금지: pause 시 중단됨 (ADR 요구사항 TR-time-004)
- Formula 2: `MonoDelta = FMath::Max(0.0, MonoNow - LastMonotonicSec)` — strict clamp
- Formula 3: `DaysElapsed = FMath::FloorToInt32(WallDeltaSec / 86400.0)`
- Rule 6 first-match-wins: 코드 순서상 Rule 8 분기보다 앞에 위치
- ADR-0001 엄수: tamper/bIsForward/DetectClockSkew 패턴 미사용, WallDelta > expected_max 분기 없음

### 실행 방법 (헤드리스)

```bash
"C:/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "MadeByClaudeCode/MadeByClaudeCode.uproject" \
  -ExecCmds="Automation RunTests MossBaby.Time.Tick.; Quit" \
  -nullrhi -nosound -log -unattended
```
