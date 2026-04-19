# Smoke Check — Moss Baby (MVP)

**Purpose**: 15분 수동 gate — `/smoke-check` 스킬이 사용. Sprint 종료 시 QA 핸드오프 전 실행.

## 체크리스트 (MVP 7일 아크 기준)

### 시작 & 초기화
- [ ] 앱 실행 → Title 또는 Day 1 Dawn 정상 진입 (≤ 5초)
- [ ] 창 크기 1024×768 기본 + 이동·리사이즈 가능
- [ ] 800×600으로 줄이면 최소에서 멈춤 (OS 레벨 강제)

### 카드 건넴 의식 (Day 1)
- [ ] Dawn 시퀀스 재생 후 Offering 상태 자동 진입 (≤ 3초)
- [ ] 카드 3장 slide-up 순차 공개 (좌 → 중 → 우)
- [ ] 마우스로 카드 위 hover → 카드 밝아짐 (건넴 트리거 되지 않음)
- [ ] 카드 LMB Hold → drag 시작
- [ ] 유리병 근처 (Offer Zone) 릴리스 → 건넴 확정 애니메이션 → Hide
- [ ] `FOnCardOffered` 브로드캐스트 로그 확인

### 상태 전환 (시각 검증)
- [ ] Menu → Dawn: 색온도 2,800K 전환
- [ ] Dawn → Offering: 3,200K 전환
- [ ] Offering → Waiting: 2,800K 복귀
- [ ] (선택) Waiting → Dream: 4,200K 차가운 달빛 전환 (anchor moment)
- [ ] Day 7 → Farewell: 2,200K + Vignette 점진 증가 (1.5-2.0s)

### Save/Load
- [ ] 카드 건넴 직후 앱 종료 → 재시작 → 같은 상태 복원
- [ ] 에러 다이얼로그, "저장 중..." 인디케이터 **미표시** (Pillar 1)

### Pillar 검증
- [ ] 어떤 모달 다이얼로그도 발생하지 않음 (Pillar 1)
- [ ] UI에 수치·확률·태그 레이블 미표시 (Pillar 2)
- [ ] Day 1에 꿈 미발생 (Dream Trigger CR-1 Day 1 Guard)
- [ ] Hover만으로 카드 자동 선택·건넴 미발생 (Pillar 1 + Input Rule 4)

### Input
- [ ] 마우스만으로 전체 플레이 완료 (Mouse-only completeness)
- [ ] `Esc` 키로 Dream Journal 닫기 (키보드 단축키 보완)
- [ ] `J` 키로 Dream Journal 토글 (편의 단축키)

### Reduced Motion (Stillness Budget)
- [ ] `bReducedMotionEnabled = true` 토글 → 모든 애니메이션 즉시 적용
- [ ] Sound 효과는 정상 (Rule 6)

### 성능 (수동 추정)
- [ ] 60fps 체감 유지 (stat fps ≤ 60 유지)
- [ ] 앱 메모리 < 1.5 GB (Task Manager 확인)

## PASS 판정

모든 체크박스 ✅ → PASS
단 하나라도 ❌ → 실패 사유 로그 + 버그 등록

## Related

- `.claude/skills/smoke-check/SKILL.md`
- 각 GDD §Acceptance Criteria — MANUAL 카테고리 항목들이 smoke 체크와 연동
