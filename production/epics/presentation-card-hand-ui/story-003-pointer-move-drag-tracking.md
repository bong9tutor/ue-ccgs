# Story 003 — `OnPointerMove` — 드래그 중 카드 위치 갱신 + Emissive 피드백

> **Epic**: [presentation-card-hand-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/card-hand-ui.md](../../../design/gdd/card-hand-ui.md) §3A CR-CHU-2 §2 (드래그 중) + §3A CR-CHU-3 Offer Zone 시각 피드백 + §4 F-CHU-3 `IsInOfferZone`
- **TR-ID**: TR-chu-001 (PointerMove 경로)
- **Governing ADR**: [ADR-0008](../../../docs/architecture/adr-0008-umg-drag-implementation.md) §Subscription Pattern `OnPointerMove` 구현
- **Engine Risk**: LOW — `SetPositionInViewport` + Axis2D Value 추출 모두 UE 5.6 stable
- **Engine Notes**: `IA_PointerMove` Axis2D `ETriggerEvent::Triggered`는 매 프레임 발행 (Hold 중 pointer 이동 시). `Value.Get<FVector2D>()`로 스크린 좌표 추출. `OnPointerMove`는 Dragging 상태에서만 일함 — Idle/Hidden에서는 early-return으로 cost 0 (ADR-0008 §Performance Implications).
- **Control Manifest Rules**:
  - Presentation Layer Rules §Required Patterns — `IA_PointerMove` Axis2D `ETriggerEvent::Triggered` → `OnPointerMove` (드래그 중 위치 갱신) (ADR-0008)
  - Presentation Layer Rules §Performance Guardrails — `OnPointerMove` cost: 드래그 중에만 실행, Idle/Hidden 상태 cost 0

## Acceptance Criteria

- **AC-CHU-04-pointer** [`MANUAL`/ADVISORY, partial] — 드래그 중 카드가 커서 위치를 매 프레임 추적. 다른 두 카드는 제자리 유지 (CR-CHU-2 §2 마지막 문장).
- **AC-CHU-07** [`AUTOMATED`/BLOCKING] — F-CHU-3 `IsInOfferZone(CardCenter, JarScreenPos, OfferZoneRadiusPx)` 결정적 계산. `OfferZoneRadiusPx=80, JarScreenPos=(640, 360)` 기준: (620, 400) → `true` (Distance≈44.7), (500, 300) → `false` (Distance≈152.3).
- **AC-CHU-16** [`MANUAL`/ADVISORY] — Offer Zone 경계 통과 시 `bInZone` 상태에 따라 카드 emissive 0.15/0.0 전환.

## Implementation Notes

1. **`OnPointerMove` 본문** (ADR-0008 §Subscription Pattern 인용):
   ```cpp
   void UCardHandWidget::OnPointerMove(const FInputActionValue& Value) {
       if (State != ECardHandUIState::Dragging || !DraggedCard.IsValid()) return;
       const FVector2D PointerPos = Value.Get<FVector2D>();
       DraggedCard->SetPositionInViewport(PointerPos);
       CurrentPointerPos = PointerPos;  // 후속 release 시 재사용 (story 004)

       const bool bInZone = IsInOfferZone(PointerPos);
       DraggedCard->SetEmissive(bInZone ? 0.15f : 0.0f);
   }
   ```

2. **`IsInOfferZone` 구현** (F-CHU-3):
   ```cpp
   bool UCardHandWidget::IsInOfferZone(const FVector2D& CardCenter) const {
       const FVector2D JarScreenPos = GetJarScreenPosition();  // 유리병 Actor 월드→스크린
       const float Distance = (CardCenter - JarScreenPos).Size();
       const auto* Settings = UCardHandUIConfigAsset::Get();
       return Distance <= Settings->OfferZoneRadiusPx;
   }
   ```

3. **`GetJarScreenPosition`** — 유리병 Actor의 월드 좌표 → 스크린 투영:
   ```cpp
   FVector2D UCardHandWidget::GetJarScreenPosition() const {
       if (!JarActor.IsValid()) return FVector2D::ZeroVector;
       FVector2D ScreenPos;
       UGameplayStatics::ProjectWorldToScreen(GetOwningPlayer(), JarActor->GetActorLocation(), ScreenPos);
       return ScreenPos;
   }
   ```
   - `JarActor`는 `TWeakObjectPtr<AActor>` — Level에서 `Tag`로 찾거나 별도 Getter 경유 (GDD §Implementation Notes OQ-CHU-4 — `FTSTicker` vs UMG Tick은 OQ로 열림, 본 story는 PointerMove 주기에서만 계산 — 매 프레임이지만 드래그 중에만).

4. **`SetEmissive`** on `UCardSlotWidget`:
   - 머티리얼 dynamic instance의 Emissive scalar parameter 직접 set. ReducedMotion 무관 (정적 하이라이트는 ReducedMotion에서도 유효).

5. **Degraded 카드 드래그 방지**:
   - `DraggedCard`가 빈 슬롯이면 story 002에서 이미 차단 — 본 story는 방어적 null check만.

## Out of Scope

- Release handler 및 ConfirmOffer 호출 (story 004)
- Drag 취소 복귀 애니메이션 (story 004)
- State 전환 (story 005)
- ReducedMotion 분기 (story 006)

## QA Test Cases

**Test Case 1 — IsInOfferZone 결정적 계산 (AC-CHU-07)**
- **Setup**: Mock `JarScreenPos = FVector2D(640, 360)`, `OfferZoneRadiusPx = 80`.
- **Verify**:
  - `IsInOfferZone(FVector2D(620, 400)) == true` (Distance = sqrt(400+1600) ≈ 44.72).
  - `IsInOfferZone(FVector2D(500, 300)) == false` (Distance = sqrt(19600+3600) ≈ 152.32).
  - `IsInOfferZone(FVector2D(640, 360)) == true` (Distance = 0).
  - `IsInOfferZone(FVector2D(720, 360)) == true` (Distance = 80, boundary inclusive).
  - `IsInOfferZone(FVector2D(721, 360)) == false` (Distance = 81, 밖).
- **Pass**: 5 assertion 모두 충족. Deterministic — 같은 입력 항상 같은 결과.

**Test Case 2 — Dragging 상태에서만 갱신**
- **Setup**: State = `Idle`, DraggedCard null. Mock `IA_PointerMove` Triggered 시뮬.
- **Verify**:
  - `OnPointerMove` early-return. DraggedCard 위치 변경 없음.
- **Pass**: 조건 충족.

**Test Case 3 — Emissive 전환 (AC-CHU-16)**
- **Setup**: State = `Dragging`, `JarScreenPos=(640, 360)`, `OfferZoneRadiusPx=80`.
- **Verify**:
  - PointerPos=(640, 360): `DraggedCard->GetEmissive() == 0.15f`.
  - PointerPos=(500, 100): `DraggedCard->GetEmissive() == 0.0f`.
- **Pass**: 두 조건 충족.

## Test Evidence

- [ ] `tests/unit/ui/offer_zone_hit_test.cpp` — Test Case 1 (5 assertion).
- [ ] `tests/unit/ui/pointer_move_guard_test.cpp` — Test Case 2.
- [ ] `tests/unit/ui/card_emissive_zone_test.cpp` — Test Case 3.

## Dependencies

- **Depends on**: story-001 (Widget 골격), story-002 (Dragging 상태 진입). Foundation Input Abstraction IA_PointerMove, CardHandUIConfigAsset (OfferZoneRadiusPx knob).
- **Unlocks**: story-004 (Release handler — CurrentPointerPos 재사용).
