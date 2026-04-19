# Epic: Dream Trigger System

> **Layer**: Feature
> **GDD**: [design/gdd/dream-trigger-system.md](../../../design/gdd/dream-trigger-system.md)
> **Architecture Module**: Feature / Dream — 희소성 관리 (3-5회/21일) + Pillar 3 계약
> **Status**: Ready
> **Stories**: 7 stories created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Dream Trigger System은 Stage 2 `FOnCardProcessedByGrowth` 구독으로 최신 TagVector 보장된 상태에서 F1 TagScore 내적 평가 + 3중 희소성 게이트(MinDreamIntervalDays 냉각 + MaxDreamsPerPlaythrough cap + 태그 임계)로 꿈 언록 여부를 결정하는 내러티브 희소성 관리 시스템이다. Pillar 3("말할 때만 말한다") 수호의 수학적 증명 — 3-5회/21일(평균 4-7일 간격) 실현. Day 21 MinimumGuaranteedDreams fallback 강제(EC-1). CR-4 처리 완료 + SaveAsync(EDreamReceived) 완료 후 `FOnDreamTriggered(FName DreamId)` 발행 → GSM Waiting→Dream 전환 요청. Budget 획득 실패 시 `PendingDreamId` 영속화 + 로드 복구.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) | Dream Trigger는 Stage 2 `FOnCardProcessedByGrowth` 구독 (Stage 1 직접 구독 금지 — 최신 TagVector 보장) | LOW |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossDreamTriggerSettings` — MinDreamIntervalDays 등. `UDreamDataAsset` 예외 (per-dream 트리거 임계) | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-dream-001 | OQ-DTS-1 PendingDreamId retry UX (E5 Budget 실패 연기) | GDD §E5 ✅ |
| TR-dream-002 | Stage 2 구독 전환 (ADR-0005) — 최신 TagVector 보장 | ADR-0005 ✅ |
| TR-dream-003 | CR-1 atomic chain — FDreamState 1-3번 필드 갱신 + SaveAsync(EDreamReceived) | GDD contract ✅ |
| TR-dream-004 | Pillar 3 rarity gates: MinDreamIntervalDays + MaxDreamsPerPlaythrough + TriggerThreshold | GDD contract ✅ |
| TR-dream-005 | EC-1 Day 21 MinimumGuaranteedDreams fallback | GDD contract ✅ |
| TR-dream-006 | UDreamDataAsset C2 schema gate (트리거 임계 코드 리터럴 금지) | ADR-0002 ✅ |

## Key Interfaces

- **Publishes**: `FOnDreamTriggered(FName DreamId)`
- **Consumes**: `FOnCardProcessedByGrowth` (Stage 2), `FOnGameStateChanged` (Dream 상태 이탈 감지 — PendingDreamId 클리어), `Pipeline.GetAllDreamAssets`, `Pipeline.GetDreamAsset`, Growth `GetTagVector()` / `GetCurrentStage()`, Save/Load `FDreamState` 복원
- **Owned types**: `FDreamState` struct (UnlockedDreamIds, LastDreamDay, DreamsUnlockedCount, PendingDreamId), `UDreamDataAsset` (field semantic owner — data-pipeline source), internal state machine (Dormant/Evaluating/Triggered)
- **Settings**: `UMossDreamTriggerSettings` (MinDreamIntervalDays, MaxDreamsPerPlaythrough, MinimumGuaranteedDreams)
- **API**: evaluation pipeline (CR-1 ~ CR-5)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Dream Trigger Subsystem 골격 + Stage 2 구독 전용 | Logic | Ready | ADR-0005 |
| 002 | F1 TagScore 공식 + CR-3 후보 필터링 | Logic | Ready | ADR-0002 |
| 003 | CR-2 희소성 게이트 (Cooldown + Cap + Day 1 + Day 21 Guards) | Logic | Ready | ADR-0011 |
| 004 | CR-4 Dream Selection (argmax) + CR-1 Atomic Chain | Logic | Ready | ADR-0005 |
| 005 | PendingDreamId 영속화 + Load 복구 재발행 + OnGameStateChanged 클리어 | Integration | Ready | ADR-0005 |
| 006 | EC-1 Day 21 MinimumGuaranteedDreams Fallback | Integration | Ready | ADR-0005 |
| 007 | Edge Cases (Degraded Pipeline + F3 Frequency Target + Manual Playtest) | Logic | Ready | ADR-0011 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-DTS-01~12)
- AC-DTS-04 Day 21 Guaranteed-Dream force-trigger — `DreamsUnlockedCount < MinimumGuaranteedDreams` 시 강제
- EC-6 Farewell 상태에서 평가 skip 테스트
- EC-8 `GetAllDreamAssets()` 빈 배열 fallback 테스트
- F2 DaysSinceLastDream = CurrentDayIndex - LastDreamDay 경계 테스트
- Stage 2 구독 — `grep "OnCardOffered.AddDynamic" DreamTrigger*.cpp` = 0건 (ADR-0005)
- CR-1 atomic — FDreamState 갱신 + SaveAsync 동일 call stack (co_await/AsyncTask/Tick 분기 금지)
- UDreamDataAsset 트리거 임계 코드 리터럴 grep = 0건 (C2 schema gate)

## Next Step

Run `/create-stories feature-dream-trigger` to break this epic into implementable stories.
