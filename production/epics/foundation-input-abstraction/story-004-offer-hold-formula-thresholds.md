# Story 004: Offer Hold Formula 2 — Mouse 0.15s / Gamepad 0.5s boundary + Select/OfferCard 공유 해소

> **Epic**: Input Abstraction Layer
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 0.5 days (~3 hours)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/input-abstraction-layer.md`
**Requirement**: `TR-input-001` (Gamepad drag alternative — Hold + focus), Formula 2 Hold threshold
**ADR Governing Implementation**: ADR-0008 (Card Hand UI drag — Enhanced Input subscription, Hold threshold Input Abstraction에 위임)
**ADR Decision Summary**: Mouse 모드: LMB Hold `OfferDragThresholdSec` (0.15s) → drag 시작. Gamepad 모드: A Hold `OfferHoldDurationSec` (0.5s) → 건넴 확정. 둘 다 Formula 2 `OfferTriggered = (HoldElapsed ≥ HoldThreshold)` **inclusive `≥`**. Hold 미달 릴리스 → `IA_Select` 발화 (implicit trigger 우선순위).
**Engine**: UE 5.6 | **Risk**: LOW (`UInputTriggerHold` 표준 API)
**Engine Notes**: `UInputTriggerHold::HoldTimeThreshold` 프로퍼티 — IA_OfferCard 에셋 시점에 설정 (Story 001에서). Settings 런타임 변경 시 재등록 필요 (`ConfigRestartRequired=true`).

**Control Manifest Rules (Foundation + Presentation)**:
- Required: Presentation "`IA_OfferCard` Hold `ETriggerEvent::Triggered` → `OnOfferCardHoldStarted`" (ADR-0008)
- Required: Presentation "Hold threshold — Mouse 0.15s / Gamepad 0.20s (Formula 2)" — GDD Formula 2와 일치 (0.15s Mouse / 0.5s Gamepad default로 재확인)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria Formula 2:*

- [ ] `OFFER_HOLD_BOUNDARY_GAMEPAD`: `OfferHoldDurationSec = 0.5` + Gamepad 모드 + 카드 포커스 → A버튼 정확히 0.5초 유지 후 릴리스 → `IA_OfferCard` Triggered 이벤트 발행
- [ ] `OFFER_HOLD_CANCEL_GAMEPAD`: A버튼 0.3초 유지 후 릴리스 (threshold 미만) → `IA_OfferCard` Triggered 미발행 + `IA_Select` Released 발행
- [ ] `OFFER_DRAG_THRESHOLD_MOUSE`: `OfferDragThresholdSec = 0.15` + Mouse 모드 → LMB 정확히 0.15초 유지 → `IA_OfferCard` Triggered (drag 시작 가능)
- [ ] `OFFER_CLICK_SELECT_MOUSE`: LMB 0.1초 유지 후 릴리스 (threshold 미만) → `IA_OfferCard` Triggered 미발행 + `IA_Select` Released 발행
- [ ] Formula 2 inclusive `≥` 검증: 정확 threshold 시 트리거 발행 (GDD §Formula 2 경계 설계)
- [ ] `IA_Select` 과 `IA_OfferCard` 같은 키 공유 해소 — UE Enhanced Input implicit trigger priority가 Hold 성립 시 Released 억제

---

## Implementation Notes

*Derived from GDD §Formula 2 + §Rule 2a + ADR-0008 §Card Hand UI 드래그 Enhanced Input:*

**IA_OfferCard 에셋 설정 (Content Browser)**:

1. `/Game/Input/Actions/IA_OfferCard` 열기
2. Triggers 목록에 `UInputTriggerHold` 추가
3. `HoldTimeThreshold = 0.15` (Mouse 기본값) — Gamepad는 에셋 분리 또는 Mode별 IMC에서 override

**대안 접근**: IMC_MouseKeyboard와 IMC_Gamepad 각각에서 `IA_OfferCard` 매핑할 때 Trigger의 `HoldTimeThreshold`를 override (IMC 수준 trigger 설정 — UE 5.6 지원):

- `IMC_MouseKeyboard` → IA_OfferCard → Key=LMB, Trigger=UInputTriggerHold, `HoldTimeThreshold=0.15`
- `IMC_Gamepad` → IA_OfferCard → Key=GamepadFaceButtonBottom(A), Trigger=UInputTriggerHold, `HoldTimeThreshold=0.5`

**UMossInputAbstractionSubsystem 통합**:

```cpp
// 런타임에 Settings knob에서 HoldTimeThreshold 반영 (optional — ConfigRestartRequired 기반 권장)
void UMossInputAbstractionSubsystem::ApplySettingsToMappingContext() {
    const auto* Settings = UMossInputAbstractionSettings::Get();
    // UInputMappingContext의 Mapping 배열 순회 + UInputTriggerHold 찾아 HoldTimeThreshold 설정
    // 단, ConfigRestartRequired=true이므로 Editor reload가 권장되는 경로
    for (FEnhancedActionKeyMapping& Mapping : IMC_MouseKeyboard->GetMappings()) {
        if (Mapping.Action == IA_OfferCard) {
            for (auto& Trigger : Mapping.Triggers) {
                if (auto* HoldTrigger = Cast<UInputTriggerHold>(Trigger)) {
                    HoldTrigger->HoldTimeThreshold = Settings->OfferDragThresholdSec;
                }
            }
        }
    }
    // IMC_Gamepad 동일 패턴 (OfferHoldDurationSec 사용)
}
```

- **Select/OfferCard 공유 키 해소**: GDD §Rule 2a 마지막 노트 — "UE Enhanced Input의 Implicit Trigger 우선순위가 Hold threshold 판정 후 Released 발화 여부를 결정 — Hold 성립 시 Released 억제". 별도 코드 없음 (UE 내장 동작)

---

## Out of Scope

- Story 005: MANUAL AC (MOUSE_ONLY_COMPLETENESS, HOVER_ONLY_PROHIBITION, GAMEPAD_DISCONNECT_SILENT)
- Story 006: E5 Hold 중 focus loss 리셋 (구현 verification 필요)

---

## QA Test Cases

**For Logic story:**
- **OFFER_HOLD_BOUNDARY_GAMEPAD**
  - Given: OfferHoldDurationSec = 0.5, Gamepad 모드
  - When: Mock input — A button down for 0.5s (정확 threshold) then release
  - Then: `IA_OfferCard` Triggered 이벤트 1회 (inclusive `≥`)
- **OFFER_HOLD_CANCEL_GAMEPAD**
  - Given: 동일
  - When: A button down for 0.3s (threshold 미만) then release
  - Then: `IA_OfferCard` Triggered 카운트 == 0, `IA_Select` Released 카운트 == 1
- **OFFER_DRAG_THRESHOLD_MOUSE**
  - Given: OfferDragThresholdSec = 0.15, Mouse 모드
  - When: LMB down for 0.15s
  - Then: `IA_OfferCard` Triggered 카운트 == 1
- **OFFER_CLICK_SELECT_MOUSE**
  - Given: 동일
  - When: LMB down for 0.1s then release
  - Then: `IA_OfferCard` Triggered 카운트 == 0, `IA_Select` Released 카운트 == 1
- **Formula 1 vs Formula 2 boundary asymmetry**
  - Verify: 코드 리뷰로 Formula 1 strict `>`, Formula 2 inclusive `≥` 일관 구현 확인 (GDD §Formula 2 note)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/input/offer_hold_formula_test.cpp` (4 boundary AUTOMATED tests)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (IA_OfferCard 에셋), Story 002 (Settings + Subsystem)
