# Story 001 — `UCardHandWidget` 위젯 골격 + `NativeConstruct` 바인딩

> **Epic**: [presentation-card-hand-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/card-hand-ui.md](../../../design/gdd/card-hand-ui.md) §3A CR-CHU-1 + §3C Interactions (Input Abstraction / GSM) + §Implementation Notes (UMG 위젯 구조)
- **TR-ID**: TR-chu-001 (골격 — 후속 stories의 기반)
- **Governing ADR**: [ADR-0008](../../../docs/architecture/adr-0008-umg-drag-implementation.md) §Subscription Pattern — `NativeConstruct` 내 Enhanced Input Component 바인딩 패턴
- **Engine Risk**: LOW — UMG `UUserWidget` + Enhanced Input 바인딩 모두 UE 5.6 stable
- **Engine Notes**: `UUserWidget::NativeConstruct`은 위젯이 뷰포트에 추가된 직후 호출. `GetOwningPlayer()->InputComponent`는 PC 존재 확인 필수 (에디터 PIE 환경에서 null 가능). `Cast<UEnhancedInputComponent>` 결과가 null일 경우 warning 로그 후 early-return — 바인딩 실패는 silent fallback(무시)으로 처리 (Pillar 1 — 알림 없음).
- **Control Manifest Rules**:
  - Presentation Layer Rules §Required Patterns — "Card Hand UI `NativeConstruct` override에서 Enhanced Input Component 바인딩" (ADR-0008 source)
  - Global Rules §Naming Conventions — `UCardHandWidget` (`U` prefix), `UCardSlotWidget`, `ECardHandUIState` (`E` prefix)
  - Global Rules §Forbidden APIs — `NativeOnMouseButtonDown/Move/Up` override 금지

## Acceptance Criteria

- **AC-CHU-bootstrap-01** [`CODE_REVIEW`/BLOCKING] — `UCardHandWidget` 클래스가 `UUserWidget` 상속, `NativeConstruct` / `NativeDestruct` override 보유. `Source/MadeByClaudeCode/UI/CardHandWidget.h` 파일 존재.
- **AC-CHU-bootstrap-02** [`CODE_REVIEW`/BLOCKING] — `NativeConstruct` 본문에서 `Cast<UEnhancedInputComponent>(PC->InputComponent)`로 Enhanced Input Component 획득, 3개 `BindAction` 호출 (`IA_OfferCard` Triggered, `IA_OfferCard` Completed, `IA_PointerMove` Triggered) — ADR-0008 §Subscription Pattern 준수.
- **AC-CHU-bootstrap-03** [`CODE_REVIEW`/BLOCKING] — `ECardHandUIState` enum 6개 값 (`Hidden=0, Revealing=1, Idle=2, Dragging=3, Offering=4, Hiding=5`) 정의 — GDD §3B States 일치.

## Implementation Notes

1. **`ECardHandUIState` 정의** (`Source/MadeByClaudeCode/UI/ECardHandUIState.h`):
   - `UENUM(BlueprintType)` + `enum class ECardHandUIState : uint8 { Hidden=0, Revealing=1, Idle=2, Dragging=3, Offering=4, Hiding=5 };`
   - `static_assert(static_cast<uint8>(ECardHandUIState::Hiding) == 5, "state drift");`

2. **`UCardHandWidget` 골격** (`Source/MadeByClaudeCode/UI/CardHandWidget.h/.cpp`):
   - `UCLASS()` + `public UUserWidget`
   - `virtual void NativeConstruct() override;`
   - `virtual void NativeDestruct() override;`
   - Private: `ECardHandUIState State = ECardHandUIState::Hidden;`, `TArray<TObjectPtr<UCardSlotWidget>> CardSlots;`, `TWeakObjectPtr<UCardSlotWidget> DraggedCard;`
   - `UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> IA_OfferCard;`
   - `UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<UInputAction> IA_PointerMove;`

