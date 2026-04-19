# Review Log: Game State Machine (+ Visual State Director)

## Review — 2026-04-17 — Verdict: NEEDS REVISION
Scope signal: L
Specialists: game-designer, systems-designer, technical-artist, qa-lead, unreal-specialist, creative-director (senior)
Blocking items: 4 | Recommended: 12 | Nice-to-Have: 9
Summary: MPC-Light Actor 아키텍처 결함(MPC가 Light Actor를 직접 구동하지 못함), LONG_GAP_SILENT의 Pillar 1 위반(즉시 Farewell 강제 → 죄책감 생산), LightAngleDeg 삼중 범위 모순, Dream defer 시간 캡 부재가 blocking. 설계 정밀도와 감정 설계 품질은 높은 수준이나, Visual State Director 흡수의 핵심 가정과 Pillar 1 준수에 수정 필요.
Prior verdict resolved: First review

### R2 수정 적용 (동일 세션)
B4 blocking 해소 + R10 recommended 적용:
- B1: OQ-6 ADR 추가 + Rule 3에 MPC-Light Actor 관계 명시
- B2: LONG_GAP_SILENT → 메마름 복구 설계로 전환. bWithered 서브컨디션 + 전환표/E4/E6/E7 재작성
- B3: Rule 3 LightAngleDeg 범위 8→45 확장 + Dream 예외 주석
- B4: DreamDeferMaxSec Tuning Knob 추가 + E5 재작성
- R3: FTSTicker → Subsystem Tick() 교체
- R4: UGameInstanceSubsystem 통일, OQ-1 해소
- R5: FOnGameStateChanged TwoParams(OldState, NewState) 확장
- R6: E16 Farewell Budget 실패 Edge Case 추가
- R7: E17 Offering ADVANCE_SILENT + 전환표 추가
- R8: MossBabySSSBoost MPC 파라미터 추가
- R9: Formula 방어 조건 (D_blend=0, D_min≤D_max, F3 출력 범위)
- R10: Downstream 소비자 목록에 Moss Baby Character #6 추가
- R11: UPrimaryDataAsset → UDataAsset 변경
- R12: AmbientIntensity HDR 파라미터 추가

### 재리뷰 시 확인 필요
- R1: AC 정밀화 (qa-lead 17건 소견 중 11개 AC 재작성 + 6개 신규 AC)
- Time GDD 동기화: LONG_GAP_SILENT 재정의에 따라 time-session-system.md 갱신 필요
- Art Bible §1 원칙 2: Dream 예외 조항 추가 필요 (Art Bible 측 수정)
- Nice-to-Have 9건은 구현/downstream 이관

## Review — 2026-04-17 — Verdict: NEEDS REVISION (R2 재리뷰, full mode)
Scope signal: L
Specialists: game-designer, systems-designer, technical-artist, qa-lead, unreal-specialist, creative-director (senior)
Blocking items: 6 (DB 3 + IB 3) | Recommended: 8
Summary: R1의 4 블로커는 모두 해소 확인. 신규 핵심 이슈: (1) Withered 경로 도달 불가 — LONG_GAP_SILENT는 항상 DayIndex=21→Farewell이므로 bWithered 진입 불가 (cross-system 논리 공백), (2) MPC 테이블에 AmbientIntensity·MossBabySSSBoost per-state 값 누락, (3) UGameInstanceSubsystem Tick() 가상함수 부재로 구현 참고 오류. creative-director: "DB-1 해소 후 R3 재리뷰 시 APPROVE 가능"
Prior verdict resolved: R1 4 blockers all resolved

### R3 수정 적용 (동일 세션)
설계 수준 블로커 3건 + 구현 참고 블로커 3건 + 권고 8건 해소:
- DB-1: Withered 트리거를 LONG_GAP_SILENT → ADVANCE_SILENT DayIndex 점프폭 감지로 변경. WitheredThresholdDays(기본 3) Tuning Knob 추가. 전환표·E4/E6/E7·Interaction §1 전면 재작성. E18 신규
- DB-2: AC-GSM-20(bWithered 설정+영속화), AC-GSM-21(해제+복원) 추가. 21 AC 총계
- DB-3: AmbientIntensity·MossBabySSSBoost 6개 상태 초기값 MPC 목표값 테이블에 추가
- IB-1: Tick() → FTSTicker::GetCoreTicker().AddTicker() 패턴으로 교체
- IB-2: PostInitialize() → Lazy-init + null check/re-acquire 패턴
- IB-3: Formula 2에 런타임 clamp + WITH_EDITOR 가드 명시
- R1: Formula 1 ease(0.5) 산술 오류 수정 (2.5→2.0)
- R2: Formula 1에 x = clamp(t/D_blend, 0, 1) 명시
- R3: DawnMinDwellSec Formula 3 범위 [2.0,4.0]→[2.0,5.0] 통일
- R4: AC-GSM-11/18 "동일 프레임" → 테스트 가능 인과 관계 재작성
- R5: E10에 MossBabySSSBoost ReducedMotion 동작 명시 (목표값 즉시 적용)
- R6: E16 Farewell Budget 실패 감정 트레이드오프 명시적 설계 결정 기록
- R7: AC-GSM-19에 OQ-6 ADR 전제조건 추가
- R8: Art Bible §1 Dream 예외는 Art Bible 측 수정 대기 (GDD에 이미 명시)

### R3 재리뷰 시 확인 필요
- 전환표 일관성: LONG_GAP_SILENT → 항상 Farewell 직행. withered 분기 완전 제거 확인
- Withered 트리거: ADVANCE_SILENT DayIndex 점프폭 감지 경로 정합성
- MPC 목표값: AmbientIntensity·MossBabySSSBoost 초기값이 Art Bible §2와 정합하는지
- Time GDD 영향: GSM이 LONG_GAP_SILENT에서 withered를 분리했으므로 Time GDD 수정 불필요 확인
- Art Bible §1 원칙 2: Dream 예외 조항 여전히 미반영 (별도 수정)
- Nice-to-Have: N1(AC-GSM-04 구체화), N2(AC-GSM-12 Ready 지연 분기), N3(E15 손상 분기 AC), N4(UENUM BlueprintType) — 구현 이관

## Review — 2026-04-17 — Verdict: APPROVED (R3 재리뷰, lean mode)
Scope signal: L
Specialists: N/A (lean mode — single-session analysis)
Blocking items: 0 | Recommended: 4
Summary: R2의 6개 블로커(DB-1 Withered 경로, DB-2 AC 부재, DB-3 MPC 테이블, IB-1 Tick, IB-2 PostInit, IB-3 런타임 방어) 전부 해소 확인. Cross-system 정합성 검증 완료 — Time GDD LONG_GAP_SILENT 계약, Stillness Budget Request/Release 패턴, Formula 산술 모두 일관. R2 creative-director "DB-1 해소 시 APPROVE 가능" 기준 충족. RECOMMENDED 4건(전환표 Dawn/Offering→Farewell 누락, Withered 트리거 ADVANCE_WITH_NARRATIVE 미표기, E6/E7 AC 부재, E18 번호 순서)은 구현 이관.
Prior verdict resolved: R2 6 blockers all resolved
