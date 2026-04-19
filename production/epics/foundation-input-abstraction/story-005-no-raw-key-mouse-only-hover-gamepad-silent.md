# Story 005: NO_RAW_KEY_BINDING CI grep + MOUSE_ONLY_COMPLETENESS + HOVER_ONLY_PROHIBITION + GAMEPAD_DISCONNECT_SILENT

> **Epic**: Input Abstraction Layer
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/input-abstraction-layer.md`
**Requirement**: `TR-input-002` (NO_RAW_KEY_BINDING — Rule 1 CODE_REVIEW), `TR-input-004` (Rule 5 Mouse-only 완결성 + Hover 금지)
**ADR Governing Implementation**: ADR-0008 (UMG raw event override 금지)
**ADR Decision Summary**: 모든 raw key/button 바인딩 금지 — `BindKey`, `BindAxis` legacy, `EKeys::` 직접 참조 0 매치. `NativeOnMouse*` override 0건 (ADR-0008). Mouse-only 전체 기능 접근 가능. Hover-only 금지 (Steam Deck 대응).
**Engine**: UE 5.6 | **Risk**: LOW
**Engine Notes**: CI grep hook만 구성. Mouse-only, Hover-only, Gamepad disconnect는 MANUAL (실기 검증). Pillar 1: 장치 분리 시 알림·모달 금지.

**Control Manifest Rules (Foundation + Presentation + Cross-cutting)**:
- Required: Pillar 1 "Input 장치 전환 알림 금지"
- Required: "Hover-only 상호작용 금지" — Pattern 2 Steam Deck 컨트롤러 모드 대응 (Cross-cutting)
- Forbidden: "`NativeOnMouseButtonDown`/`NativeOnMouseMove`/`NativeOnMouseButtonUp` override" — ADR-0008

---

## Acceptance Criteria

*From GDD §Acceptance Criteria Rule 1, 4, 5, E2:*

- [ ] `NO_RAW_KEY_BINDING` CODE_REVIEW: `grep -rE "BindKey\|BindAxis\|EKeys::" Source/MadeByClaudeCode/ --include="*.cpp" --include="*.h"` 0 매치 (Input 서브시스템 파일 제외)
- [ ] `NO_NATIVEONMOUSE_OVERRIDE` CODE_REVIEW (ADR-0008 보강): `grep -rE "NativeOnMouseButtonDown\|NativeOnMouseMove\|NativeOnMouseButtonUp" Source/MadeByClaudeCode/ --include="*.h"` 0 매치
- [ ] `MOUSE_ONLY_COMPLETENESS` MANUAL: 게임패드 미연결 + 키보드 미사용 → 마우스만으로 (카드 선택, 카드 건넴, 일기 열기/닫기, 메뉴 진입/퇴장) 전체 기능 접근 가능
- [ ] `HOVER_ONLY_PROHIBITION` MANUAL: 카드 3장 위에 마우스 hover 3초 → 시각 highlight만, `OnCardSelected` delegate 미발행, 게임 상태 불변
- [ ] `GAMEPAD_DISCONNECT_SILENT` MANUAL: Gamepad 모드 활성 중 USB 케이블 물리적 분리 → 팝업·모달·알림 미생성, 마우스 이동 시 Mouse 모드 정상 전환
- [ ] `.claude/hooks/input-no-raw-key-binding.sh` + `.claude/hooks/input-no-nativeonmouse.sh` 스크립트 작성

---

## Implementation Notes

*Derived from GDD §Rule 1, 4, 5 + ADR-0008 §Forbidden Approaches:*

**CI grep hooks**:

```bash
# .claude/hooks/input-no-raw-key-binding.sh
#!/usr/bin/env bash
# Input Abstraction Rule 1: raw key binding 금지 (NO_RAW_KEY_BINDING)
set -e
PATTERN='BindKey\|BindAxis\|EKeys::'
# Input 서브시스템 파일 자체는 제외
MATCHES=$(grep -rEn "$PATTERN" \
    Source/MadeByClaudeCode/ \
    --include="*.cpp" --include="*.h" \
    --exclude-dir="Input" \
    || true)
if [[ -n "$MATCHES" ]]; then
    echo "ERROR: NO_RAW_KEY_BINDING 위반 — raw key binding 검출:"
    echo "$MATCHES"
    exit 1
fi
echo "OK: NO_RAW_KEY_BINDING 통과"

