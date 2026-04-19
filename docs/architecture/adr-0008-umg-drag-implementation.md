# ADR-0008: UMG 드래그 구현 — Enhanced Input Subscription Pattern

## Status

**Accepted** (2026-04-19 — 결정 명확, Card Hand UI sprint 진입 시 채택. Implementation 검증은 sprint 시)

## Date

2026-04-19

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | UI / UMG / Input |
| **Knowledge Risk** | LOW — Enhanced Input + UMG NativeTick 모두 안정 |
| **References Consulted** | card-hand-ui.md §CR-CHU-2 + §OQ-CHU-1, input-abstraction-layer.md §Rule 1 (raw key 금지) + §Rule 4 (Hover-only 금지), `engine-reference/unreal/modules/input.md` |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | Card Hand UI sprint 시 드래그 인터랙션 수동 검증 (AC-CHU-04/05/06) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None |
| **Enables** | Card Hand UI 드래그-투-오퍼 인터랙션 구현 — Pillar 2 "건넴의 의식" 직접 구현 |
| **Blocks** | Card Hand UI sprint — 본 ADR 없이는 드래그 코드 작성 시 Input Abstraction Rule 1 위반 가능 |
| **Ordering Note** | Card Hand UI sprint 진입 전. ADR-0006 (Card Eager) + ADR-0012 (GetLastCardDay)와 함께 Card 영역 ADR 묶음 |

## Context

### Problem Statement

