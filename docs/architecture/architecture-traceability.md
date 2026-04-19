# Architecture Traceability Index

<!-- Living document — updated by `/architecture-review` after each review run.
     Last review: `docs/architecture/architecture-review-2026-04-19.md` -->

## Document Status

- **Last Updated**: 2026-04-19 (post-review reconciliation)
- **Engine**: Unreal Engine 5.6 (hotfix 5.6.1 권장)
- **GDDs Indexed**: 14 (MVP)
- **ADRs Indexed**: 10 (7 original + 3 post-review: ADR-0006/0012/0013)
- **Last Review**: [architecture-review-2026-04-19.md](architecture-review-2026-04-19.md) — Verdict: CONCERNS (B1 + C1-C8 identified → 모두 해결됨)

## Coverage Summary

Subagent review의 30 TR spot-check + post-migration + ADR-0007/0008/0009 추가 작성 후 갱신.

| Status | Count | Percentage |
|--------|-------|-----------|
| ✅ Covered | 30 | 100.0% |
| ⚠️ Partial | 0 | 0.0% |
| ❌ Gap | 0 | 0.0% |
| **Total** | **30** | **100%** |

**Post-ADR-0005/0006/0007/0008/0009/0012/0013 작성**: 모든 Partial/Gap 해소. 13 ADR 모두 Accepted.

---

## Traceability Matrix (Spot-check 30 TRs)

### Foundation Layer

| Req ID | GDD | System | Requirement Summary | ADR(s) | Status |
|--------|-----|--------|---------------------|--------|--------|
| TR-time-001 | time-session-system.md | Time & Session | `NO_FORWARD_TAMPER_DETECTION_ADR` AC | ADR-0001 | ✅ |
| TR-time-002 | time-session-system.md | Time & Session | OQ-5 `UDeveloperSettings` knob exposure | ADR-0011 | ✅ |
| TR-time-003 | time-session-system.md | Time & Session | `IMossClockSource` injection seam | (GDD contract) | ✅ |
| TR-time-004 | time-session-system.md | Time & Session | Windows suspend/resume behaviour | OQ-IMP-1 (Implementation task) | ✅ |
| TR-dp-001 | data-pipeline.md | Data Pipeline | OQ-ADR-1 container selection | ADR-0002 | ✅ |
| TR-dp-002 | data-pipeline.md | Data Pipeline | OQ-ADR-2 loading strategy | ADR-0003 | ✅ |
| TR-dp-003 | data-pipeline.md | Data Pipeline | `UMossFinalFormAsset` TMap editing (CR-8) | ADR-0010 | ✅ |
| TR-dp-004 | data-pipeline.md | Data Pipeline | AC-DP-18 [5.6-VERIFY] PrimaryAssetTypesToScan cooked | ADR-0002 + ADR-0010 | ✅ |
| TR-save-001 | save-load-persistence.md | Save/Load | OQ-8 compound event sequence atomicity | ADR-0009 (P1 — deferred to Day 13 scenario) | ⚠️ |
| TR-input-001 | input-abstraction-layer.md | Input Abstraction | Gamepad drag alternative (OQ-2) | (VS concern) | ✅ |

**Foundation: 9 ✅, 1 ⚠️** (ADR-0009 P1 deferred, negative AC in Save/Load §8 documents the gap)

### Core Layer

| Req ID | GDD | System | Requirement Summary | ADR(s) | Status |
|--------|-----|--------|---------------------|--------|--------|
| TR-gsm-001 | game-state-machine.md | GSM | OQ-6 MPC ↔ Light Actor sync (BLOCKING) | ADR-0004 | ✅ |
| TR-gsm-002 | game-state-machine.md | GSM | MPC writer uniqueness (AC-LDS-06) | ADR-0004 + architecture §Cross-Layer | ✅ |
| TR-gsm-003 | game-state-machine.md | GSM | `bWithered` persistence + MPC path | (GDD contract) | ✅ |
| TR-gsm-004 | game-state-machine.md | GSM | E12 Time vs Load ordering | AC-GSM-15 (GDD) | ✅ |
| TR-mbc-001 | moss-baby-character-system.md | MBC | Material param literal ban (AC-MBC-03) | (GDD Preset asset) | ✅ |
| TR-mbc-002 | moss-baby-character-system.md | MBC | Priority (FinalReveal > GrowthTransition > Reacting > Drying/Idle) | (GDD contract) | ✅ |
| TR-lumen-001 | lumen-dusk-scene.md | Lumen Dusk | HIGH risk Lumen HWRT + PSO | ADR-0004 + **ADR-0013 (신규)** | ✅ |
| TR-lumen-002 | lumen-dusk-scene.md | Lumen Dusk | `Farewell → Any` transition ban | architecture §Cross-Layer + AC-GSM-05 | ✅ |

