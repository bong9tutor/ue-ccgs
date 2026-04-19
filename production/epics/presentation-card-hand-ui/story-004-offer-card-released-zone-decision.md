# Story 004 — `OnOfferCardReleased` — Offer Zone 판정 + `ConfirmOffer` 분기

> **Epic**: [presentation-card-hand-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/card-hand-ui.md](../../../design/gdd/card-hand-ui.md) §3A CR-CHU-2 §3-4 (릴리스) + §3A CR-CHU-4 건넴 확정 시퀀스 + §3A CR-CHU-5 드래그 취소 + §5 EC-CHU-1, EC-CHU-2
- **TR-ID**: TR-chu-001 + TR-chu-003 (ConfirmOffer false → 복귀)
- **Governing ADR**: [ADR-0008](../../../docs/architecture/adr-0008-umg-drag-implementation.md) §Subscription Pattern `OnOfferCardReleased` 구현
- **Engine Risk**: LOW — `ConfirmOffer` 호출 + Stillness Release 모두 UE 5.6 stable
- **Engine Notes**: `IA_OfferCard` `ETriggerEvent::Completed`는 Mouse LMB 릴리스 또는 Gamepad 버튼 릴리스 시 발행. Release 시점 pointer 위치는 `CurrentPointerPos` (story 003에서 갱신). `ConfirmOffer` 반환은 sync (Card System Stage 1 delegate 전후 구분 없음). Stage 1 `FOnCardOffered` broadcast가 여기에서 발생 — MBC Reacting 즉각 진입 + Growth 태그 가산 (ADR-0005 Stage 1).
- **Control Manifest Rules**:
  - Presentation Layer Rules §Required Patterns — `IA_OfferCard` Completed → `OnOfferCardReleased` (확정/취소 분기) (ADR-0008)
  - Feature Layer Rules §Required Patterns — "Card가 유일 publisher" — UI는 `Card->ConfirmOffer`만 호출, `FOnCardOffered.Broadcast`는 Card System 내부 (ADR-0005)
  - Feature Layer Rules §Forbidden Approaches — "Never broadcast `FOnCardOffered` outside Card System"

## Acceptance Criteria

- **AC-CHU-05** [`INTEGRATION`/BLOCKING] — Offer Zone 내 릴리스 시 `ConfirmOffer(CardId)` 호출 → `true` 반환. `FOnCardOffered(CardId)` 발행 1회 (Card System 내부). State → `Offering`. 건넴 애니메이션 시작.
- **AC-CHU-06** [`INTEGRATION`/BLOCKING] — Offer Zone 밖 릴리스 시 `ConfirmOffer` 미호출. 카드 `CardReturnDurationSec`(0.3s) 내 원래 슬롯 복귀. State → `Idle`. 재선택 가능.
- **TR-chu-003** / **EC-CHU-2** [`INTEGRATION`/BLOCKING] — `ConfirmOffer` 가 `false` 반환 시 카드 원래 슬롯 복귀 + `UE_LOG(Warning, "ConfirmOffer failed for CardId=%s")`. State → `Idle`. 재시도 허용.

## Implementation Notes

1. **`OnOfferCardReleased` 본문** (ADR-0008 §Subscription Pattern 인용):
   ```cpp
   void UCardHandWidget::OnOfferCardReleased(const FInputActionValue& Value) {
       if (State != ECardHandUIState::Dragging || !DraggedCard.IsValid()) return;
       const bool bInZone = IsInOfferZone(CurrentPointerPos);
       if (bInZone) {
           const FName CardId = DraggedCard->GetCardId();
           const bool bAccepted = Card->ConfirmOffer(CardId);
           if (bAccepted) {
               State = ECardHandUIState::Offering;
               StartConfirmAnimation();  // CR-CHU-4
           } else {
               State = ECardHandUIState::Idle;
               StartReturnAnimation();  // EC-CHU-2 ConfirmOffer false
               UE_LOG(LogCardHandUI, Warning,
                      TEXT("ConfirmOffer failed for CardId=%s"), *CardId.ToString());
           }
       } else {
           State = ECardHandUIState::Idle;
           StartReturnAnimation();  // EC-CHU-1 Offer Zone 밖
       }
       Stillness->Release(MotionHandle);
       DraggedCard->SetEmissive(0.0f);
       DraggedCard.Reset();
   }
   ```

