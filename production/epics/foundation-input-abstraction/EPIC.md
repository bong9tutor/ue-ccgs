# Epic: Input Abstraction Layer

> **Layer**: Foundation
> **GDD**: [design/gdd/input-abstraction-layer.md](../../../design/gdd/input-abstraction-layer.md)
> **Architecture Module**: Foundation / Input — Enhanced Input wrapping + Mouse/Gamepad unified abstraction
> **Status**: Ready
> **Stories**: 5 stories created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Input Abstraction Layer는 UE 5.6 Enhanced Input 위에 `IA_OfferCard` (Hold), `IA_PointerMove` (Axis2D) 등 의도 레벨 Input Action을 정의하여 Mouse/Gamepad 입력 경로를 통합한다. 원시 `FPointerEvent` 또는 `FKey` 바인딩을 금지하고 모든 상위 레이어가 Input Action 구독만 사용하도록 강제한다(Rule 1). Mouse(0.15s) vs Gamepad(0.20s) Hold threshold 차이를 추상화 내부로 흡수. 마지막 사용 장치 자동 감지로 `EInputMode` + `OnInputModeChanged` delegate 발행(알림·모달 없음). Steam Deck 트랙패드는 Mouse 에뮬 경로로 통합.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossInputAbstractionSettings` — Mouse/Gamepad Hold threshold knob | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-input-001 | Gamepad drag alternative (OQ-2) — Hold + focus 기반 | GDD contract (VS sprint) ✅ |
| TR-input-002 | Rule 1 `NO_RAW_KEY_BINDING` — CODE_REVIEW AC | GDD contract ✅ |
| TR-input-003 | Rule 3 `EInputMode` auto-detection + hysteresis | GDD contract ✅ |
| TR-input-004 | Rule 5 Mouse로 전체 게임 기능 접근 가능 (Input Rule 5 = AC-CHU-12) | GDD contract ✅ |

## Key Interfaces

- **Publishes**: `OnInputModeChanged(EInputMode NewMode)`, Input Action bindings (`IA_OfferCard`, `IA_PointerMove`, etc.)
- **Consumes**: UE 5.6 Enhanced Input Subsystem (`UEnhancedInputLocalPlayerSubsystem`)
- **Owned types**: `EInputMode` (Mouse, Gamepad), IMC (Input Mapping Context) assets
- **Settings**: `UMossInputAbstractionSettings` (MouseHoldThresholdSec=0.15, GamepadHoldThresholdSec=0.20, InputModeHysteresisSec)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | 6 InputAction 에셋 정의 + IMC_MouseKeyboard + IMC_Gamepad 매핑 컨텍스트 | Config/Data | Ready | ADR-0011 |
| 002 | UMossInputAbstractionSubsystem + UMossInputAbstractionSettings + IMC 등록 | Logic | Ready | ADR-0011 |
| 003 | Input Mode Auto-Detection + Hysteresis (Formula 1) + OnInputModeChanged delegate | Logic | Ready | ADR-0011 |
| 004 | Offer Hold Formula 2 — Mouse 0.15s / Gamepad 0.5s boundary + Select/OfferCard 공유 해소 | Logic | Ready | ADR-0008 |
| 005 | NO_RAW_KEY_BINDING CI grep + MOUSE_ONLY + HOVER_ONLY + GAMEPAD_DISCONNECT MANUAL | Config/Data | Ready | ADR-0008 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증
- `NO_RAW_KEY_BINDING` CODE_REVIEW grep — `NativeOnMouse*` override 0건, `FKey` 직접 바인딩 0건
- Mouse Hold 0.15s / Gamepad Hold 0.20s trigger AUTOMATED 테스트 (Formula 2)
- InputModeChanged hysteresis 테스트 — 장치 전환이 부드럽고 알림 발생 안 함
- Steam Deck 트랙패드 → Mouse 에뮬 동일 코드 경로 수동 검증

## Next Step

Run `/create-stories foundation-input-abstraction` to break this epic into implementable stories.
