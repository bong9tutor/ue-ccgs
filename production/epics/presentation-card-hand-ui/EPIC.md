# Epic: Card Hand UI

> **Layer**: Presentation
> **GDD**: [design/gdd/card-hand-ui.md](../../../design/gdd/card-hand-ui.md)
> **Architecture Module**: Presentation / Card Hand — drag-to-offer UMG + Enhanced Input subscription
> **Status**: Ready
> **Stories**: 7 created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Card Hand UI는 Offering 상태 진입 시 `GetDailyHand()`로 3장 카드 위젯을 제시하고, `IA_OfferCard` Hold Triggered + `IA_PointerMove` Enhanced Input 구독으로 drag-to-offer 인터랙션을 소유한다(R2 재정의). `UDragDropOperation` + `NativeOnMouse*` override 금지 — Input Abstraction Rule 1 준수(ADR-0008). Offer Zone hit test + `ConfirmOffer(FName)` 호출 + Stillness Budget Motion/Standard 슬롯 획득. `FOnGameStateChanged(_, Offering)` 구독으로 Show 시퀀스, 다른 상태 진입 시 즉시 Hide + 드래그 취소(EC-CHU-4). Hover-only 상호작용 금지(Pattern 2 — Steam Deck 컨트롤러 대응). Card hover는 시각 highlight 전용(`NativeOnMouseEnter/Leave` 허용).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0008](../../../docs/architecture/adr-0008-umg-drag-implementation.md) | Enhanced Input Subscription — `IA_OfferCard` Hold + `IA_PointerMove`. `NativeOnMouse*` override + `UDragDropOperation` 금지 | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-chu-001 | OQ-CHU-1 UMG 드래그 구현 방식 | ADR-0008 ✅ |
| TR-chu-002 | AC-CHU-12 마우스만으로 전체 게임 기능 접근 가능 (Input Rule 5) | GDD contract ✅ |
| TR-chu-003 | EC-CHU-2 ConfirmOffer false 반환 시 원래 위치 복귀 (CardReturnDurationSec 0.3s) | GDD contract ✅ |
| TR-chu-004 | EC-CHU-4 `FOnGameStateChanged(Offering, _)` 강제 이탈 시 드래그 즉시 취소 + Hide | GDD contract ✅ |
| TR-chu-005 | AC-CHU-13 ReducedMotion ON 시 모든 애니메이션 즉시 표시·Hide | GDD contract ✅ |
| TR-chu-006 | Hover-only 상호작용 금지 (Pattern 2) | control-manifest + ux/interaction-patterns ✅ |

## Key Interfaces

- **Publishes**: 없음 (UI는 Card System API 호출로 통신)
- **Consumes**: `FOnGameStateChanged` (Show/Hide trigger), `OnInputModeChanged` (device swap), `IA_OfferCard` + `IA_PointerMove` Input Action, Card `GetDailyHand()` / `ConfirmOffer(FName)`, Stillness Budget `Request(Motion/Standard)` + `IsReducedMotion()`
- **Owned types**: `UCardHandWidget` (UUserWidget), `UCardSlotWidget` (카드 위젯), `ECardHandUIState` (Hidden/Idle/Dragging/Offering), `FStillnessHandle` (RAII)
- **Settings**: 없음 (또는 `UMossCardHandUISettings` — CardReturnDurationSec 등 추후 검토)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [`UCardHandWidget` 위젯 골격 + `NativeConstruct` 바인딩](story-001-card-hand-widget-skeleton-native-construct.md) | UI | Ready | ADR-0008 |
| 002 | [`OnOfferCardHoldStarted` — Dragging 상태 진입 + Stillness Request](story-002-offer-card-hold-start-dragging-entry.md) | UI | Ready | ADR-0008 |
| 003 | [`OnPointerMove` — 드래그 중 카드 위치 갱신 + Emissive 피드백](story-003-pointer-move-drag-tracking.md) | UI | Ready | ADR-0008 |
| 004 | [`OnOfferCardReleased` — Offer Zone 판정 + `ConfirmOffer` 분기](story-004-offer-card-released-zone-decision.md) | Integration | Ready | ADR-0008 |
| 005 | [State machine 완성 + `FOnGameStateChanged` 구독 + Show/Hide](story-005-state-machine-gsm-subscription.md) | Integration | Ready | ADR-0008 |
| 006 | [ReducedMotion 분기 — 애니메이션 즉시 표시/Hide](story-006-reduced-motion-branches.md) | Visual/Feel | Ready | ADR-0008 |
| 007 | [Degraded 모드 (0-2장) + Mouse-only Manual QA + Forbidden API grep](story-007-degraded-mode-and-manual-qa.md) | UI | Ready | ADR-0008 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-CHU-01~14)
- AC-CHU-12 마우스 전용 전체 기능 접근 (키보드 불필요) MANUAL 검증
- AC-CHU-13 ReducedMotion 애니메이션 스킵 테스트
- `NativeOnMouseButtonDown/Move/Up` grep in `UCardHandWidget*.cpp` = 0건 (ADR-0008)
- `UDragDropOperation` grep in `UCardHandWidget*.cpp` = 0건 (ADR-0008)
- `IA_OfferCard` Hold 0.15s (Mouse) / 0.20s (Gamepad) 정확 trigger INTEGRATION 테스트
- Hover-only drag 시작 금지 — hover만으로 state 변경 테스트 0건
- EC-CHU-4 상태 이탈 시 드래그 즉시 취소 + Stillness Handle Release 테스트

## Next Step

Run `/create-stories presentation-card-hand-ui` to break this epic into implementable stories.
