# Story 003: CR-4 성장 단계 평가 + FOnDayAdvanced 구독

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §CR-4 + AC-GAE-04/05/16/17
- **TR-ID**: TR-growth-001 (단계 평가 경로)
- **Governing ADR**: [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) (Threshold knob)
- **Engine Risk**: LOW
- **Engine Notes**: Time System `FOnDayAdvanced(int32 NewDayIndex)` 구독. 동일 프레임 `FOnCardOffered` + `FOnDayAdvanced` 도착 시 CR-1 먼저, CR-4 나중 (EC-5).
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — Growth는 `FOnDayAdvanced` 구독 (Time → Growth 직접)
  - Global Rules §Naming Conventions — `FOnGrowthStageChanged(EGrowthStage)` delegate `F` + `On` prefix

## Acceptance Criteria

1. **AC-GAE-04** (AUTOMATED/BLOCKING) — Accumulating / Sprout + `GrowingDayThreshold=3`, `FOnDayAdvanced(3)` 수신 → `CurrentStage==Growing`, `FOnGrowthStageChanged(Growing)` 정확히 1회 발행
2. **AC-GAE-05** (AUTOMATED/BLOCKING) — Sprout + 이전 `FOnDayAdvanced` 미수신, `FOnDayAdvanced(16)` 수신 (다단계 건너뛰기) → `CurrentStage==Mature`, `FOnGrowthStageChanged` Mature 1회만 (Growing 미발행)
3. **AC-GAE-16** (AUTOMATED/BLOCKING) — 동일 프레임 `FOnCardOffered(Spring)` → `FOnDayAdvanced(3)` → CR-1 먼저 (`TagVector["Season.Spring"]==1.0f`), CR-4 나중 (`CurrentStage==Growing`, 1회 발행)
4. **AC-GAE-17** (AUTOMATED/BLOCKING) — Resolved 상태에서 `FOnDayAdvanced` 수신 시 CR-4 미실행, 추가 이벤트 미발행
5. 동일 단계로의 전환 시도 (예: Sprout → Sprout) → `FOnGrowthStageChanged` 미발행

## Implementation Notes

- `FOnGrowthStageChanged(EGrowthStage NewStage)` delegate 선언 — `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam`
- CR-4 단순 lookup 테이블 (임계값 ≤ 현재 DayIndex 중 최대 단계):
  - `DayIndex >= GameDurationDays` → Mature (FinalForm 전환은 CR-5 별도)
  - `DayIndex >= MatureDayThreshold` → Mature
  - `DayIndex >= GrowingDayThreshold` → Growing
  - else → Sprout
- **다단계 건너뛰기** (EC-10): 중간 단계를 순회하지 않고 `EvaluateTargetStage(DayIndex)` 1회 호출 → 결과가 CurrentStage와 다르면 1회 발행
- **동일 프레임 순서 보장**: Time 시스템이 `FOnDayAdvanced` 발행 시점은 MBC/Card 처리와 독립. Growth 내부 `OnDayAdvancedHandler`는 CR-1과 배타적 — handler 재진입 없음 (EC-5는 자연스럽게 처리됨)
- EC-6/7 guard: `if (InternalState != Accumulating) return;`
- Thresholds는 `UMossGrowthSettings` (ADR-0011 — Story 001에서 정의)

## Out of Scope

- CR-5 Day 21 Final Form 결정 (Story 004)
- CR-1 태그 가산 (Story 002 — 의존)
- MVP 스케일 F5 day scaling (configuration만, 이 story에서는 thresholds를 읽기만)

## QA Test Cases

**Given** Sprout + `GrowingDayThreshold=3`, **When** `FOnDayAdvanced(2)` 수신, **Then** `CurrentStage==Sprout`, 이벤트 미발행.

**Given** Sprout + `GrowingDayThreshold=3`, **When** `FOnDayAdvanced(3)`, **Then** `CurrentStage==Growing`, `FOnGrowthStageChanged(Growing)` 1회.

**Given** Sprout + 이전 DayAdvanced 미수신, **When** `FOnDayAdvanced(16)` (다단계 점프), **Then** `CurrentStage==Mature`, `FOnGrowthStageChanged(Mature)` 1회, Growing 이벤트 미발행.

**Given** 동일 프레임 `FOnCardOffered(Spring) → FOnDayAdvanced(3)`, **When** Growth 처리, **Then** TagVector에 Spring 포함 + CurrentStage==Growing (CR-1 먼저, CR-4 나중).

**Given** Resolved 상태, **When** `FOnDayAdvanced(16)`, **Then** CurrentStage 불변, 이벤트 미발행 (EC-7).

## Test Evidence

- **Unit test**: `tests/unit/Growth/test_cr4_stage_evaluation.cpp`
- Multi-stage jump 테스트: AC-GAE-05
- Same-frame ordering: AC-GAE-16

## Dependencies

- Story 001 (Subsystem 골격)
- Story 002 (CR-1 — 동일 프레임 순서 검증용)
- `foundation-time-session` (FOnDayAdvanced publisher)
