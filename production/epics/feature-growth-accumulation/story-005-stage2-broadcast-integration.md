# Story 005: Stage 2 FOnCardProcessedByGrowth 발행 + Day 21 통합 시퀀스

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §CR-1 step 7 + §Interactions §4b + AC-GAE-02
- **TR-ID**: TR-growth-004 (Stage 2 publisher), TR-growth-006 (Day 21 accuracy)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) — **2단계 Delegate 패턴 핵심**
- **Engine Risk**: LOW
- **Engine Notes**: UE Multicast Delegate 등록 순서 비공개 계약 — 본 story는 C++ 호출 스택 순서로 결정성 보장. Stage 2 발행 = CR-1 handler의 **마지막 statement**.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — **2단계 Delegate 패턴**: Stage 1 FOnCardOffered (Card→Growth, MBC), Stage 2 FOnCardProcessedByGrowth (Growth→GSM, Dream Trigger)
  - Feature Layer §Required Patterns — "Growth `OnCardOfferedHandler`의 **마지막 statement**는 `OnCardProcessedByGrowth.Broadcast(Result)`"
  - Feature Layer §Required Patterns — `FGrowthProcessResult` struct payload: `FName CardId` + `bool bIsDay21Complete` + `FName FinalFormId`
  - Feature Layer §Forbidden Approaches — "Never subscribe MBC to Stage 2" (cross-check)
  - Feature Layer §Forbidden Approaches — "Never use UE Multicast Delegate `AddDynamicWithPriority`"

## Acceptance Criteria

1. **AC-GAE-02** (CODE_REVIEW/BLOCKING) — CR-1 구현 소스에서 Step 2-5 + step 6 + **step 7 `OnCardProcessedByGrowth.Broadcast` (ADR-0005 Stage 2)** 사이에 async 분기 0건
2. `FGrowthProcessResult` USTRUCT 정의 — `FName CardId`, `bool bIsDay21Complete`, `FName FinalFormId`
3. `FOnCardProcessedByGrowth` delegate 선언: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardProcessedByGrowth, const FGrowthProcessResult&, Result)`
4. CR-1 handler 마지막 statement로 `OnCardProcessedByGrowth.Broadcast(Result)` 호출 — grep 검증: `grep "OnCardProcessedByGrowth.Broadcast" Growth*.cpp` = 1건, CR-1 handler 내 위치
5. Day 21 통합 시퀀스 (AC-CS-16 cross-epic): `ConfirmOffer(CardId)` → Stage 1 발행 → Growth CR-1 전체 (태그 가산 + Day 21 체크 + EvaluateFinalForm + `FOnFinalFormReached` + `SaveAsync(ECardOffered)`) → **마지막으로** Stage 2 발행 → GSM + Dream Trigger subscriber 실행
6. Day 21 CR-5 경로 통합: `if (CurrentDayIndex == GameDurationDays && TotalCardsOffered > 0) { FName FormId = EvaluateFinalForm(); ... Result.bIsDay21Complete = true; Result.FinalFormId = FormId; }`
7. Day 21 외: `Result.bIsDay21Complete = false`, `Result.FinalFormId = NAME_None`

## Implementation Notes

- **CR-1 step 완성본** (ADR-0005 §Key Interfaces):
  ```cpp
  void UGrowthAccumulationSubsystem::OnCardOfferedHandler(FName CardId) {
      if (InternalState != Accumulating) return;  // EC-6
      const FGiftCardRow* Row = Pipeline->GetCardRow(CardId);
      if (!Row) { UE_LOG(LogGrowth, Warning, TEXT("...")); return; }  // EC-8

      // Step 2: 태그 가산 (F1)
      for (const FName& Tag : Row->Tags) { /* W_card × W_cat */ }
      // Step 3-4: LastCardDay, TotalCardsOffered
      FGrowthState.LastCardDay = CurrentDayIndex;
      FGrowthState.TotalCardsOffered += 1;
      // Step 5: MaterialHints 재계산 (Story 006)

      // CR-5 Day 21 분기
      FGrowthProcessResult Result;
      Result.CardId = CardId;
      if (CurrentDayIndex == GameDurationDays) {
          FName FormId = EvaluateFinalForm();  // F3 argmax
          FGrowthState.FinalFormId = FormId;
          Result.bIsDay21Complete = true;
          Result.FinalFormId = FormId;
          OnFinalFormReached.Broadcast(FormId);
          InternalState = Resolved;  // EC-6 future guard
      }

      // Step 6: SaveAsync (동일 call stack)
      GetSaveLoadSubsystem()->SaveAsync(ESaveReason::ECardOffered);

      // Step 7: Stage 2 발행 (마지막 statement)
      OnCardProcessedByGrowth.Broadcast(Result);
  }
  ```
- **`UPROPERTY(BlueprintAssignable)`** on Stage 2 delegate — BP 노출
- **호출 스택 순서 보장 (ADR-0005)**: Stage 2가 마지막 statement → GSM/Dream handler 실행 시점에 FinalFormId 확정 + SaveAsync 큐 등록 완료 상태
- **MBC 구독 경로 변경 없음**: MBC는 Stage 1 구독 유지 — 본 story 범위 외지만 코드 리뷰에서 확인 (grep `OnCardOffered.AddDynamic` + MBC = 매치 보존)

## Out of Scope

- GSM subscriber 구현 (core-game-state-machine epic)
- Dream Trigger subscriber 구현 (feature-dream-trigger)
- MBC Stage 1 구독 유지 검증 (core-moss-baby-character — cross-check만)
- Card System ConfirmOffer + Stage 1 publisher (feature-card-system)

## QA Test Cases

**Given** Accumulating + Day 5 + `TagVector={}`, **When** `FOnCardOffered("Card_A")` (non-Day 21), **Then** Stage 2 Result: `{CardId="Card_A", bIsDay21Complete=false, FinalFormId=NAME_None}`, Broadcast 1회.

**Given** DayIndex == GameDurationDays(21) + FormA/FormB assets, **When** Day 21 `FOnCardOffered`, **Then** Stage 2 Result: `bIsDay21Complete=true`, `FinalFormId="FormA"`, Broadcast 1회 (마지막 statement), `FOnFinalFormReached("FormA")` 이미 발행됨, `SaveAsync(ECardOffered)` 큐 등록 완료.

**Given** Stage 2 구독자 GSM + Dream mock, **When** Day 21 Stage 2 broadcast, **Then** GSM Farewell P0 전환 + Dream evaluation 스킵 (bIsDay21Complete 분기).

**Given** CR-1 handler 구현 소스, **When** `grep "OnCardProcessedByGrowth.Broadcast" Growth*.cpp`, **Then** 매치 1건, CR-1 handler 마지막 statement (AC-GAE-02 grep).

## Test Evidence

- **Integration test**: `tests/integration/Day21Sequence/test_card_to_final_form_to_farewell.cpp`
- **CODE_REVIEW**: CR-1 step 순서 + Stage 2 broadcast 위치 검증 (AC-GAE-02 expanded)
- **Grep gate**: `grep "OnCardProcessedByGrowth.Broadcast" Source/MadeByClaudeCode/` = Growth 파일만, 1건

## Dependencies

- Story 002 (CR-1 atomic chain)
- Story 004 (CR-5 EvaluateFinalForm)
- `feature-card-system` Stage 1 publisher (cross-epic)
- `core-game-state-machine` Stage 2 subscriber (cross-epic — integration 테스트 시)
- `feature-dream-trigger` Stage 2 subscriber (cross-epic — integration 테스트 시)
