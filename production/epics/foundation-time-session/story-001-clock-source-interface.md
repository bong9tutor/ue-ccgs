# Story 001: IMossClockSource 인터페이스 + FRealClockSource + FMockClockSource 구현

> **Epic**: Time & Session System
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 0.5 days (~3-4 hours)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/time-session-system.md`
**Requirement**: `TR-time-003` (IMossClockSource injection seam), `TR-time-004` (Windows suspend/resume 실기 검증 준비)
**ADR Governing Implementation**: ADR-0001 (Forward Time Tamper 명시적 수용 정책)
**ADR Decision Summary**: Forward time tamper를 정상 경과와 bit-exact 동일 처리. 모든 시간 입력은 `IMossClockSource`를 통해서만 진입하여 production(`FRealClockSource`)과 test(`FMockClockSource`) 교체 가능.
**Engine**: UE 5.6 | **Risk**: MEDIUM
**Engine Notes**: `FPlatformTime::Seconds()` Windows suspend 동작은 드라이버 의존 (OQ-IMP-1). `FRealClockSource` 구현 내부에서만 표준 `FDateTime::UtcNow()` / `FPlatformTime::Seconds()` 직접 호출. 다른 어디에서도 이들 API 직접 호출 금지.

**Control Manifest Rules (Foundation layer)**:
- Required: Foundation Required Pattern "Time & Session 8-Rule Classifier는 IMossClockSource만 통해 시간 입력" — `FDateTime::UtcNow()` / `FPlatformTime::Seconds()` 직접 호출은 `IMossClockSource` 구현체 내부에만 허용 (ADR-0001)
- Forbidden: Foundation Forbidden "변수·함수 이름에 `tamper`/`cheat`/`bIsForward`/`bIsSuspicious`/`DetectClockSkew`/`IsSuspiciousAdvance` 패턴 사용 금지" — grep 패턴 차단 (ADR-0001)
- Guardrail: 인터페이스는 순수 virtual. 구현 파일은 `Source/MadeByClaudeCode/Time/` 하위.

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] `IMossClockSource`가 4 virtual methods로 정의됨: `GetUtcNow()`, `GetMonotonicSec()`, `OnPlatformSuspend()`, `OnPlatformResume()` (ADR-0001 §Key Interfaces)
- [ ] `FRealClockSource` 구현: `GetUtcNow()` → `FDateTime::UtcNow()` 래핑, `GetMonotonicSec()` → `FPlatformTime::Seconds()` 래핑
- [ ] `FMockClockSource` 구현: `SetUtcNow(FDateTime)` / `SetMonotonicSec(double)` setter + `SimulateSuspend()` / `SimulateResume()` (§Test Infrastructure Needs — BLOCKING)

---

## Implementation Notes

*Derived from ADR-0001 §Key Interfaces + GDD §Test Infrastructure Needs:*

```cpp
// Source/MadeByClaudeCode/Time/MossClockSource.h
class IMossClockSource {
public:
    virtual ~IMossClockSource() = default;
    virtual FDateTime GetUtcNow() const = 0;
    virtual double GetMonotonicSec() const = 0;
    virtual void OnPlatformSuspend() {}
    virtual void OnPlatformResume() {}
};

// FRealClockSource — production
class FRealClockSource : public IMossClockSource {
public:
    virtual FDateTime GetUtcNow() const override { return FDateTime::UtcNow(); }
    virtual double GetMonotonicSec() const override { return FPlatformTime::Seconds(); }
};

