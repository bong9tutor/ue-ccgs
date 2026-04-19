# Story 007: GetLastCardDay() int32 Sentinel Pull API

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 1시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §CR-7 `FGrowthState.LastCardDay` + §Interactions §3 MBC
- **TR-ID**: TR-growth-003 (GetLastCardDay API shape)
- **Governing ADR**: [ADR-0012](../../../docs/architecture/adr-0012-growth-getlastcardday-api.md) — `int32 GetLastCardDay() const`, sentinel 0 = 미건넴
- **Engine Risk**: LOW
- **Engine Notes**: FreshStart `FGrowthState` 기본값 → `LastCardDay = 0` → 자동 "미건넴" 상태.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — "Growth `int32 GetLastCardDay() const` API: return 0 = 미건넴 (sentinel), > 0 = 해당 day"
  - Feature Layer §Forbidden Approaches — "Never use `TOptional<int32>` or `bool TryGetLastCardDay(int32&)` for `GetLastCardDay()`"

## Acceptance Criteria

1. `int32 UGrowthAccumulationSubsystem::GetLastCardDay() const` public API
2. `return FGrowthState.LastCardDay;` — sentinel 0 = 미건넴 (헤더 주석 명시)
3. FreshStart 상태: `GetLastCardDay() == 0`
4. 카드 건넴 후: `GetLastCardDay() == CurrentDayIndex`
5. Consumer 호환성: Card `bHandOffered = (GetLastCardDay() == CurrentDayIndex)`, MBC `DayGap = FMath::Max(0, CurrentDayIndex - GetLastCardDay())` — EC-11 가드
6. **ADR-0012 Forbidden 준수**: `TOptional<int32>` 반환 또는 `bool TryGetLastCardDay(int32& OutDay)` 시그니처 grep = 0건

## Implementation Notes

```cpp
/**
 * @brief 마지막 카드 건넴 일차를 반환한다.
 * @return 0 = 미건넴 sentinel (FreshStart 또는 재시작 후 당일 미건넴).
 *         > 0 = 해당 DayIndex에 카드 건넴 완료.
 * @see ADR-0012 int32 sentinel 0 채택 근거
 */
int32 UGrowthAccumulationSubsystem::GetLastCardDay() const {
    return FGrowthState.LastCardDay;
}
```

- **Sentinel `0` 정당화** (ADR-0012): `DayIndex` 범위 `[1, 21]` → `0`은 유효값 제외
- MBC Formula 2 EC-11 호환: `DayGap = max(0, DayIndex - GetLastCardDay())` — BACKWARD_GAP으로 `LastCardDay > CurrentDayIndex` 시에도 음수 방어

## Out of Scope

- MBC DryingFactor 계산 구현 (core-moss-baby-character)
- Card `bHandOffered` 복원 구현 (feature-card-system Story)

## QA Test Cases

**Given** FreshStart `FGrowthState`, **When** `GetLastCardDay()` 호출, **Then** 반환값 0 (ADR-0012 §Validation).

**Given** Day 3에 카드 건넴 완료, **When** `GetLastCardDay()` 호출, **Then** 반환값 3.

**Given** BACKWARD_GAP으로 LastCardDay=5, CurrentDayIndex=3, **When** MBC `DayGap = max(0, 3-5)` 계산, **Then** DayGap = 0 (EC-11 호환).

**Given** Growth 구현 소스, **When** `grep "TOptional<int32> GetLastCardDay\|TryGetLastCardDay" Source/MadeByClaudeCode/Growth/`, **Then** 매치 0건.

## Test Evidence

- **Unit test**: `tests/unit/Growth/test_getlastcardday_sentinel.cpp`
- **CODE_REVIEW grep**: Forbidden signatures 0건

## Dependencies

- Story 001 (Subsystem 골격 + FGrowthState struct)
- Story 002 (CR-1 — LastCardDay writer)