3. **`NativeConstruct` 본문** (ADR-0008 §Subscription Pattern 인용):
   ```cpp
   void UCardHandWidget::NativeConstruct() {
       Super::NativeConstruct();
       APlayerController* PC = GetOwningPlayer();
       if (!PC) return;  // silent fallback — Pillar 1
       if (auto* InputComp = Cast<UEnhancedInputComponent>(PC->InputComponent)) {
           InputComp->BindAction(IA_OfferCard, ETriggerEvent::Triggered, this, &UCardHandWidget::OnOfferCardHoldStarted);
           InputComp->BindAction(IA_OfferCard, ETriggerEvent::Completed, this, &UCardHandWidget::OnOfferCardReleased);
           InputComp->BindAction(IA_PointerMove, ETriggerEvent::Triggered, this, &UCardHandWidget::OnPointerMove);
       } else {
           UE_LOG(LogCardHandUI, Warning, TEXT("EnhancedInputComponent not available — bindings skipped"));
       }
   }
   ```

4. **Handler stub 선언** (각 빈 구현 — 후속 stories에서 채움):
   - `void OnOfferCardHoldStarted(const FInputActionValue& Value);`
   - `void OnOfferCardReleased(const FInputActionValue& Value);`
   - `void OnPointerMove(const FInputActionValue& Value);`

5. **`UCardSlotWidget` 골격** (`Source/MadeByClaudeCode/UI/CardSlotWidget.h/.cpp`):
   - `UCLASS()` + `public UUserWidget`
   - Private: `FName CardId;`, `TObjectPtr<UTexture2D> IconTexture;`
   - Public: `FName GetCardId() const { return CardId; }`

## Out of Scope

- `OnOfferCardHoldStarted` / `OnPointerMove` / `OnOfferCardReleased` 본문 구현 (story 002, 003, 004)
- Offer Zone hit test (story 004)
- State machine 전환 로직 (story 005)
- ReducedMotion 분기 (story 006)
- GSM `FOnGameStateChanged` 구독 (story 005)
- Card System `GetDailyHand()` / `ConfirmOffer` 통합 (story 004)

## QA Test Cases

**Test Case 1 — Widget 구조 (AC-CHU-bootstrap-01, 03)**
- **Setup**: 프로젝트 빌드 성공.
- **Verify**:
  - `grep "class UCardHandWidget.*: public UUserWidget" Source/MadeByClaudeCode/UI/` 매치 1건.
  - `grep "enum class ECardHandUIState" Source/MadeByClaudeCode/UI/` 매치 1건.
  - `grep "Hiding = 5" Source/MadeByClaudeCode/UI/` 매치 1건.
- **Pass**: 모든 grep 충족 + 컴파일 성공.

**Test Case 2 — Enhanced Input 바인딩 (AC-CHU-bootstrap-02)**
- **Setup**: `CardHandWidget.cpp` 파일.
- **Verify**:
  - `grep "BindAction.*IA_OfferCard.*ETriggerEvent::Triggered"` 매치 1건.
  - `grep "BindAction.*IA_OfferCard.*ETriggerEvent::Completed"` 매치 1건.
  - `grep "BindAction.*IA_PointerMove.*ETriggerEvent::Triggered"` 매치 1건.
- **Pass**: 3건 매치 모두 충족.

**Test Case 3 — Forbidden APIs 배제 (ADR-0008)**
- **Setup**: `Source/MadeByClaudeCode/UI/CardHandWidget*` 전 파일.
- **Verify**:
  - `grep "NativeOnMouseButtonDown\|NativeOnMouseButtonUp\|NativeOnMouseMove\|UDragDropOperation"` = 0건.
- **Pass**: grep 0건.

## Test Evidence

- [ ] `tests/unit/ui/card_hand_widget_skeleton_test.cpp` — Widget 클래스 생성, Enhanced Input Component 미장착 PC 경로 검증 (fallback warning).
- [ ] CI static analysis grep — ADR-0008 Validation Criteria §1-2 통과.

## Dependencies

- **Depends on**: Foundation Input Abstraction (IA_OfferCard / IA_PointerMove Input Action 자산 존재).
- **Unlocks**: story-002 (Hold started handler), story-003 (PointerMove handler), story-004 (Offer Zone + ConfirmOffer), story-005 (state machine + GSM 구독).
