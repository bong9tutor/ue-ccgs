# Story 006 — ReducedMotion 분기 — 애니메이션 즉시 표시/Hide

> **Epic**: [presentation-card-hand-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: Visual/Feel
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2h

## Context

- **GDD**: [design/gdd/card-hand-ui.md](../../../design/gdd/card-hand-ui.md) §3A CR-CHU-6 Reveal (ReducedMotion ON: 즉각 표시) + §3A CR-CHU-7 Hide (ReducedMotion ON: 즉각 Hide) + §3C Interaction 4 Stillness Budget (`IsReducedMotion()` = true 시 모든 애니메이션 건너뜀)
- **TR-ID**: TR-chu-005 (AC-CHU-13 ReducedMotion 즉시 표시·Hide)
- **Governing ADR**: [ADR-0008](../../../docs/architecture/adr-0008-umg-drag-implementation.md) §Consequences — ReducedMotion 일관 처리 (Stillness Budget 통합)
- **Engine Risk**: LOW — UMG Animation skip + 직접 state set 모두 UE 5.6 stable
- **Engine Notes**: `Stillness->IsReducedMotion()`은 런타임 매 호출 체크 (ADR-0011 convention 2 — hot reload 지원, `ConfigRestartRequired` 없음). Animation 생략 시 최종 프레임 상태를 `SetVisibility` + 위젯 파라미터 직접 설정으로 재현 — Widget Animation `Stop()` 후 수동 적용.
- **Control Manifest Rules**:
  - Feature Layer Rules §Required Patterns — Stillness Budget Request Deny 시 대체 경로
  - Cross-Layer Rules §Cross-Cutting — 알림 없이 silent 처리 (Pillar 1)

## Acceptance Criteria

- **AC-CHU-13** / **TR-chu-005** [`AUTOMATED`/BLOCKING] — `Stillness.bReducedMotionEnabled = true`일 때:
  - Reveal: 카드 3장 즉각 표시, 공개 애니메이션 생략.
  - 드래그 취소 복귀: 카드 즉각 원위치 (0.3s animation 생략).
  - 건넴 확정: 즉각 페이드 아웃 (0.5s animation 생략).
  - Hide: 즉각 Collapsed (0.25s fade 생략).
  - `Stillness->IsReducedMotion()` = true 경로가 4개 지점에서 실행됨 (로그 또는 카운터로 검증).

## Implementation Notes

1. **`StartRevealAnimation` ReducedMotion 분기**:
   ```cpp
   void UCardHandWidget::StartRevealAnimation() {
       if (Stillness && Stillness->IsReducedMotion()) {
           // 즉각 표시 — CR-CHU-6 ReducedMotion ON 경로
           for (auto& CardSlot : CardSlots) {
               CardSlot->SetRenderOpacity(1.0f);
               CardSlot->SetVisibility(ESlateVisibility::Visible);
           }
           State = ECardHandUIState::Idle;
           return;
       }
       // 정상 순차 Reveal 경로
       PlayAnimation(RevealAnimation);
       // 완료 콜백에서 State = Idle
   }
   ```

2. **`StartReturnAnimation` ReducedMotion 분기**:
   ```cpp
   void UCardHandWidget::StartReturnAnimation() {
       if (Stillness && Stillness->IsReducedMotion()) {
           // 즉각 원위치 복귀 — EC-CHU-1 / EC-CHU-2 ReducedMotion
           DraggedCardOriginalSlot->SetPositionInViewport(DraggedCardSlotPosition);
           return;
       }
       PlayAnimation(ReturnAnimation);  // SmoothStep 0.3s
   }
   ```

3. **`StartConfirmAnimation` ReducedMotion 분기**:
   ```cpp
   void UCardHandWidget::StartConfirmAnimation() {
       if (Stillness && Stillness->IsReducedMotion()) {
           // 즉각 페이드 아웃 — CR-CHU-4 ReducedMotion
           for (auto& CardSlot : CardSlots) {
               CardSlot->SetVisibility(ESlateVisibility::Collapsed);
           }
           State = ECardHandUIState::Hidden;
           return;
       }
       PlayAnimation(ConfirmAnimation);  // 카드 유리병 이동 + 페이드 0.5s
   }
   ```

4. **`StartHideAnimation` ReducedMotion 분기**:
   ```cpp
   void UCardHandWidget::StartHideAnimation() {
       if (Stillness && Stillness->IsReducedMotion()) {
           // 즉각 Hide — CR-CHU-7 ReducedMotion
           for (auto& CardSlot : CardSlots) {
               CardSlot->SetVisibility(ESlateVisibility::Collapsed);
           }
           State = ECardHandUIState::Hidden;
           return;
       }
       PlayAnimation(HideAnimation);  // 페이드 아웃 0.25s
   }
   ```

5. **Hover Emissive는 ReducedMotion 무관**:
   - CR-CHU-2 Hover 시각 highlight 유지 (정적 값 변경 — 애니메이션 아님). `SetEmissive(0.15f)`는 ReducedMotion에서도 정상 실행.

## Out of Scope

- Stillness Budget 자체 구현 (Feature stillness-budget 에픽)
- `bReducedMotionEnabled` 플래그 저장·로드 (Foundation Save/Load — Settings)
- 애니메이션 에셋 구성 (아트 파이프라인)

## QA Test Cases

**Test Case 1 — Reveal ReducedMotion ON (AC-CHU-13)**
- **Setup**: Mock `Stillness->IsReducedMotion()` returns `true`. State = `Hidden`.
- **Verify**:
  - GSM `OnGameStateChanged(Dawn, Offering)` broadcast.
  - `GetState() == ECardHandUIState::Idle` 즉시 (Revealing 상태를 거치지 않거나 0 duration).
  - `PlayAnimation(RevealAnimation)` 호출 0회.
  - 모든 CardSlot `GetVisibility() == Visible`.
- **Pass**: 4 조건 충족.

**Test Case 2 — Return ReducedMotion ON (AC-CHU-13)**
- **Setup**: State = `Dragging`, DraggedCard = CardA. Mock IsReducedMotion = true. `CurrentPointerPos` outside OfferZone.
- **Verify**:
  - Mock `IA_OfferCard` Completed broadcast.
  - `PlayAnimation(ReturnAnimation)` 호출 0회.
  - CardA 위젯 좌표 == OriginalSlotPosition (1 frame 이내).
- **Pass**: 3 조건 충족.

**Test Case 3 — Confirm ReducedMotion ON (AC-CHU-13)**
- **Setup**: State = `Dragging`, ConfirmOffer returns `true`. Mock IsReducedMotion = true.
- **Verify**:
  - `PlayAnimation(ConfirmAnimation)` 호출 0회.
  - 모든 CardSlot `GetVisibility() == Collapsed` 즉시.
  - `GetState() == ECardHandUIState::Hidden`.
- **Pass**: 3 조건 충족.

**Test Case 4 — Hide ReducedMotion ON (AC-CHU-13)**
- **Setup**: State = `Idle`. Mock IsReducedMotion = true.
- **Verify**:
  - Mock GSM `OnGameStateChanged(Offering, Waiting)` broadcast.
  - `PlayAnimation(HideAnimation)` 호출 0회.
  - 모든 CardSlot Collapsed 즉시.
- **Pass**: 3 조건 충족.

**Test Case 5 — Normal (ReducedMotion OFF) 경로 검증**
- **Setup**: Mock IsReducedMotion = false.
- **Verify**:
  - Reveal/Return/Confirm/Hide 4 지점 모두 `PlayAnimation` 호출 1회씩.
- **Pass**: 4 호출 모두 확인.

## Test Evidence

- [ ] `tests/unit/ui/card_hand_reduced_motion_test.cpp` — Test Cases 1-4 (ReducedMotion ON).
- [ ] `tests/unit/ui/card_hand_normal_motion_test.cpp` — Test Case 5 (OFF 경로 regression).

## Dependencies

- **Depends on**: story-005 (state machine + Reveal/Hide 호출 지점), Feature Stillness Budget (`IsReducedMotion()` API).
- **Unlocks**: story-007 (통합 manual QA + Degraded).
