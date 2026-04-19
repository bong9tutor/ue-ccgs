---
name: save-load GDD 리뷰 이력
description: save-load-persistence.md R1 → R2 재검토 결과 요약. R2 verdict 및 잔여 이슈 포함
type: project
---

R1 verdict: MAJOR REVISION NEEDED (7 BLOCKER)
R2 status: 7 BLOCKER 모두 적용 완료, QA adversarial re-review 수행 (2026-04-17)

R2 재검토에서 새로 발견된 이슈:
- BLOCKING: ENGINE_ARCHIVE_VERSION_MISMATCH AC 분류가 AUTOMATED+INTEGRATION인데 CI 환경 준비 없음 (Test Infra에서 누락)
- BLOCKING: E4_TEMP_CRASH_RECOVERY의 "프로세스 kill 래퍼" BLOCKING 격상됐지만 AC에서 AUTOMATED 유지 — 조건부 강등 로직 자체가 검증 안 됨
- RECOMMENDED: WSN_RESET AC 없음 (Formula 2 Overflow Safety 커버되지 않음)
- RECOMMENDED: E23 DeinitFlushTimeoutSec ±0.5초 허용치가 너무 관대함
- RECOMMENDED: MIGRATION_EXCEPTION_ROLLBACK이 UE C++ 예외 비활성화 문제 미해결 상태로 AUTOMATED 유지

**Why:** Foundation Layer는 downstream 9개 시스템이 의존하므로 AC 분류 오류가 릴리스 게이트 오판으로 연결됨
**How to apply:** 이 GDD의 다음 revision(R3) 준비 시 위 이슈들을 먼저 검토할 것
