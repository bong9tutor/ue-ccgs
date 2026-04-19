# Epic: Stillness Budget

> **Layer**: Feature
> **GDD**: [design/gdd/stillness-budget.md](../../../design/gdd/stillness-budget.md)
> **Architecture Module**: Feature / Stillness — 동시 이벤트 상한 single source + Reduced Motion 공개 API
> **Status**: Ready
> **Stories**: 7 stories created (2026-04-19). Story 007 (ADR-0007 Hybrid) Deferred to VS.
> **Manifest Version**: 2026-04-19

## Overview

Stillness Budget은 3 채널(Motion/Particle/Sound) × 3 우선순위(Background/Standard/Narrative)로 동시 시각·청각 이벤트 상한을 정의·강제하는 단일 출처다. RAII `FStillnessHandle` 반환 + 우선순위 선점 + `OnBudgetRestored` 이벤트로 복귀 트리거. Reduced Motion toggle을 공개 API로 제공(Accessibility MVP 범위 유지). Hybrid 엔진 통합(ADR-0007): 게임 레이어 truth + 엔진 `USoundConcurrency`(MaxCount=32) + Niagara Effect Type 3종(Background/Standard/Narrative)은 hardware-level safety net. Pillar 1("조용한 존재") 장기 수호자 — "이상하게 조용하다"는 감각의 수학적 보증.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0007](../../../docs/architecture/adr-0007-stillness-budget-engine-concurrency-integration.md) | Hybrid — Stillness Budget = single source (game layer) + `SC_MossBaby_Stillness`(MaxCount=32) + Niagara Effect Type 3종 (hardware safety net). VS Audio System sprint 진입 시 구현 | LOW (VS 전까지 game-layer only) |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UStillnessBudgetAsset` (per-scene) 유지 — UDeveloperSettings 예외. knob은 asset 내부 | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-stillness-001 | OQ-1 Sound Concurrency + Niagara Scalability 통합 | ADR-0007 (VS sprint 진입 시 구현) ✅ |
| TR-stillness-002 | RAII `FStillnessHandle` + idempotent Release | GDD contract ✅ |
| TR-stillness-003 | 우선순위 선점 (Narrative > Standard > Background) + OnPreempted delegate | GDD contract ✅ |
| TR-stillness-004 | Rule 6 Reduced Motion 공개 API (`IsReducedMotion()`) | GDD contract ✅ |
| TR-stillness-005 | OnBudgetRestored per-channel delegate (Motion/Particle, Sound 제외) | GDD contract ✅ |

## Key Interfaces

- **Publishes**: `OnBudgetRestored(EStillnessChannel)`, `OnPreempted(FStillnessHandle)`
- **Consumes**: `Pipeline.GetStillnessBudgetAsset()` (Initialize 1회 pull + 캐싱)
- **Owned types**: `EStillnessChannel` (Motion/Particle/Sound), `EStillnessPriority` (Background/Standard/Narrative), `FStillnessHandle` (Move-only RAII), `UStillnessBudgetAsset` (field semantic owner — data-pipeline source)
- **Settings**: 없음 (콘텐츠 자산 `UStillnessBudgetAsset`이 knob 소유)
- **API**: `Request(EStillnessChannel, EStillnessPriority) -> FStillnessHandle`, `Release(FStillnessHandle)`, `IsReducedMotion() -> bool`, `SetReducedMotion(bool)`

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Stillness Budget Subsystem 골격 + 채널/우선순위 Enum | Logic | Ready | ADR-0011 |
| 002 | Request/Release API + RAII FStillnessHandle (Move-only) | Logic | Ready | — |
| 003 | 우선순위 선점 + OnPreempted Delegate + 재진입 가드 | Logic | Ready | — |
| 004 | Reduced Motion API + F2 EffLimit + Rule 6 | Logic | Ready | — |
| 005 | OnBudgetRestored Per-Channel Delegate (E3) | Logic | Ready | — |
| 006 | DegradedFallback Unavailable 모드 + Log Throttle | Logic | Ready | — |
| 007 | ADR-0007 Hybrid Integration — VS Deferred Stub | Integration | Deferred | ADR-0007 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증
- Motion/Particle/Sound 채널 독립 슬롯 카운팅 테스트
- Narrative > Standard > Background 선점 우선순위 테스트 + OnPreempted delegate 검증
- `FStillnessHandle` RAII 소멸자 자동 Release 테스트
- `IsReducedMotion()` toggle 시 per-channel `OnBudgetRestored` 발행 (Sound 제외) 테스트
- Dream Journal UI priority=Standard 일관성 (stillness-budget.md:296 수정됨)
- VS Audio System 진입 전까지는 Sound 채널 slot counting만 구현, `SC_MossBaby_Stillness` USoundConcurrency 자산은 VS sprint에서 추가

## Next Step

Run `/create-stories feature-stillness-budget` to break this epic into implementable stories.
