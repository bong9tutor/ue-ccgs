# Card System — Design Review Log

## Review R1 — 2026-04-18 — Verdict: NEEDS REVISION
Scope signal: L (다중 시스템 통합, 3+ 수식, OQ-CS-3 ADR 필수)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, creative-director
Blocking items: 3 | Recommended: 16
Summary: 4명 만장일치로 EC-CS-17(Day 21 구독 순서)이 UE Multicast 비공개 구현에 의존하는 것을 Critical로 판정. 비영속 DailyHand가 "세계가 오늘 이 3장을 보냈다" Player Fantasy와 충돌. AC 요약 테이블 수량 오류(AUTOMATED 12→13, CODE_REVIEW 3→2). 구조는 건강하나 3건 해결 필요.
Prior verdict resolved: First review

### Blocking Items (R1)
1. EC-CS-17: OQ-CS-3 Blocking ADR 격상 + 명시적 호출 체인 요구
2. EC-CS-12: 결정론적 시드(PlaythroughSeed + DayIndex) 도입
3. AC 요약 테이블 수량 수정

### Revisions Applied (R2 — same session)
- CR-CS-1 Step 3: FRandomStream + 결정론적 시드 + 경계 fallback
- CR-CS-5: PlaythroughSeed 영속화 설계 + 설계 근거
- EC-CS-12: 결정론적 재생성으로 갱신
- EC-CS-17: Blocking ADR 요구 + 명시적 호출 체인 설계 필요
- EC-CS-19, EC-CS-20: DayIndex=0 guard + GameDurationDays < 4 검증 신규
- AC-CS-02: SegmentLength 하드코딩 제거
- AC-CS-15: 결정론 모순 해소 (공식 검증으로 재작성)
- AC 요약 테이블: AUTOMATED 13 + CODE_REVIEW 2
- OQ-CS-3: Open → Blocking
- OQ-CS-7: PlaythroughSeed 영속화 신규
- Implementation Notes 섹션 신규 (FGameplayTag, FRandomStream, delegate, 타이머)
- InitializeDependency 제약 명시
- FName CardId 네이밍 컨벤션 + 중복 검증
- InSeasonWeight ≥ OffSeasonWeight PostLoad 역전 방어

### Deferred to Implementation / Next Review
- AC-CS-04 통합 범위 명확화
- AC-CS-07 3 케이스 분리
- AC-CS-16 OQ-CS-3 ADR 해결 후 재작성
- Coverage gap ACs 5건 (EC-CS-3, EC-CS-6, Tuning 경계값, GSM E9 타임아웃)
- 소프트 억제 체감 플레이테스트 AC
- Pillar 4 Day 21 마지막 카드 의식 경계 (Card Hand UI GDD 작성 시)

## Review R2 — 2026-04-18 — Verdict: NEEDS REVISION (경미)
Scope signal: L (다중 시스템 통합, 3+ 수식, OQ-CS-3 ADR 필수)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, creative-director
Blocking items: 3 | Recommended: 12 | Nice-to-Have: 4
Summary: Full review (5명 전문가). R1 blocking 3건 모두 해소 확인. 신규 blocking 3건 발견: (1) ConfigAsset PostLoad 검증 하한 미명시 — TotalWeight=0 퇴행 가능, (2) 상태 전환 표에 on-demand/재시작 경로 누락 + Guard 모순, (3) CR-CS-3 타임아웃 degraded 경로에 AC 부재. creative-director: 모두 국소 수정(30분 이내)이며 설계 구조 변경 아님. game-designer가 HandSize=3 미정당화와 결정론적 시드의 판타지 충돌을 제기했으나 creative-director가 기각 (프로토타입에서 검증 / R1 합의 유지).
Prior verdict resolved: Yes — R1 blocking 3건 전부 해소

### Blocking Items (R2)
1. [systems-designer] ConfigAsset PostLoad 검증 하한 — InSeasonWeight/OffSeasonWeight/ConsecutiveDaySuppression = 0 시 TotalWeight=0
2. [systems-designer] 상태 전환 표 — Uninitialized→Preparing on-demand 경로 누락 + Offered→Preparing guard가 EC-CS-13과 모순
3. [qa-lead] AC-CS-18 — CR-CS-3 GSM 타임아웃 degraded 경로에 AC 없음

### Revisions Applied (R3 — same session)
- Tuning Knobs: ConsecutiveDaySuppression 안전 범위 (0.0, 1.0] → [0.05, 1.0], 극단 시 TotalWeight=0 경고 추가
- PostLoad 검증 블록 신설: 6개 검증 규칙 (InSeasonWeight, OffSeasonWeight, ConsecutiveDaySuppression, 역전 방어, HandSize, GameDurationDays)
- 상태 전환 표: Uninitialized→Preparing via OnPrepareRequested 행 추가 + Ready→Offered via bHandOffered 복원 행 추가
- 재시작 시퀀스 서술: "Uninitialized에서 시작" 명시, Offered 상태 복원 경로 명확화
- AC-CS-18(INTEGRATION) 추가: GSM 타임아웃 degraded 경로 검증
- AC 요약 테이블 갱신: INTEGRATION 2→3, 합계 17→18

