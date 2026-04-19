# Story 005 — Dream DoF Formula 1 (`ApertureFStop` Lerp) + Clamp [1.4, 4.0]

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2h

## Context

- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 2 (DoF) + §Formula 1 (DoFParameters) + §States Dream + §Edge Case E7
- **TR-ID**: TR-lumen-006 (AC-LDS-16 Dream DoF Formula 1 clamp [1.4, 4.0])
- **Governing ADR**: **ADR-0004** §Decision §2 — Dream DoF 강화는 Lumen Dusk가 카메라 DoF 직접 제어.
- **Engine Risk**: LOW (Post-process DoF는 Lumen GI와 독립 — GPU budget F2에 포함)
- **Engine Notes**: Dream 진입 시 `ApertureFStop = BaseFStop + DreamFStopDelta × DreamBlend`. 조리개가 열릴수록(F값 낮을수록) 피사계 심도가 얕아져 배경이 더 흐려지고, 이끼 아이에 시선 집중. Dream 이탈 시 역방향 Lerp. Clamp [1.4, 4.0] — 극단값 방어.
- **Control Manifest Rules**: Core Layer Rules §Required Patterns — `PP_MossDusk` DoF 소유 (Vignette/DoF/Contrast).

## Acceptance Criteria

- **AC-LDS-16** [`AUTOMATED`/BLOCKING] — Formula 1 DoF 계산 (BaseFStop=2.0, DreamFStopDelta=−0.5)에서 DreamBlend=0.0, 0.5, 1.0 입력 시 ApertureFStop = 2.0, 1.75, 1.5 (±0.01). clamp [1.4, 4.0] 범위 내.

## Implementation Notes

1. **Formula 1 pure function** (`Source/MadeByClaudeCode/Core/LumenDusk/LumenDuskFormulas.h`):
   ```cpp
   static float ComputeApertureFStop(float BaseFStop, float DreamFStopDelta, float DreamBlend) {
       const float Value = BaseFStop + DreamFStopDelta * FMath::Clamp(DreamBlend, 0.0f, 1.0f);
       return FMath::Clamp(Value, 1.4f, 4.0f);  // AC-LDS-16 clamp
   }
   ```
2. **`ALumenDuskSceneActor::StartDoFLerp(float From, float To, float Duration)`** (story 004 `OnGameStateChangedHandler` 확장):
   - `if (NewState == EGameState::Dream) { StartDoFLerp(Config->BaseFStop, Config->BaseFStop + Config->DreamFStopDelta, 1.35f); }`
   - `if (OldState == EGameState::Dream && NewState == EGameState::Waiting) { StartDoFLerp(Current, Config->BaseFStop, 1.1f); }` — Dream 이탈 시 역방향 (Dream→Waiting 블렌드 D=1.1s).
3. **DoF tick**:
   - `FTSTicker` 60Hz. `DreamBlend`는 GSM MPC 블렌드 동기 — MPC Lerp 진행률 참조 또는 자체 Lerp + MPC와 동기 Duration.
   - 매 tick: `ApertureFStop = ComputeApertureFStop(Base, Delta, Blend); PostProcessVolume->Settings.DepthOfFieldFstop = ApertureFStop;`
4. **E7 Dream DoF Lerp 도중 Dream 이탈 (Budget Deny로 Dream 연기)**:
   - GSM이 Dream 진입 취소 시 (story GSM-007 E5 Dream defer) → `OnGameStateChanged(Waiting, Waiting)` 또는 상태 변화 없음. DoF Lerp 취소 + `ApertureFStop = BaseFStop` 복귀.
   - 구현: `StopDoFLerp()` API — GSM이 Dream 취소 시 호출.
5. **`FocusDistance` 고정** (Formula 1):
   - `FocusDistance = DistanceCameraToJar_cm` 고정값. 런타임 불변. `CameraComponent->PostProcessSettings.DepthOfFieldFocalDistance = 50.0f;` 초기화.

## Out of Scope

- Dream Trigger → GSM → Dream state 전환 체인 (Dream Trigger epic + GSM epic)
- Ambient particle Dream reduction (story 009 Formula 3)
- GSM Dream defer (GSM epic story 007)

## QA Test Cases

**Test Case 1 — AC-LDS-16 Formula 1 math**
- **Given**: `BaseFStop = 2.0, DreamFStopDelta = -0.5`.
- **When**: `DreamBlend = 0.0, 0.5, 1.0, 1.5 (out of range), -0.1 (out of range)`.
- **Then**: 출력 `2.0, 1.75, 1.5, 1.5 (clamp blend), 2.0` (±0.01). 모든 출력 ∈ [1.4, 4.0].

**Test Case 2 — Clamp 극단값 방어**
- **Given**: `BaseFStop = 1.5, DreamFStopDelta = -1.0` (극단 튜닝).
- **When**: `DreamBlend = 1.0`.
- **Then**: `ComputeApertureFStop = 1.5 + (-1.0) = 0.5 → clamp → 1.4`.

**Test Case 3 — Waiting → Dream → Waiting round-trip**
- **Given**: `CurrentState = Waiting`, `ApertureFStop = 2.0`.
- **When**: Dream 진입 (1.35s) → Dream 이탈 (1.1s).
- **Then**: 
  - Dream peak (t=1.35s): `ApertureFStop = 1.5` (DreamBlend=1.0).
  - Waiting 복귀 (t=2.45s): `ApertureFStop = 2.0` (기본값 복원).

## Test Evidence

- [ ] `tests/unit/lumen-dusk/formula1_dof_test.cpp` — AC-LDS-16 순수 수학
- [ ] `tests/integration/lumen-dusk/dream_dof_lerp_test.cpp` — 왕복 Lerp
- [ ] `tests/unit/lumen-dusk/dof_clamp_test.cpp` — 극단값 방어

## Dependencies

- **Depends on**: story-004 (Post-process Volume + Lerp skeleton)
- **Unlocks**: GSM Dream 전환 통합 (smoke test cross-epic)
