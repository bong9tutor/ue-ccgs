# Story 002: FSessionRecord 구조체 + double precision runtime round-trip

> **Epic**: Time & Session System
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/time-session-system.md`
**Requirement**: `TR-time-003` (IMossClockSource contract의 타입 정의 부분)
**ADR Governing Implementation**: ADR-0001 (Forward Time Tamper — `FSessionRecord` 직렬화는 변경된 wall clock 값을 단순히 기록)
**ADR Decision Summary**: `FSessionRecord`는 Time이 field를 정의, Save/Load가 직렬화 소유. `LastMonotonicSec`은 **`double` 필수** (float 금지) — 21일 + 소수점 정밀도 유지.
**Engine**: UE 5.6 | **Risk**: LOW
**Engine Notes**: `USTRUCT(BlueprintType)` + `UPROPERTY()` 필수 — GC 안전성 (coding-standards §USaveGame). `FGuid` SessionUuid은 `FGuid::NewGuid()` 런타임 생성.

**Control Manifest Rules (Foundation layer)**:
- Required: Global Rules Naming Convention "Struct types = F prefix + PascalCase" — `FSessionRecord`
- Required: `static_assert(std::is_same_v<decltype(FSessionRecord::LastMonotonicSec), double>)` CODE_REVIEW AC
- Forbidden: `float LastMonotonicSec` 타입 선언 — grep 매치 0건

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] SESSION_RECORD_DOUBLE_TYPE_DECLARATION: `LastMonotonicSec` 필드 `double` 타입으로 선언. `static_assert(std::is_same_v<decltype(FSessionRecord::LastMonotonicSec), double>)` 컴파일 통과. `float LastMonotonicSec` grep 매치 0건 (CODE_REVIEW)
- [ ] SESSION_RECORD_DOUBLE_PRECISION_RUNTIME: `LastMonotonicSec = 1814400.123` 대입 → USaveGame 기반 직렬화 → 역직렬화 round-trip 결과가 원본과 ≤ 0.001초 오차 (AUTOMATED)

---

## Implementation Notes

*Derived from GDD §Formula 2 precision note + §Dependencies #3:*

```cpp
// Source/MadeByClaudeCode/Time/SessionRecord.h
USTRUCT(BlueprintType)
struct FSessionRecord {
    GENERATED_BODY()

    UPROPERTY() FGuid SessionUuid;
    UPROPERTY() int32 DayIndex = 1;              // [1, 21] clamp range
    UPROPERTY() FDateTime LastWallUtc;           // FDateTime 내부 int64 ticks
    UPROPERTY() double LastMonotonicSec = 0.0;   // double 필수 — Formula 2 precision
    UPROPERTY() int32 NarrativeCount = 0;        // [0, NarrativeCap] clamp
    UPROPERTY() int32 SessionCountToday = 0;     // DayIndex 변화 시 리셋 (Dep #4)
    UPROPERTY() FString LastSaveReason;          // ESaveReason enum name string (Save/Load contract)
};

// 컴파일 타임 검증 (CODE_REVIEW AC 강제)
static_assert(std::is_same_v<decltype(FSessionRecord::LastMonotonicSec), double>,
              "monotonic field must be double — Formula 2 precision contract");
```

- 파일: `Source/MadeByClaudeCode/Time/SessionRecord.h`
- 주석: "DO NOT change `double` to `float` — see GDD Formula 2 precision note"
- Save/Load가 실제 직렬화 소유 (Foundation `foundation-save-load` epic story-003에서 구체 구현)
- `LastSaveReason`은 `ESaveReason` enum의 이름 문자열 저장 (Save/Load GDD §ESaveReason enum 6 contract)

---

## Out of Scope

- Story 001: `IMossClockSource` (선행)
- Story 003: 이 struct를 소비하는 Subsystem 뼈대

---

## QA Test Cases

**For Logic story:**
- **SESSION_RECORD_DOUBLE_TYPE_DECLARATION**
  - Given: `FSessionRecord` struct 정의 소스
  - When: 정적 분석
  - Then: `static_assert` 컴파일 통과 + `grep "float LastMonotonicSec" Source/` 0 매치
  - Edge cases: `decltype` expression 다른 타입(`long double`, `int64`) 바꿔 시도 → 컴파일 실패 확인
- **SESSION_RECORD_DOUBLE_PRECISION_RUNTIME**
  - Given: `FSessionRecord` 인스턴스, `LastMonotonicSec = 1814400.123` (21일 + 0.123초)
  - When: USaveGame 기반 serialize → deserialize round-trip
  - Then: `TestNearlyEqual(loaded.LastMonotonicSec, 1814400.123, 0.001)` 통과
  - Edge cases: `1e-9` 극소값과 `1e9` 극대값 round-trip

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/time-session/session_record_test.cpp` (UE Automation Framework — `TestNearlyEqual` 매크로 + `static_assert` 컴파일 smoke)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (IMossClockSource — 타입 의존 없으나 동일 모듈 내 배치)
- Unlocks: Story 003 (Subsystem이 `FSessionRecord`를 멤버로 보유)
