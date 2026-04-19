# Story 007: Edge Cases (Degraded Pipeline + F3 Frequency Target + Manual Playtest)

> **Epic**: feature-dream-trigger
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 1.5시간

## Context

- **GDD Reference**: `design/gdd/dream-trigger-system.md` §EC-7/EC-8/EC-9 + §F3 FrequencyTarget + AC-DTS-17/19/20
- **TR-ID**: TR-dream-004 (Pillar 3 frequency validation)
- **Governing ADR**: None (GDD contract)
- **Engine Risk**: LOW
- **Engine Notes**: Data Pipeline DegradedFallback 시 `GetAllDreamAssets()` 빈 배열 → 평가 종료. F3는 설계 검증용 — 실행 시간 아닌 테스트 시 검증.

## Acceptance Criteria

1. **AC-DTS-17** (AUTOMATED/BLOCKING) — `GetAllDreamAssets()` 빈 배열 (Pipeline DegradedFallback) → `FOnCardProcessedByGrowth` 수신 시 `FOnDreamTriggered` 미발행, `LogDreamTrigger Warning` 1회, `FDreamState` 불변 (EC-8)
2. **AC-DTS-19** (AUTOMATED/ADVISORY) — Full 기본 파라미터 (MinInterval=4, MaxDreams=5, EarliestDay=2) → F3 `ExpectedDreamCount ∈ [3.0, 5.0]` (목표 3-5회/21일 범위)
3. **AC-DTS-20** (MANUAL/ADVISORY) — MVP 7일 플레이스루 반복 5회, 각 플레이스루에서 꿈 수령 횟수 기록 → 최소 1회 이상 (EC-1 보장), 7일 내 꿈 없는 플레이스루 0회, 일기 화면 열람 후 꿈 텍스트 올바르게 표시
4. EC-7 (카드 미건넴 상태 평가): `TotalWeight < KINDA_SMALL_NUMBER` → TagScore=0 → 모든 후보 제외, 평가 종료 (방어 코드 유지)
5. EC-9 (시스템 시계 역행): `DaysSinceLastDream < 0` → `max(0, ...)` 가드 (Story 003 참조) — 본 story에서는 재확인만

## Implementation Notes

- **AC-DTS-17 구현** (EC-8):
  ```cpp
  void UDreamTriggerSubsystem::OnCardProcessedByGrowthHandler(...) {
      // ... guard chain
      TArray<const UDreamDataAsset*> Assets = Pipeline->GetAllDreamAssets();
      if (Assets.Num() == 0) {
          UE_LOG(LogDreamTrigger, Warning, TEXT("GetAllDreamAssets returned empty — DegradedFallback"));
          return;  // FDreamState 불변, 미발행
      }
      // ... 후보 수집
  }
  ```
- **F3 ExpectedDreamCount 테스트**:
  ```cpp
  TEST_CASE("F3 FrequencyTarget — Full defaults in [3, 5]") {
      const float Expected = (21.0f - 5.0f) / 4.0f;  // EarliestPossible=5 (Day 1 제외 + 첫 interval)
      const float Min = FMath::Min(Expected, 5.0f);
      REQUIRE(Min >= 3.0f);
      REQUIRE(Min <= 5.0f);
  }
  ```
- **AC-DTS-20 Manual Playtest doc**: `production/qa/evidence/dream-mvp-7day-playtest.md` — 5회 반복 기록

## Out of Scope

- Playtest tooling (retrospective skill)
- F3 설계 검증 수식 외 regression (OQ-DTS-5 P2 Spike)

## QA Test Cases

**Given** Pipeline DegradedFallback (GetAllDreamAssets 빈 배열), **When** FOnCardProcessedByGrowth 수신, **Then** Warning 1회, 미발행, FDreamState 불변 (AC-DTS-17).

**Given** F3 공식 + Full 기본값, **When** ExpectedDreamCount 계산, **Then** [3.0, 5.0] 범위 내 (AC-DTS-19 ADVISORY).

**Given** MVP 7일 플레이스루 5회 반복, **When** 각 플레이스루 꿈 카운트, **Then** 최소 1회 꿈 보장, 일기 화면 표시 정상 (AC-DTS-20 MANUAL).

**Given** `TotalWeight < KINDA_SMALL_NUMBER` (Day 1 빈 TagVector), **When** F1, **Then** Score=0, 평가 종료 (EC-7).

## Test Evidence

- **Unit test**: `tests/unit/Dream/test_ec8_degraded_fallback.cpp`
- **Formula test**: `tests/unit/Dream/test_f3_frequency_target.cpp` (AC-DTS-19)
- **Manual playtest**: `production/qa/evidence/dream-mvp-7day-playtest.md` (AC-DTS-20)

## Dependencies

- Story 001-004 (Dream Trigger core)
- `foundation-data-pipeline` (GetAllDreamAssets)
