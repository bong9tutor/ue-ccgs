# Story 001: 6 InputAction 에셋 정의 + IMC_MouseKeyboard + IMC_Gamepad 매핑 컨텍스트

> **Epic**: Input Abstraction Layer
> **Status**: Awaiting Editor
> **Layer**: Foundation
> **Type**: Config/Data
> **Estimate**: 0.3 days (~2 hours, Editor 수동)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/input-abstraction-layer.md`
**Requirement**: `TR-input-002` (Rule 1 NO_RAW_KEY_BINDING — 6 Action 정의)
**ADR Governing Implementation**: None (GDD contract 직접 구현)
**ADR Decision Summary**: 6 Game Action (`IA_PointerMove`, `IA_Select`, `IA_OfferCard`, `IA_NavigateUI`, `IA_OpenJournal`, `IA_Back`) + 2 Input Mapping Context (`IMC_MouseKeyboard` 구현, `IMC_Gamepad` 구조만 정의 — MVP).
**Engine**: UE 5.6 | **Risk**: LOW (Enhanced Input 표준 API)
**Engine Notes**: `UInputAction` + `UInputMappingContext` 에셋은 Content Browser에서 생성. `UInputTriggerHold`의 `HoldTimeThreshold`로 Hold 임계 시간 설정 (Formula 2 연계).

**Control Manifest Rules (Foundation layer + Presentation)**:
- Required: Presentation "Card Hand UI 드래그는 Enhanced Input Subscription" — `IA_OfferCard` Hold (ADR-0008)
- Forbidden: Presentation "`NativeOnMouseButtonDown`/`NativeOnMouseMove`/`NativeOnMouseButtonUp` override" — raw event 처리 금지

---

## Acceptance Criteria

*From GDD §Acceptance Criteria §Rule 1, 2a:*

- [ ] `ACTION_COUNT_COMPLETE`: `UInputAction` 에셋 6개 정확히 존재 — `IA_PointerMove` (Axis2D), `IA_Select` (bool), `IA_OfferCard` (bool), `IA_NavigateUI` (Axis2D), `IA_OpenJournal` (bool), `IA_Back` (bool)
- [ ] `BINDING_TABLE_COMPLETE`: `IMC_MouseKeyboard` 매핑 컨텍스트에 6 Action 모두 바인딩 — Rule 2a 테이블 따름:
  - `IA_PointerMove` ← Mouse XY (Axis2D 연속)
  - `IA_Select` ← LMB Released (Hold 미달 시)
  - `IA_OfferCard` ← LMB Hold (`OfferDragThresholdSec` 0.15s)
  - `IA_NavigateUI` ← Arrow Keys (Axis2D)
  - `IA_OpenJournal` ← J Pressed
  - `IA_Back` ← Esc Pressed
- [ ] `IMC_Gamepad` 컨텍스트 존재 (MVP: 구조만 정의) — 5 Action 바인딩 스텁 (VS에서 구현):
  - `IA_Select` ← A Released
  - `IA_OfferCard` ← A Hold (`OfferHoldDurationSec` 0.5s)
  - `IA_NavigateUI` ← D-pad / Left Stick
  - `IA_OpenJournal` ← Y
  - `IA_Back` ← B
- [ ] Content Browser에서 각 에셋 이름 + ValueType 수동 확인 — 본 story의 Pass 조건

---

## Implementation Notes

*Derived from GDD §Core Rules §Rule 1, 2, 2a + §Formula 2:*

**에셋 생성 절차 (UE Editor)**:

1. `Content/Input/Actions/` 폴더 생성
2. 각 `UInputAction` 에셋 생성:
   - Content Browser → Add → Input → Input Action
   - 이름: `IA_PointerMove`, `IA_Select`, `IA_OfferCard`, `IA_NavigateUI`, `IA_OpenJournal`, `IA_Back`
   - Value Type: `Axis2D` (PointerMove, NavigateUI) 또는 `Digital (bool)` (나머지)
3. `Content/Input/Contexts/` 폴더 생성
4. `IMC_MouseKeyboard` (Input Mapping Context) 생성:
   - 6 Action 각각 매핑 추가
   - `IA_OfferCard` 매핑: Key=LMB, Trigger=`UInputTriggerHold`, `HoldTimeThreshold=0.15` (Story 001에서 직접 설정; Story 003에서 `UMossInputAbstractionSettings`에서 읽도록 래핑)
   - `IA_Select` 매핑: Key=LMB, Trigger=`UInputTriggerReleased` (implicit 우선순위로 Hold 성립 시 Released 억제 — GDD 참고)
5. `IMC_Gamepad` (MVP 구조만): 각 Action 매핑 *존재*는 하지만 실제 구현은 비워두거나 VS에서 채움

**폴더 위치 문서화**: `.claude/agent-memory/ux-designer/input-assets.md` (또는 프로젝트 위키)에 경로 기록

---

## Out of Scope

- Story 002: `UMossInputAbstractionSubsystem` 뼈대 + mapping context 등록 자동화
- Story 003: `UMossInputAbstractionSettings` (Hold threshold knob)
- Story 004: Input mode auto-detection + hysteresis
- Story 005: 장치 전환 silent + Mouse-only 완결성 MANUAL

---

## QA Test Cases

**For Config/Data story:**
- **ACTION_COUNT_COMPLETE (CODE_REVIEW/Manual)**
  - Setup: Content Browser → `Content/Input/Actions/`
  - Verify: 6 `UInputAction` 에셋 존재, 각 이름 + ValueType 일치 (Axis2D × 2, Digital × 4)
  - Pass: 스크린샷 `production/qa/evidence/input-actions-[date].md`
- **BINDING_TABLE_COMPLETE (CODE_REVIEW/Manual)**
  - Setup: `Content/Input/Contexts/IMC_MouseKeyboard`
  - Verify: 각 Action의 Key + Trigger Type이 Rule 2a 일치
  - Pass: 6 바인딩 모두 확인. `IA_OfferCard`의 Trigger가 `UInputTriggerHold`이고 `HoldTimeThreshold ≈ 0.15`
- **IMC_Gamepad 구조 존재**
  - Setup: `IMC_Gamepad` asset
  - Verify: 5 Action 각각의 매핑 슬롯 존재 (Key 미설정 허용 — VS에서 채움)
  - Pass: 자산 파일 존재 + Content Browser에서 열 수 있음

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- `production/qa/evidence/input-actions-[date].md` (스크린샷 + 6 에셋 목록)
- Content Browser 스크린샷

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (Foundation 독립)
- Unlocks: Story 002, 003

---

## Completion Notes (Partial — Awaiting Editor Asset Creation)

**Partially Completed**: 2026-04-19 (agent automation limit)
**Status**: **Awaiting Editor** — Config/Data 스토리이고 .uasset 바이너리 생성은 UE Editor GUI 필수 작업. Agent 자동화 불가.

**Agent 완료 작업**:
- `production/qa/evidence/input-actions-placeholder-2026-04-19.md` 신규 — 사용자 수동 에셋 생성 가이드 + 체크리스트 + 스크린샷 템플릿
- tech-debt TD-008 등록 (Sprint 1 종료 전 해결 필수)

**사용자 수동 작업 필요** (Sprint 1 종료 전):
1. UE Editor에서 `Content/Input/Actions/` 폴더 생성
2. 6 UInputAction 에셋 생성 (`IA_PointerMove` Axis2D + 5 Digital)
3. `Content/Input/Contexts/IMC_MouseKeyboard` 생성 + Rule 2a 바인딩 테이블
4. `IA_OfferCard` Trigger = UInputTriggerHold (HoldTimeThreshold 0.15 — Formula 2 마우스 임계)
5. `IMC_Gamepad` 구조 생성 (5 Action 슬롯, MVP)
6. 4장 스크린샷 evidence 첨부

**Story 1-12/1-13 진행 전제**: C++ 코드는 에셋 이름 상수 (`IA_PointerMove` 등) 참조하여 런타임 로드. 에셋이 없어도 C++ 코드 작성은 가능하나 실제 테스트는 에셋 생성 이후.

**Sprint 1 완료 게이트**: 이 Story는 Must Have. 사용자 에디터 작업 완료 후 Status: Awaiting Editor → Complete 전환, sprint-status.yaml 갱신.
