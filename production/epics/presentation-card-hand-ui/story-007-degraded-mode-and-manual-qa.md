# Story 007 — Degraded 모드 (0-2장) + Mouse-only Manual QA + Forbidden API grep

> **Epic**: [presentation-card-hand-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/card-hand-ui.md](../../../design/gdd/card-hand-ui.md) §3A CR-CHU-8 Degraded 모드 + §5 EC-CHU-6 카드 0장 + §8 AC-CHU-11 Hover-only 금지 + AC-CHU-12 Mouse-only 완결
- **TR-ID**: TR-chu-002 (Mouse-only) + TR-chu-006 (Hover-only 금지)
- **Governing ADR**: [ADR-0008](../../../docs/architecture/adr-0008-umg-drag-implementation.md) §UMG NativeOnMouse* 사용 금지 + §Validation Criteria (grep 0건)
- **Engine Risk**: LOW — Widget visibility + grep은 UE 5.6 stable
- **Engine Notes**: Card System Degraded 상태 확인은 `Card->IsDegraded()` 또는 `GetDailyHand().Num() < 3` 체크. AC-CHU-14/15는 자동화 가능 — Mock CardSystem으로 `DailyHand.Num() = 0` / `= 2` 주입. AC-CHU-12는 실기 매뉴얼 — production/qa/evidence/에 checklist 저장.
- **Control Manifest Rules**:
  - Presentation Layer Rules §Forbidden Approaches — "Never override NativeOnMouseButtonDown/Move/Up" + "Never use UDragDropOperation" + "Never make drag start on hover-only"
  - Global Rules §Forbidden APIs — 동일 2건 명시 (ADR-0008)
  - Cross-Layer Rules §Cross-Cutting — Hover-only 금지 (Pattern 2, Steam Deck)

## Acceptance Criteria

- **AC-CHU-14** [`AUTOMATED`/BLOCKING] — Card System Degraded Ready, `DailyHand.Num() = 0`. `OnGameStateChanged(Dawn, Offering)` 수신. 빈 슬롯 3개 표시. 드래그 입력 없음. `UE_LOG(Error, "CardHandUI: 0 cards in Degraded mode")` 1회 발생.
- **AC-CHU-15** [`AUTOMATED`/BLOCKING] — Card System Degraded, `DailyHand.Num() = 2`. 카드 2장 정상 표시. 빈 슬롯 1개 회색 테두리. 2장 중 1장 드래그 → `ConfirmOffer` 정상 호출.
- **AC-CHU-11** [`MANUAL`/ADVISORY] — 마우스를 카드 위에 올리기만 (클릭 없이). 시각 하이라이트만 발생, 카드 건넴·선택 게임 상태 변화 없음 (Hover-only 금지).
- **AC-CHU-12** / **TR-chu-002** [`MANUAL`/ADVISORY] — 게임패드 미연결, 키보드 미사용. 마우스만으로 Reveal → Drag → Offer Zone 건넴 → Hide 전체 플로우 완결. 키보드·게임패드 없이 차단되는 기능 없음.
- **AC-CHU-Forbidden-01** [`CODE_REVIEW`/BLOCKING] — `grep "NativeOnMouseButtonDown\|NativeOnMouseButtonUp\|NativeOnMouseMove\|UDragDropOperation" Source/MadeByClaudeCode/UI/CardHand*` = 0건 (ADR-0008 §Validation Criteria §2).

## Implementation Notes

1. **`PopulateCardSlots` Degraded 처리** (CR-CHU-8):
   ```cpp
   void UCardHandWidget::PopulateCardSlots(const TArray<FName>& Hand) {
       for (int32 i = 0; i < 3; ++i) {
           if (i < Hand.Num() && !Hand[i].IsNone()) {
               CardSlots[i]->SetCardId(Hand[i]);
               CardSlots[i]->SetDragEnabled(true);
           } else {
               CardSlots[i]->SetCardId(NAME_None);
               CardSlots[i]->SetEmptyFrameVisible(true);  // 회색 테두리
               CardSlots[i]->SetDragEnabled(false);  // HitTest에서 제외
           }
       }
       if (Hand.Num() == 0) {
           UE_LOG(LogCardHandUI, Error, TEXT("CardHandUI: 0 cards in Degraded mode"));
       }
   }
   ```

2. **`UCardSlotWidget::SetDragEnabled`**:
   - `bDragEnabled` 멤버 추가. `HitTestCardUnderCursor` (story 002)에서 `!CardSlot->IsDragEnabled()`인 슬롯 skip.

3. **Hover-only 금지 — NativeOnMouseEnter/Leave 시각만** (ADR-0008 §UMG NativeOnMouse*):
   ```cpp
   FReply UCardSlotWidget::NativeOnMouseEnter(const FGeometry& G, const FPointerEvent& E) {
       if (!bDragEnabled) return Super::NativeOnMouseEnter(G, E);
       SetEmissive(0.05f);  // 시각 highlight 전용
       return FReply::Unhandled();  // 게임 상태 변경 없음
   }

   FReply UCardSlotWidget::NativeOnMouseLeave(const FPointerEvent& E) {
       if (!bDragEnabled) return Super::NativeOnMouseLeave(E);
       if (!bIsDragging) SetEmissive(0.0f);
       return FReply::Unhandled();
   }
   ```
   - 절대 금지: `NativeOnMouseButtonDown/Move/Up` override, `UDragDropOperation` 사용.

