# Story 011 — Compound Event Boundary (`BeginCompoundEvent`/`EndCompoundEvent`)

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Edge Case E13 (Day 21) + Day 13 compound scenario (ADR-0009)
- **TR-ID**: TR-gsm-003 + TR-gsm-006 (compound sequence atomicity)
- **Governing ADR**: **ADR-0009** §Decision + §API + §GSM 사용 패턴 (Day 13 예시) + §IsCompoundEventTrigger 판단 기준.
- **Engine Risk**: LOW
- **Engine Notes**: Save/Load의 coalesce 정책(§5 마지막 문단)이 본 결정의 핵심 mechanism. GSM은 frame 내 SaveAsync 호출을 묶기만 함. Day 13 시나리오 = long-gap + dream + narrative 동시 충족.
- **Control Manifest Rules**: Foundation Layer Rules §Required Patterns — "Compound 이벤트는 `GSM->BeginCompoundEvent(PrimaryReason)` / `EndCompoundEvent()` boundary로 batch". Foundation Layer Rules §Forbidden — "Never write sequence-level atomicity API in Save/Load layer".

## Acceptance Criteria

- **AC-GSM-19-Compound** (GDD 확장, 본 story에서 신규 추가) [`INTEGRATION`/BLOCKING] — Day 13 compound scenario (long-gap + dream + narrative) 시뮬 시, 3+ in-memory mutation이 단일 disk commit으로 coalesce. `SaveAsync` 호출 카운트 = 1 (worker thread commit), in-memory 호출 카운트 = 3+.

## Implementation Notes

1. **`UMossGameStateSubsystem` API 추가** (ADR-0009 §API):
   ```cpp
   public:
       void BeginCompoundEvent(ESaveReason PrimaryReason);
       void EndCompoundEvent();
       bool IsInCompoundEvent() const { return bCompoundEventActive; }
   private:
       bool bCompoundEventActive = false;
       ESaveReason CompoundPrimaryReason;
       int32 CoalescedSaveCount = 0;
   ```
2. **`IsCompoundEventTrigger()` 판단 로직** (ADR-0009 §IsCompoundEventTrigger):
   - 조건 ≥2 동시 만족 시 compound:
     - `ADVANCE_WITH_NARRATIVE` 수신 + narrative cap 여유
     - `FOnDreamTriggered` 도착 예정 (Dream Trigger가 이번 frame에 평가할 card)
     - `bWithered` 해제 pending
3. **사용 패턴 (`OnTimeActionResolved`)**:
   - ADR-0009 §GSM 사용 패턴 pseudocode 그대로 follow.
   - `if (Action == ADVANCE_WITH_NARRATIVE && IsCompoundEventTrigger()) { BeginCompoundEvent(ENarrativeEmitted); }` → 기존 분기 로직 실행 → `if (bCompoundEventActive) { EndCompoundEvent(); }`.
4. **Save/Load coalesce 의존**: Save/Load epic의 coalesce 구현(ADR-0009 §Save/Load 측 변경 — 최소)이 선행되어야 함. 본 story는 Save/Load의 coalesce를 "trust" — mock으로 `SaveAsync` counter만 검증.
5. **`CoalescedSaveCount` 디버그 로그**: `EndCompoundEvent()`에서 `UE_LOG(LogGSM, Verbose, ...)` — 실제 commit 카운트와 대조 가능.

## Out of Scope

- Save/Load coalesce 구현 자체 (Foundation Save/Load epic — ADR-0009 의존)
- Dream selection + narrative publish (Dream Trigger epic, Time System epic)
- Day 21 E13 (story 004 — Stage 2 handler가 이미 처리)

## QA Test Cases

**Test Case 1 — AC-GSM-19-Compound Day 13 scenario**
- **Given**: `Record.DayIndex = 13`, long-gap 상태 (`bWithered = true`), Mock Dream Trigger가 이번 카드 evaluate 시 Dream 반환, narrative cap 여유.
- **When**: 플레이어 카드 건넴 → Stage 1 발행 → Growth handler → Stage 2 → Dream Trigger evaluate + Dream publish → Narrative emit.
- **Then**: 
  - `BeginCompoundEvent(ENarrativeEmitted)` 호출 1회.
  - In-memory `SaveAsync(E*)` 호출 ≥3회 (withered clear, dream receive, narrative emit).
  - Worker thread commit 횟수 = 1 (coalesce 검증 — Mock Save/Load counter).
  - `EndCompoundEvent()` 호출 1회.

**Test Case 2 — Non-compound day (single SaveAsync)**
- **Given**: 일반 Day 5 카드 건넴 (Dream 없음, narrative 없음).
- **When**: Stage 1 → Stage 2 → GSM Offering→Waiting.
- **Then**: `IsCompoundEventTrigger() == false`. `BeginCompoundEvent` 미호출. `SaveAsync` 호출 1회 (Growth 내부).

**Test Case 3 — BeginCompoundEvent 재진입 방지**
- **Given**: `bCompoundEventActive = true`.
- **When**: `BeginCompoundEvent(ECardOffered)` 재호출.
- **Then**: Warning 로그 발행. 상태 불변 (`CompoundPrimaryReason` 최초 값 유지).

## Test Evidence

- [ ] `tests/integration/game-state/compound_event_day13_test.cpp` — AC-GSM-19-Compound 시나리오
- [ ] `tests/unit/game-state/begin_end_compound_test.cpp` — API 재진입 방지

## Dependencies

- **Depends on**: story-004 (Stage 2 subscription), story-007 (E4 Dream defer coordination), story-008 (withered persistence)
- **Unlocks**: Save/Load coalesce integration (Foundation Save/Load epic)