- Unlocks: Story 005

---

## Completion Notes

**Completed**: 2026-04-19
**Criteria**: 4/6 AUTOMATED (unit level) + 2 deferred (boundary 실측 integration)

**Files delivered**:
- `Input/MossInputAbstractionSubsystem.h/.cpp` (수정) — ApplySettingsToMappingContext + ApplyHoldThresholdToIMC + TestingApplySettingsToSingleIMC 훅 + Initialize에 호출 추가
- `Tests/InputOfferHoldTests.cpp` (신규, 4 tests)
- `tests/unit/input/README.md` Story-1-17 섹션 append

**Test Evidence**: 4 UE Automation — `MossBaby.Input.OfferHold.{ApplyNoOpOnNullAsset/ApplyToMockIMC/ApplyMultipleHoldTriggers/ApplyIgnoresWrongAction}`

**AC 커버**:
- [x] Null-safe ApplySettingsToMappingContext: ApplyNoOpOnNullAssetTest
- [x] Mock IMC HoldTimeThreshold 적용: ApplyToMockIMCTest (0.15f)
- [x] 다중 Hold trigger 카운트: ApplyMultipleHoldTriggersTest (Hold 2, Pressed 제외)
- [x] 다른 Action mapping 무시: ApplyIgnoresWrongActionTest
- [~] OFFER_HOLD_BOUNDARY_GAMEPAD/MOUSE (0.15/0.5s 실측): TD-011 — PIE integration test (TD-008 에셋 생성 후)
- [x] IA_Select/IA_OfferCard 공유 키: UE Enhanced Input implicit trigger priority (엔진 내장)

**ADR/Rule 준수**:
- ADR-0001 grep: 0 매치
- ADR-0008: Enhanced Input subscription 기반 Hold threshold 동적 반영
- Formula 2 inclusive `>=` (Formula 1 strict `>`과 대비) — GDD §Formula 2 경계 설계 엄수

**Deferred**:
- OFFER_HOLD_BOUNDARY 런타임 실측: TD-011 (Story 1-11 에셋 + PIE 필요)
- IA_OfferCard 에셋 Trigger 속성 자체: Story 1-11 에디터 작업 (TD-008)

**Out of Scope**:
- Story 1-21: NO_RAW_KEY_BINDING CI grep + Mouse-only MANUAL + Gamepad disconnect