2. **`StartReturnAnimation`** — CR-CHU-5 복귀:
   - SmoothStep, `CardReturnDurationSec` (기본 0.3s, config asset knob).
   - Stillness Budget `Motion` Standard 재요청 (복귀 애니메이션 별개 슬롯).
   - Timeline 또는 UMG Animation — ReducedMotion 시 생략 (story 006).
   - 완료 콜백: `bIsReturning = false` 클리어.

3. **`StartConfirmAnimation`** — CR-CHU-4 건넴 확정 (stub, 완성은 story 005):
   - 카드 유리병 방향 이동 + 페이드 아웃 0.5s.
   - 완료 후 GSM `FOnGameStateChanged(Offering, Waiting)` 수신으로 Hiding 전개 (story 005).

4. **Card System 통합**:
   - `Card = GetGameInstance()->GetSubsystem<UCardSystemSubsystem>();` — NativeConstruct 시 cache.
   - `ConfirmOffer(FName CardId)` → `bool`: Card System 내부에서 `bHandOffered = true` 설정 + `FOnCardOffered.Broadcast(CardId)` 발행 (ADR-0005 Stage 1).

## Out of Scope

- 건넴 확정 애니메이션 세부 연출 (story 005에서 CR-CHU-4 완성)
- State machine 전체 전환 표 (story 005 — Hide 시퀀스 포함)
- GSM `FOnGameStateChanged` 구독 (story 005)
- ReducedMotion 분기 (story 006)
- EC-CHU-4 강제 이탈 처리 (story 005)
- Gamepad Hold 완료 즉시 ConfirmOffer (CR-CHU-9 — VS)

## QA Test Cases

**Test Case 1 — Zone 내 릴리스 + ConfirmOffer true (AC-CHU-05)**
- **Setup**: State = `Dragging`, DraggedCard = CardA, `CurrentPointerPos` = JarScreenPos. Mock Card System returns `true` for `ConfirmOffer("Card_A")`.
- **Verify**:
  - `Card->ConfirmOffer` 호출 1회 with `CardId = "Card_A"`.
  - `FOnCardOffered` broadcast 카운트 == 1 (Card System 내부 — Stage 1).
  - `GetState() == ECardHandUIState::Offering`.
  - `Stillness->GetActiveCount(Motion)` decrement됨 (Release 호출).
- **Pass**: 4 조건 모두 충족.

**Test Case 2 — Zone 밖 릴리스 (AC-CHU-06)**
- **Setup**: State = `Dragging`, CurrentPointerPos = (0, 0). Card System mock 호출 카운트 0.
- **Verify**:
  - `Card->ConfirmOffer` 호출 0회.
  - `GetState() == ECardHandUIState::Idle`.
  - `StartReturnAnimation` 호출 확인 (flag 또는 mock).
  - 0.3s 후 카드 슬롯 원위치 (위젯 좌표 ≈ slot origin).
- **Pass**: 4 조건 모두 충족.

**Test Case 3 — ConfirmOffer false (TR-chu-003, EC-CHU-2)**
- **Setup**: State = `Dragging`, CurrentPointerPos = JarScreenPos. Mock Card System returns `false` for `ConfirmOffer`.
- **Verify**:
  - `GetState() == ECardHandUIState::Idle` (Offering 진입 안 함).
  - `UE_LOG(Warning)` 메시지 "ConfirmOffer failed" 발행 1건.
  - `StartReturnAnimation` 호출 확인.
- **Pass**: 3 조건 모두 충족.

## Test Evidence

- [ ] `tests/integration/ui/card_hand_confirm_offer_accepted_test.cpp` — AC-CHU-05.
- [ ] `tests/integration/ui/card_hand_offer_zone_miss_test.cpp` — AC-CHU-06.
- [ ] `tests/integration/ui/card_hand_confirm_offer_rejected_test.cpp` — TR-chu-003 / EC-CHU-2.
- [ ] CI grep: `grep "OnCardOffered.Broadcast" Source/MadeByClaudeCode/UI/CardHand*` = 0건 (ADR-0005 — Card가 유일 publisher).

## Dependencies

- **Depends on**: story-001 (골격), story-002 (Dragging 진입), story-003 (PointerMove CurrentPointerPos 갱신 + IsInOfferZone). Feature Card System (`ConfirmOffer` API).
- **Unlocks**: story-005 (state machine + Hide 전환), story-006 (ReducedMotion 분기 통합).
