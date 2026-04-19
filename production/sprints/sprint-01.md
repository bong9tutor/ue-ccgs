# Sprint 01 — 2026-04-21 to 2026-05-02

> **Milestone**: MVP Foundation (Target: 2026-06-16, ~8 weeks from today)
> **Review mode**: lean — PR-SPRINT director gate skipped
> **Manifest Version**: 2026-04-19 ([control-manifest.md](../../docs/architecture/control-manifest.md))

## Sprint Goal

Foundation 4 시스템의 **뼈대(skeleton)와 핵심 계약**을 확립하여 Core sprint (GSM / MBC / Lumen Dusk) 진입 가능한 상태를 만든다. Time/Pipeline/Save/Load/Input Subsystem 골격 + 주요 공용 타입(FSessionRecord, IMossClockSource, FGiftCardRow, UMossSaveData, IA_OfferCard) 확정 + 초기 CI grep hook.

## Capacity

- **Solo developer** (Windows / UE 5.6 C++)
- Total days: **10 working days** (2 weeks)
- Effective: 6 hours/day = **60 hours**
- Buffer (20%): **12 hours** — 첫 UE 프로젝트 셋업 예측 오차 + 5.6 API 실기 학습
- Available: **48 hours**
- Story 평균: ~3 hours → 약 **13-16 stories** 가능

## Tasks

### Must Have (Critical Path — Foundation skeleton)

| ID | Task | Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------|-----------|-------------|---------------------|
| 1-1 | Time 001 — `IMossClockSource` + FRealClockSource + FMockClockSource | gameplay-programmer | 0.5 | None | [story-001](../epics/foundation-time-session/story-001-clock-source-interface.md) AC |
| 1-2 | Time 002 — `FSessionRecord` struct + double precision round-trip | gameplay-programmer | 0.3 | 1-1 | [story-002](../epics/foundation-time-session/story-002-session-record-struct.md) AC |
| 1-3 | Time 003 — `UMossTimeSessionSubsystem` 뼈대 + enums + Settings | gameplay-programmer | 0.5 | 1-2 | [story-003](../epics/foundation-time-session/story-003-subsystem-skeleton-enums.md) AC |
| 1-4 | Pipeline 001 — Schema Types (FGiftCardRow/UMossFinalFormAsset/UDreamDataAsset/UStillnessBudgetAsset) + Settings | gameplay-programmer | 0.5 | None | [story-001](../epics/foundation-data-pipeline/story-001-schema-types-settings.md) AC |
| 1-5 | Pipeline 003 — `UDataPipelineSubsystem` 4-state machine + pull API contract | gameplay-programmer | 0.5 | 1-4 | [story-003](../epics/foundation-data-pipeline/story-003-subsystem-state-machine.md) AC |
| 1-6 | Pipeline 004 — Catalog Registration Loading + DegradedFallback | gameplay-programmer | 0.7 | 1-5 | [story-004](../epics/foundation-data-pipeline/story-004-catalog-registration-loading.md) AC |
| 1-7 | Save/Load 001 — `UMossSaveData` USaveGame + ESaveReason + Settings | gameplay-programmer | 0.3 | 1-2 (FSessionRecord) | [story-001](../epics/foundation-save-load/story-001-save-data-schema-enum.md) AC |
| 1-8 | Save/Load 002 — Subsystem 4-trigger lifecycle + coalesce | gameplay-programmer | 0.7 | 1-7 | [story-002](../epics/foundation-save-load/story-002-subsystem-lifecycle-triggers.md) AC |
| 1-9 | Save/Load 003 — Header block + CRC32 + Dual-slot Load | gameplay-programmer | 0.5 | 1-8 | [story-003](../epics/foundation-save-load/story-003-header-crc-slot-selection.md) AC |
| 1-10 | Save/Load 004 — `FMossSaveSnapshot` + Atomic write + Ping-pong | gameplay-programmer | 0.5 | 1-9 | [story-004](../epics/foundation-save-load/story-004-atomic-write-snapshot.md) AC |
| 1-11 | Input 001 — 6 Input Actions + 2 IMC assets | gameplay-programmer | 0.3 | None | [story-001](../epics/foundation-input-abstraction/story-001-input-actions-mapping-contexts.md) AC |
| 1-12 | Input 002 — `UMossInputAbstractionSubsystem` + Settings + IMC 등록 | gameplay-programmer | 0.5 | 1-11 | [story-002](../epics/foundation-input-abstraction/story-002-subsystem-settings-mapping-registration.md) AC |
| 1-13 | Input 003 — Auto-detection + Hysteresis + OnInputModeChanged | gameplay-programmer | 0.5 | 1-12 | [story-003](../epics/foundation-input-abstraction/story-003-input-mode-auto-detect-hysteresis.md) AC |

