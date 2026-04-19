---
name: card-system GDD 리뷰 이력
description: card-system.md R1→R2 리뷰 결과. R2에서 5 R1갭 미해소 + 신규 BLOCKER 확인
type: project
---

R1 verdict: 알 수 없음 (대화 시작 시 R2로 진입)
R2 verdict: REVISION NEEDED
R3 verdict: REVISION NEEDED (2026-04-18)

R3에서 확인된 BLOCKER:
- F1 — AC-CS-05 분류 오류: GSM 직접 호출 시나리오는 AUTOMATED 아닌 INTEGRATION
- F2 — AC-CS-15 방법론 혼재: 공식 단위테스트와 분포 수렴 검증 미분리 (AC-CS-15a/b로 분리 필요)
- F3 — AC-CS-16: OQ-CS-3 ADR 여전히 Blocking 상태 → INTEGRATION 테스트 설계 불가. [BLOCKED: OQ-CS-3 미해소] 태그 필요
- F4 — EC-CS-19(DayIndex=0), EC-CS-20(GameDurationDays<4) Logic 타입 AC 누락 → AUTOMATED BLOCKING 필요

R3에서 확인된 ADVISORY:
- F5 — AC-CS-07: 3개 실패 케이스 여전히 미분리 (R2 ADVISORY 미반영)
- F6 — CR-CS-3 Preparing 진행 중 타임아웃 교차 시나리오 커버 갭
- F7 — AC-CS-04 시간 상한 "< 5.0초"는 타임아웃 경계, 정상 SLA 아님 (50ms 권장)

R2 기준으로 해소된 항목:
- GSM E9 타임아웃 경로: AC-CS-18 추가로 해소
- 소프트 억제 분포: AC-CS-15 범위 추가 (단, 방법론 문제 남음 — F2)
- FGuid 스탈 참조: OQ-CS-1 RESOLVED

잔존 미해소 항목:
- OQ-CS-3 (Blocking ADR): R2→R3 그대로 Blocking
- EC-CS-3, EC-CS-6, EC-CS-17 AC 없음 (EC-CS-3 경미, EC-CS-6/17는 OQ-CS-3 연관)
- Tuning Knob 경계값 PostLoad 검증 AC (EC-CS-20 연관)

**Why:** Card System은 Growth, GSM, UI의 상류이므로 AC 분류 오류가 릴리스 게이트 오판으로 연결
**How to apply:** R4 리뷰 시 OQ-CS-3 ADR 해소 여부를 먼저 확인. AC-CS-05 INTEGRATION 격상, AC-CS-15 분리, AC-CS-16 BLOCKED 해소 여부 점검
