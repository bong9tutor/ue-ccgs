# Story 005: OnBudgetRestored Per-Channel Delegate (E3)

> **Epic**: feature-stillness-budget
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 1시간

## Context

- **GDD Reference**: `design/gdd/stillness-budget.md` §E3 (ReducedMotion OFF 복귀) + AC-SB-13
- **TR-ID**: TR-stillness-005 (OnBudgetRestored per-channel — Motion/Particle, Sound 제외)
- **Governing ADR**: None (GDD contract)
- **Engine Risk**: LOW
- **Engine Notes**: Sound 채널은 Rule 6에서 ReducedMotion 영향 없음 → OnBudgetRestored 발행 대상 아님.

## Acceptance Criteria

1. **AC-SB-13** (INTEGRATION) — ReducedMotion=ON 중 Deny된 소비자가 `OnBudgetRestored` 구독 중, `SetReducedMotion(false)` 복원 → `OnBudgetRestored(Motion)` + `OnBudgetRestored(Particle)` delegate 각각 1회 발행
2. `FOnBudgetRestored(EStillnessChannel Channel)` delegate 선언 — `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam`
3. Sound 채널은 OnBudgetRestored 발행 대상 아님 (Rule 6 — Sound 영향 없음)
4. SetReducedMotion true → false 전환 시에만 발행 (false → true는 미발행)

## Implementation Notes

- **`OnBudgetRestored` delegate** (GDD §E3):
  ```cpp
  DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBudgetRestored, EStillnessChannel, Channel);

  UPROPERTY(BlueprintAssignable)
  FOnBudgetRestored OnBudgetRestored;
  ```
- **SetReducedMotion 확장** (Story 004 확장):
  ```cpp
  void UStillnessBudgetSubsystem::SetReducedMotion(bool bEnabled) {
      if (!Asset) return;
      const bool bPrevious = Asset->bReducedMotionEnabled;
      Asset->bReducedMotionEnabled = bEnabled;
      if (bPrevious && !bEnabled) {
          // ON → OFF 복원 (E3)
          OnBudgetRestored.Broadcast(EStillnessChannel::Motion);
          OnBudgetRestored.Broadcast(EStillnessChannel::Particle);
          // Sound 미발행 (Rule 6)
      }
  }
  ```

## Out of Scope

- `bReducedMotionEnabled` 영속화 (OQ-2 VS)
- 소비자 구독 구현 (Card Hand UI, MBC 등)

## QA Test Cases

**Given** ReducedMotion=ON + Deny 누적, `OnBudgetRestored` 구독 중, **When** `SetReducedMotion(false)`, **Then** `OnBudgetRestored(Motion)` + `OnBudgetRestored(Particle)` 발행, Sound 미발행 (AC-SB-13).

**Given** ReducedMotion=OFF, **When** `SetReducedMotion(true)`, **Then** OnBudgetRestored 미발행 (ON 전환 시 미발행).

**Given** ReducedMotion=ON → OFF → ON → OFF, **When** 각 전환, **Then** OnBudgetRestored는 ON→OFF 전환마다 2회 (Motion+Particle) 발행.

## Test Evidence

- **Integration test**: `tests/integration/Stillness/test_onbudget_restored.cpp` (AC-SB-13)

## Dependencies

- Story 001 (Subsystem)
- Story 004 (SetReducedMotion API)
