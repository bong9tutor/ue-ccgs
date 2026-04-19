---
name: Project: Moss Baby Design State
description: Moss Baby 시스템 설계 현황 — 승인/검토 중/미시작 상태 추적
type: project
---

프로젝트명: **Moss Baby (이끼 아이)** — 데스크톱 cozy companion, 21일 실시간, UE 5.6 C++

**Why:** 솔로 인디 개발. 설계 세션마다 진행 상태를 추적해 중복 재설계를 방지.

**How to apply:** GDD 작성 태스크 수신 시 현재 시스템 상태를 확인해 중복·누락 방지.

## 시스템 상태 (2026-04-18 기준)

| # | 시스템 | 상태 |
|---|---|---|
| 1 | Time & Session System | Approved (R3) |
| 2 | Data Pipeline | Approved (R3) |
| 3 | Save/Load Persistence | Needs Revision |
| 4 | Input Abstraction Layer | Approved (R2) |
| 5 | Game State Machine | Needs Revision |
| 6 | Moss Baby Character System | Approved (R3) |
| 7 | Growth Accumulation Engine | Approved (R4) |
| 8 | Card System | Approved (R3) |
| 9 | Dream Trigger System | Not Started (Prototype Early) |
| 10 | Stillness Budget | Approved (R1) |
| 11 | Lumen Dusk Scene | Not Started |
| 12 | Card Hand UI | Not Started |
| 13 | Dream Journal UI | Not Started |
| 14 | Window & Display Management | **In Review (작성 완료 2026-04-18)** |
| 15-19 | VS / Full Vision systems | Not Started |

## 4 Game Pillars (요약)
1. 조용한 존재 — 알림 없음, 강요 없음
2. 선물의 시학 — 숫자 아닌 감정
3. 말할 때만 말한다 — 꿈의 희소성
4. 끝이 있다 — 21일

## Design Doc 위치
- GDD: `design/gdd/`
- 시스템 인덱스: `design/gdd/systems-index.md`
- 게임 컨셉: `design/gdd/game-concept.md`
