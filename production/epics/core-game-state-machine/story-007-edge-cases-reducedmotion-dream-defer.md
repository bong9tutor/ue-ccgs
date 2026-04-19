# Story 007 — Edge Cases E1-E10: ReducedMotion / Dream Defer / Blend Interrupts

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Edge Cases E1-E10 + §Formula 1 mid-transition restart
- **TR-ID**: TR-gsm-002 (MPC writer uniqueness 파생 — ReducedMotion 0프레임 적용)
- **Governing ADR**: ADR-0004 §Validation Criteria (mid-transition restart AC-GSM-08).
- **Engine Risk**: LOW (개별 edge case는 로직 — Lumen GPU 영향은 story 005에 집약)
- **Engine Notes**: `DreamDeferMaxSec` 초과 시 Budget 강제 선점(P0급)으로 Dream 진입 — 앵커 모먼트 소실 방지. LONG_GAP_SILENT은 항상 DayIndex=21 (Time GDD 전제).
- **Control Manifest Rules**: Cross-Cutting — Day counter 영구 미표시. UI 알림·팝업 금지.

## Acceptance Criteria

- **AC-GSM-02** [`AUTOMATED`/BLOCKING] — Dream 상태를 `LastPersistedState`로 담고 있는 `FSessionRecord`를 로드 시 실제 초기 상태 Waiting (E1).
- **AC-GSM-03** [`AUTOMATED`/BLOCKING] — Dream 상태에서 MPC 블렌드 진행 중 LONG_GAP_SILENT(P0) 도달 시 Dream 즉시 종료 + Farewell 전환 + Stillness Budget 핸들 Release (E4).
- **AC-GSM-08** [`AUTOMATED`/BLOCKING] — Waiting→Dream 블렌드 50% 진행 중(t=D_blend/2) P0 인터럽트(Farewell) 도달 시 새 전환의 V_start가 인터럽트 시점 블렌드 중간값이며 Waiting으로 먼저 돌아가지 않음 (F1 mid-transition restart).
- **AC-GSM-10** [`AUTOMATED`/BLOCKING] — MPC 블렌드 진행 중 ReducedMotion ON 토글 시 현재 Lerp 즉시 중단 + 목표 MPC값 동일 프레임 적용. OFF 복귀 시 MPC 재블렌드 미발생 (E10).
- **AC-GSM-13** [`AUTOMATED`/BLOCKING] — Stillness Budget 전체 점유 상태에서 `FOnDreamTriggered` 도달 시 Waiting 유지 + `OnBudgetRestored` 구독 등록 + 슬롯 해제 시 Dream 재시도 (E5).
- **AC-GSM-14** [`AUTOMATED`/BLOCKING] — 5일치 `ADVANCE_SILENT` 빠른 연속 수신 시 Dawn→Offering→Waiting 시퀀스 1회만 재생 + DayIndex 최종 목표값 (E8).

## Implementation Notes

1. **E1 Dream demotion** (AC-GSM-02):
   - `HandleLoadComplete`에서 `if (Record.LastPersistedState == EGameState::Dream) { Record.LastPersistedState = EGameState::Waiting; }` 정규화.
2. **E4 Dream → LONG_GAP_SILENT** (AC-GSM-03):
   - `HandleTimeActionResolved(ETimeAction::LONG_GAP_SILENT)`에서 현재 상태 불문 Farewell P0. Dream 상태이면 `DreamStillnessHandle.Release()` 선행.
3. **E5 Dream defer** (AC-GSM-13):
   - `HandleDreamTriggered`에서 `StillnessBudget::Request(Narrative)` 실패 시 `bDreamDeferred = true`. `OnBudgetRestored(Narrative)` delegate subscribe. `DreamDeferMaxSec` 타이머 시작 — 만료 시 P0급 선점 재시도.
4. **E8 Fast consecutive ADVANCE_SILENT** (AC-GSM-14):
   - DayIndex 전진 신호는 Time GDD에서 최종값으로 제공 (coalesced). GSM은 Dawn→Offering→Waiting 시퀀스 재생 1회만 허용 — `bDawnSequenceInProgress` guard.
5. **E10 ReducedMotion** (AC-GSM-10):
   - `OnReducedMotionChanged(bool bNew)` handler 구독 (Accessibility Subsystem — TBD). `bNew == true` 시 현재 `TickMPC`에서 `D_blend = 0`으로 강제 → 다음 frame에서 `V_target` 즉시 적용. OFF 복귀 시 MPC 재블렌드 없음 (이미 target 도달).
6. **F1 mid-transition restart** (AC-GSM-08):
   - `RequestStateTransition(P0)` 수신 시 현재 `V(t)`를 새 transition의 `V_start`로 사용. `TransitionContext`에 `V_start/V_target/ElapsedTime` 저장.

## Out of Scope

- Compound events (story 011 E13)
- Withered lifecycle (story 008 E18/AC-GSM-20/21)
- Narrative overlay (story 009)
- Load branches E11/E12/E14/E15 (story 010)

## QA Test Cases

**Test Case 1 — AC-GSM-08 Mid-transition restart**
- **Given**: Start Waiting→Dream blend (D_blend=1.35s, t=0.675s → 50%). `Formula 1` 출력 `ColorTempK = 3,500` (중간값).
- **When**: `RequestStateTransition(Farewell, P0)`.
- **Then**: 새 transition의 `V_start = 3,500` (인터럽트 시점 값). `V_target = 2,200` (Farewell). `Waiting 2,800` 으로 돌아가지 않음.

**Test Case 2 — AC-GSM-10 ReducedMotion toggle**
- **Given**: Waiting→Dream blend 진행 중 (t=0.5s, 37%).
- **When**: ReducedMotion ON.
- **Then**: 동일 frame 내 `ColorTempK = 4,200` (Dream target). `MossBabySSSBoost = 0.40`. OFF 복귀 후 `OnGameStateChanged` 재발행 없음.

**Test Case 3 — AC-GSM-14 Coalesced day advance**
- **Given**: 5일 경과 시뮬, Time이 `ADVANCE_SILENT(NewDayIndex=6)` 1회 발행 (Time의 coalesce 후).
- **When**: GSM handler 실행.
- **Then**: Dawn entry 1회, Offering entry 1회, Waiting entry 1회 — 각 5회가 아닌 1회.

**Test Case 4 — AC-GSM-13 Budget deny + restore**
- **Given**: Mock Budget이 Narrative slot 점유 중. `FOnDreamTriggered` 발행.
- **When**: Handler 실행.
- **Then**: `CurrentState = Waiting` (Dream 미진입). `bDreamDeferred = true`. Mock Budget `OnBudgetRestored(Narrative)` 발행 → Dream 진입 재시도.

## Test Evidence

- [ ] `tests/unit/game-state/dream_demotion_test.cpp` — AC-GSM-02 E1
- [ ] `tests/integration/game-state/dream_long_gap_interrupt_test.cpp` — AC-GSM-03 E4
- [ ] `tests/unit/game-state/mid_transition_restart_test.cpp` — AC-GSM-08
- [ ] `tests/integration/game-state/reduced_motion_test.cpp` — AC-GSM-10
- [ ] `tests/integration/game-state/dream_defer_test.cpp` — AC-GSM-13
- [ ] `tests/integration/game-state/coalesced_advance_test.cpp` — AC-GSM-14

## Dependencies

- **Depends on**: story-003 (MPC tick F1), story-005 (Light Actor for ReducedMotion path), story-006 (settings)
- **Unlocks**: story-010 (load branches E11/E12/E14/E15 depend on E1 demotion)
