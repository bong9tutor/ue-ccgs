# Story 002 — `OnOfferCardHoldStarted` — Dragging 상태 진입 + Stillness Request

> **Epic**: [presentation-card-hand-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/card-hand-ui.md](../../../design/gdd/card-hand-ui.md) §3A CR-CHU-2 드래그-투-오퍼 (Hold 시작) + §3B States (Idle → Dragging) + §3C Interactions 4 (Stillness Budget Motion Standard)
- **TR-ID**: TR-chu-001 (드래그 시작 경로)
- **Governing ADR**: [ADR-0008](../../../docs/architecture/adr-0008-umg-drag-implementation.md) §Subscription Pattern `OnOfferCardHoldStarted` 구현
- **Engine Risk**: LOW — `USlateBlueprintLibrary::GetAbsoluteCoordinatesFromLocal` + `FGeometry::IsUnderLocation`은 UE 5.6 stable
- **Engine Notes**: `IA_OfferCard` Hold `ETriggerEvent::Triggered`는 Foundation Input Abstraction의 Hold threshold(Mouse 0.15s / Gamepad 0.20s) 초과 시 발행. Hold 판정 자체는 Input Abstraction 책임 — Card Hand UI는 Triggered 시점 이후 드래그 처리만 담당. Hit test는 `GetCachedGeometry()` 기반 — 위젯이 뷰포트에 추가된 후에만 유효.
- **Control Manifest Rules**:
  - Presentation Layer Rules §Required Patterns — "Drag state: Idle → Dragging → (Offering | Idle) — Dragging 중 다른 OnOfferCardHoldStarted 무시 (EC-CHU-8)" (ADR-0008 source)
  - Presentation Layer Rules §Performance Guardrails — Hold threshold Mouse 0.15s / Gamepad 0.20s (ADR-0008, input-abstraction-layer.md)
  - Feature Layer Rules §Required Patterns — Stillness Budget Motion Standard 요청 (ADR-0007)

## Acceptance Criteria

- **AC-CHU-04** [`MANUAL`/ADVISORY, partial] — 카드 위에서 LMB 0.15s 유지 후 Hold 인식 시 카드가 드래그 모드 진입 (실제 커서 추적은 story 003 — PointerMove). 본 story는 상태 진입만 검증.
- **AC-CHU-08-guard** [`AUTOMATED`/BLOCKING] — EC-CHU-8 대응: `State == Dragging`일 때 `OnOfferCardHoldStarted` 재호출은 early-return, `DraggedCard` 교체 없음.

## Implementation Notes

1. **`OnOfferCardHoldStarted` 본문** (ADR-0008 §Subscription Pattern 인용):
   ```cpp
   void UCardHandWidget::OnOfferCardHoldStarted(const FInputActionValue& Value) {
       if (State != ECardHandUIState::Idle) return;  // EC-CHU-8: Dragging 중 재호출 무시
       UCardSlotWidget* HoveredCard = HitTestCardUnderCursor();
       if (!HoveredCard) return;  // 카드 밖 Hold → 무시 (CR-CHU-2 §1)
       DraggedCard = HoveredCard;
       State = ECardHandUIState::Dragging;
       MotionHandle = Stillness->Request(EStillnessChannel::Motion, EStillnessPriority::Standard);
   }
   ```

2. **`HitTestCardUnderCursor` 구현** (ADR-0008 §Hit Test 함수 인용):
   ```cpp
   UCardSlotWidget* UCardHandWidget::HitTestCardUnderCursor() const {
       FVector2D MousePos;
       if (!UWidgetLayoutLibrary::GetMousePositionScaledByDPI(GetOwningPlayer(), MousePos.X, MousePos.Y)) {
           return nullptr;
       }
       for (auto& CardSlot : CardSlots) {
           if (!CardSlot) continue;
           const FGeometry& Geo = CardSlot->GetCachedGeometry();
           if (Geo.IsUnderLocation(MousePos)) {
               return CardSlot;
           }
       }
       return nullptr;
   }
   ```

3. **Stillness Budget 획득**:
   - `Stillness = GetGameInstance()->GetSubsystem<UMossStillnessBudgetSubsystem>();` — `NativeConstruct` 시 cache 또는 첫 호출 시 lazy 획득.
   - `MotionHandle`은 멤버: `FStillnessHandle MotionHandle;` (RAII — Dragging 종료 시 Release).
   - Deny 시 `MotionHandle.IsValid() == false` — 상태 진입은 진행하되 애니메이션 생략 (CR-CHU-2 ReducedMotion 경로와 합쳐서 story 006).

4. **Degraded 카드 처리** (CR-CHU-8):
   - `HitTestCardUnderCursor` 시 빈 슬롯(`CardSlot->GetCardId().IsNone()`)은 드래그 불가 — hit test 내부에서 `continue`.

## Out of Scope

- PointerMove 핸들러 (story 003)
- Offer Zone hit test + ConfirmOffer (story 004)
- Release handler 분기 (story 004)
- State machine 전체 (story 005)
- Gamepad 대체 흐름 (story 007 또는 VS)

## QA Test Cases

**Test Case 1 — Idle → Dragging 전환**
- **Setup**: Card Hand UI Idle 상태, 카드 위젯 3개 배치, Mock Enhanced Input Component로 `IA_OfferCard` Triggered 시뮬.
- **Verify**:
  - Hold 이후 `GetState() == ECardHandUIState::Dragging`.
  - `DraggedCard.IsValid() == true`.
  - `Stillness->GetActiveCount(Motion) >= 1`.
- **Pass**: 세 조건 모두 충족.

**Test Case 2 — 카드 밖 Hold (CR-CHU-2 §1)**
- **Setup**: Mouse 위치가 카드 외부 (Slot 영역 밖).
- **Verify**:
  - `OnOfferCardHoldStarted` 호출 후 `GetState() == ECardHandUIState::Idle` (변화 없음).
  - `DraggedCard.IsValid() == false`.
- **Pass**: 두 조건 모두 충족.

**Test Case 3 — EC-CHU-8 이중 Hold guard (AC-CHU-08-guard)**
- **Setup**: State 이미 Dragging, DraggedCard = CardA.
- **Verify**:
  - `OnOfferCardHoldStarted` 재호출 후 `DraggedCard == CardA` (교체 없음).
  - `Stillness->GetActiveCount(Motion) == 1` (증가 없음).
- **Pass**: 두 조건 모두 충족.

## Test Evidence

- [ ] `tests/unit/ui/card_hand_drag_start_test.cpp` — Mock PC + Mock Enhanced Input + Mock Stillness. 3 Test Cases 구현.
- [ ] CI static analysis — `OnOfferCardHoldStarted` 본문에 `State != ECardHandUIState::Idle` guard 존재 grep.

## Dependencies

- **Depends on**: story-001 (widget 골격), Foundation Input Abstraction (IA_OfferCard), Feature Stillness Budget (Motion Standard 슬롯).
- **Unlocks**: story-003 (PointerMove 갱신), story-004 (release handler).
