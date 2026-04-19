# Story 004: Reduced Motion API + F2 EffLimit + Rule 6

> **Epic**: feature-stillness-budget
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 1.5시간

## Context

- **GDD Reference**: `design/gdd/stillness-budget.md` §Rule 6 (Reduced Motion) + §F2 (EffLimit) + §E1 (MaxSlots=0) + §E2 (toggle ON 활성 슬롯) + AC-SB-06/07
- **TR-ID**: TR-stillness-004 (Rule 6 IsReducedMotion 공개 API)
- **Governing ADR**: None (GDD contract, ADR-0007 게임-레이어 truth 유지)
- **Engine Risk**: LOW
- **Engine Notes**: Reduced Motion은 Motion/Particle EffLimit=0으로 강제. Sound는 영향 없음.

## Acceptance Criteria

1. **AC-SB-06** (AUTOMATED) — `bReducedMotionEnabled=true`, Motion/Particle MaxSlots=2, `Request(Motion, Narrative)` / `Request(Particle, Narrative)` → 둘 다 Invalid
2. **AC-SB-07** (AUTOMATED) — `bReducedMotionEnabled=true`, Sound MaxSlots=3, Active=0, `Request(Sound, Standard)` → 유효 핸들 (Sound EffLimit = MaxSlots 유지)
3. 공개 API: `bool IsReducedMotion() const` / `void SetReducedMotion(bool bEnabled)`
4. F2 구현: Motion/Particle → `bReducedMotionEnabled ? 0 : MaxSlots_ch`, Sound → `MaxSlots_ch` (불변)
5. **E2 기존 슬롯 처리**: ReducedMotion ON 시 기존 활성 핸들 강제 해제하지 않음 — 자연 Release까지 유지 (Formula 2 주석 "과거 Grant 소급 취소 없음")
6. **E1 MaxSlots=0**: `UStillnessBudgetAsset`에서 MaxSlots=0 설정 → 해당 채널 모든 요청 Deny, Initialize 시 `UE_LOG(Warning)` 1회

## Implementation Notes

- **F2 구현**:
  ```cpp
  int32 UStillnessBudgetSubsystem::ComputeEffLimit(EStillnessChannel Channel) const {
      if (!Asset) return 0;
      const int32 Base = GetMaxSlots(Channel);  // Asset UPROPERTY
      if (Asset->bReducedMotionEnabled &&
          (Channel == EStillnessChannel::Motion || Channel == EStillnessChannel::Particle)) {
          return 0;  // Rule 6
      }
      return Base;
  }
  ```
- **Public API**:
  ```cpp
  bool UStillnessBudgetSubsystem::IsReducedMotion() const {
      return Asset && Asset->bReducedMotionEnabled;
  }
  void UStillnessBudgetSubsystem::SetReducedMotion(bool bEnabled) {
      if (!Asset) return;
      Asset->bReducedMotionEnabled = bEnabled;
      // OnBudgetRestored 발행은 Story 005
  }
  ```
- **E2 behavior**: toggle ON → 신규 요청만 Deny. 기존 Active 슬롯은 자연 Release 대기.
- **Rule 6 Sound 무영향**: 청각 접근성 별도 관심사 — Accessibility Layer (#18, VS)가 소유

## Out of Scope

- OnBudgetRestored delegate (Story 005)
- `bReducedMotionEnabled` 영속화 (OQ-2 — Title & Settings UI VS)
- Accessibility Layer UX (VS)

## QA Test Cases

**Given** `bReducedMotionEnabled=true`, Motion/Particle MaxSlots=2, **When** `Request(Motion, Narrative)` + `Request(Particle, Narrative)`, **Then** 둘 다 Invalid 핸들 (AC-SB-06).

**Given** `bReducedMotionEnabled=true`, Sound MaxSlots=3, Active=0, **When** `Request(Sound, Standard)`, **Then** 유효 핸들 (AC-SB-07).

**Given** ReducedMotion=OFF + Motion Active=2, **When** `SetReducedMotion(true)`, **Then** EffLimit=0 즉시 적용, 기존 2 Active 유지 (E2).

**Given** MaxSlots=0 설정된 채널, **When** `Request()`, **Then** Deny, Initialize 시 Warning 1회 (E1).

**Given** 공개 API 테스트, **When** `IsReducedMotion()` 호출 후 `SetReducedMotion(true)`, **Then** 이후 `IsReducedMotion() == true`.

## Test Evidence

- **Unit test**: `tests/unit/Stillness/test_reduced_motion.cpp` + `test_f2_efflimit.cpp`
- E1/E2 edge tests 독립

## Dependencies

- Story 001 (Subsystem + Asset)
- Story 002 (Request API — F2 통합)