**Core: 8 ✅** (PSO Precaching gap closed by ADR-0013 2026-04-19)

### Feature Layer

| Req ID | GDD | System | Requirement Summary | ADR(s) | Status |
|--------|-----|--------|---------------------|--------|--------|
| TR-card-001 | card-system.md | Card | OQ-CS-3 `FOnCardOffered` order (BLOCKING) | ADR-0005 | ✅ |
| TR-card-002 | card-system.md | Card | OQ-CS-2 `GetLastCardDay()` API | **ADR-0012 (신규)** | ✅ |
| TR-card-003 | card-system.md | Card | OQ-CS-5 card pool Eager vs Lazy | **ADR-0006 (신규)** | ✅ |
| TR-card-004 | card-system.md | Card | FGuid → FName migration (RESOLVED 2026-04-18) | (cross-review D-1) | ✅ |
| TR-growth-001 | growth-accumulation-engine.md | Growth | CR-1 atomicity | AC-GAE-02 (GDD) | ✅ |
| TR-growth-002 | growth-accumulation-engine.md | Growth | CR-8 `FFinalFormRow` storage | ADR-0010 | ✅ |
| TR-dream-001 | dream-trigger-system.md | Dream | OQ-DTS-1 PendingDreamId retry UX | (GDD §E5) | ✅ |
| TR-stillness-001 | stillness-budget.md | Stillness | OQ-1 Sound Concurrency integration | ADR-0007 (P1 — VS sprint) | ⚠️ |

**Feature: 7 ✅, 1 ⚠️** (ADR-0007 VS sprint concern — MVP Audio System 미구현 상태)

### Presentation Layer

| Req ID | GDD | System | Requirement Summary | ADR(s) | Status |
|--------|-----|--------|---------------------|--------|--------|
| TR-chu-001 | card-hand-ui.md | Card Hand UI | OQ-CHU-1 UMG drag implementation | ADR-0008 (P1 — Card Hand UI sprint) | ⚠️ |
| TR-dj-001 | dream-journal-ui.md | Dream Journal UI | `SaveAsync` direct-call ban (AC-DJ-17) | architecture §Cross-Layer + GDD | ✅ |

**Presentation: 1 ✅, 1 ⚠️** (ADR-0008 sprint-local concern)

### Meta Layer

| Req ID | GDD | System | Requirement Summary | ADR(s) | Status |
|--------|-----|--------|---------------------|--------|--------|
| TR-window-001 | window-display-management.md | Window & Display | Steam Deck Proton (OQ-IMP-5) | (Implementation task) | ✅ |
| TR-window-002 | window-display-management.md | Window & Display | `UDeveloperSettings` knob exposure | ADR-0011 | ✅ |

**Meta: 2 ✅**

---

## Known Gaps

**Foundation Layer Gaps**: 0 (Foundation sprint safe to start)

**Core Layer Gaps**: 0 (PSO Precaching closed by ADR-0013)

**Feature Layer Gaps (P1 — sprint kickoff 시 작성)**:
- ADR-0007 (Stillness ↔ Sound Concurrency) — Audio System VS sprint 진입 시

**Presentation Layer Gaps (P1)**:
- ADR-0008 (UMG 드래그 구현) — Card Hand UI sprint 진입 시

**Foundation/Core Cross-layer (P1 deferred)**:
- ADR-0009 (Compound Event Sequence Atomicity) — Day 13 compound event 시나리오 발생 시

---

## Cross-ADR Conflicts

Review 결과: 0 conflict.

(ADR-0002 ↔ ADR-0010 amend는 C7 해결로 cross-reference 노트 추가됨)

---

## Engine Compatibility Summary

