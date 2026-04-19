# Input Actions Asset Evidence — PLACEHOLDER

**Story**: `production/epics/foundation-input-abstraction/story-001-input-actions-mapping-contexts.md`
**Status**: ⏳ Awaiting UE Editor Manual Asset Creation
**Created**: 2026-04-19
**Type**: Config/Data (에셋 생성은 Agent 자동화 불가 — UE Editor GUI 필수)

---

## 에셋 생성 가이드 (사용자 수동 작업)

### 1. 폴더 생성

```
Content/Input/Actions/
Content/Input/Contexts/
```

UE Editor → Content Browser 우클릭 → New Folder.

### 2. UInputAction 에셋 6개 생성

Content Browser → Add → Input → Input Action. 각각 다음 이름으로:

| 에셋명 | Value Type | 용도 |
|-------|-----------|------|
| `IA_PointerMove` | Axis2D | 마우스/스틱 이동 (연속) |
| `IA_Select` | Digital (bool) | 선택·확인 (LMB Released / A Released) |
| `IA_OfferCard` | Digital (bool) | 카드 건넴 드래그 (LMB/A Hold) |
| `IA_NavigateUI` | Axis2D | UI 방향 이동 (Arrow/D-pad/Stick) |
| `IA_OpenJournal` | Digital (bool) | 저널 열기 (J / Y) |
| `IA_Back` | Digital (bool) | 뒤로가기·취소 (Esc / B) |

### 3. IMC_MouseKeyboard (Input Mapping Context) 생성

Content Browser → Add → Input → Input Mapping Context. 이름: `IMC_MouseKeyboard`.

**6 Action 매핑**:

| Action | Key | Trigger | 비고 |
|--------|-----|---------|------|
| `IA_PointerMove` | Mouse XY (X Axis + Y Axis) | — (기본 Axis 연속) | — |
| `IA_Select` | Mouse Left Button | `UInputTriggerReleased` | Hold 성립 시 억제 (GDD §Rule 2a) |
| `IA_OfferCard` | Mouse Left Button | `UInputTriggerHold` (`HoldTimeThreshold = 0.15`) | Formula 2 마우스 임계 |
| `IA_NavigateUI` | Arrow Keys (Up/Down/Left/Right) | — | Axis2D 합성 |
| `IA_OpenJournal` | J Key | `UInputTriggerPressed` | — |
| `IA_Back` | Escape Key | `UInputTriggerPressed` | — |

### 4. IMC_Gamepad 생성 (MVP: 구조만)

Content Browser → Add → Input → Input Mapping Context. 이름: `IMC_Gamepad`.

**5 Action 매핑 슬롯** (Key는 VS sprint에서 실제 바인딩):

| Action | Key (예정) | Trigger | 비고 |
|--------|-----------|---------|------|
| `IA_Select` | Gamepad Face Button Bottom (A) | `UInputTriggerReleased` | — |
| `IA_OfferCard` | Gamepad Face Button Bottom (A) | `UInputTriggerHold` (`HoldTimeThreshold = 0.5`) | Formula 2 게임패드 임계 |
| `IA_NavigateUI` | Gamepad Left Thumbstick 2D-Axis | — | D-pad 병행 가능 |
| `IA_OpenJournal` | Gamepad Face Button Top (Y) | `UInputTriggerPressed` | — |
| `IA_Back` | Gamepad Face Button Right (B) | `UInputTriggerPressed` | — |

**MVP 범위**: 매핑 슬롯 *존재*만 확인. 실제 Key 바인딩은 VS sprint에서 완성.

---

## 검증 체크리스트 (사용자 수동 검증)

- [ ] `Content/Input/Actions/` 폴더 존재
- [ ] 6 `UInputAction` 에셋 존재 (이름 + ValueType 일치)
- [ ] `Content/Input/Contexts/IMC_MouseKeyboard` 존재 + 6 Action 모두 바인딩
- [ ] `IA_OfferCard` Trigger = `UInputTriggerHold` (HoldTimeThreshold ≈ 0.15)
- [ ] `Content/Input/Contexts/IMC_Gamepad` 존재 + 5 Action 매핑 슬롯

### 스크린샷 권장

다음 스크린샷을 이 파일 하단에 첨부 또는 별도 .png로 저장 (`production/qa/evidence/input-actions-screenshots-2026-04-19/`):

1. Content Browser `Content/Input/Actions/` 6 에셋 목록
2. `IA_OfferCard` 상세 (Trigger Hold + HoldTimeThreshold 값)
3. `IMC_MouseKeyboard` 상세 (6 Action 바인딩 테이블)
4. `IMC_Gamepad` 상세 (5 Action 슬롯)

---

## 다음 단계

1. ✅ UE Editor에서 에셋 6 + 컨텍스트 2 생성
2. ✅ 스크린샷 4장 첨부
3. ✅ 체크리스트 전체 체크
4. ✅ Story 1-11 status를 `awaiting-editor` → `done`으로 변경
5. ✅ tech-debt TD-008 close

완료 후 `production/epics/foundation-input-abstraction/story-001-input-actions-mapping-contexts.md` Completion Notes의 Evidence 섹션 갱신.

---

## 참고 문서

- [Enhanced Input docs (Epic Games)](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine)
- GDD §Rule 2a: `design/gdd/input-abstraction-layer.md`
- Formula 2 (Hold threshold): mouse 0.15s / gamepad 0.5s
- Story 1-12/1-13이 이 에셋들을 런타임에 로드하여 `UMossInputAbstractionSubsystem`에 등록.