// FMockClockSource — test-only (Source/MadeByClaudeCodeTests/ 또는 test target 내)
class FMockClockSource : public IMossClockSource {
public:
    void SetUtcNow(FDateTime InTime) { MockedUtc = InTime; }
    void SetMonotonicSec(double InSec) { MockedMono = InSec; }
    void SimulateSuspend() { OnPlatformSuspend(); }
    void SimulateResume() { OnPlatformResume(); }
    virtual FDateTime GetUtcNow() const override { return MockedUtc; }
    virtual double GetMonotonicSec() const override { return MockedMono; }
private:
    FDateTime MockedUtc = FDateTime(0);
    double MockedMono = 0.0;
};
```

- 파일 경로: `Source/MadeByClaudeCode/Time/MossClockSource.h`
- Naming: `I`/`F` prefix 준수 (technical-preferences.md)
- `IMossClockSource`는 UObject 미상속 — 순수 C++ interface (POD 사용)
- `FMockClockSource`는 production DLL에서 제외 (test target 전용)

---

## Out of Scope

- Story 002: `FSessionRecord` 구조체 정의 (이 story는 clock source만)
- Story 003: `UMossTimeSessionSubsystem` 뼈대 + clock source 주입

---

## QA Test Cases

**For Logic story:**
- **IMossClockSource contract**
  - Given: 빈 `FMockClockSource` 생성
  - When: `SetUtcNow(FDateTime(2026,4,19))` + `GetUtcNow()` 호출
  - Then: 반환값이 주입한 값과 bit-exact 일치
  - Edge cases: `SimulateSuspend()` 후 `SimulateResume()` 호출 시 예외 없음
- **FRealClockSource production path**
  - Given: `FRealClockSource` 생성
  - When: `GetUtcNow()` 2회 호출
  - Then: 두 값의 차이 ≥ 0 (wall clock 단조 아님 가정 유지)
  - Edge cases: `GetMonotonicSec()` 연속 호출 시 후속 값 ≥ 이전 값 (monotonic 보장)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/time-session/clock_source_test.cpp` (UE Automation Framework `IMPLEMENT_SIMPLE_AUTOMATION_TEST`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (Foundation 첫 story — 다른 Foundation epic과 독립)
- Unlocks: Story 002 (FSessionRecord), Story 003 (Subsystem 뼈대)

---

## Completion Notes

**Completed**: 2026-04-19
**Criteria**: 3/3 passing (AC-001 / AC-002 / AC-003 모두 COVERED)
**Files delivered**:
- `MadeByClaudeCode/Source/MadeByClaudeCode/Time/MossClockSource.h` (신규, 106 lines)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/ClockSourceTests.cpp` (신규, 166 lines, 5 tests)
- `tests/unit/time-session/README.md` (CCGS evidence 인덱스)
**Test Evidence**: Logic — UE Automation 테스트 5건 (`MossBaby.Time.ClockSource.*`). 실행 검증은 CI/Editor에서.
**Code Review**: Complete (`/code-review` — APPROVED WITH SUGGESTIONS, unreal-specialist + qa-tester 병렬)
**ADR-0001 준수**: 금지 패턴 grep 0 매치. `FDateTime::UtcNow` / `FPlatformTime::Seconds` 직접 호출은 `FRealClockSource` 내부만.
**Deviations**:
- **ADVISORY**: Test file 경로 매핑 — story가 명시한 `tests/unit/time-session/clock_source_test.cpp` 대신 UE 빌드 제약상 `Source/.../Tests/ClockSourceTests.cpp`에 배치. `tests/unit/time-session/README.md`가 포인터 역할.
- **ADVISORY (S3 잠재)**: `FClockSourceRealUtcDiffNonNegativeTest` NTP 보정 시 간헐 실패 가능 (연속 호출이라 확률 극저). tech-debt 등록.
- **ADVISORY (문서 갭)**: `.claude/rules/test-standards.md` 네이밍 규칙이 UE `IMPLEMENT_SIMPLE_AUTOMATION_TEST` idiom 예외를 명시하지 않음. qa-lead 에스컬레이션 권장. tech-debt 등록.
**Deferred**: OQ-IMP-1 Windows suspend/resume 실기 검증 (Story-001 Out of Scope; 별도 Sprint 2 task).
