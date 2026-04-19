# Story 006: DegradedFallback Unavailable 모드 + Log Throttle

> **Epic**: feature-stillness-budget
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 1시간

## Context

- **GDD Reference**: `design/gdd/stillness-budget.md` §Rule 7 DegradedFallback + §E9 + AC-SB-08/09
- **TR-ID**: TR-stillness-001 (DegradedFallback behavior)
- **Governing ADR**: None (GDD contract). ADR-0007 Hybrid은 VS sprint에서 통합 — 본 story는 game-layer Unavailable만
- **Engine Risk**: LOW
- **Engine Notes**: Pipeline GetStillnessBudgetAsset() == nullptr 시. 하드코딩 기본값 금지 — 전면 Deny (fail-close).

## Acceptance Criteria

1. **AC-SB-08** (AUTOMATED) — Asset == nullptr (DegradedFallback) + 세 채널 `Request()` → 모두 Invalid 핸들, 하드코딩 기본값 사용 없음
2. **AC-SB-09** (AUTOMATED) — DegradedFallback 모드에서 `Request()` 5회 연속 호출 → `UE_LOG(LogStillness, Warning)` 정확히 1회
3. E9: DegradedFallback 시 활성 슬롯 존재 불가 (Initialize 시점 결정)
4. Log throttle — 첫 Request() 시 Warning 1회, 이후 동일 경로 재호출 시 silent

## Implementation Notes

- **DegradedFallback 진입** (Rule 7):
  ```cpp
  void UStillnessBudgetSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
      Super::Initialize(Collection);
      Collection.InitializeDependency<UDataPipelineSubsystem>();
      Asset = Pipeline->GetStillnessBudgetAsset();  // nullptr 가능
      bLoggedUnavailable = false;
      // ... (normal path)
  }

  FStillnessHandle UStillnessBudgetSubsystem::Request(
      EStillnessChannel Channel, EStillnessPriority Priority) {
      if (!Asset) {
          if (!bLoggedUnavailable) {
              UE_LOG(LogStillness, Warning, TEXT("StillnessBudgetAsset unavailable — DegradedFallback"));
              bLoggedUnavailable = true;  // Log throttle (AC-SB-09)
          }
          return FStillnessHandle{};  // Invalid (fail-close)
      }
      // ... (정상 경로)
  }
  ```
- **Rule 7 fail-close 원칙**: 엔진 레이어도 거부 — "비활성화 ≠ 모든 효과 허용"
- **E9 불변식**: DegradedFallback 모드에서는 슬롯 Grant 자체가 불가 → 활성 슬롯 존재 불가

## Out of Scope

- Pipeline fail-close 구현 (foundation-data-pipeline)
- Recovery mechanism (N/A — DegradedFallback은 Initialize 시점 결정)

## QA Test Cases

**Given** Pipeline DegradedFallback, **When** 3 채널 각각 Request, **Then** 모두 Invalid, hardcoded defaults 없음 (AC-SB-08).

**Given** DegradedFallback, **When** `Request()` 5회 연속, **Then** Warning 1회만 기록 (AC-SB-09 log throttle).

**Given** DegradedFallback 모드, **When** 여러 소비자가 각기 다른 Request 호출, **Then** 활성 슬롯 수 0 유지 (E9).

## Test Evidence

- **Unit test**: `tests/unit/Stillness/test_degraded_fallback.cpp`
- Log 호출 횟수 카운트

## Dependencies

- Story 001 (Subsystem + Pipeline pull)
- Story 002 (Request API)
