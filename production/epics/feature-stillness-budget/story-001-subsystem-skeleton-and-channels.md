# Story 001: Stillness Budget Subsystem 골격 + 채널/우선순위 Enum

> **Epic**: feature-stillness-budget
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 1.5시간

## Context

- **GDD Reference**: `design/gdd/stillness-budget.md` §Rule 1 (순수 쿼리 서비스) + §Rule 2 (채널 정의) + §Rule 3 (우선순위)
- **TR-ID**: TR-stillness-002 (RAII + idempotent Release — prerequisite)
- **Governing ADR**: [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) (`UStillnessBudgetAsset` 예외 유지 — per-scene DataAsset)
- **Engine Risk**: LOW
- **Engine Notes**: `UGameInstanceSubsystem` 상태 머신 아님 — 순수 쿼리 서비스. `UStillnessBudgetAsset` (ADR-0011 예외) — single instance PrimaryDataAsset.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — "Stillness Budget ↔ Engine concurrency Hybrid" (본 story는 game-layer만)
  - Cross-Layer §Exceptions (UPrimaryDataAsset 유지) — "`UStillnessBudgetAsset` 유지"

## Acceptance Criteria

1. `UStillnessBudgetSubsystem : UGameInstanceSubsystem` 클래스 정의 + `Source/MadeByClaudeCode/Stillness/` 배치
2. `EStillnessChannel { Motion, Particle, Sound }` enum (3종 — 채널 간 독립)
3. `EStillnessPriority { Background = 0, Standard = 1, Narrative = 2 }` enum
4. `UStillnessBudgetAsset : UPrimaryDataAsset` 정의 — `MotionSlots (2)`, `ParticleSlots (2)`, `SoundSlots (3)`, `bReducedMotionEnabled (false)` — 모두 `UPROPERTY(EditAnywhere)`, 코드 리터럴 금지 (AC-SB-10)
5. `Initialize()` — Data Pipeline 의존 선언 + `GetStillnessBudgetAsset()` 1회 pull + 캐싱
6. DegradedFallback 처리 (Rule 7): Asset == nullptr → Unavailable 모드 진입 (Story 003 detail)
7. 활성 슬롯 집계 내부 구조: 채널별 `TArray<FActiveSlot>` — priority + handle id 보유

## Implementation Notes

- **채널 독립성 (Rule 2)**: 3 채널은 독립 `TArray<FActiveSlot>` — 슬롯 공유 없음
- **순수 쿼리 서비스 (Rule 1)**: 내부 상태(Running, Paused 등) 갖지 않음
- **ADR-0011 예외**: `UStillnessBudgetAsset`은 GDD가 명시한 자산 형태 유지 (per-scene editable), `UDeveloperSettings` 전환은 미래 revision 검토
- AC-SB-10 (CODE_REVIEW): 하드코딩된 슬롯 수 리터럴 금지 — 반드시 UPROPERTY 참조

## Out of Scope

- Request/Release API (Story 002)
- RAII Handle (Story 002)
- 선점 로직 (Story 003)
- Reduced Motion API (Story 004)
- OnBudgetRestored delegate (Story 005)
- **`SC_MossBaby_Stillness` USoundConcurrency 자산 생성 — VS Audio System sprint로 연기 (ADR-0007)**
- **Niagara Effect Type 3종 자산 (EFT_MossBaby_Background/Standard/Narrative) — VS sprint로 연기 (ADR-0007)**

## QA Test Cases

**Given** Fresh GameInstance, **When** Stillness Subsystem Initialize + Pipeline GetStillnessBudgetAsset 성공, **Then** Normal 모드 진입, 채널별 빈 슬롯 배열 초기화.

**Given** Pipeline DegradedFallback (GetStillnessBudgetAsset == nullptr), **When** Initialize, **Then** Unavailable 모드 진입, `UE_LOG(Warning)` 1회.

**Given** Stillness 구현 소스, **When** `grep -rnE "= [0-9]+" Source/MadeByClaudeCode/Stillness/*.cpp` (슬롯 리터럴 검색), **Then** `UStillnessBudgetAsset` UPROPERTY 참조만 (AC-SB-10).

## Test Evidence

- **Unit test**: `tests/unit/Stillness/test_subsystem_initialize.cpp`
- **CODE_REVIEW grep**: AC-SB-10 literal check

## Dependencies

- `foundation-data-pipeline` (UStillnessBudgetAsset registration + pull API)
