# Story 002 — 상태 전환 테이블 + 4단계 우선순위 시스템

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Rule 2 (우선순위 시스템) + §States and Transitions (전체 전환 표)
- **TR-ID**: TR-gsm-005 (AC-GSM-05 Farewell 종단 불변)
- **Governing ADR**: N/A — GDD contract. ADR-0004 §Decision §1 참조 (GSM이 전환 시작점).
- **Engine Risk**: LOW
- **Engine Notes**: `RequestStateTransition(EGameState, EPriority)` API는 P0 인터럽트 허용하되 `Farewell → Any` 경로 금지. GDD §Forbidden Transitions 참조.
- **Control Manifest Rules**: Cross-Cutting Constraints — `Farewell → Any` 상태 전환 분기 금지 (AC-GSM-05 grep 0건, Pillar 4).

## Acceptance Criteria

- **AC-GSM-04** [`AUTOMATED`/BLOCKING] — Dawn→Offering 전환(P1) 진행 중 Dream Trigger 이벤트(P2) 도달 시, Dream 전환 거부 + 자동 순서 완료.
- **AC-GSM-05** [`CODE_REVIEW`/BLOCKING] — 상태 전환 분기 코드 전체에서 `Farewell → Any` 방향 전환 분기 grep 0건.

## Implementation Notes

1. **`EPriority` enum** (`Source/MadeByClaudeCode/Core/GameState/EGameStatePriority.h`):
   - `P0 = 0` (시스템 강제), `P1 = 1` (자동 순서), `P2 = 2` (조건부 이벤트), `P3 = 3` (플레이어 명시).
2. **`RequestStateTransition(EGameState NewState, EGameStatePriority Priority)` API**:
   - 내부 저장된 현재 진행 중인 transition의 priority와 비교 — 더 높은(숫자 작은) priority만 인터럽트 허용.
   - **Farewell guard**: `if (CurrentState == EGameState::Farewell) { UE_LOG(Warning, ...); return false; }` — `Farewell → Any` 분기 자체 부재 (AC-GSM-05 grep). 단, `RequestStateTransition`은 early-return만 하며 분기 없음.
3. **Transition table 구현**: GDD §전체 전환 표를 `TMap<TTuple<EGameState, EGameState>, EGameStatePriority>` 또는 `switch` 로 구현. Forbidden transitions (`Farewell → *`, `Dawn → Dream`, `Offering → Dream`)은 `switch` default에서 reject.
4. **Guard 조건**: 각 P1/P2 전환은 `CanTransitionTo(From, To, Guard)` helper로 검증. GDD §States and Transitions 표의 Guard 열 매핑.
5. **테스트 가능성**: `SetCurrentStateForTest(EGameState)` editor-only 진입점 추가 (`#if WITH_EDITOR` 가드) — 자동화 테스트에서 특정 상태 주입용.

## Out of Scope

- MPC / Light Actor 구동 (story 003-004)
- Delegate publish (story 004)
- Edge cases E1-E17 개별 처리 (story 007-009)

## QA Test Cases

**Test Case 1 — AC-GSM-04 P0 > P1 > P2 priority**
- **Given**: `CurrentState = Dawn`, P1 Dawn→Offering 진행 중 (`BlendProgress = 0.5`).
- **When**: `RequestStateTransition(Dream, P2)` 호출.
- **Then**: P2 < P1 아님(숫자 반대) 검증 — 이 문서의 numeric convention에서 P2(2) > P1(1)이므로 reject. `CurrentState` 여전히 Dawn→Offering 진행. `Dream` 미진입.

**Test Case 2 — AC-GSM-05 Farewell terminal**
- **Setup**: `grep -nE "Farewell.*(->|=>).*\b(Menu|Dawn|Offering|Waiting|Dream)\b" Source/MadeByClaudeCode/` 실행.
- **Verify**: 매치 0건.
- **Pass**: grep 결과 empty.

**Test Case 3 — Forbidden transitions (Dawn→Dream, Offering→Dream)**
- **Given**: `CurrentState = Dawn` (또는 `Offering`).
- **When**: `RequestStateTransition(Dream, P0)` 호출.
- **Then**: `CanTransitionTo` false → reject. `CurrentState` 유지.

## Test Evidence

- [ ] `tests/unit/game-state/priority_system_test.cpp` — P0/P1/P2 우선순위 매트릭스
- [ ] `tests/unit/game-state/forbidden_transitions_test.cpp` — Farewell 종단 + Dawn/Offering→Dream reject
- [ ] `tests/unit/game-state/farewell_grep_test.md` — AC-GSM-05 grep 명령어 + 예상 출력 문서화

## Dependencies

- **Depends on**: story-001 (EGameState + Subsystem skeleton)
- **Unlocks**: story-003 (MPC tick), story-007 (edge cases E4 E10)
