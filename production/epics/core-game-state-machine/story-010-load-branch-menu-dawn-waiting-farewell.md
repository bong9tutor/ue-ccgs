# Story 010 — `FOnLoadComplete` 분기 + E11/E12/E14/E15 Load Edge Cases

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §전체 전환 표 (Menu→*) + §Edge Cases E11/E12/E14/E15
- **TR-ID**: TR-gsm-004 (E12 Time vs Load ordering — AC-GSM-15)
- **Governing ADR**: N/A — GDD contract. Save/Load epic과 협력.
- **Engine Risk**: LOW
- **Engine Notes**: `FOnLoadComplete(bFreshStart, bHadPreviousData)` + `FOnTimeActionResolved` 수신 순서 역전 가능 (E12) — 단일 버퍼에 보관. Menu 손상 분기(E15)는 양 슬롯 손상 확인 후 명시적 승인 — 자동 Dawn 금지.
- **Control Manifest Rules**: Cross-Cutting — Save/Load modal UI · "저장 중..." 인디케이터 금지 (E15 "이끼 아이는 깊이 잠들었습니다" 다이얼로그는 Title & Settings UI VS — MVP에서는 콘솔 로그만).

## Acceptance Criteria

- **AC-GSM-15** [`AUTOMATED`/BLOCKING] — Menu 상태 + `FOnLoadComplete` 미수신 상태에서 `FOnTimeActionResolved` 먼저 도달 시 Time 신호 단일 버퍼 보관 + 적용 보류 + `FOnLoadComplete` 후 올바른 분기 (E12).

## Implementation Notes

1. **`HandleLoadComplete(bool bFreshStart, bool bHadCorruption)`**:
   - GDD §전환 표 Menu 행 모두 구현:
     - `bFreshStart=true, bHadCorruption=false` → `RequestStateTransition(Dawn, P0)` (DayIndex=1).
     - `bHadCorruption=true` → Menu 유지 + `OnSaveCorruption.Broadcast()` delegate 발행 (Title & Settings UI VS 구독 — MVP에서는 로그만).
     - `Record.LastPersistedState == Farewell` → `RequestStateTransition(Farewell, P0)`.
     - `Record.LastPersistedState == Waiting` + 같은 날 카드 미건넴 → `RequestStateTransition(Offering, P0)`.
     - `Record.LastPersistedState == Waiting` + 같은 날 카드 건넴 → `RequestStateTransition(Waiting, P0)`.
     - 날짜 전진 감지 → `RequestStateTransition(Dawn, P0)` (E14 exception: 같은 날 재진입 시 Dawn skip).
2. **E12 Time 신호 buffering** (AC-GSM-15):
   - `bool bLoadCompleteReceived = false;` + `TOptional<ETimeAction> BufferedTimeAction;`
   - `HandleTimeActionResolved`에서 `if (!bLoadCompleteReceived) { BufferedTimeAction = Action; return; }` — 보류.
   - `HandleLoadComplete` 완료 시 `bLoadCompleteReceived = true;` + `if (BufferedTimeAction.IsSet()) { HandleTimeActionResolved(BufferedTimeAction.GetValue()); BufferedTimeAction.Reset(); }`.
   - 단일 버퍼 — 버퍼 점유 중 두 번째 `FOnTimeActionResolved` 수신 시 최신 값으로 교체 (또는 reject — GDD §E12 "단일 버퍼").
3. **E11 Farewell Entry crash recovery**:
   - `LastPersistedState`가 여전히 `Waiting`이고 Day 21 조건 충족 시 해당 세션에서 Farewell 재진입 — 자동 처리 (정상 Day 21 경로).
4. **E14 Dawn skip on same-day re-entry**:
   - `HandleLoadComplete` 시 `if (Record.DayIndex == Record.LastCompletedDayIndex && Record.bHasOfferedCardToday) { RequestStateTransition(Waiting, P0); return; }` — Dawn 우회.
5. **E15 Menu corruption**:
   - `FSessionRecord` 초기화 API는 Save/Load epic. GSM은 `OnSaveCorruption` 발행만 + `bMenuCorruptionMode = true` 플래그. VS Title UI에서 "다시 시작" 확인 후 `ResetAndStart()` API 호출 → `RequestStateTransition(Dawn, P0)`.

## Out of Scope

- Save/Load 구현 자체 (Foundation Save/Load epic)
- Title & Settings UI 다이얼로그 (VS)
- `FOnLoadComplete` publish (Foundation Save/Load epic)

## QA Test Cases

**Test Case 1 — AC-GSM-15 E12 Time before Load**
- **Given**: `CurrentState = Menu`, `bLoadCompleteReceived = false`.
- **When**: `FOnTimeActionResolved(ADVANCE_SILENT)` 먼저 → `FOnLoadComplete(bFreshStart=true, bHadCorruption=false)` 이후.
- **Then**: 
  - Time 수신 시점: `BufferedTimeAction.IsSet() == true`. `CurrentState` 여전히 Menu.
  - Load 수신 시점: Menu→Dawn 전환. 이후 buffered Time action 재처리.

**Test Case 2 — Menu→Dawn fresh start**
- **Given**: `CurrentState = Menu`.
- **When**: `FOnLoadComplete(true, false)`.
- **Then**: `CurrentState = Dawn`, `DayIndex = 1`.

**Test Case 3 — Menu→Farewell (last persisted)**
- **Given**: `Record.LastPersistedState = Farewell`.
- **When**: `FOnLoadComplete(false, false)`.
- **Then**: `CurrentState = Farewell` (복원).

**Test Case 4 — E14 same-day re-entry no Dawn**
- **Given**: `Record.DayIndex = 5`, `Record.LastCompletedDayIndex = 5`, `Record.bHasOfferedCardToday = true`.
- **When**: `FOnLoadComplete(false, false)`.
- **Then**: `CurrentState = Waiting` (Dawn 우회).

## Test Evidence

- [ ] `tests/integration/game-state/load_complete_branches_test.cpp` — 6개 Menu 전환 분기
- [ ] `tests/integration/game-state/e12_buffered_time_test.cpp` — AC-GSM-15 순서 역전
- [ ] `tests/unit/game-state/same_day_no_dawn_test.cpp` — E14

## Dependencies

- **Depends on**: story-001 (enum), story-002 (transitions), story-006 (entry actions), story-007 (E1 Dream demotion), story-008 (withered persistence)
- **Unlocks**: Save/Load Schema integration test (Foundation epic)
