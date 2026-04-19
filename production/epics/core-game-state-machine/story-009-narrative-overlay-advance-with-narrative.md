# Story 009 — Narrative Overlay (Rule 5 `ADVANCE_WITH_NARRATIVE`)

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Rule 5 (ADVANCE_WITH_NARRATIVE 오버레이)
- **TR-ID**: TR-gsm-004 (E12 Time vs Load ordering 파생)
- **Governing ADR**: N/A — GDD contract.
- **Engine Risk**: LOW
- **Engine Notes**: 시스템 내러티브는 Waiting 상태 내 일시적 콘텐츠 오버레이 — 별도 상태 아님. `NarrativeCapPerPlaythrough(3)` 검사는 Time System이 발행 전 수행 — GSM은 cap 검사 없음.
- **Control Manifest Rules**: Cross-Cutting — UI 알림·팝업·모달 금지 (Narrative는 Content 레이어 위임 — sound/text는 Audio/Journal 시스템).

## Acceptance Criteria

- **AC-GSM-11** [`INTEGRATION`/BLOCKING] — GSM Waiting 상태 + Budget Narrative 슬롯 가용 시 `ADVANCE_WITH_NARRATIVE` 신호 도달에 GSM 상태 `Waiting` 유지 + MPC 파라미터 변경 없음 + `Narrative` 슬롯 점유 상태(`FStillnessHandle` 유효) 확인. 슬롯 점유 중 두 번째 `ADVANCE_WITH_NARRATIVE` 수신 시 두 번째 요청 거부.

## Implementation Notes

1. **`HandleTimeActionResolved(ETimeAction::ADVANCE_WITH_NARRATIVE)`**:
   - 조건: `CurrentState == EGameState::Waiting`. 다른 상태에서는 무시 (GDD §Rule 5 암시).
   - Stillness Budget `Request(Narrative, Narrative)` — `FStillnessHandle` 반환.
   - Grant 성공 시: `NarrativeHandle = Handle;` → `OnNarrativeOverlayStarted.Broadcast()` — Content 레이어(Audio/Journal) 구독 전파.
   - Deny 시: 조용히 skip (GDD §Rule 5 fallback 명시 없음 — Silent skip 채택).
2. **두 번째 요청 거부 (AC-GSM-11 second request)**:
   - `if (NarrativeHandle.IsValid()) { return; }` — 진입 가드.
3. **Narrative 완료 처리**:
   - Content 레이어가 `OnNarrativeOverlayCompleted` 발행 시 GSM이 `NarrativeHandle.Release()` + handle invalidate.
   - MVP에서 Audio/Journal 미구현이면 timer-based auto-release 허용 (`NarrativeOverlayDurationSec` 기본 2.0s — `UMossGameStateSettings` knob).
4. **MPC 불변 보장**:
   - Narrative overlay는 MPC target 변경 없음 (GDD §Rule 5 "MPC 변경 없음"). `TickMPC`는 Waiting target 계속 사용.
5. **DayIndex 전진 시 Dawn 시퀀스 선행** (GDD §Rule 5 §Time System 상호작용):
   - `ADVANCE_WITH_NARRATIVE`가 DayIndex 변화 포함하면 Dawn→Offering→Waiting 시퀀스 완료 후 Narrative overlay 재생. 구현: Dawn 시퀀스 완료 flag 확인 후 narrative publish 지연.

## Out of Scope

- Narrative content (텍스트/사운드) 재생은 Content 레이어 (Audio/Journal 시스템)
- `NarrativeCapPerPlaythrough(3)` 검사는 Time System epic
- Dream narrative 텍스트는 Dream Trigger epic

## QA Test Cases

**Test Case 1 — AC-GSM-11 Narrative 정상 진입**
- **Given**: `CurrentState = Waiting`, Mock Budget `Narrative` slot 가용.
- **When**: `ADVANCE_WITH_NARRATIVE` 수신.
- **Then**: `CurrentState` 여전히 `Waiting`. MPC target 변경 없음 (`ColorTempK` 불변). `NarrativeHandle.IsValid() == true`.

**Test Case 2 — AC-GSM-11 두 번째 요청 거부**
- **Given**: `NarrativeHandle` 이미 valid (첫 번째 overlay 진행 중).
- **When**: 두 번째 `ADVANCE_WITH_NARRATIVE` 수신.
- **Then**: 두 번째 요청 거부. 로그 1회. Budget 추가 Request 호출 없음 (`Request` counter 여전히 1).

**Test Case 3 — Budget Deny fallback**
- **Given**: `CurrentState = Waiting`, Mock Budget `Narrative` slot 점유 중.
- **When**: `ADVANCE_WITH_NARRATIVE` 수신.
- **Then**: `CurrentState` Waiting 유지. `NarrativeHandle.IsValid() == false`. Silent skip 로그. MPC 불변.

## Test Evidence

- [ ] `tests/integration/game-state/narrative_overlay_test.cpp` — AC-GSM-11 3 scenario
- [ ] `tests/unit/game-state/narrative_mpc_invariant_test.cpp` — MPC target 불변 검증

## Dependencies

- **Depends on**: story-003 (MPC tick), story-004 (delegate), Stillness Budget epic (Request/Release API)
- **Unlocks**: Audio System (VS), Dream Journal UI (Narrative content rendering — 별도 epic)
