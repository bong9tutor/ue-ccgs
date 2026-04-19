# Moss Baby Character System — Review Log

## Review — 2026-04-17 — Verdict: NEEDS REVISION
Scope signal: M (moderate complexity)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, creative-director (synthesis)
Blocking items: 6 | Recommended: 6
Summary: Formula 수준 결함(정수 나눗셈, D=0 제로 나눗셈), DryingFactor 오버레이 공식 부재, AC 타입 오분류(MBC-04) 및 엣지 케이스 AC 누락 3건(E3/E4/E9), 카드 반응 보간 커브 미정의가 blocking. Drying 메카닉의 Pillar 1 위반 리스크에 대해 creative-director가 중립적 리프레이밍("고요한 대기") 권고. MVP 단일 최종 형태와 Player Fantasy 모순은 MODERATE로 하향 — 머티리얼 변주 부분 달성 주석으로 해결. 12건 모두 즉시 수정 적용됨.
Prior verdict resolved: First review

### Blocking Items Resolved (R1)
1. Formula 2 float 캐스팅 + 경계값 검증표
2. Formula 1 D=0 예외 규칙
3. Formula 5 (DryingFactor Overlay) 신규 추가
4. AC-MBC-04 순수 함수 검증으로 재작성
5. AC-MBC-13/14/15 추가 (E3/E4/E9 커버)
6. Formula 4 (카드 반응 지수 감쇠) 신규 추가

### Recommended Items Resolved (R1)
7. Drying → "고요한 대기" 리프레이밍 (서리 톤, 죄책감 금지)
8. MVP Player Fantasy 머티리얼 변주 주석
9. AC-MBC-06 결정론적 순수 함수 테스트
10. BreathingAmplitude 안전 범위 [0.01, 0.03], DryingThreshold 최소 2
11. 상태 우선순위 감정 설계 근거 추가
12. CR-1 "0프레임" → Scene Proxy 재빌드 설명

## Review — 2026-04-17 — Verdict: NEEDS REVISION (Minor)
Scope signal: M (moderate complexity)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, creative-director (synthesis)
Blocking items: 4 | Recommended: 6
Summary: R1의 12건 결함 모두 해결 확인. R2 적대적 심층 리뷰에서 Formula 1 SmoothStep t>D 오버런 방어 부재, BreathingAmplitude 안전 범위와 A≤E_base 규칙 모순(Sprout 충돌), AC-MBC-09 타입 오분류(AUTOMATED→INTEGRATION), Formula 5 DryingOverlay AC 커버리지 전무가 blocking. Recommended로 Formula 4 V_base 전환 순서 명시, CR-3 중복 카드 AC 추가(MBC-17), AC-MBC-15 결정론적 재작성, AC-MBC-02/08 개선. Creative-director: game-designer의 "카드 누적 감각 부재" BLOCKING을 RECOMMENDED로 하향 — 느린 머티리얼 변화가 곧 누적 감각(Pillar 2). 10건 모두 즉시 수정 적용됨. AC 합계 15→17.
Prior verdict resolved: Yes — R1 12건 전부 해소

### Blocking Items Resolved (R2)
1. Formula 1 t 클램프 추가 (`t = min(t, D_transition)` — SmoothStep 오버런 방어)
2. BreathingAmplitude 안전 범위 [0.01, 0.03] → [0.01, 0.02] (A ≤ E_base 모순 해소)
3. AC-MBC-09 AUTOMATED → INTEGRATION 재분류
4. AC-MBC-16 추가 (Formula 5 DryingOverlay AUTOMATED 경계값 검증)

### Recommended Items Resolved (R2)
5. Formula 4 V_base QuietRest→Reacting 전환 순서 명시 (DryingFactor 리셋 후 프리셋 기준)
6. AC-MBC-17 추가 (CR-3 중복 카드 반응 차단 검증)
7. AC-MBC-15 결정론적 재작성 (타이머 주입, 사인 위상 수치 검증)
8. AC-MBC-02 THEN 상태 쿼리 기반 재정의 (-nullrhi 호환)
9. AC-MBC-08 경계값 중간 구간 추가 (G=3→0.2, G=4→0.4)
10. Status 헤더 R2 업데이트

## Review — 2026-04-17 — Verdict: APPROVED
Scope signal: M (moderate complexity)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, creative-director (synthesis)
Blocking items: 0 | Recommended: 6 | Nice-to-Have: 9
Summary: R1의 6건 + R2의 4건 blocking 이슈 모두 해소 확인. R3 full adversarial review에서 신규 blocking 0건. game-designer가 MVP 단일 최종 형태에 의한 Player Fantasy 붕괴와 균일 카드 반응의 Pillar 2 위반을 BLOCKING으로 제기했으나 creative-director가 의도된 MVP 스코프 경계로 판정하여 ADVISORY로 하향. systems-designer의 F=0 나눗셈 방어와 E2 리타겟 V_current 규칙도 구현 단계 사안으로 ADVISORY 하향. unreal-specialist가 메시 슬롯 일치 아트 제약 누락과 UPrimaryDataAsset→UDataAsset 변경을 RECOMMENDED로 제기. qa-lead는 blocking 없이 6 ADVISORY(AC 표현 명확화) + 3 MINOR(커버리지 갭) 발견. 3회 리뷰를 거쳐 구현 가능 수준 도달.
Prior verdict resolved: Yes — R2 10건(4B+6R) 전부 해소

### Recommended Items (R3 — 구현/아트 파이프라인 이관)
1. Visual Requirements에 메시 슬롯 일치 제약 추가 (4종 메시 = 동일 슬롯 수 1개) [unreal-specialist]
2. UPrimaryDataAsset → UDataAsset 변경 [unreal-specialist]
3. CR-1 Lumen GI 흐려짐 "1-2프레임" → "수 프레임(4-6)" [unreal-specialist]
4. Formula 1 E2 리타겟 V_current = 실측값 명시 [systems-designer]
5. Formula 5 DryingFactor 자기 보간 제외 명시 [systems-designer]
6. DataAsset ClampMin/ClampMax 제약 명시 [systems-designer]

### Nice-to-Have Items (R3)
7. 브리딩 진폭 지각 가능성 에디터 검증 [game-designer]
8. Day 21 FinalReveal 인터럽트 감정 연속성 플레이테스트 검증 [game-designer]
9. AC-MBC-10 -nullrhi 호환 표현 재작성 [qa-lead]
10. AC-MBC-14 "동시" 정의 명확화 [qa-lead]
11. AC-MBC-17 타이머 주입 명시 [qa-lead]
12. 커버리지 갭 3건 (FinalReveal AC, SmoothStep 오버런 AC, A≤E₀ AC) [qa-lead]
13. SSS + HWRT Lumen UE 5.6 에디터 시각 검증 [unreal-specialist]
14. GrowthTransition 후 QuietRest 트리거 경쟁 경로 Edge Case 추가 [systems-designer]
15. Formula 3 Sprout E=0.0 도달 시각 허용 판단 [systems-designer]
