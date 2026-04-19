# Story 008: NO_ANTITAMPER_LOGIC + NO_SAVE_INDICATOR_UI CODE_REVIEW CI grep hooks

> **Epic**: Save/Load Persistence
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/save-load-persistence.md`
**Requirement**: `TR-save-002` (NO_ANTITAMPER_LOGIC), `TR-save-003` (NO_SAVE_INDICATOR_UI — Pillar 1)
**ADR Governing Implementation**: ADR-0001 (Forward Time Tamper 수용 → Save/Load anti-tamper 금지)
**ADR Decision Summary**: `grep -ri "tamper|cheat|anti.*detect|forward.*jump"` Save 관련 = 0 매치. `grep -r "SavingWidget|SaveIndicator|저장 중" Source/MadeByClaudeCode/UI/` = 0 매치. E27/E28 외부 편집·Forward tamper 비정책.
**Engine**: UE 5.6 | **Risk**: LOW (grep hook)
**Engine Notes**: ADR-0001이 Time GDD와 Save/Load GDD 영역 모두 커버 — 본 story는 Save/Load 측 CI hook만 추가 (Time 측은 `foundation-time-session` Story 007).

**Control Manifest Rules (Foundation + Cross-cutting)**:
- Required: "Save/Load modal UI · "저장 중..." 인디케이터 금지" — `NO_SAVE_INDICATOR_UI` (Pillar 1)
- Forbidden: "anti-tamper hash·signature·file integrity 검증" — `USaveGame` CRC32 외 추가 금지 (ADR-0001)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] `NO_ANTITAMPER_LOGIC` CODE_REVIEW: `grep -ri "tamper\|cheat\|anti.*detect\|forward.*jump" Source/MadeByClaudeCode/` 0 매치. E27 정책 주석 존재 확인
- [ ] `NO_SAVE_INDICATOR_UI` CODE_REVIEW: `grep -r "SavingWidget\|SaveIndicator\|저장 중\|Saving\.\.\." Source/MadeByClaudeCode/UI/` 0 매치
- [ ] `NO_FORWARD_TAMPER_DETECTION_ADR` (Time GDD에도 있으나 Save/Load에서도 참조 필요): ADR-0001 파일 존재 + Accepted 상태 확인
- [ ] `.claude/hooks/save-load-no-antitamper.sh` 스크립트 작성 + CI 통합
- [ ] `.claude/hooks/save-load-no-indicator.sh` 스크립트 작성 + CI 통합

---

## Implementation Notes

*Derived from GDD §E27 + §Criterion NO_ANTITAMPER_LOGIC + ADR-0001 §Migration Plan:*

```bash
# .claude/hooks/save-load-no-antitamper.sh
#!/usr/bin/env bash
# ADR-0001: Save/Load 측 anti-tamper 패턴 금지
set -e
PATTERN='tamper\|cheat\|anti.*detect\|forward.*jump'
MATCHES=$(grep -riEn "$PATTERN" Source/MadeByClaudeCode/ || true)
if [[ -n "$MATCHES" ]]; then
    echo "ERROR: NO_ANTITAMPER_LOGIC 위반 — Save/Load 측 anti-tamper 패턴 검출:"
    echo "$MATCHES"
    exit 1
fi
echo "OK: NO_ANTITAMPER_LOGIC 통과"

# .claude/hooks/save-load-no-indicator.sh
#!/usr/bin/env bash
# Pillar 1: Save indicator UI 금지
set -e
PATTERN='SavingWidget\|SaveIndicator\|저장 중\|Saving\.\.\.'
MATCHES=$(grep -rEn "$PATTERN" Source/MadeByClaudeCode/UI/ Source/MadeByClaudeCode/Widgets/ 2>/dev/null || true)
if [[ -n "$MATCHES" ]]; then
    echo "ERROR: NO_SAVE_INDICATOR_UI 위반 — save indicator UI 검출:"
    echo "$MATCHES"
    exit 1
fi
echo "OK: NO_SAVE_INDICATOR_UI 통과"
```

- 두 hook 모두 CI/pre-commit에 통합 (update-config skill 활용 가능)
- E27 정책 주석 추가: `MossSaveLoadSubsystem.h` 상단에 "// E27/E28: 외부 편집·Forward Tamper 비정책 — anti-tamper hash 추가 금지 (ADR-0001)"

---

## Out of Scope

- Time epic `foundation-time-session` Story 007에서 이미 `NO_FORWARD_TAMPER_DETECTION` (탐지 패턴) hook 작성됨 — 본 story는 Save/Load 측 `NO_ANTITAMPER_LOGIC` (anti-tamper 구조) hook 분리

---

## QA Test Cases

**For Config/Data story:**
- **NO_ANTITAMPER_LOGIC (CODE_REVIEW)**
  - Setup: `.claude/hooks/save-load-no-antitamper.sh`
  - Verify: hook 실행 → exit 0
  - Pass: 0 매치 확인. 실험: 가짜 `bool bIsTamperDetected = false;` 삽입 후 재실행 → exit 1
- **NO_SAVE_INDICATOR_UI (CODE_REVIEW)**
  - Setup: `.claude/hooks/save-load-no-indicator.sh`
  - Verify: hook 실행
  - Pass: 0 매치. 실험: `Source/MadeByClaudeCode/UI/SavingWidget.h` 가짜 생성 → exit 1
- **NO_FORWARD_TAMPER_DETECTION_ADR**
  - Setup: `docs/architecture/adr-0001-forward-time-tamper-policy.md`
  - Verify: 파일 존재 + `## Status: **Accepted**` 포함
  - Pass: 두 조건 참

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- `.claude/hooks/save-load-no-antitamper.sh` + `.claude/hooks/save-load-no-indicator.sh` (CI hooks)
- CI 통합 smoke check: 두 hook 모두 exit 0

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: None (CI hook은 Story 004+ 실제 구현 완료 후 검증 가능, 그러나 hook 자체는 병렬 작성 가능)
- Unlocks: None (epic 최종 검증)
