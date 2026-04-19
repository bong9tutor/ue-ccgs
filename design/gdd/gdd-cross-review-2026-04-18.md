# Cross-GDD Review Report

**Date**: 2026-04-18
**GDDs Reviewed**: 9
**Systems Covered**: Time & Session (#1), Data Pipeline (#2), Save/Load (#3), Input Abstraction (#4), Stillness Budget (#10), Game State Machine (#5), Moss Baby Character (#6), Growth Accumulation (#7), Card System (#8)
**Review Mode**: Full (Consistency + Design Theory + Scenario Walkthrough)
**Specialists**: systems-designer (consistency), game-designer (design theory)

---

## Consistency Issues

### Blocking (2)

**🔴 C-1: FSessionRecord.PlaythroughSeed 필드 누락**
- **관련 GDD**: card-system.md, save-load-persistence.md
- Card System CR-CS-5: `FSessionRecord.PlaythroughSeed(uint32)` 의존 — 결정론적 카드 재생성 전체가 이 필드에 의존
- Save/Load GDD의 FSessionRecord에 해당 필드 없음. Entity registry에도 없음
- Card System OQ-CS-7에서 추가 필요로 명시했으나 Save/Load GDD 미갱신
- **필요 조치**: Save/Load GDD + entity registry FSessionRecord에 `PlaythroughSeed: uint32` 필드 추가

**🔴 C-2: GSM이 FOnDayAdvanced 발행자로 오기술**
- **관련 GDD**: game-state-machine.md, time-session-system.md
- GSM §Waiting Entry: "`FOnDayAdvanced(DayIndex)` 브로드캐스트"라고 기술
- Time GDD §Interaction 3 + registry: FOnDayAdvanced source = Time. GSM은 구독자
- **영향**: 구현자가 GSM에서 이중 발행 코드를 작성할 위험
- **필요 조치**: GSM §Waiting Entry에서 FOnDayAdvanced 발행 텍스트 제거/수정

### Warnings (5)

**⚠️ C-3**: MBC CR-3 "0.3s 시각 반응" vs Formula 4 "1.65s 실질 복귀" — 텍스트 불일치. Overview·Interactions의 "0.3s" 수정 필요.
**⚠️ C-4**: WitheredThresholdDays(GSM, 기본 3) vs DryingThresholdDays(MBC, 기본 2) — 별개 개념이나 관계 미기술. 한쪽에 명확화 필요.
**⚠️ C-5**: ConsecutiveDaySuppression=0 — Card System Tuning Knobs 텍스트("완전 배제")와 PostLoad clamp([0.05, 1.0])가 모순. 0 금지 명시 필요.
**⚠️ C-6**: Card System §Stale 참조 수정 대상 — Growth/GSM GDD 본문은 이미 FName 사용. 해당 항목 제거 필요.
**⚠️ C-7**: F-CS-1 `GameDurationDays / 4` — C++ int 나눗셈 시 SegmentLength 변동 위험. explicit float cast 주석 추가 권장.

### Info (4)

**ℹ️**: IAL-GSM 역방향 Dependencies 누락 — GSM이 IAL의 `IA_Back`/`OnInputModeChanged` 소비하나 GSM Dependencies에 IAL 미등재.
**ℹ️**: MBC→Save/Load 의존 경로 — 실질 의존 대상은 Growth(#7) 경유. MBC Dependencies 명확화 권장.
**ℹ️**: AC-CS-16 `[BLOCKED by OQ-CS-3]` — OQ-CS-3 ADR을 MVP 이전 해소 대상으로 우선순위화 필요.
**ℹ️**: FOnCardOffered FGuid→FName 마이그레이션 — Growth/GSM GDD 본문 확인 완료, 실제 충돌 없음.

---

## Game Design Issues

### Blocking (2)

**🔴 D-1: 꿈 빈도 정의 불일치**
- **관련 GDD**: game-concept.md, game-state-machine.md, (Dream Trigger — 미작성)
- game-concept.md: "2–4일 간격" → 21일 중 5–10회
- GSM GDD: "Dream 상태는 21일 중 2–3회"
- **모순**: 2-4일 간격이면 5-10회, 2-3회이면 7-10일 간격. 완전히 다른 경험
- **필요 조치**: Dream Trigger GDD 작성 전에 단일 수치 합의 → game-concept.md + GSM GDD 동기화

**🔴 D-2: Day 21 Final Form 미결정 위험**
- **관련 GDD**: game-state-machine.md (E13), growth-accumulation-engine.md (CR-5), card-system.md (OQ-CS-3)
- FOnCardOffered → Growth(태그 가산 + Final Form 결정) → GSM(Farewell) 순서 보장 필수
- Card System OQ-CS-3에서 이미 Blocking ADR로 추적 중
- **필요 조치**: OQ-CS-3 ADR 해소 (명시적 호출 체인 또는 2단계 delegate 패턴)

### Warnings (7)

**⚠️ D-3**: Dawn 상태 Stillness Budget 소비 규칙 누락 — GSM 또는 Stillness Budget GDD에 명시 필요.
**⚠️ D-4**: Day 15-20 "징조 단계" 구현 시스템 부재 — Dream Trigger GDD 작성 시 반영 필요.
**⚠️ D-5**: Element/Emotion 카드에 시각 피드백 공백 — MVP에서 Pillar 2 약화 가능. Growth OQ-GAE-3 MVP 격상 검토.
**⚠️ D-6**: 부재 후 복귀 감정 톤 — Withered 해제 시 MBC 반응 강화 여부 미정의.
**⚠️ D-7**: "Nothing happened" 세션(Day 8-15) 리텐션 기준 미정의.
**⚠️ D-8**: MVP Day 7 이벤트 과부하 — 시즌 전환 + Mature + Final Form 동시 발생. Stillness Narrative 슬롯 경합 가능.
**⚠️ D-9**: Day 1 FOnDayAdvanced 발행 시점 불명확 — Time 분류 완료 즉시인지 GSM Dawn 후인지 시퀀스 문서화 필요.

---

## Cross-System Scenario Issues

**Scenarios walked**: 5

### Blockers

**🔴 Day 21 마지막 건넴** — Card System, Growth, GSM
FOnCardOffered 수신 순서(Growth 먼저 → GSM Farewell 나중) 미보장. OQ-CS-3 ADR 해소 필수. D-2와 동일 근본 원인.

### Warnings

**⚠️ Day 1 첫 실행** — Time, Card, GSM
FOnDayAdvanced(1) 발행 시점과 GSM Dawn→Offering 전환 순서의 명시적 시퀀스 문서화 필요.

**⚠️ Day 7 MVP** — Card, Growth, MBC, Stillness
시즌 전환 + Mature + Final Form이 동일 세션에 겹쳐 Stillness Narrative 슬롯 과부하 가능.

**⚠️ Day 15 성장 전환** — MBC, Stillness Budget
GrowthTransition이 Narrative 슬롯인지 Standard 슬롯인지 미정의.

### Info

**ℹ️ 앱 재시작 (Day 10, 미건넴)** — Save/Load, Time, GSM, Card
잘 처리됨. 결정론적 시드 + bHandOffered 추론 경로 정합 확인.

---

## Pillar Alignment

| 시스템 | 주 Pillar | 정합 | 비고 |
|--------|----------|------|------|
| Time & Session | P1, P3, P4 | ✓ | Silent absorption 철저 |
| Data Pipeline | P2, P3 | ✓ | C1 schema guard = P2 보호 핵심 |
| Save/Load | P1, P4 | ✓ | Silent rollback + NarrativeCount |
| Input Abstraction | P1, P2 | ✓ | Hover-only 금지 명시 |
| Stillness Budget | P1, P3 | ✓ | 동시 이벤트 상한 = 조용함 기술적 보장 |
| Game State Machine | P1, P3, P4 | ✓ | Dream 희소성 + Farewell 단일 진입 |
| Moss Baby Character | P1, P2, P4 | ✓ | 과잉 반응 금지 명시 |
| Growth Accumulation | P2, P4 | ✓* | *Element/Emotion 피드백 공백 → P2 약화 가능 |
| Card System | P1, P2 | ✓ | 취소 불가 = "잘못된 선택은 없다" |

**Anti-pillar 위반**: 없음 (Challenge, Fellowship, Economy 모두 배제 유지)

---

## GDDs Flagged for Revision

| GDD | Reason | Type | Priority |
|-----|--------|------|----------|
| save-load-persistence.md | PlaythroughSeed 필드 추가 (C-1) | Consistency | Blocking |
| game-state-machine.md | FOnDayAdvanced 발행자 오기술 (C-2) | Consistency | Blocking |
| game-concept.md | 꿈 빈도 수치 통일 (D-1) | Design Theory | Blocking |

---

## Verdict: CONCERNS

🔴 Blocking 4건 발견. 단, 모두 **문서 수정 수준**(필드 추가 1건, 텍스트 수정 2건, 수치 통일 1건)이며 설계 재고 불필요. D-2(Day 21 순서 보장)는 Card System OQ-CS-3에서 이미 Blocking ADR로 추적 중.

⚠️ Warning 12건은 구현/downstream GDD 작성 시 자연 해소 가능하나, Dawn Stillness 규칙(D-3)과 GrowthTransition 슬롯 등급(시나리오 3)은 아키텍처 전에 결정 권장.

**Architecture는 C-1(PlaythroughSeed)과 C-2(FOnDayAdvanced 발행자) 수정 후 시작 가능.** D-1(꿈 빈도)은 Dream Trigger GDD 작성 전까지 해소하면 충분.