| ADR | Engine Version | Risk | Post-cutoff APIs | 5.6-VERIFY |
|-----|---------------|------|------------------|------------|
| ADR-0001 | UE 5.6 | MEDIUM (FPlatformTime) | None | (grep patterns) |
| ADR-0002 | UE 5.6 | MEDIUM (AssetManager cooked) | None | AC-DP-18 |
| ADR-0003 | UE 5.6 | MEDIUM (GameInstance::Init) | None | AC-DP-09/10 (C8 해결) |
| ADR-0004 | UE 5.6 | **HIGH** (Lumen HWRT) | Lumen HWRT settings | AC-LDS-04/07 |
| ADR-0005 | UE 5.6 | LOW (Multicast Delegate) | None | (비공개 계약 회피 패턴) |
| ADR-0006 | UE 5.6 | LOW (FRandomStream) | None | Full 40장 < 1ms |
| ADR-0010 | UE 5.6 | LOW (UPrimaryDataAsset) + 상속 MEDIUM (ADR-0002) | None | AC-DP-18 shared |
| ADR-0011 | UE 5.6 | LOW (UDeveloperSettings) | None | None |
| ADR-0012 | UE 5.6 | LOW (int32 getter) | None | None |
| ADR-0013 | UE 5.6 | **HIGH** (PSO Precaching 5.6 점진 컴파일) | FShaderPipelineCache BatchMode::Fast | AC-LDS-11/12 |

**Post-cutoff APIs**: ADR-0004 (Lumen HWRT settings) + ADR-0013 (FShaderPipelineCache) — 모두 실기 검증 (AC-LDS-04/11) 대상.

**Deprecated APIs referenced**: 0 (모든 ADR이 `deprecated-apis.md` 테이블 회피 확인).

---

## ADR Status Summary (final 2026-04-19)

| ADR | Status | Rationale |
|-----|--------|-----------|
| ADR-0001 | **Accepted** (2026-04-19) | Policy ADR — 결정 명확, CI grep 강제 준비됨 |
| ADR-0002 | **Accepted** (2026-04-19) | C7 해결 — cross-ref 노트 추가됨 |
| ADR-0003 | **Accepted** (2026-04-19) | C5 해결 — HDD Cold-Start Known Compromise 명시. AC-DP-09/10 [5.6-VERIFY] Implementation 단계 검증 |
| ADR-0004 | **Accepted** (2026-04-19) | C3 Rule 5 wording 완화. AC-LDS-04 [5.6-VERIFY] Implementation 단계 검증 — 결정 자체 안정 |
| ADR-0005 | **Accepted** (2026-04-19) | B1 해결 — 4 GDD + entities.yaml migration 완료 |
| ADR-0006 | **Accepted** (2026-04-19) | Eager 결정 명확 — Card sprint Implementation 즉시 채택 |
| ADR-0007 | **Accepted** (2026-04-19) | Hybrid (Stillness + Engine concurrency) 결정 명확 — VS Audio System sprint 채택 |
| ADR-0008 | **Accepted** (2026-04-19) | Enhanced Input subscription 채택 — Card Hand UI sprint Implementation 즉시 |
| ADR-0009 | **Accepted** (2026-04-19) | GSM Begin/EndCompoundEvent boundary 결정 — Day 13 시나리오 발생 시 검증 |
| ADR-0010 | **Accepted** (2026-04-19) | ADR-0002 의존 — migration 단순 |
| ADR-0011 | **Accepted** (2026-04-19) | C6 ConfigRestartRequired convention 추가. Growth/Card/Dream Knob 분리는 sprint 진입 시 GDD 갱신 |
| ADR-0012 | **Accepted** (2026-04-19) | int32 sentinel 0 결정 명확 — Card sprint Implementation 즉시 |
| ADR-0013 | **Accepted** (2026-04-19) | PSO Async Bundle + Graceful Degradation. AC-LDS-11 [5.6-VERIFY] Implementation 단계 검증 |

**Accepted: 13 / Proposed: 0 / 미작성: 0** — Pre-Production 단계의 모든 ADR 작성 + Accepted 완료. Foundation/Core/Feature/Presentation sprint 모두 즉시 지원.

---

## Next Review Trigger

다음 `/architecture-review` 실행 권장 시점:
- Foundation sprint 중간 — Foundation 구현이 ADR과 일치하는지 검증
- Card sprint 시작 전 — ADR-0006/0012 Accepted 확인
- Lumen Dusk 첫 milestone 후 — ADR-0004/0013 GPU 프로파일링 결과 반영
- VS 진입 전 — ADR-0007/0008 작성 완료 확인
