# Story 005: PendingDreamId 영속화 + Load 복구 재발행 + OnGameStateChanged 클리어

> **Epic**: feature-dream-trigger
> **Layer**: Feature
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/dream-trigger-system.md` §Interactions §5 (PendingDreamId 클리어) + EC-4/5 + AC-DTS-13/14
- **TR-ID**: TR-dream-001 (PendingDreamId retry UX — OQ-DTS-1)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) (OQ-DTS-2 RESOLVED 2026-04-18)
- **Engine Risk**: LOW
- **Engine Notes**: OQ-DTS-2 (PendingDreamId 클리어 책임) — Dream Trigger가 `FOnGameStateChanged` 구독, `OldState == Dream` 감지 시 즉시 클리어.

## Acceptance Criteria

1. **AC-DTS-13** (INTEGRATION/BLOCKING) — 꿈 트리거 완료 후 `FDreamState = {UnlockedDreamIds: ["DreamA"], LastDreamDay: 5, DreamsUnlockedCount: 1}` 세이브 → 재시작 → Load 완료 → 복원된 FDreamState 저장 전과 동일, `DreamsUnlockedCount == 1`, 게이트 B 4회 남음
2. **AC-DTS-14** (INTEGRATION/BLOCKING) — `PendingDreamId = "DreamX"` (미표시 꿈) 세이브 → 재시작 → GSM Waiting 안정화 → Dream Trigger `FOnDreamTriggered("DreamX")` 재발행 1회 (EC-5)
3. OnGameStateChanged handler (OQ-DTS-2): `OldState == EGameState::Dream` 수신 시 → `FDreamState.PendingDreamId = NAME_None` + `SaveAsync(EDreamReceived)` 호출
4. 종료 시 `OldState == Dream && NewState == Farewell`도 동일 처리
5. EC-4 동일 세션 `ADVANCE_WITH_NARRATIVE` + Dream 동시 대기: Dream Trigger는 연기 인지하지 않음, `FOnDreamTriggered` 발행 + SaveAsync 완료 후 Dormant 복귀. GSM이 연기 결정.

## Implementation Notes

- **Load 복구** (GDD §Load 복구):
  ```cpp
  void UDreamTriggerSubsystem::OnLoadCompleteHandler(const FMossSaveSnapshot& Snapshot) {
      FDreamState = Snapshot.DreamState;
      if (!FDreamState.PendingDreamId.IsNone()) {
          InternalState = Triggered;
          OnDreamTriggered.Broadcast(FDreamState.PendingDreamId);  // 재발행 (AC-DTS-14)
      } else {
          InternalState = Dormant;
      }
  }
  ```
- **OnGameStateChanged handler** (OQ-DTS-2 RESOLVED):
  ```cpp
  void UDreamTriggerSubsystem::OnGameStateChangedHandler(
      EGameState OldState, EGameState NewState) {
      if (OldState == EGameState::Dream) {
          FDreamState.PendingDreamId = NAME_None;
          GetSaveLoad()->SaveAsync(ESaveReason::EDreamReceived);
      }
  }
  ```
- **EC-4 연기 분리** (GDD §Interactions §5 + CR-5): Dream Trigger는 `FDreamState` 갱신 + SaveAsync + Broadcast 완료 후 Dormant 복귀. GSM이 Stillness Budget Narrative slot 점유 확인 → 연기는 GSM 책임. `PendingDreamId`는 다음 세션 재시도용.

## Out of Scope

- GSM 연기 구현 (core-game-state-machine)
- Stillness Budget Narrative 선점 (feature-stillness-budget)
- Day 21 강제 트리거 (Story 006)

## QA Test Cases

**Given** `FDreamState = {UnlockedDreamIds: ["DreamA"], LastDreamDay: 5, DreamsUnlockedCount: 1, PendingDreamId: NAME_None}` 세이브, **When** 재시작 + Load, **Then** 모든 필드 복원, Dormant 상태 (AC-DTS-13).

**Given** `FDreamState.PendingDreamId = "DreamX"` 세이브, **When** 재시작 + Load + GSM Waiting 안정화, **Then** `FOnDreamTriggered("DreamX")` 1회 재발행 (AC-DTS-14).

**Given** PendingDreamId = "DreamX" + GSM이 Waiting→Dream 전환 후 Dream→Waiting, **When** OnGameStateChangedHandler 수신, **Then** `FDreamState.PendingDreamId = NAME_None` + SaveAsync 호출 (OQ-DTS-2).

**Given** Dream 상태에서 Farewell 전환 (GSM E13), **When** OnGameStateChangedHandler, **Then** PendingDreamId 클리어 (OldState==Dream 공통 경로).

## Test Evidence

- **Integration test**: `tests/integration/Dream/test_pending_dream_recovery.cpp` (AC-DTS-14)
- **Roundtrip test**: `tests/integration/Dream/test_fdreamstate_persistence.cpp` (AC-DTS-13)

## Dependencies

- Story 001 (Subsystem + FOnGameStateChanged 구독)
- Story 004 (FDreamState 갱신 경로)
- `foundation-save-load` (FMossSaveSnapshot + OnLoadComplete)
- `core-game-state-machine` (FOnGameStateChanged delegate)
