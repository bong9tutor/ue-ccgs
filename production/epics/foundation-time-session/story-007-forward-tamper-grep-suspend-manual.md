# Story 007: Forward Tamper CI grep hook + Windows Suspend/Resume 실기 검증

> **Epic**: Time & Session System
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Config/Data
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/time-session-system.md`
**Requirement**: `TR-time-001` (NO_FORWARD_TAMPER_DETECTION_ADR CI grep) + `TR-time-004` (Windows suspend/resume manual verification — OQ-IMP-1)
**ADR Governing Implementation**: ADR-0001 (Forward Time Tamper 명시적 수용 — `## Migration Plan Step 3` CI grep hook 추가 의무)
**ADR Decision Summary**: `grep -r "tamper|DetectClockSkew|bIsForward|bIsSuspicious|IsSuspiciousAdvance" Source/MadeByClaudeCode/` = 0 매치 강제. Windows 10 S3 / Windows 11 S0ix / 노트북 덮개 닫기 3-matrix 수동 검증.
**Engine**: UE 5.6 | **Risk**: MEDIUM
**Engine Notes**: `FPlatformTime::Seconds()`가 Windows suspend 동안 동작은 드라이버 의존. BIOS RTC alarm으로 가속 가능. MANUAL AC로 release-gate 검증 (AUTOMATED 격상은 OQ-IMP-1 완료 후 재검토).

**Control Manifest Rules (Foundation layer)**:
- Required: Migration Plan Step 3 "CI 정적 분석 hook 추가" (ADR-0001)
- Required: ADR 파일 존재 + `## Status: Accepted` — `NO_FORWARD_TAMPER_DETECTION_ADR` AC
- Forbidden: grep 매치 발생 시 CI 차단 (ADR-0001 Validation Criteria)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] NO_FORWARD_TAMPER_DETECTION: `grep -r "tamper\|DetectClockSkew\|bIsForward\|bIsSuspicious\|IsSuspiciousAdvance" Source/MadeByClaudeCode/` 0 매치 (CODE_REVIEW)
- [ ] NO_FORWARD_TAMPER_DETECTION_ADR: `docs/architecture/adr-0001-forward-time-tamper-policy.md` 파일 존재 + `## Status: Accepted` 마킹 + PR 체크리스트에 "ADR-0001 위반 여부" 항목 포함 (CODE_REVIEW)
- [ ] SUSPEND_RESUME_WALL_ADVANCE: Windows 10 S3 / Windows 11 S0ix / 노트북 덮개 닫기 3-matrix 각각 1회 검증 — 3시간 suspend 후 `ADVANCE_SILENT` 1회 + `MonoDelta < 5s` + `WallDelta ≈ 3h (±5min)` + `DayIndex` 불변 + 알림 미발생 (MANUAL)

---

## Implementation Notes

*Derived from ADR-0001 §Migration Plan + GDD §Criterion SUSPEND_RESUME_WALL_ADVANCE:*

**1. CI grep hook 추가**:

파일: `.claude/hooks/detect-forward-tamper.sh` (또는 `.github/workflows/ci.yml` 스텝)

```bash
#!/usr/bin/env bash
# ADR-0001: Forward Tamper 탐지 패턴 금지 (grep 0 매치 강제)
set -e
PATTERN='tamper\|DetectClockSkew\|bIsForward\|bIsSuspicious\|IsSuspiciousAdvance'
MATCHES=$(grep -rEn "$PATTERN" Source/MadeByClaudeCode/ || true)
if [[ -n "$MATCHES" ]]; then
    echo "ERROR: ADR-0001 위반 — Forward Tamper 탐지 패턴 검출:"
    echo "$MATCHES"
    exit 1
fi
echo "OK: NO_FORWARD_TAMPER_DETECTION 통과"
```

- `.claude/settings.json`에 pre-commit 또는 PR gate로 통합 (update-config skill 활용 가능)
- `.github/workflows/ci.yml` 또는 기존 CI에 추가

**2. PR 체크리스트 업데이트**:

`.github/PULL_REQUEST_TEMPLATE.md` (또는 기존 CCGS PR 가이드)에 추가:
```markdown
- [ ] ADR-0001 (Forward Tamper Acceptance) 위반 없음 — tamper 탐지 패턴 grep 0 매치 확인
```

