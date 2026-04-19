# Story 006 — 브리딩 사인파 Formula 3 + Idle 분기 내부 전용

> **Epic**: [core-moss-baby-character](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2h

## Context

- **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md) §CR-4 (브리딩 Idle 호흡) + §Formula 3 (브리딩 사인파) + §Edge Case E9 (브리딩 + Reacting 충돌)
- **TR-ID**: N/A (GDD-derived formula)
- **Governing ADR**: N/A — GDD contract.
- **Engine Risk**: LOW
- **Engine Notes**: `EmissionStrength`를 4-6초 주기 사인파로 ±0.02 진동. **Idle 상태에서만** 활성. Reacting·GrowthTransition 상태에서 일시 중단. ReducedMotion ON 시 즉시 중단.
- **Control Manifest Rules**: Cross-Cutting — Tick 남용 금지 (이벤트/타이머 기반 우선, Tick은 렌더 관련에만).

## Acceptance Criteria

- **AC-MBC-06** [`AUTOMATED`/BLOCKING] — E_base=0.03, A=0.02, P=5.0 (고정 입력)에서 Formula 3 순수 함수에 t=0.0, 1.25, 2.5, 3.75, 5.0 입력 시 출력값 각각 0.03, 0.05, 0.03, 0.01, 0.03 (오차 ±0.001).
- **AC-MBC-07** [`CODE_REVIEW`/BLOCKING] — 호흡 사인파 적용 코드 (EmissionStrength 사인파 계산 함수 호출처) grep 시 상태 분기 패턴 확인하면 `EMossCharacterState::Idle` 분기 내부에서만 호출. Reacting/GrowthTransition/FinalReveal/QuietRest 분기 내 호출 0건 (CR-4).
- **AC-MBC-15** [`AUTOMATED`/BLOCKING] — Idle 브리딩 활성 중 (t_interrupted 시점 기록) FOnCardOffered 트리거 시 Reacting 진입에서 (1) 브리딩 사인파 즉시 중단 — EmissionStrength가 사인파 계산 없이 고정. (2) Reacting 완료 판정을 타이머 주입으로 강제 트리거 후 Idle 복귀 시, EmissionStrength = E_base + A × sin(2π × t_interrupted / P) (±0.001) — 중단 시점 위상에서 연속 재개 확인 (E9).

## Implementation Notes

1. **Formula 3 pure function**:
   ```cpp
   static float BreathingCycle(float EBase, float A, float P, float T) {
       checkf(A <= EBase + KINDA_SMALL_NUMBER, TEXT("A must be ≤ EBase"));
       const float Value = EBase + A * FMath::Sin(2.0f * PI * T / P);
       return FMath::Max(0.0f, Value);  // Output clamp to ≥ 0 (GDD §Formula 3)
   }
   ```
2. **Idle-only 실행 가드** (AC-MBC-07):
   - `AMossBaby::TickBreathing(float DeltaTime)` 함수는 **`if (CurrentState != EMossCharacterState::Idle) { return; }` 선행 필수**.
   - `BreathingTimer` 또는 `FTSTicker`는 Idle 진입 시 시작, 타 state 진입 시 중단.
3. **State transition 통합**:
   - `EnterState(Idle)`에서 `BreathingPhaseT = 0.0f;` 또는 재개 시점 위상 유지 (AC-MBC-15).
   - `ExitState(Idle)`에서 `BreathingTimer` 중단 + `BreathingPhaseAtInterrupt = BreathingPhaseT;` 저장.
   - Reacting 종료 후 Idle 복귀 시 `BreathingPhaseT = BreathingPhaseAtInterrupt;` — 연속성 (E9).
4. **ReducedMotion**:
   - `if (IsReducedMotion()) { return; }` — TickBreathing 진입 시 즉시 종료.
5. **Amplitude clamp (GDD Formula 3 "A ≤ E_base")**:
   - `UMossBabyPresetAsset`의 `BreathingAmplitude` UPROPERTY에 `ClampMax=0.02` (Sprout E_base 0.02 상한).

## Out of Scope

- State machine transitions (story 002)
- Reacting decay Formula 4 (story 003)
- QuietRest vocabulary (story 005)

## QA Test Cases

**Test Case 1 — AC-MBC-06 Formula 3 math**
- **Given**: `E_base = 0.03, A = 0.02, P = 5.0`.
- **When**: `t = 0.0, 1.25, 2.5, 3.75, 5.0` 순수 함수 호출.
- **Then**: 출력 `0.03, 0.05, 0.03, 0.01, 0.03` (±0.001). sin(0)=0, sin(π/2)=1, sin(π)=0, sin(3π/2)=-1, sin(2π)=0.

**Test Case 2 — AC-MBC-07 Idle-only grep**
- **Setup**: `grep -rnE "BreathingCycle\(" Source/MadeByClaudeCode/Core/MossBaby/ -B 3`
- **Verify**: 매치 각각의 3줄 앞에 `if (CurrentState == EMossCharacterState::Idle)` 또는 `TickBreathing` 함수 내부 (함수 자체가 Idle 가드 포함) 확인.
- **Pass**: 모든 매치가 Idle 가드 하위에 있음.

**Test Case 3 — AC-MBC-15 Phase resume**
- **Given**: Idle 상태에서 `BreathingPhaseT = 1.25s` (위상 π/2, E=0.05). 
- **When**: (1) `FOnCardOffered` 발행 → Reacting 진입 → 5τ_sss=1.65s 경과 → Reacting exit → Idle 복귀. (2) Idle 복귀 직후 `BreathingPhaseT` 확인.
- **Then**: 
  - Reacting 진입 시점: `BreathingPhaseAtInterrupt = 1.25s` 저장. Breathing timer 중단.
  - Reacting 중 EmissionStrength는 Formula 4 감쇠만 (Formula 3 관여 없음).
  - Idle 복귀 시: `BreathingPhaseT = 1.25s` 복원. 다음 frame에서 `BreathingCycle(0.03, 0.02, 5.0, 1.25) = 0.05` (±0.001).

**Test Case 4 — ReducedMotion suspend**
- **Given**: `IsReducedMotion() == true`, Idle 상태.
- **When**: `TickBreathing` 호출.
- **Then**: 즉시 return. MID EmissionStrength 변화 없음.

## Test Evidence

- [ ] `tests/unit/moss-baby/formula3_breathing_test.cpp` — AC-MBC-06 순수 수학
- [ ] `tests/unit/moss-baby/breathing_idle_grep.md` — AC-MBC-07 CI grep 문서화
- [ ] `tests/integration/moss-baby/breathing_phase_resume_test.cpp` — AC-MBC-15

## Dependencies

- **Depends on**: story-002 (state machine), story-003 (Reacting for phase save/restore)
- **Unlocks**: None (leaf story)
