# Story 005 — State machine 완성 + `FOnGameStateChanged` 구독 + Show/Hide

> **Epic**: [presentation-card-hand-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/card-hand-ui.md](../../../design/gdd/card-hand-ui.md) §3B States and Transitions (전체 전환 표) + §3A CR-CHU-6 Reveal + §3A CR-CHU-7 Hide + §5 EC-CHU-4 강제 이탈
- **TR-ID**: TR-chu-001 + TR-chu-004 (EC-CHU-4 강제 이탈 즉시 취소)
- **Governing ADR**: [ADR-0008](../../../docs/architecture/adr-0008-umg-drag-implementation.md) §Subscription Pattern (드래그 상태 머신)
- **Engine Risk**: LOW — `UGameInstanceSubsystem` delegate 구독은 UE 5.6 stable. Widget Animation도 stable.
- **Engine Notes**: `FOnGameStateChanged`는 `UMossGameStateSubsystem`이 발행. Widget은 `NativeConstruct`에서 구독, `NativeDestruct`에서 해제 — `AddDynamic` + `RemoveDynamic` 쌍 필수 (leak 방지). Reveal 시 `GetDailyHand()` pull은 Card System API (ADR-0006 Eager — Dawn 진입 시 이미 샘플링 완료된 상태).
- **Control Manifest Rules**:
  - Presentation Layer Rules §Required Patterns — Drag state 전환 (ADR-0008)
  - Feature Layer Rules §Required Patterns — Card pool Eager — `GetDailyHand()`는 즉시 반환 (ADR-0006)
  - Global Rules §Cross-Cutting — UI 알림·팝업 금지 (Pillar 1)

## Acceptance Criteria

- **AC-CHU-08** [`INTEGRATION`/BLOCKING] — `FOnGameStateChanged(Dawn, Offering)` 수신 → `GetDailyHand()` pull. Revealing → 좌→중→우 `CardRevealStaggerSec`(0.08s) 간격 → Idle.
- **AC-CHU-09** [`INTEGRATION`/BLOCKING] — `FOnGameStateChanged(Offering, Waiting)` 수신 (카드 건넴 완료) → Hiding → `CardHideDurationSec` 내 Hide 완료 → Hidden.
- **AC-CHU-10** / **TR-chu-004** / **EC-CHU-4** [`INTEGRATION`/BLOCKING] — `Dragging` 중 `FOnGameStateChanged(Offering, _)` 강제 이탈 수신 → 드래그 즉시 취소 + `Stillness->Release(MotionHandle)` + 전체 Hide. `ConfirmOffer` 미호출.

## Implementation Notes

1. **GSM 구독** (`NativeConstruct` 확장):
   ```cpp
   void UCardHandWidget::NativeConstruct() {
       Super::NativeConstruct();
       // ... Enhanced Input 바인딩 (story 001)

       if (auto* GSM = GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>()) {
           GSM->OnGameStateChanged.AddDynamic(this, &UCardHandWidget::HandleGameStateChanged);
       }
   }

   void UCardHandWidget::NativeDestruct() {
       if (auto* GSM = GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>()) {
           GSM->OnGameStateChanged.RemoveDynamic(this, &UCardHandWidget::HandleGameStateChanged);
       }
       Super::NativeDestruct();
   }
   ```

2. **`HandleGameStateChanged` 본문**:
   ```cpp
   void UCardHandWidget::HandleGameStateChanged(EGameState OldState, EGameState NewState) {
       // Hidden → Revealing (AC-CHU-08)
       if (NewState == EGameState::Offering && State == ECardHandUIState::Hidden) {
           const TArray<FName> Hand = Card->GetDailyHand();  // ADR-0006 Eager
           PopulateCardSlots(Hand);  // 카드 위젯 구성
           State = ECardHandUIState::Revealing;
           StartRevealAnimation();  // CR-CHU-6 순차 공개
           return;
       }

       // EC-CHU-4 강제 이탈 (Dragging 중)
       if (OldState == EGameState::Offering && State == ECardHandUIState::Dragging) {
           State = ECardHandUIState::Hiding;
           if (DraggedCard.IsValid()) {
               DraggedCard->SetEmissive(0.0f);
               DraggedCard.Reset();
           }
           Stillness->Release(MotionHandle);
           StartHideAnimation();
           return;
       }

       // Idle → Hiding (AC-CHU-09)
       if (OldState == EGameState::Offering && State == ECardHandUIState::Idle) {
           State = ECardHandUIState::Hiding;
           StartHideAnimation();
           return;
       }

       // Offering(내부) → Hiding은 CR-CHU-4 StartConfirmAnimation 완료 콜백에서 처리
   }
   ```

