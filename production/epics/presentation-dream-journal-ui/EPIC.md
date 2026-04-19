# Epic: Dream Journal UI

> **Layer**: Presentation
> **GDD**: [design/gdd/dream-journal-ui.md](../../../design/gdd/dream-journal-ui.md)
> **Architecture Module**: Presentation / Dream Journal — 읽기 전용 일기 UI + bHasUnseenDream indicator
> **Status**: Ready
> **Stories**: 6 created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Dream Journal UI는 21일 동안 수집한 꿈 조각을 조용히 재방문하는 읽기 전용 book-style UI다. `UMossSaveData.UnlockedDreamIds` + `bHasUnseenDream` 읽기로 렌더링, `Pipeline.GetDreamBodyText(FName)`로 본문 조회. 새 꿈 수신 시 아이콘 펄스(`bHasUnseenDream=true` trigger + Stillness Standard 슬롯 1회). 페이지 열람 시 `bHasUnseenDream=false` **in-memory 변경만** — 직접 `SaveAsync()` 호출 금지(AC-DJ-17), 다음 저장 이유에 포함. `FOnGameStateChanged` 구독으로 Waiting 상태에서만 활성. 수치·확률·태그 레이블 표시 금지(`NO_HARDCODED_STATS_UI`, Pillar 2). "N/M 언록됨" 문자열 금지(AC-DJ-13).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| *(직접 governing ADR 없음 — 표준 UMG 패턴)* | Dream Journal UI는 control manifest Global Rules + Cross-Cutting Constraints + Presentation Layer Rules 준수 | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-dj-001 | AC-DJ-17 `SaveAsync` 직접 호출 금지 (CODE_REVIEW) | architecture §Cross-Layer + GDD ✅ |
| TR-dj-002 | OQ-DJ-1 `bHasUnseenDream` 단일 bool (RESOLVED MVP) | GDD contract ✅ |
| TR-dj-003 | AC-DJ-13 `NO_HARDCODED_STATS_UI` — 수치·확률·태그 레이블 + "N/M 언록됨" 금지 | control-manifest Cross-Cutting ✅ |
| TR-dj-004 | AC-DJ-14 아이콘 펄스 (Reduced Motion 시 정적 하이라이트) | GDD contract ✅ |
| TR-dj-005 | AC-DJ-15/16 `bHasUnseenDream` 왕복 (in-memory → 다음 SaveAsync 포함 → 재시작 시 복원) | GDD contract ✅ |

## Key Interfaces

- **Publishes**: 없음 (UI는 Save/Load in-memory 변경만)
- **Consumes**: `FOnGameStateChanged` (Waiting 진입 시 활성), `Pipeline.GetDreamBodyText(FName)`, Save/Load `UnlockedDreamIds` + `bHasUnseenDream` 읽기, Dream Trigger `FOnDreamTriggered` 간접 수신(GSM 경유), Stillness Budget `Request(Motion/Standard)` + `IsReducedMotion()`
- **Owned types**: `UDreamJournalWidget`, `UDreamPageWidget` (per-page), WidgetSwitcher + 좌우 Flip 애니메이션
- **Settings**: 없음 (또는 `UMossDreamJournalSettings` 추후 검토)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [Widget 계층 골격 + GSM `Waiting` 전용 접근 제어](story-001-widget-hierarchy-gsm-access-control.md) | UI | Ready | — |
| 002 | [`UnlockedDreamIds` 목록 렌더링 + 빈 상태 + 탐색 네비게이션](story-002-unlocked-dream-list-rendering.md) | UI | Ready | — |
| 003 | [Rich Text Block 본문 렌더링 + DegradedFallback 빈 본문](story-003-rich-text-body-rendering.md) | UI | Ready | — |
| 004 | [Opening/Closing 트랜지션 + 페이지 넘김 + ReducedMotion 분기](story-004-open-close-page-flip-animations.md) | Visual/Feel | Ready | — |
| 005 | [`bHasUnseenDream` 펄스 애니메이션 + in-memory 토글 + Save/Load 왕복](story-005-has-unseen-dream-pulse-and-inmemory-toggle.md) | Integration | Ready | — |
| 006 | [CODE_REVIEW grep 게이트 — `SaveAsync` 금지 + `NO_HARDCODED_STATS_UI` + 접근성 수동](story-006-code-review-grep-gates.md) | UI | Ready | — |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-DJ-01~17)
- AC-DJ-17 CODE_REVIEW — `grep "SaveAsync" Source/MadeByClaudeCode/UI/Dream*` = 0건
- AC-DJ-13 `NO_HARDCODED_STATS_UI` — 숫자·확률·"언록됨" 문자열 grep = 0건
- AC-DJ-14 펄스 애니메이션 상태 AUTOMATED assert
- AC-DJ-15 `bHasUnseenDream=true` 상태에서 꿈 페이지 열기 → in-memory false 변경
- AC-DJ-16 재시작 시 `bHasUnseenDream=true`로 세이브됐으면 아이콘 펄스 재생
- Rich Text Block + String Table 경유 한국어 시적 포맷팅 (Nanum Myeongjo / Noto Serif KR 권장 폰트)
- `FOnGameStateChanged`로 Waiting 이외 상태에서 UI Hide 확인

## Next Step

Run `/create-stories presentation-dream-journal-ui` to break this epic into implementable stories.
