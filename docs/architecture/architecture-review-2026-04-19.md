# Architecture Review Report — 2026-04-19

**Reviewer**: Technical Director subagent (independent context, fresh read)
**Scope**: `architecture.md v1.1` + 7 ADRs (0001/0002/0003/0004/0005/0010/0011) + 14 MVP GDDs + engine reference
**Review Mode**: lean (elevated to full-depth critical audit per user request)
**Bias Mitigation**: Executed via `technical-director` subagent with fresh context — `/architecture-review` skill's fresh-session requirement satisfied by subagent delegation

## Verdict: **CONCERNS** 🟡

Architecture substantively sound (24/30 TR coverage, zero cross-ADR contradictions, HIGH/MEDIUM engine risks correctly flagged, 5 Principles applied consistently). But **1 BLOCKER + 8 Concerns** prevent unqualified APPROVE.

## A. TR Coverage (spot-check 30 TRs)

| Layer | Covered | Partial | Gap |
|---|---|---|---|
| Foundation (10) | 9 ✅ | 1 ⚠️ (ADR-0009 P1) | 0 |
| Core (8) | 7 ✅ | 1 ⚠️ (PSO Precaching) | 0 |
| Feature (8) | 5 ✅ | 1 ⚠️ (ADR-0007 P1 VS) | 2 ❌ (ADR-0012, ADR-0006 — Card sprint) |
| Presentation (2) | 1 ✅ | 1 ⚠️ (ADR-0008 P1) | 0 |
| Meta (2) | 2 ✅ | 0 | 0 |
| **Total** | **24 ✅** | **4 ⚠️** | **2 ❌** |

Foundation coverage 10/10 → **Foundation sprint readiness: PASS**.
Gaps 2건은 Card sprint-local (ADR-0012 + ADR-0006).

## B. Cross-ADR Conflict Detection

| Check | Result |
|---|---|
| ADR-0002 ↔ ADR-0010 amend | ⚠️ clean but subtle — Concern C7 (cross-ref note 추가 권장) |
| ADR-0004 ↔ Lumen Dusk Rule 3 | ✅ consistent (scene = MPC read-only, GSM drives Light Actors directly) |
| **ADR-0005 ↔ 4 affected GDDs** | ❌ **BLOCKER B1 — Migration Plan 미실행** |
| ADR-0001 ↔ ADR-0011 | ✅ consistent |
| ADR-0002 ↔ 0003 ↔ 0010 | ✅ consistent |
| ADR-0011 Config asset scope | ⚠️ minor Growth/Card/Dream split 모호 |

## C. Engine Compatibility Audit

**APPROVE** — 모든 7 ADR이 UE 5.6 targeting, Knowledge Risk 정확 flag, 5.6-VERIFY AC 명시, deprecated API 미사용.
- Minor: AC-DP-09/10에 [5.6-VERIFY] 라벨 권장 (Concern C8)

## D. Architecture Principle Adherence

| Principle | Verdict |
|---|---|
| 1 Pillar-First | ✅ |
| 2 Schema as Guard | ✅ |
| 3 Per-Trigger Atomicity | ✅ (ADR-0009 P1 tracked) |
| 4 Foundation Independence + Layer Boundaries | ⚠️ **Rule 5 wording vs ADR-0004 practice — Concern C3** |
| 5 Engine-Aware | ✅ |

## E. Critical Findings

### 🔴 BLOCKER

**B1 — ADR-0005 Migration Plan 미실행**
- Card GDD line 229/237/245, Dream Trigger GDD line 63/198/212/474/618, GSM GDD line 133/225에 여전히 `FOnCardOffered` 단일 subscribe 모델
- entities.yaml: `FOnCardProcessedByGrowth` delegate 미등록, `FGrowthProcessResult` struct 미등록
- AC-CS-16 `[BLOCKED by OQ-CS-3]` 마킹 미해소
- **영향**: sprint 시작 시 ADR-0005가 reverse한 subscription topology를 구현하게 됨

### 🟡 Concerns