Card Hand UI (#12)의 드래그-투-오퍼 인터랙션 구현 방식 — 두 candidates:
- **(a) UMG 네이티브 드래그**: `UUserWidget::NativeOnMouseButtonDown` / `NativeOnMouseMove` / `NativeOnMouseButtonUp` 직접 override + `UDragDropOperation`
- **(b) Enhanced Input subscription**: `IA_OfferCard` Hold Triggered + `IA_PointerMove` Axis2D 구독, UMG는 시각 표현만

### Constraints

- **Input Abstraction Rule 1 (raw key 금지)**: 게임플레이 코드에서 `BindKey`, `EKeys::` 직접 사용 금지. UMG NativeOnMouseButtonDown은 `FPointerEvent` 받음 — raw mouse event 직접 처리에 해당하나 UMG framework 표준
- **Mouse-only completeness (Rule 5)**: 마우스만으로 전체 게임 기능 접근 — Card Hand UI는 핵심 인터랙션
- **Gamepad alternative (CR-CHU-9, VS)**: 동일 인터랙션이 Gamepad Hold + D-pad 네비게이션으로 대체 가능해야 함
- **Steam Deck 호환**: Mouse 모드 (트랙패드 에뮬) + Gamepad 모드 둘 다 작동
- **결정성 + 테스트 가능성**: AC-CHU-07 (`IsInOfferZone` 결정적 계산), AC-CHU-13 (ReducedMotion 즉시 적용)

### Requirements

- 드래그 시작·이동·종료가 결정적
- Mouse + Gamepad 양쪽 동일 추상화 (Input Abstraction `IA_OfferCard`)
- ReducedMotion 일관 처리 (Stillness Budget 통합)
- Hover-only 금지 강제 (Pattern 2)
- UE Multicast Delegate 패턴과 정합 (UMG framework도 결국 delegate 기반)

## Decision

**Enhanced Input Subscription 채택** — Card Hand UI는 Input Abstraction의 `IA_OfferCard` Hold Triggered + `IA_PointerMove`를 구독한다. UMG NativeOnMouse* override 사용 금지 (raw event 처리 회피).

### Subscription Pattern

```cpp
UCLASS()
class UCardHandWidget : public UUserWidget {
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override {
        Super::NativeConstruct();
        APlayerController* PC = GetOwningPlayer();
        if (auto* InputComp = Cast<UEnhancedInputComponent>(PC->InputComponent)) {
            // IA_OfferCard Hold Triggered → drag 시작
            InputComp->BindAction(IA_OfferCard, ETriggerEvent::Triggered, this, &OnOfferCardHoldStarted);
            InputComp->BindAction(IA_OfferCard, ETriggerEvent::Completed, this, &OnOfferCardReleased);

            // IA_PointerMove Axis2D → 드래그 중 카드 위치 갱신
            InputComp->BindAction(IA_PointerMove, ETriggerEvent::Triggered, this, &OnPointerMove);
        }
    }

private:
    void OnOfferCardHoldStarted(const FInputActionValue& Value) {
        if (State != ECardHandUIState::Idle) return;  // Dragging 중 무시 (EC-CHU-8)
        // 카드 위 hover 검증 + Dragging 진입
        if (auto* HoveredCard = HitTestCardUnderCursor()) {
            DraggedCard = HoveredCard;
            State = ECardHandUIState::Dragging;
            Stillness->Request(EStillnessChannel::Motion, EStillnessPriority::Standard);
        }
    }

    void OnPointerMove(const FInputActionValue& Value) {
        if (State != ECardHandUIState::Dragging || !DraggedCard.IsValid()) return;
        const FVector2D PointerPos = Value.Get<FVector2D>();
        DraggedCard->SetPositionInViewport(PointerPos);

        // F-CHU-3 Offer Zone hit test
        const bool bInZone = IsInOfferZone(PointerPos);
        DraggedCard->SetEmissive(bInZone ? 0.15f : 0.0f);
    }

    void OnOfferCardReleased(const FInputActionValue& Value) {
        if (State != ECardHandUIState::Dragging || !DraggedCard.IsValid()) return;
        const bool bInZone = IsInOfferZone(GetCurrentPointerPos());
        if (bInZone) {
            // Card.ConfirmOffer 호출 (CR-CHU-4)
            const bool bAccepted = Card->ConfirmOffer(DraggedCard->GetCardId());
            if (bAccepted) {
                State = ECardHandUIState::Offering;
                StartConfirmAnimation();
            } else {
                State = ECardHandUIState::Idle;
                StartReturnAnimation();  // EC-CHU-2 ConfirmOffer false
            }
        } else {
            State = ECardHandUIState::Idle;
            StartReturnAnimation();  // EC-CHU-1 Offer Zone 밖 릴리스
        }
        Stillness->Release(MotionHandle);
    }
};
```

### UMG NativeOnMouse* 사용 금지

- `NativeOnMouseButtonDown`, `NativeOnMouseMove`, `NativeOnMouseButtonUp` override **금지** — raw event 처리, Input Abstraction 우회
- `NativeOnMouseEnter`, `NativeOnMouseLeave` **허용** — 시각 hover 피드백 (highlight) 전용. 게임 상태 변경 금지 (Pattern 2 Hover-only 금지 강제)
- `UDragDropOperation` 사용 금지 — UMG 자체 drag framework는 Enhanced Input과 통합 어려움

### Hit Test 함수

```cpp
UCardSlotWidget* UCardHandWidget::HitTestCardUnderCursor() const {
    const FVector2D MousePos = USlateBlueprintLibrary::GetAbsoluteCoordinatesFromLocal(GetCachedGeometry(), CurrentPointerPos);
    for (auto& CardSlot : CardSlots) {
        if (CardSlot->GetCachedGeometry().IsUnderLocation(MousePos)) {
            return CardSlot;
        }
    }
    return nullptr;
}
```

## Alternatives Considered

### Alternative 1: UMG NativeOnMouse* override + UDragDropOperation

- **Description**: 표준 UMG 드래그 framework
- **Pros**: UMG-native — 디자이너 친화 (Blueprint widget 노출)
- **Cons**:
  1. **Input Abstraction Rule 1 위반** — `FPointerEvent` 직접 처리 = raw mouse event
  2. **Gamepad alternative 분기 코드** — UMG 드래그는 Mouse 전제, Gamepad는 별도 경로 (focus + Hold)
  3. `UDragDropOperation` payload 패턴은 Card → Growth/MBC 같은 cross-system 통신과 부정합 (Card.ConfirmOffer 직접 호출이 더 명확)
  4. **결정성 약함** — UMG drag tick은 framework 내부 — AC 자동화 어려움
- **Rejection Reason**: Input Abstraction Rule 1 위반이 결정적

### Alternative 2: Enhanced Input subscription (채택)

- 본 ADR §Decision

### Alternative 3: Hybrid — Mouse는 NativeOnMouse, Gamepad는 Enhanced Input

- **Description**: 두 입력 방식별 다른 코드 경로
- **Pros**: 각 방식 native 활용
- **Cons**: 두 경로 유지 부담. Steam Deck 트랙패드 (Mouse 에뮬) 모드에서 어느 경로?
- **Rejection Reason**: 두 경로 = 2× 버그 표면적

## Consequences

### Positive

- **Input Abstraction Rule 1 준수**: raw event 처리 0건 (AC `NO_RAW_KEY_BINDING` grep 통과)
- **Mouse + Gamepad 동일 추상화**: `IA_OfferCard` Hold Triggered가 두 입력에서 동일 의미 (Hold threshold만 다름 — Formula 2)
- **결정성**: Input Abstraction의 Hold trigger는 결정적 — AC-CHU-04 (Hold 정확히 0.15s 시 trigger) 자동화 가능
- **Steam Deck 호환**: 트랙패드 → Mouse 에뮬 → 동일 코드 경로
- **Hover-only 금지 강제**: NativeOnMouseEnter/Leave는 시각 highlight만, drag는 IA_OfferCard Hold만 — Pattern 2 자연 준수

### Negative

- **UMG framework 일부 미사용**: UDragDropOperation 미사용 — UMG documentation의 표준 드래그 패턴과 다름. 디자이너가 처음에 혼란 가능. **Mitigation**: `interaction-patterns.md` Pattern 1 (Drag-to-Offer)에서 본 패턴을 명시
- **Card Hand UI Tick 중간 의존**: `OnPointerMove`가 매 frame 호출 — UMG Tick 사용. **Mitigation**: 정상 — 드래그 중에만 Dragging state, Idle/Hidden 상태에서는 cost 0

### Risks

- **Hit test 정확성**: `IsUnderLocation` 결과가 DPI scaling 환경에서 부정확 가능. **Mitigation**: `USlateBlueprintLibrary::GetAbsoluteCoordinatesFromLocal` 사용 + AC-CHU-07 자동화 테스트로 정확성 검증
- **Gamepad 드래그 시 PointerMove 부재**: Gamepad는 Axis2D pointer 없음 — Hold + D-pad 네비게이션으로 카드 선택 후 Hold 완료 시 즉시 건넴 (CR-CHU-9). 본 ADR 영향 없음 — 별도 경로

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `card-hand-ui.md` | §OQ-CHU-1 "UMG 드래그 구현 방식 — `NativeOnMouseButtonDown`/`Move` 직접 오버라이드 vs Input Abstraction `IA_PointerMove` + `IA_OfferCard` 구독" | **본 ADR이 OQ-CHU-1의 정식 결정** |
| `card-hand-ui.md` | §CR-CHU-2 드래그-투-오퍼 제스처 | 본 ADR §Subscription Pattern 구체화 |
| `card-hand-ui.md` | §CR-CHU-9 Gamepad 대체 (CR-CHU-9) | 동일 IA_OfferCard 추상화 — 별도 코드 경로 불필요 |
| `card-hand-ui.md` | AC-CHU-04 ~ AC-CHU-07 | 본 ADR §Subscription Pattern으로 자동화 가능 |
| `input-abstraction-layer.md` | §Rule 1 raw key/button 바인딩 금지 | 본 ADR이 Rule 1 준수 |
| `input-abstraction-layer.md` | §Rule 4 Hover-only 금지 | 본 ADR §UMG NativeOnMouse* 사용 금지 — NativeOnMouseEnter/Leave는 시각만 |
| `interaction-patterns.md` | Pattern 1 Drag-to-Offer | 본 ADR이 구현 방식 확정 |

## Performance Implications

- **CPU (드래그 중)**: `OnPointerMove` per frame 호출 — Hit test + position update ~10μs. 60fps에서 무시 가능
- **CPU (Idle)**: 0 (Enhanced Input subscription만 활성)
- **Memory**: Subscription overhead per delegate ~32 bytes — 무시 가능

## Migration Plan

Card Hand UI sprint 진입 시:
1. `UCardHandWidget::NativeConstruct` — Enhanced Input subscription 등록
2. `OnOfferCardHoldStarted` / `OnPointerMove` / `OnOfferCardReleased` 구현
3. `HitTestCardUnderCursor` 구현 (USlateBlueprintLibrary)
4. AC-CHU-04 ~ AC-CHU-07 자동화 테스트 추가 (`MockInputAction` fixture로 시뮬)
5. **AC NO_RAW_KEY_BINDING grep 확장**: `NativeOnMouseButtonDown\|NativeOnMouseButtonUp\|NativeOnMouseMove\|UDragDropOperation` Card Hand UI 디렉토리 grep 0건 검증

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| Enhanced Input subscription 채택 | grep `BindAction.*IA_OfferCard\|BindAction.*IA_PointerMove` | Card Hand UI 파일 매치 ≥ 1건 |
| UMG NativeOnMouse* 미사용 | grep `NativeOnMouseButtonDown\|NativeOnMouseButtonUp\|NativeOnMouseMove\|UDragDropOperation` | Card Hand UI 디렉토리 0건 |
| AC-CHU-04 ~ 07 통과 | 자동화 테스트 | 모두 PASS |
| Steam Deck 트랙패드 | 수동 — Steam Deck에서 카드 드래그 시도 | 정상 작동 (Mouse 에뮬로 동일 경로) |

## Related Decisions

- **card-hand-ui.md** §OQ-CHU-1 — 본 ADR의 직접 출처
- **input-abstraction-layer.md** §Rule 1 + §Rule 4 — 본 ADR이 두 Rule 준수
- **interaction-patterns.md** Pattern 1 + Pattern 2 + Pattern 3 — 본 ADR 구현 방식이 세 Pattern과 일관
- **ADR-0006** (Card Eager) — 같은 Card 영역 ADR 묶음
- **ADR-0012** (GetLastCardDay) — 같은 Card 영역 ADR 묶음