**Must Have total: ~6.3 days (~38 hours)**

### Should Have

| ID | Task | Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------|-----------|-------------|---------------------|
| 1-14 | Time 004 — Between-session Classifier (Rules 1-4) + Formulas 1, 4, 5 | gameplay-programmer | 0.7 | 1-3 | [story-004](../epics/foundation-time-session/story-004-between-session-classifier.md) AC |
| 1-15 | Pipeline 002 — DefaultEngine.ini PrimaryAssetTypesToScan + AC-DP-18 [5.6-VERIFY] | gameplay-programmer | 0.5 | 1-4 | [story-002](../epics/foundation-data-pipeline/story-002-defaultengine-ini-primary-asset-types.md) AC + 5.6-VERIFY cooked build |
| 1-16 | Save/Load 005 — Migration chain (Formula 4) + archive fallback | gameplay-programmer | 0.5 | 1-10 | [story-005](../epics/foundation-save-load/story-005-migration-chain-archive.md) AC |
| 1-17 | Input 004 — Offer Hold Formula 2 (Mouse 0.15s / Gamepad 0.20s) | gameplay-programmer | 0.5 | 1-13 | [story-004](../epics/foundation-input-abstraction/story-004-offer-hold-formula-thresholds.md) AC |

**Should Have total: ~2.2 days (~13 hours)**

### Nice to Have (여유 시)

| ID | Task | Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------|-----------|-------------|---------------------|
| 1-18 | Time 005 — In-session 1Hz Tick (Rules 5-8) + FTSTicker | gameplay-programmer | 0.5 | 1-14 | [story-005](../epics/foundation-time-session/story-005-in-session-tick-rules.md) AC |
| 1-19 | Pipeline 006 — T_init budget 실측 + 3단계 임계 로그 | gameplay-programmer | 0.5 | 1-6 | [story-006](../epics/foundation-data-pipeline/story-006-tinit-budget-overflow-thresholds.md) AC (AC-DP-09/10 [5.6-VERIFY]) |
| 1-20 | Save/Load 006 — NarrativeCount atomic + Compound negative AC | gameplay-programmer | 0.5 | 1-10 | [story-006](../epics/foundation-save-load/story-006-narrative-atomic-compound-negative.md) AC |
| 1-21 | Input 005 — `NO_RAW_KEY_BINDING` CI grep + Mouse-only MANUAL | gameplay-programmer | 0.3 | 1-13 | [story-005](../epics/foundation-input-abstraction/story-005-no-raw-key-mouse-only-hover-gamepad-silent.md) AC |

**Nice to Have total: ~1.8 days (~11 hours)**

## Carryover from Previous Sprint