| # | Concern | 영향 |
|---|---|---|
| **C1** | ADR-0012 (`GetLastCardDay()`) 미작성 — Card sprint 전 필수 | `bHandOffered` 복원 정확성 (Pillar 4) |
| **C2** | ADR-0006 (카드 풀 Eager vs Lazy) 미작성 — 기본 Eager 확정 기록 필요 | Card sprint 결정 기록 |
| **C3** | Architecture Rule 5 wording이 ADR-0004 practice보다 엄격 ("Engine API 직접 호출은 Foundation/Meta에 한정" — 그러나 ADR-0004 GSM이 Light Actor 직접 호출) | Rule 5 wording 완화 또는 ADR-0004 exception 명시 |
| **C4** | PSO Precaching 별도 ADR 부재 (HIGH risk 도메인이지만 AC-LDS-11 ADVISORY만 커버) | Lumen Dusk 첫 milestone 전 ADR 필요 |
| **C5** | ADR-0003 HDD cold-start 완화책 취약 ("SSD 90%" 통계 의존) | Pillar 1 응답성 potential 위반 |
| **C6** | ADR-0011 "Restart Required" convention 미정 | Hot reload 한계 처리 일관성 |
| **C7** | ADR-0002 ↔ ADR-0010 amend 메커니즘 textually subtle | 독자가 ADR-0002 단독 읽을 때 혼동 |
| **C8** | AC-DP-09/10 [5.6-VERIFY] 라벨 누락 | GameInstance::Init sync timing은 5.6-sensitive |

### 💡 Additional E.1-E.5 Findings

1. **Uncovered TRs** (5건):
   - Save file path i18n — Korean username (UserSettingsDir non-ASCII)
   - Subsystem deinitialization order (teardown 순서 미명시)
   - Time 1Hz FTSTicker registration timing vs Save Initialize race
   - PSO Precaching cold-start 전략
   - WriteSeqNumber uint32 overflow recovery ADR

2. **Weakest ADRs**: ADR-0003 (HDD 통계 의존), ADR-0011 (Restart Required 방임)

3. **Implementation blind spots**:
   - AC-LDS-04 GPU budget 지속 초과 시 SWRT 폴백 escalation decision boundary 미정
   - Save/Load FlushOnly (T2) + SaveAsync 동시 reentrancy guard는 문서 산재 (BLOCKING AC 없음)

4. **GDD-ADR drift**: B1 참조 (가장 중요한 drift)

5. **Overengineering verdict**: None — 모든 7 ADR이 제약조건에 최적 단순성 채택

## F. ADR Accepted Transition Recommendations

| ADR | Recommended next status |
|---|---|
| **ADR-0001** | **Accept** — 결정 명확, GDD linkage crystal clear |
| **ADR-0002** | Accept **after C7** (cross-ref 노트 추가) |
| **ADR-0003** | **Keep Proposed** — C5 (HDD 완화) 해결 전 |
| **ADR-0004** | **Keep Proposed** — C3 Rule 5 + C4 PSO 분리 해결 전 + 첫 GPU 프로파일링 결과 |
| **ADR-0005** | Accept **after B1** (migration 실행) — ADR 텍스트 자체는 excellent |
| **ADR-0010** | **Accept** — ADR-0002 의존, migration 단순 |
| **ADR-0011** | **Keep Proposed** — C6 (RestartRequired convention) + Growth/Card/Dream 분리 명확화 전 |

## Minimal Path to APPROVE

1. ADR-0005 Migration Plan §1-9 실행 (GDD 4건 + entities.yaml + AC unblock) — 1 세션 **[B1]**
2. ADR-0012 작성 — 0.5 세션 (C1, Card sprint 전)
3. ADR-0006 작성 — 0.5 세션 (C2, Card sprint 전)
4. Architecture Rule 5 wording 완화 — 5분 (C3)
5. ADR-0013 PSO Precaching 전략 OR ADR-0004 scope 확장 — 1 세션 (C4)
6. ADR-0003 Consequences 보강 (HDD 통계 의존 명시적 Known Compromise) OR preload-background 하이브리드 — 0.5 세션 (C5)
7. ADR-0002 cross-ref 노트 (C7) + AC [5.6-VERIFY] 라벨 (C8) + ADR-0011 RestartRequired convention (C6) — 15분

**Total: ~4 세션 to APPROVE**

## Foundation Sprint Readiness

**CONDITIONAL PASS** — Foundation layer 10/10 TRs 커버. 다음 조건으로 Foundation sprint 시작 가능:
- ADR-0005 migration 실행 (BLOCKER — Foundation sprint와 병행 가능, 단 sprint 전 완료)
- ADR-0003 current text 수용 (HDD compromise 명시적 기록)
- ADR-0012/0006은 Card sprint kickoff 전까지 defer 가능

## Next Actions (권장 순서)

1. 이 리뷰 파일 저장 → `docs/architecture/architecture-review-2026-04-19.md`
2. **B1 해결 — ADR-0005 Migration** (GDD 4건 + entities.yaml)
3. C7, C8, C3 wording fix (신속 처리)
4. Architecture traceability index 생성 (`docs/architecture/architecture-traceability.md`)
5. ADR Accepted 전환 (0001, 0002 after C7, 0005 after B1, 0010)
6. `/gate-check pre-production` 재실행
