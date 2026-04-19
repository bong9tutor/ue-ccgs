---
name: Moss Baby 프로젝트 컨텍스트
description: 이끼 아이 게임의 핵심 정체성, 4개 Pillar, 시스템 구조, UX 제약 사항
type: project
---

Moss Baby(이끼 아이)는 UE 5.6 기반의 데스크톱 cozy companion 게임. 매일 한 번 이끼 영혼에게 선물 카드를 건네며 21일 아크를 진행.

**Why:** UX 모든 결정이 이 게임의 정체성에서 출발해야 함.
**How to apply:** 인터랙션 설계 시 항상 4개 Pillar를 대조. 특히 UI 피드백·알림·타이머 관련 결정에 Pillar 1과 2가 직접 충돌할 수 있음.

## 4개 Pillar

- **Pillar 1 (조용한 존재)**: 알림·재촉·타이머 없음. Hover-only 인터랙션 금지. 창 포커스 상실 시 알림 없음
- **Pillar 2 (선물의 시학)**: 카드에 숫자(수치 효과, 확률) 절대 표시 금지. 이름·설명·일러스트만
- **Pillar 3 (말할 때만 말한다)**: 꿈 반응은 희소. 매일 강제 반응 없음
- **Pillar 4 (끝이 있다)**: 21일 종료, 무한 루프 없음

## 핵심 시스템 (UX 관련)

- **Card Hand UI (#12)**: ux-designer 소유. drag-to-offer 제스처, 카드 레이아웃, Offering 상태 Show/Hide
- **Dream Journal UI (#13)**: ux-designer 소유
- **Input Abstraction (#4)**: Mouse 우선, Gamepad는 VS. Hover-only 금지 강제
- **Stillness Budget (#10)**: 동시 애니메이션/파티클 상한 — UI 애니메이션도 Budget 준수 필수
- **Game State Machine (#5)**: Offering 상태에서만 카드 패 표시

## 기술 제약

- **플랫폼**: PC (Windows). Steam Deck 호환 고려 (Gamepad VS)
- **최소 해상도**: 800×600
- **UMG 사용** (Slate-only 금지)
- **Hover-only 금지**: 모든 인터랙션은 명시적 클릭/드래그로 완결
- **수치 표시 금지**: 카드에 태그 레이블, 확률, 효과 수치 표시 불가

## 완성된 GDD

- `design/gdd/card-hand-ui.md` — 2026-04-18 작성 완료 (AC 16개, 8섹션 완전)
