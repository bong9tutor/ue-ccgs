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