### Deferred to Implementation / Next Review (R2 신규)
- [unreal-specialist] HashCombine 크로스 플랫폼 비결정론 — PC 단일 빌드에서 안전, ADR 후보
- [systems-designer] Tags 포맷 ("Season.Spring") + 복수 계절 태그 허용 여부 명시
- [qa-lead] AC-CS-04 응답 시간 ~50ms 구체화, AC-CS-07 3 케이스 분리, AC-CS-05 INTEGRATION 격상
- [qa-lead] EC-CS-19/EC-CS-20 신규 엣지 케이스 AC 추가
- [unreal-specialist] UGameInstanceSubsystem vs UWorldSubsystem 계층 종속성 — ADR에서 결정
- [unreal-specialist] TOptional vs FGiftCardRow* — DataTable 관용구, ADR 필요 시
- [unreal-specialist] FGameplayTagContainer 전환 ADR 우선순위 격상
- [game-designer] 의식 확인 순간, 계절 전환 체감, ConsecutiveDaySuppression 리프레이밍 — Card Hand UI GDD에서 반영

## Review R3 — 2026-04-18 — Verdict: APPROVED
Scope signal: L (다중 시스템 통합, 3 수식, 1 Blocking ADR, 7 의존성)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, creative-director
Blocking items: 4 (즉시 해소) | Recommended: 4 | Nice-to-Have: 3
Summary: Full review (5명 전문가). R2 blocking 3건 모두 해소 확인. R3 신규 blocking 4건 발견 — 전부 AC 분류 정정 및 누락 AC 추가로, 설계 변경 아닌 편집 수준. 즉시 적용 완료. creative-director: "3회 리뷰를 거치며 총 6건의 genuine blocker가 모두 해소된 건강한 GDD. 핵심 설계는 견고하고 Pillar 정렬이 잘 되어 있으며 구현 준비 완료. 추가 리뷰 패스는 수확 체감 영역." game-designer가 HandSize=3 정당화와 결정론적 시드 판타지 긴장을 재제기했으나 creative-director가 R2 판정 유지 (프로토타입 검증 + 1문장 근거 추가 권장).
Prior verdict resolved: Yes — R2 blocking 3건 전부 해소

### Blocking Items (R3) — All Resolved
1. [systems-designer, qa-lead] EC-CS-19/EC-CS-20 AUTOMATED AC 누락 → AC-CS-13b, AC-CS-20a 추가
2. [qa-lead] AC-CS-05 분류 오류 (AUTOMATED→INTEGRATION) → 재분류 완료
3. [qa-lead] AC-CS-15 방법론 혼재 → AC-CS-15a (공식 검증) + AC-CS-15b (Monte Carlo 수렴) 분리
4. [qa-lead] AC-CS-16 OQ-CS-3 의존 → [BLOCKED by OQ-CS-3] 태그 추가

### Revisions Applied (R3 — same session)
- AC-CS-05: `AUTOMATED` → `INTEGRATION` 재분류
- AC-CS-13b 신규: DayIndex=0 guard 검증 (AUTOMATED)
- AC-CS-20a 신규: GameDurationDays<4 PostLoad fallback 검증 (AUTOMATED)
- AC-CS-15 → AC-CS-15a (공식 결정론 검증) + AC-CS-15b (10K Monte Carlo 수렴, 고정 시드)
- AC-CS-16: `[BLOCKED by OQ-CS-3]` 태그 + ADR 해소 후 테스트 설계 가능 명시
- AC 요약 테이블 갱신: AUTOMATED 13→15, INTEGRATION 3→4, 합계 18→21

### Deferred to Implementation / ADR (R3 신규)
- [systems-designer, unreal-specialist] HashCombine 엔진 업그레이드 취약점 문서화 + 회귀 테스트 AC 권장
- [game-designer] HandSize=3 Tuning Knob에 Hick's Law/Paradox of Choice 1문장 근거 추가
- [game-designer] CR-CS-5에 결정론적 시드 vs 판타지 긴장 명시 1문장 추가
- [unreal-specialist] FTSTicker 근거 오류 → FTimerManager 권장으로 Implementation Notes 수정
- [unreal-specialist] Dynamic delegate → non-dynamic 확정 (ADR에서 결정)
- [unreal-specialist] OQ-CS-6 Asset Manager 등록 — 구현 시작 전 해소 권장
- [unreal-specialist] FName 대소문자 중복 검증 CODE_REVIEW AC 추가 권장
- [game-designer] 시즌 가중치 격차 메타게임 위험 — 플레이테스트 가설 기록
- [systems-designer] F-CS-2 Output Range 최솟값 기재 — 안전 범위 극단 반영
- [systems-designer] 전환 표 Offered→무시(동일 Day) 케이스 명시
