# Story 004 — `FOnGameStateChanged` Delegate 브로드캐스트 + Stage 2 구독

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Downstream 소비자 + §Card System/Growth §Interactions 4 (ADR-0005 Stage 2)
- **TR-ID**: TR-gsm-006 (AC-GSM-16 Day 21 Farewell P0 전환 순서)
- **Governing ADR**: **ADR-0005** §Decision §Stage 2 — GSM은 `FOnCardProcessedByGrowth` Stage 2 전용. Stage 1 `FOnCardOffered` 직접 구독 금지.
- **Engine Risk**: LOW
- **Engine Notes**: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameStateChanged, EGameState, OldState, EGameState, NewState)` + `UPROPERTY(BlueprintAssignable)`. Blueprint 프로토타이핑 지원. `FOnGameStateChanged`는 MPC 블렌드 **시작 시점**에 발행 (블렌드 완료가 아님).
- **Control Manifest Rules**: Core Layer Rules §Required Patterns — "GSM이 Offering→Waiting / Farewell P0 전환은 `FOnCardProcessedByGrowth` (Stage 2)만 트리거". Core Layer Rules §Forbidden — "Never subscribe GSM to Stage 1 `FOnCardOffered`".

## Acceptance Criteria

- **AC-GSM-16** [`AUTOMATED`/BLOCKING] — `DayIndex == GameDurationDays` + Offering 완료 시, Waiting 진입 직후 조건 평가에서 `FOnDayAdvanced` 미발행 + 즉시 Farewell P0 전환 (E13).
- **AC-GSM-18** [`INTEGRATION`/BLOCKING] — 임의 상태 전환 시 `FOnGameStateChanged(OldState, NewState)` 브로드캐스트 1회 발생, MPC `ColorTemperatureK` 목표값 방향 변화 시작, MVP 필수 downstream 3개(Lumen Dusk, Card Hand UI, Dream Journal UI) 콜백 수신 각 1회. 브로드캐스트는 MPC Lerp 시작 이전 또는 동시 — 역순이면 실패.

## Implementation Notes

1. **Delegate 선언**: `MossGameStateSubsystem.h` 상단에 `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameStateChanged, EGameState, OldState, EGameState, NewState);` + `UPROPERTY(BlueprintAssignable) FOnGameStateChanged OnGameStateChanged;`
2. **Stage 2 구독**:
   - Growth Subsystem의 `FOnCardProcessedByGrowth` delegate를 `Initialize`에서 `AddDynamic(this, &UMossGameStateSubsystem::OnCardProcessedByGrowth)` 구독.
   - **금지**: `OnCardOffered.AddDynamic(...)` — Stage 1 직접 구독 금지. CI grep 검증: `grep -rn "OnCardOffered.AddDynamic" Source/MadeByClaudeCode/Core/GameState/` = 0건.
3. **`OnCardProcessedByGrowth(const FGrowthProcessResult& Result)`**:
   - `if (Result.bIsDay21Complete) { RequestStateTransition(EGameState::Farewell, EGameStatePriority::P0); return; }`
   - else: `if (CurrentState == EGameState::Offering) { RequestStateTransition(EGameState::Waiting, EGameStatePriority::P1); }`
4. **브로드캐스트 순서**: `RequestStateTransition` 성공 시 (1) `OldState = CurrentState` 저장 → (2) `CurrentState = NewState` 갱신 → (3) `OnGameStateChanged.Broadcast(OldState, NewState)` 발행 → (4) MPC Lerp 시작 (TickMPC가 다음 프레임부터 새 target 참조). Broadcast가 Lerp 시작보다 **이전 또는 동시** — 역순 시 AC-GSM-18 실패.
5. **Downstream 초기 구독 등록**: 본 story 범위 아님 — 각 downstream (MBC story, Lumen Dusk story, Card Hand UI story)이 자체적으로 `OnGameStateChanged.AddDynamic` 호출.

## Out of Scope

- Stage 1 `FOnCardOffered` 구독은 MBC story (core-moss-baby-character)
- Growth의 Stage 2 delegate publish는 Growth epic (feature-growth-accumulation)
- Compound event coalesce (story 011)

## QA Test Cases

**Test Case 1 — AC-GSM-16 Day 21 P0**
- **Given**: `CurrentState = Offering`, `DayIndex = 21`, Growth broadcasts `FOnCardProcessedByGrowth({CardId, bIsDay21Complete=true, FinalFormId="Moss_Final_A"})`.
- **When**: GSM의 `OnCardProcessedByGrowth` handler 실행.
- **Then**: `CurrentState` transition chain `Offering → Farewell` (Waiting 우회). `FOnDayAdvanced` 이벤트 미발행 (Dream Trigger epoxy 방지).

**Test Case 2 — AC-GSM-18 Broadcast order + downstream count**
- **Given**: Mock subscribers (3개) `AddDynamic(On*Fired)` 등록. `CurrentState = Offering`, Growth broadcast `FOnCardProcessedByGrowth({bIsDay21Complete=false})`.
- **When**: Handler 실행 완료 + 다음 frame TickMPC 1회 실행.
- **Then**: 
  - 3 Mock subscribers 모두 `OnGameStateChangedFired` 카운트 = 1.
  - Broadcast 시각 T_bc, 첫 MPC SetScalar 시각 T_mpc 측정 시 `T_bc <= T_mpc`.

**Test Case 3 — Stage 1 subscription ban grep**
- **Setup**: `grep -rn "OnCardOffered\.AddDynamic\|OnCardOffered\.AddUObject" Source/MadeByClaudeCode/Core/GameState/`
- **Verify**: 매치 0건 — GSM은 Stage 1 `FOnCardOffered` 직접 구독 금지 (ADR-0005).
- **Pass**: grep empty.

## Test Evidence

- [ ] `tests/integration/game-state/stage2_day21_test.cpp` — AC-GSM-16 Day 21 P0 시퀀스
- [ ] `tests/integration/game-state/broadcast_order_test.cpp` — AC-GSM-18 순서 + downstream count
- [ ] `tests/unit/game-state/stage1_ban_grep.md` — CI grep 문서화

## Dependencies

- **Depends on**: story-001 (Subsystem skeleton), story-002 (transition table), story-003 (MPC tick)
- **Unlocks**: story-005 (Light Actor), story-008 (compound Day 21 E13)
