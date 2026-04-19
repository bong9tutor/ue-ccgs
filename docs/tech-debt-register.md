# Technical Debt Register

Claude Code Game Studios 프레임워크 기반 프로젝트의 기술 부채 추적 문서.
`/story-done`에서 advisory로 발견된 항목과 `/code-review`에서 지적된 수정 연기 항목을 기록한다.

**Last Updated**: 2026-04-19

---

## Active Items

### TD-001 — FClockSourceRealUtcDiffNonNegativeTest NTP 간헐 실패 위험

| 필드 | 값 |
|---|---|
| **Severity** | S3 (잠재) |
| **Source** | `/code-review` Story-001 (2026-04-19) |
| **Origin Story** | `production/epics/foundation-time-session/story-001-clock-source-interface.md` |
| **File** | `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/ClockSourceTests.cpp:119-139` |
| **Description** | `FRealClockSource::GetUtcNow()` 연속 호출 간 NTP 보정이 개입하면 두 번째 값이 첫 번째보다 작을 수 있음. 즉각 연속 호출이라 확률 극저이지만 CI에서 NTP 동기화 이벤트와 겹치면 flaky test 발생 가능. |
| **Impact** | CI 신뢰도. 간헐적 빌드 실패로 원인 추적 시간 소비 가능성. |
| **Proposed Fix** | (a) CI 환경 NTP 동기화 정책 확인 후 허용 범위 내라면 `// test-standards deterministic exception` 주석 추가, (b) 또는 `FMockClockSource` 기반 시나리오로 재작성하고 실제 UTC는 smoke 레벨로만 검증. |
| **Owner** | qa-lead (향후 결정) |
| **Status** | Open |
| **Link** | — |

### TD-003 — FSessionRecord UPROPERTY(SaveGame) specifier 결정 (Story 1-7 의존)

| 필드 | 값 |
|---|---|
| **Severity** | S3 (설계 결정 미확정) |
| **Source** | `/code-review` Story 1-2 unreal-specialist (2026-04-19) |
| **Origin Story** | `production/epics/foundation-time-session/story-002-session-record-struct.md` |
| **File** | `MadeByClaudeCode/Source/MadeByClaudeCode/Time/SessionRecord.h` |
| **Description** | Story 1-2 `FSessionRecord`의 모든 필드가 `UPROPERTY()` 단순 선언. 현재 round-trip 테스트는 `ArIsSaveGame=false` 경로로 통과. 그러나 Story 1-7 Save/Load Subsystem이 `USaveGame` 기반 `ArIsSaveGame=true` 경로를 선택하면, 모든 필드에 `UPROPERTY(SaveGame)` 소급 추가 필요. 선택 시기: Story 1-7 착수 시. |
| **Impact** | Story 1-7에서 직렬화 경로 결정 시 헤더 일괄 수정 필요. 미처리 시 Save 파일이 빈 FSessionRecord로 저장됨 (silent data loss). |
| **Proposed Fix** | Story 1-7 `story-001-save-data-schema-enum.md`에 "직렬화 경로 결정 (ArIsSaveGame true vs false)" 전제 조건 섹션 추가. true 선택 시 SessionRecord.h 필드 전체에 `(SaveGame)` 추가. |
| **Owner** | Story 1-7 담당자 |
| **Status** | Open (Story 1-7 착수 시 해결) |
| **Link** | Story 1-2 Completion Notes ADVISORY 섹션 |

### TD-002 — test-standards.md가 UE IMPLEMENT_SIMPLE_AUTOMATION_TEST idiom 예외 미명시

| 필드 | 값 |
|---|---|
| **Severity** | S4 (문서 갭) |
| **Source** | `/code-review` Story-001 qa-tester (2026-04-19) |
| **Origin Story** | `production/epics/foundation-time-session/story-001-clock-source-interface.md` |
| **File** | `.claude/rules/test-standards.md` |
| **Description** | `test-standards.md`는 `test_[system]_[scenario]_[expected]` 네이밍 패턴을 강제. 하지만 UE Automation Framework의 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` 매크로는 `F[Name]Test` 클래스명을 생성하므로 규칙 준수 불가. 엔진 idiom 예외 조항이 필요. |
| **Impact** | 미래 UE 테스트 구현자가 `/code-review` 또는 `/story-done`에서 불필요한 advisory를 받음. 프레임워크 규칙과 엔진 관용구의 충돌. |
| **Proposed Fix** | `test-standards.md`에 "엔진별 idiom 예외" 섹션 추가. UE: `F[TestName]Test` + `MossBaby.[System].[Category].*` 카테고리 규칙 명시. |
| **Owner** | qa-lead |
| **Status** | Open |
| **Link** | — |

---

## Resolved Items

*(None yet)*

---

## Severity Legend

| Level | 설명 |
|---|---|
| **S1** | Critical — 릴리스 차단 |
| **S2** | High — 다음 sprint 내 해결 필수 |
| **S3** | Medium — 같은 layer/epic 내 해결 권장 |
| **S4** | Low — 편의 개선, 백로그 |

## Process

- 새 항목은 **Active Items** 상단에 append (`TD-NNN` 연번)
- 해결 시 **Resolved Items**로 이동 + PR/commit 링크 추가
- `/tech-debt` 스킬로 주기적 리뷰·트리아지