**3. Windows Suspend/Resume 수동 테스트 protocol**:

파일: `production/qa/evidence/suspend-resume-manual-test.md`

```markdown
# SUSPEND_RESUME 수동 검증 체크리스트

## 절차 (공통)
1. 게임 실행 후 30초 안정화
2. `FSessionRecord.LastWallUtc`, `.LastMonotonicSec` 로그 캡처
3. OS sleep (아래 matrix 참조)
4. 3시간 대기 (또는 BIOS RTC alarm 가속)
5. 깨움
6. `TickInSession()` 첫 호출의 로그 캡처

## Test Matrix (최소 3회)

### Case A: Windows 10 S3 Sleep (데스크톱)
- 시작 메뉴 > 절전
- 기대: `ADVANCE_SILENT` 1회, MonoDelta < 5s, WallDelta ≈ 3h

### Case B: Windows 11 S0ix Modern Standby (노트북)
- 시작 메뉴 > 절전
- 디바이스 매니저 > 시스템 디바이스에서 Modern Standby 활성 확인
- 기대: 동일

### Case C: 노트북 덮개 닫기 (Windows 10 또는 11)
- 덮개 닫기 → 3시간 → 덮개 열기
- 기대: 동일

## 체크리스트 (각 Case별)
- [ ] delegate `OnTimeActionResolved` 1회 발행 (`ADVANCE_SILENT`)
- [ ] MonoDelta 측정값 < `SuspendMonotonicThresholdSec` (5초)
- [ ] WallDelta 측정값 ≈ 3시간 (±5분)
- [ ] `DayIndex` 변화 없음
- [ ] 진단 로그에 `SUSPEND_RESUME` 분류 기록
- [ ] UI 알림·모달·팝업 미발생

## 결과 차이 발견 시
OQ-IMP-1 갱신 + ADR-0001 §Engine Compatibility Knowledge Risk 조정
```

---

## Out of Scope

- `.claude/rules/forward-tamper-policy.md` agent rule 작성 (별도 Task — ADR-0001 §Migration Plan Step 5, 본 story는 CI grep + 수동 테스트만)

---

## QA Test Cases

**For Config/Data story:**
- **NO_FORWARD_TAMPER_DETECTION (CODE_REVIEW)**
  - Setup: 모든 Source 파일 + CI hook 활성
  - Verify: `grep -rE "tamper|DetectClockSkew|bIsForward|bIsSuspicious|IsSuspiciousAdvance" Source/MadeByClaudeCode/` 실행
  - Pass: 0 매치 + CI hook exit code 0
  - 실험: 가짜 `bool bIsForward = true;` 줄 삽입 후 hook 재실행 → exit code 1 확인
- **NO_FORWARD_TAMPER_DETECTION_ADR (CODE_REVIEW)**
  - Setup: `docs/architecture/adr-0001-forward-time-tamper-policy.md` 열기
  - Verify: 첫 섹션 `## Status` 내용 = `**Accepted**`, PR 템플릿에 체크리스트 항목 존재
  - Pass: 두 조건 모두 참
- **SUSPEND_RESUME_WALL_ADVANCE (MANUAL)**
  - Setup: `production/qa/evidence/suspend-resume-manual-test.md` 체크리스트 준비 + 테스트 하드웨어 (Win10 데스크톱 + Win11 노트북)
  - Verify: 3-matrix 각각 1회 실행, 로그 + 스크린샷 수집
  - Pass: 6개 체크포인트 모두 통과, `production/qa/evidence/suspend-resume-evidence-[date].md`에 결과 기록 + QA lead 서명

---

## Test Evidence

**Story Type**: Config/Data
**Required evidence**:
- CI grep hook 통과 smoke check (`.claude/hooks/detect-forward-tamper.sh` exit 0)
- `production/qa/evidence/suspend-resume-evidence-[date].md` (MANUAL, Implementation 단계 실기)
- ADR-0001 파일 존재 확인 + PR 템플릿 grep 체크리스트

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 004, 005 (classifier 구현 완료해야 suspend 테스트 의미)
- Unlocks: None (epic 최종 검증)
