# Epic: Game State Machine (+ Visual State Director)

> **Layer**: Core
> **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md)
> **Architecture Module**: Core / GSM — 6-state + MPC orchestration + compound event boundary
> **Status**: Ready
> **Stories**: 12 stories — see Stories table
> **Manifest Version**: 2026-04-19

## Overview

Game State Machine은 6 GameState(Menu → Dawn → Offering → Waiting → Dream → Farewell)를 중앙에서 관리하며 Visual State Director 책임(Lumen/Niagara/Light 파라미터의 MPC orchestration)을 흡수한다(R2). Rule 3의 8개 MPC 파라미터 목표값과 Rule 4의 SmoothStep blend duration을 TickMPC 내 순차 갱신(ADR-0004 Hybrid — MPC scalar + Light Actor 직접 구동). Day 21 순서 보장을 위해 Stage 2 `FOnCardProcessedByGrowth` 구독(Stage 1 직접 구독 금지). Day 13 compound event 원자성을 `BeginCompoundEvent/EndCompoundEvent` boundary로 구현(ADR-0009). `Farewell → Any` 전환 분기 금지(Pillar 4).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0004](../../../docs/architecture/adr-0004-mpc-light-actor-sync.md) | Hybrid — GSM TickMPC에서 MPC + DirectionalLight/SkyLight 직접 구동. Lumen GI 반영 | HIGH (Lumen HWRT GPU 5ms budget) |
| [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) | GSM은 Stage 2 `FOnCardProcessedByGrowth`만 구독. Day 21 `bIsDay21Complete=true`면 Farewell P0 즉시 | LOW |
| [ADR-0009](../../../docs/architecture/adr-0009-compound-event-sequence-atomicity.md) | `BeginCompoundEvent/EndCompoundEvent` API 제공 — Save/Load coalesce 활용 | LOW |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossGameStateSettings` — BlendDurationBias, DawnMinDwellSec, CardSystemReadyTimeoutSec, WitheredThresholdDays 등. MPC asset은 `UGameStateMPCAsset` 예외 | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-gsm-001 | OQ-6 BLOCKING — MPC ↔ Light Actor 동기화 | ADR-0004 ✅ |
| TR-gsm-002 | MPC writer uniqueness (AC-LDS-06) | ADR-0004 + architecture §Cross-Layer ✅ |
| TR-gsm-003 | `bWithered` persistence + MPC path (E18 — vocabulary: MBC CR-5 "Quiet Rest") | GDD contract ✅ |
| TR-gsm-004 | E12 Time vs Load ordering (AC-GSM-15) | GDD contract ✅ |
| TR-gsm-005 | AC-GSM-05 `Farewell → Any` 전환 grep 0건 | GDD contract ✅ |
| TR-gsm-006 | AC-GSM-16 Day 21 Farewell P0 전환 순서 | ADR-0005 ✅ |

## Key Interfaces

- **Publishes**: `FOnGameStateChanged(OldState, NewState)`, `FOnFarewellReached(Reason)` (re-emit)
- **Consumes**: `FOnTimeActionResolved`, `FOnCardProcessedByGrowth` (Stage 2), `FOnLoadComplete`, `FOnDreamTriggered`
- **Owned types**: `EGameState`, `UGameStateMPCAsset` (per-state target values), `bCompoundEventActive` flag
- **Settings**: `UMossGameStateSettings`
- **API**: `RegisterSceneLights(ADirectionalLight*, ASkyLight*)`, `BeginCompoundEvent(ESaveReason)`, `EndCompoundEvent()`, `RequestStateTransition(EGameState, EPriority)`

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [`EGameState` enum + `UMossGameStateSubsystem` 골격](story-001-egamestate-enum-subsystem-skeleton.md) | Logic | Ready | ADR-0004 |
| 002 | [상태 전환 테이블 + 4단계 우선순위 시스템](story-002-transition-table-priority-system.md) | Logic | Ready | — (GDD contract) |
| 003 | [`UGameStateMPCAsset` + `FTSTicker` MPC Lerp 루프](story-003-mpc-asset-ftsticker-loop.md) | Logic | Ready | ADR-0004 |
| 004 | [`FOnGameStateChanged` Delegate 브로드캐스트 + Stage 2 구독](story-004-fongamestatechanged-delegate-broadcast.md) | Integration | Ready | ADR-0005 |
| 005 | [`RegisterSceneLights` API + DirectionalLight/SkyLight 구동](story-005-register-scene-lights-directional-skylight.md) | Integration | Ready | ADR-0004 |
| 006 | [6-State Entry/Exit Actions + `DawnMinDwellSec` Guard](story-006-entry-exit-actions-dawn-offering-waiting.md) | Logic | Ready | ADR-0011 |
| 007 | [Edge Cases E1-E10: ReducedMotion / Dream Defer / Blend Interrupts](story-007-edge-cases-reducedmotion-dream-defer.md) | Integration | Ready | ADR-0004 |
| 008 | [Withered Lifecycle + `FSessionRecord.bWithered` 영속화](story-008-withered-lifecycle-persistence.md) | Integration | Ready | ADR-0005 |
| 009 | [Narrative Overlay (Rule 5 `ADVANCE_WITH_NARRATIVE`)](story-009-narrative-overlay-advance-with-narrative.md) | Integration | Ready | — (GDD contract) |
| 010 | [`FOnLoadComplete` 분기 + E11/E12/E14/E15 Load Edge Cases](story-010-load-branch-menu-dawn-waiting-farewell.md) | Integration | Ready | — (GDD contract) |
| 011 | [Compound Event Boundary (`BeginCompoundEvent`/`EndCompoundEvent`)](story-011-compound-event-boundary-day-13.md) | Integration | Ready | ADR-0009 |
| 012 | [Visual Acceptance Manual Sign-off (AC-GSM-19)](story-012-visual-acceptance-manual-signoff.md) | Visual/Feel | Ready | ADR-0004 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-GSM-01~21)
- AC-LDS-04 [5.6-VERIFY] Lumen GPU ≤ 5ms budget 6 상태 각각 실측
- AC-LDS-06 grep — 씬 코드의 `SetScalarParameterValue`/`SetVectorParameterValue` = 0건
- AC-LDS-07 6 상태 시각 구분 QA 리드 sign-off (MANUAL)
- AC-LDS-08 Waiting→Dream frame delta ≤ 100K/frame @60fps (AUTOMATED)
- AC-GSM-05 `Farewell → Any` 전환 분기 grep = 0건
- AC-GSM-08 mid-transition restart 결정성 테스트
- AC-GSM-20/21 Withered Lifecycle 테스트 (vocabulary: MBC CR-5 "Quiet Rest" 준수)

## Next Step

Run `/create-stories core-game-state-machine` to break this epic into implementable stories.
