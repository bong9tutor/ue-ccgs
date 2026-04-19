---
name: 프로젝트 컨텍스트 — Moss Baby (이끼 아이)
description: 이 프로젝트의 핵심 설계 사실 — 게임 아이덴티티, 아크 구조, 기존 시스템 계약
type: project
---

Moss Baby는 UE 5.6 기반 cozy desktop companion 게임. 매일 3장 중 1장의 선물 카드를 21일 동안 이끼 아이에게 건넨다. 태그 누적 벡터가 최종 형태를 결정.

**Why:** 4개 게임 필라(조용한 존재 / 선물의 시학 / 말할 때만 말한다 / 끝이 있다)가 모든 설계 결정의 기준.

**How to apply:** 시스템 설계 시 항상 필라 위반 여부를 먼저 체크. 특히 Pillar 2(숫자 없는 카드), Pillar 1(재촉·페널티 없음)이 Card System에 직결.

**기존 확정 계약:**
- GSM이 Dawn 진입 시 Card System에 Prepare 신호 발행
- Card System은 5초(CardSystemReadyTimeoutSec) 내에 Ready 응답 필요
- 타임아웃 시 E9 Degraded Fallback — 빈 패로 Offering 진입
- FOnCardOffered(CardId) 발행 시 Offering→Waiting 전환
- FOnDayAdvanced(DayIndex) — Card System이 구독

**규모:**
- MVP: 카드 10장, 7일 아크
- Full: 카드 40장, 21일 아크, 최종 형태 8-12종

added: 2026-04-18