4. **AC-CHU-12 MANUAL QA script** (`production/qa/evidence/ac-chu-12-mouse-only.md`):
   - Step 1: 게임패드 disconnect, 키보드 unplug.
   - Step 2: 게임 실행, Dawn → Offering 진입.
   - Step 3: 카드 3장 Reveal 관찰.
   - Step 4: Mouse만으로 카드 hover → 0.15s Hold → 유리병으로 drag → release.
   - Step 5: 건넴 확정 애니메이션 + Hide 관찰.
   - Step 6: Waiting → 다음 Dawn cycle에서 재시도.
   - Pass: 모든 Step 차단 없이 완료. 키보드·게임패드 요구 없음.

5. **AC-CHU-11 MANUAL QA script** (`production/qa/evidence/ac-chu-11-hover-only.md`):
   - Step 1: Offering 상태, 카드 3장.
   - Step 2: Mouse를 CardA 위로 움직임 (Hold 없이 hover만).
   - Step 3: CardA emissive highlight 관찰.
   - Step 4: 5초 대기 — State 변화 없음 (`GetState() == Idle` 유지).
   - Step 5: Mouse를 Offer Zone 근처로 hover (Hold 없이) — 카드 변화 없음.
   - Pass: hover만으로 state/ConfirmOffer 미발생.

## Out of Scope

- OQ-CHU-2 (Reveal 방향 슬라이드 vs 페이드) — art-director 결정
- OQ-CHU-3 (DPI 스케일 OfferZoneRadiusPx 보정) — Implementation 단계 OQ
- OQ-CHU-4 (JarScreenPos Tick 방식) — Implementation 단계 OQ
- Gamepad 대체 흐름 (CR-CHU-9 — VS)
- Input Mode swap (CR-CHU-10 — VS)

## QA Test Cases

**Test Case 1 — Degraded 0장 (AC-CHU-14)**
- **Setup**: Mock Card System returns `[]` for `GetDailyHand`, `IsDegraded() == true`.
- **Verify**:
  - Reveal 후 CardSlots 3개 모두 `bDragEnabled == false`.
  - `UE_LOG(Error, "CardHandUI: 0 cards in Degraded mode")` 카운트 == 1.
  - `OnOfferCardHoldStarted` 시뮬 시 State 변화 없음 (HitTest fail).
- **Pass**: 3 조건 충족.

**Test Case 2 — Degraded 2장 (AC-CHU-15)**
- **Setup**: Mock Card System returns `["Card_A", "Card_B"]`.
- **Verify**:
  - CardSlots[0], CardSlots[1] `bDragEnabled == true`, `GetCardId()` non-none.
  - CardSlots[2] `bDragEnabled == false`, `EmptyFrameVisible == true`.
  - CardA 드래그 시 `ConfirmOffer("Card_A")` 정상 호출.
- **Pass**: 3 조건 충족.

**Test Case 3 — Forbidden API grep (AC-CHU-Forbidden-01)**
- **Setup**: `Source/MadeByClaudeCode/UI/CardHand*` 전 파일.
- **Verify**:
  - `grep "NativeOnMouseButtonDown"` = 0건.
  - `grep "NativeOnMouseButtonUp"` = 0건.
  - `grep "NativeOnMouseMove"` = 0건.
  - `grep "UDragDropOperation"` = 0건.
- **Pass**: 4 grep 모두 0건.

**Test Case 4 — Hover 시각 highlight만 (AC-CHU-11)**
- **Setup**: `NativeOnMouseEnter/Leave` 호출 시뮬.
- **Verify**:
  - `CardSlot->GetEmissive() == 0.05` after Enter.
  - `FReply::Unhandled()` 반환 — `NativeOnMouseEnter` (게임 상태 변경 없음).
  - State == `Idle` 유지.
  - `DraggedCard.IsValid() == false`.
- **Pass**: 4 조건 충족.

**Test Case 5 — Manual QA evidence**
- `production/qa/evidence/ac-chu-11-hover-only.md` 체크리스트 작성 + 리드 서명.
- `production/qa/evidence/ac-chu-12-mouse-only.md` 체크리스트 작성 + 리드 서명.

## Test Evidence

- [ ] `tests/unit/ui/card_hand_degraded_zero_test.cpp` — AC-CHU-14.
- [ ] `tests/integration/ui/card_hand_degraded_two_test.cpp` — AC-CHU-15.
- [ ] `tests/unit/ui/card_hand_hover_only_test.cpp` — AC-CHU-11.
- [ ] CI grep check — AC-CHU-Forbidden-01 (4 pattern 0건).
- [ ] `production/qa/evidence/ac-chu-11-hover-only.md` — 수동 체크리스트.
- [ ] `production/qa/evidence/ac-chu-12-mouse-only.md` — 수동 체크리스트.

## Dependencies

- **Depends on**: story-001~006 (전 스토리 완료 후 manual QA 수행).
- **Unlocks**: Epic Done — `/story-done` 집합 실행 후 Epic 종료.
