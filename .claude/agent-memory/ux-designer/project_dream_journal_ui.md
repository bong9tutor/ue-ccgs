---
name: Dream Journal UI GDD 작성 완료
description: Dream Journal UI(#13) GDD R2. OQ-DJ-1 해결, Stillness Budget 우선순위 명시, Dream Trigger GDD 링크.
type: project
---

Dream Journal UI GDD R2를 `design/gdd/dream-journal-ui.md`에 업데이트 완료 (2026-04-18).

**Why:** Systems Index에서 #13은 MVP 범위, Presentation Layer. R1에서 3개 블로킹 항목이 있었으며 R2에서 모두 수정됨.

**핵심 설계 결정:**
- 접근 제어: `EGameState::Waiting`에서만 진입점 표시. 에러 메시지 대신 진입점 숨김.
- 새 꿈 알림: `bHasUnseenDream` bool 플래그 + 아이콘 조용한 펄스. 알림 팝업 없음 (Pillar 1).
- 텍스트: `Pipeline.GetDreamBodyText(FName DreamId) → FText` on-demand pull. 실패 시 빈 페이지 (에러 없음).
- Stillness Budget: Opening/Closing/PageTurn 모두 `Motion, Standard` 슬롯 (Narrative 아님). Deny 시 트랜지션 생략만 — 접근 차단 금지.
- Save/Load 관계: **read-only**. `SaveAsync()` 직접 호출 없음. `bHasUnseenDream = false`는 in-memory 갱신만.
- 강제 닫기: Dream 또는 Farewell 상태 진입 시 Closing 트랜지션 생략하고 즉시 제거.

**R2 수정 사항:**
- Stillness Budget 우선순위: Standard 확정. Stillness Budget GDD §Downstream의 Narrative 기재는 오기입 — Stillness Budget GDD 수정 필요.
- OQ-DJ-1 RESOLVED: 단일 `bool bHasUnseenDream` 확정. 꿈 3-5개 규모에서 개별 추적은 과도. Full Vision에서 `TSet<FName> UnseenDreamIds` 확장 검토.
- Dream Trigger GDD 링크: `design/gdd/dream-trigger-system.md` 파일 존재 확인. Dependencies 테이블 및 양방향 계약 섹션에 링크 추가. "동시 작성 중" 주석 제거.

**미결 사항 (OQ):**
- OQ-DJ-1: RESOLVED — 단일 `bool bHasUnseenDream`
- OQ-DJ-2: 빈 슬롯 완전 숨김 vs. 채워진 슬롯만 암시 — 아트 디렉터 협의 필요
- OQ-DJ-3: 긴 텍스트 스크롤 vs. auto-fit — Writer 길이 가이드라인 연계
- OQ-DJ-4: 강제 닫기 시 읽음 처리 여부 — 기본값 "열었으면 읽은 것" 권장

**How to apply:** OQ-DJ-2 확정 전 아트 디렉터 협의 필요. Stillness Budget GDD에서 Dream Journal을 Narrative로 기재한 부분을 Standard로 수정해야 함.