# .claude/hooks/input-no-nativeonmouse.sh
#!/usr/bin/env bash
# ADR-0008: NativeOnMouse* override 금지
set -e
PATTERN='NativeOnMouseButtonDown\|NativeOnMouseMove\|NativeOnMouseButtonUp'
MATCHES=$(grep -rEn "$PATTERN" \
    Source/MadeByClaudeCode/ \
    --include="*.h" \
    || true)
if [[ -n "$MATCHES" ]]; then
    echo "ERROR: NO_NATIVEONMOUSE_OVERRIDE 위반 — raw mouse event override 검출:"
    echo "$MATCHES"
    exit 1
fi
echo "OK: NO_NATIVEONMOUSE_OVERRIDE 통과"
```

**MANUAL 체크리스트 문서**: `production/qa/evidence/input-manual-checklist.md`

```markdown
# Input Abstraction MANUAL 체크리스트

## MOUSE_ONLY_COMPLETENESS
- [ ] 게임패드 미연결, 키보드 미사용 환경 준비
- [ ] 카드 3장 중 1장 선택 (마우스 클릭)
- [ ] 카드 건넴 (마우스 drag)
- [ ] 일기 열기 (UI 버튼 클릭)
- [ ] 일기 닫기 (UI 닫기 버튼 또는 외부 클릭)
- [ ] 메뉴 진입 + 퇴장 (UI 버튼 클릭)
- Pass: 6 항목 모두 통과

## HOVER_ONLY_PROHIBITION
- [ ] Card Hand UI에 카드 3장 표시
- [ ] 각 카드 위 3초 마우스 hover
- [ ] Pass: 시각 highlight만, `OnCardSelected` delegate 미발행 (로그 확인), `FSessionRecord` 불변

## GAMEPAD_DISCONNECT_SILENT
- [ ] Gamepad 모드 활성 (A버튼 1회 눌러 Mode=Gamepad 전환)
- [ ] Gamepad USB 케이블 물리적 분리
- [ ] Pass: 팝업/모달/알림 없음 (스크린 관찰)
- [ ] 마우스 이동 시 Mouse 모드 전환 + 커서 표시
- [ ] 카드 선택/건넴 정상 작동
```

---

## Out of Scope

- E5 Hold 중 focus loss 리셋 (구현 verification 필요 — Story 006 또는 Card Hand UI sprint)

---

## QA Test Cases

**For Config/Data story:**
- **NO_RAW_KEY_BINDING (CODE_REVIEW)**
  - Setup: `.claude/hooks/input-no-raw-key-binding.sh`
  - Verify: hook 실행 → exit 0
  - Pass: 0 매치. 실험: 가짜 `InputComponent->BindKey(EKeys::Q, ...)` 삽입 (Input 서브시스템 외 파일) → exit 1
- **NO_NATIVEONMOUSE_OVERRIDE (CODE_REVIEW)**
  - Setup: `.claude/hooks/input-no-nativeonmouse.sh`
  - Verify: 실행 → exit 0
  - 실험: 가짜 `virtual FReply NativeOnMouseButtonDown(...) override` 삽입 → exit 1
- **MOUSE_ONLY_COMPLETENESS (MANUAL)**
  - Setup: `production/qa/evidence/input-manual-checklist.md` 체크리스트 준비
  - Verify: 6 checkpoint 각각 통과 + 스크린 녹화
  - Pass: `production/qa/evidence/mouse-only-evidence-[date].md`
- **HOVER_ONLY_PROHIBITION (MANUAL)**
  - Setup: Card Hand UI 활성 (Card Hand UI sprint에서 전체 검증 — 본 story는 기본 경로만 확인)
  - Verify: hover 3초간 delegate 로그 + state 불변
  - Pass: `production/qa/evidence/hover-only-evidence-[date].md`
- **GAMEPAD_DISCONNECT_SILENT (MANUAL)**
  - Setup: Gamepad 연결
  - Verify: 연결→모드 전환→물리적 분리→scan
  - Pass: 3 checkpoint 통과 스크린샷

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- `.claude/hooks/input-no-raw-key-binding.sh` + `.claude/hooks/input-no-nativeonmouse.sh` (CI hooks)
- `production/qa/evidence/mouse-only-evidence-[date].md` (MANUAL)
- `production/qa/evidence/hover-only-evidence-[date].md` (MANUAL, 필요 시 Card Hand UI sprint에서 최종 검증)
- `production/qa/evidence/gamepad-disconnect-evidence-[date].md` (MANUAL)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001, 002, 003, 004 (Input 인프라 완성)
- Unlocks: None (epic 최종 검증)
