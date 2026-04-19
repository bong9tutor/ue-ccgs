# Story 008: FGrowthState 영속화 + Load 복구 (EC-17 재발행)

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §States and Transitions (로드 복구) + §EC-16/17 + AC-GAE-10/19/20
- **TR-ID**: TR-growth-001 (persistence path)
- **Governing ADR**: [ADR-0009](../../../docs/architecture/adr-0009-compound-event-sequence-atomicity.md) (per-trigger atomicity — Save/Load)
- **Engine Risk**: LOW
- **Engine Notes**: `FOnLoadComplete` 수신 시 `FGrowthState` 복원 → FinalFormId 유무 분기 → 상태 결정 + EC-17 재발행.
- **Control Manifest Rules**:
  - Foundation Layer §Required Patterns — "Save/Load는 per-trigger atomicity만 보장"
  - Global Rules §Cross-Cutting Constraints — "USaveGame 필드는 전부 `UPROPERTY()` 표기"

## Acceptance Criteria

1. **AC-GAE-10** (INTEGRATION/BLOCKING) — `TagVector = {Season.Spring: 3.0}`, `CurrentStage = Growing`, `TotalCardsOffered = 3`, `LastCardDay = 2` 저장 → 앱 재시작 → Load 완료 → 모든 필드 저장 전 값과 동일, Growth가 Growing Accumulating 상태로 복원
2. **AC-GAE-19** (INTEGRATION/BLOCKING) — `FinalFormId = "FormA"`, `CurrentStage = Mature` 저장 → 앱 재시작 → Load 완료 → Resolved 직행, `FOnFinalFormReached("FormA")` 1회 재발행, `FOnGrowthStageChanged` 추가 미발행 (EC-17)
3. **AC-GAE-20** (MANUAL/ADVISORY) — `TagVector = {Season.Spring: 2.0}`, `TotalCardsOffered = 2` 상태에서 카드 1장 건넴 직후 Task Manager 강제 종료 → 앱 재시작 → `TotalCardsOffered ≥ 0`, `TagVector` 모든 값 ≥ 0.0f, `TotalCardsOffered` 차이 ≤ 1 (EC-16)
4. `FOnLoadComplete` subscriber 구현 — `FGrowthState` 복원 후 상태 결정:
   - `FinalFormId.IsNone()` + `CurrentStage != FinalForm` → Accumulating 상태 진입 (CurrentStage 존중)
   - `!FinalFormId.IsNone()` → Resolved 직행 + `OnFinalFormReached.Broadcast(FinalFormId)` 1회 (EC-17)
5. `FGrowthState` 모든 필드 `UPROPERTY()` 표기 (GC 안전성)

## Implementation Notes

- **Load 복구 분기** (GDD §States and Transitions + EC-17):
  ```cpp
  void UGrowthAccumulationSubsystem::OnLoadCompleteHandler(const FMossSaveSnapshot& Snapshot) {
      FGrowthState = Snapshot.GrowthState;  // 복원
      if (!FGrowthState.FinalFormId.IsNone()) {
          InternalState = Resolved;
          OnFinalFormReached.Broadcast(FGrowthState.FinalFormId);  // EC-17 재발행
      } else {
          InternalState = Accumulating;
          // CurrentStage는 복원된 값 존중 — 이벤트 미발행 (이미 과거 세션에서 발행됨)
      }
  }
  ```
- **EC-16 완화** (AC-GAE-20 ADVISORY): CR-1 atomicity + SaveAsync per-trigger atomicity로 "직전 완료 저장" 시점까지 복원 — 최대 1장 누락. 별도 이중 쓰기 불필요.
- **EC-17 재발행**: MBC가 최종 형태 메시를 화면에 표시하려면 이벤트 필요 — FinalFormId 유무만으로 분기, `FOnGrowthStageChanged`는 추가 미발행 (이미 Mature 상태)
- `SaveLoadSubsystem->OnLoadComplete.AddDynamic(this, &OnLoadCompleteHandler)` — `Initialize` 내 등록

## Out of Scope

- Save/Load 기본 구현 (foundation-save-load)
- `FMossSaveSnapshot` POD whitelist 검증 (Save/Load 소유)
- Corrupted save handling (Save/Load 소유)

## QA Test Cases

**Given** 저장 `{TagVector: {Season.Spring: 3.0}, CurrentStage: Growing, TotalCardsOffered: 3, LastCardDay: 2}`, **When** 앱 재시작 + Load 완료, **Then** 모든 필드 원복, InternalState = Accumulating, CurrentStage = Growing, 이벤트 미발행 (AC-GAE-10).

**Given** 저장 `{FinalFormId: "FormA", CurrentStage: Mature}`, **When** Load 완료, **Then** InternalState = Resolved, `FOnFinalFormReached("FormA")` 1회 재발행, `FOnGrowthStageChanged` 미발행 (AC-GAE-19 / EC-17).

**Given** `{TagVector: {Season.Spring: 2.0}, TotalCardsOffered: 2}` 저장 후 카드 1장 건넴 중 강제 종료, **When** 재시작 + Load, **Then** `TotalCardsOffered ≥ 2` 또는 `3`, `TagVector` 일관 (EC-16 / AC-GAE-20 manual).

## Test Evidence

- **Integration test**: `tests/integration/Growth/test_persistence_roundtrip.cpp` (AC-GAE-10)
- **EC-17 test**: `tests/integration/Growth/test_load_final_form_recovery.cpp` (AC-GAE-19)
- **Manual QA doc**: `production/qa/evidence/growth-force-kill-recovery.md` (AC-GAE-20)

## Dependencies

- Story 001 (Subsystem Initialize + FOnLoadComplete 구독)
- Story 004 (FOnFinalFormReached delegate)
- `foundation-save-load` Epic (FMossSaveSnapshot + OnLoadComplete)