| Task | Reason | New Estimate |
|------|--------|-------------|
| None | This is Sprint 1 — no previous sprint | — |

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| **UE 5.6 첫 Subsystem 등록 학습 곡선** — 개인 튜토리얼 저장소로 엔진 첫 실사용 | HIGH | MEDIUM | Buffer 20% 확보 + 1-1/1-4/1-7/1-11 (skeleton stories)에 여유 배분. 시간 초과 시 Should Have 1건 defer |
| **Solo velocity 알 수 없음** — 첫 sprint, 실제 속도 unmeasured | HIGH | MEDIUM | Must Have 13 stories만 커밋, Should/Nice는 stretch. Sprint 중간(5일차) burndown 체크 |
| **OQ-IMP-1 Windows suspend 실기** — Time Story 007 필수 (MANUAL, Win10/11 2대 필요) | MEDIUM | LOW | Story 007은 Nice to Have에도 포함 안 함 — 별도 Sprint 2 task. Sprint 1은 interface 준비만 |
| **Pipeline AC-DP-18 [5.6-VERIFY] Cooked 빌드 실기** — 첫 Cook 파이프라인 시도 | MEDIUM | MEDIUM | Should Have 1-15에 포함. 실패 시 DegradedFallback 경로가 이미 ADR-0003에 있으므로 기능 블로킹 없음 |
| **Save/Load dual-slot ping-pong 구현 복잡도** — 처음 구현 시 atomic rename 이슈 가능 | MEDIUM | HIGH | 1-9/1-10 총 1.0 day 배정 (상대적으로 넉넉). CRC32 먼저(1-9), atomic write 나중(1-10)으로 단계화 |
| **Foundation 4 epic이 의존 0건이나 Save/Load가 Time FSessionRecord 공유** — 병렬화 한계 | LOW | LOW | 1-7은 1-2 이후로 스케줄. Time 001-003 → Save/Load 001-004 순차. Pipeline/Input은 독립 |

## Dependencies on External Factors

- **UE 5.6 + Visual Studio 2022** 설치 완료 (Windows 10/11)
- **`MadeByClaudeCode.uproject` 빌드 환경** — 이미 `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/` 스캐폴드 존재
- **GitHub Actions CI 워크플로우** — `.github/workflows/tests.yml` 이미 존재, Sprint 1 stories는 CI 녹색 유지 의무

## QA Plan

> ⚠️ **No QA Plan yet**: This sprint starts without a QA plan.
> 
> Recommend running `/qa-plan sprint` before starting implementation of the first story (1-1).
> 
> Sprint 1 stories' ADEQUATE verdict + Given/When/Then test case specs per story는 현재 각 story 파일의 `## QA Test Cases` 섹션에 placeholder로 있지만, **qa-lead 공식 승인 전 상태**. Production → Polish gate 진입 시 QA sign-off 필수.

## Definition of Done for this Sprint

- [ ] All Must Have tasks (1-1 ~ 1-13) completed — `/story-done` per story
- [ ] All tasks pass acceptance criteria
- [ ] QA plan exists (`production/qa/qa-plan-sprint-01.md`) — *권장: 구현 시작 전 작성*
- [ ] All Logic stories have passing unit tests in `tests/unit/[system]/` (UE Automation Framework, `-nullrhi` 헤드리스)
- [ ] All Integration stories have passing tests or playtest evidence
- [ ] Smoke check passed (`/smoke-check sprint`)
- [ ] QA sign-off report: APPROVED or APPROVED WITH CONDITIONS (`/team-qa sprint`)
- [ ] No S1 or S2 bugs in delivered features
- [ ] Design documents updated for any deviations
- [ ] Code reviewed and merged (solo — self-review with `/code-review`)
- [ ] CI grep hooks operational (start): `NO_FORWARD_TAMPER_DETECTION` + `NO_RAW_KEY_BINDING` placeholder

## Next Sprint Preview (Sprint 02 — 2026-05-05 to 2026-05-16)

Sprint 02 목표 예정: **Foundation 마감 + Core sprint 시작**
- Should/Nice Have 잔여 Foundation stories 완료 (Time 006-007, Pipeline 005-007, Save/Load 006-008)
- Core 시작: GSM Story 001-004 (EGameState + Subsystem + 상태 전환 테이블 + MPC Asset)

## Scope Check Reminder

> **Scope check**: 이 sprint는 epic 원본 범위 내 Must Have 13 stories만 포함 (Should/Nice는 stretch). Epic 밖 story 추가 없음. 필요 시 `/scope-check foundation-time-session` 또는 `/scope-check foundation-data-pipeline` 실행으로 검증.