3. **`StartRevealAnimation`** (CR-CHU-6):
   - 왼쪽 → 가운데 → 오른쪽 순차, `CardRevealStaggerSec` 간격.
   - Stillness Budget `Motion` Standard 요청. Deny 시 즉각 표시.
   - 완료 콜백: `State = ECardHandUIState::Idle;`.

4. **`StartHideAnimation`** (CR-CHU-7):
   - 전체 카드 패 페이드 아웃 `CardHideDurationSec` (0.25s).
   - 완료 콜백: `State = ECardHandUIState::Hidden;` + `SetVisibility(ESlateVisibility::Collapsed);`.

5. **State Transition Guard (AC-CHU-EC10)** (CR-CHU-EC-10):
   - Revealing 중에는 `OnOfferCardHoldStarted` 무시 (story 002의 `State != Idle` guard가 이미 처리).

6. **EC-CHU-3 focus loss 처리**:
   - Input Abstraction이 focus loss 시 `IA_OfferCard` Hold 리셋 → Completed 발행 → `OnOfferCardReleased`가 Dragging → Idle 처리 (story 004). 본 story에서 추가 작업 불필요.

## Out of Scope

- Reveal 애니메이션 시각 세부 (OQ-CHU-2: 슬라이드 업 vs 페이드 인 — art-director 결정)
- ReducedMotion 분기 (story 006)
- 카드 0장 Degraded 모드 (story 007)
- Input Mode swap Gamepad→Mouse 처리 (CR-CHU-10 — VS)

## QA Test Cases

**Test Case 1 — Dawn → Offering Reveal (AC-CHU-08)**
- **Setup**: State = `Hidden`. Mock Card System returns `["Card_A", "Card_B", "Card_C"]` for `GetDailyHand`.
- **Verify**:
  - Mock GSM broadcast `OnGameStateChanged(Dawn, Offering)`.
  - `GetState() == ECardHandUIState::Revealing` (즉시).
  - `Card->GetDailyHand` 호출 1회.
  - 0.24s(= 3장 × 0.08s stagger) 후 `GetState() == ECardHandUIState::Idle`.
- **Pass**: 4 조건 충족.

**Test Case 2 — Offering → Waiting Hide (AC-CHU-09)**
- **Setup**: State = `Idle`.
- **Verify**:
  - Mock GSM broadcast `OnGameStateChanged(Offering, Waiting)`.
  - `GetState() == ECardHandUIState::Hiding`.
  - `CardHideDurationSec` 후 `GetState() == ECardHandUIState::Hidden`.
- **Pass**: 3 조건 충족.

**Test Case 3 — EC-CHU-4 강제 이탈 (AC-CHU-10)**
- **Setup**: State = `Dragging`, DraggedCard = CardA, Stillness MotionHandle 활성.
- **Verify**:
  - Mock GSM broadcast `OnGameStateChanged(Offering, Farewell)`.
  - `GetState() ∈ {Hiding, Hidden}` (즉시 Hide 시퀀스 진입).
  - `Card->ConfirmOffer` 호출 0회 (건넴 없음).
  - `Stillness->GetActiveCount(Motion)` decrement됨 (Release).
  - `DraggedCard.IsValid() == false`.
- **Pass**: 5 조건 충족.

**Test Case 4 — Subscription cleanup**
- **Setup**: Widget 생성 후 NativeDestruct 호출.
- **Verify**:
  - `GSM->OnGameStateChanged` subscriber count decrement됨.
  - 이후 GSM broadcast가 widget의 handler를 호출하지 않음.
- **Pass**: 두 조건 충족.

## Test Evidence

- [ ] `tests/integration/ui/card_hand_reveal_test.cpp` — AC-CHU-08.
- [ ] `tests/integration/ui/card_hand_hide_test.cpp` — AC-CHU-09.
- [ ] `tests/integration/ui/card_hand_forced_exit_test.cpp` — AC-CHU-10, EC-CHU-4.
- [ ] `tests/integration/ui/card_hand_lifetime_test.cpp` — Test Case 4 subscription cleanup.

## Dependencies

- **Depends on**: story-001 (골격), story-002 (Dragging 진입), story-003 (PointerMove), story-004 (Release). Core GSM (FOnGameStateChanged), Feature Card System (GetDailyHand ADR-0006 Eager).
- **Unlocks**: story-006 (ReducedMotion), story-007 (Degraded + MANUAL evidence).
