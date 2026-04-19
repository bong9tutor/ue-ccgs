# Epic: Growth Accumulation Engine

> **Layer**: Feature
> **GDD**: [design/gdd/growth-accumulation-engine.md](../../../design/gdd/growth-accumulation-engine.md)
> **Architecture Module**: Feature / Growth — 태그 벡터 집적 + 단계 평가 + Day 21 Final Form 결정
> **Status**: Ready
> **Stories**: 9 stories created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Growth Accumulation Engine은 매일 건네진 카드의 태그(`TArray<FName>`)를 누적 벡터(`TMap<FName, float>`)로 집적하여 성장 단계 전환(Sprout → Growing → Mature)과 Day 21 최종 형태 결정을 담당하는 순수 데이터 처리 시스템이다. Stage 1 `FOnCardOffered` 구독 → CR-1 atomic chain(태그 가산 → LastCardDay → TotalCardsOffered → MaterialHints → CR-4 단계 평가 → CR-5 Day 21 EvaluateFinalForm → SaveAsync → Stage 2 `FOnCardProcessedByGrowth` broadcast). Stage 2 발행을 handler의 마지막 statement로 배치하여 C++ call stack 순서로 Day 21 race 해소(ADR-0005). `UMossFinalFormAsset` per-form 자산으로 F3 Score 내적 계산(ADR-0010).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) | Growth는 Stage 1 subscriber + Stage 2 publisher. Handler 마지막 statement로 `OnCardProcessedByGrowth.Broadcast` | LOW |
| [ADR-0010](../../../docs/architecture/adr-0010-ffinalformrow-storage-format.md) | FinalForm = `UMossFinalFormAsset : UPrimaryDataAsset` per-form (TMap 편집 위해) | LOW |
| [ADR-0012](../../../docs/architecture/adr-0012-growth-getlastcardday-api.md) | `int32 GetLastCardDay() const` — sentinel 0 = 미건넴 | LOW |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossGrowthSettings` — GrowingDayThreshold, MatureDayThreshold. `UGrowthConfigAsset` 예외 (per-content 가중치) | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-growth-001 | CR-1 atomicity — step 2-6 동일 call stack (AC-GAE-02) | GDD contract ✅ |
| TR-growth-002 | CR-8 `FFinalFormRow` storage format | ADR-0010 ✅ |
| TR-growth-003 | OQ-CS-2 `GetLastCardDay()` API 형태 | ADR-0012 ✅ |
| TR-growth-004 | Stage 2 `FOnCardProcessedByGrowth` publisher (ADR-0005) | ADR-0005 ✅ |
| TR-growth-005 | F3 Score 내적 + argmax(Score) → FinalFormId 결정 (CR-5) | GDD contract ✅ |
| TR-growth-006 | AC-GAE-06 Day 21 Final Form 결정 정확성 | ADR-0005 + ADR-0010 ✅ |

## Key Interfaces

- **Publishes**: `FOnCardProcessedByGrowth(const FGrowthProcessResult&)` (Stage 2), `FOnGrowthStageChanged(EGrowthStage)`, `FOnFinalFormReached(FName)`
- **Consumes**: `FOnCardOffered` (Stage 1), `FOnDayAdvanced`, `Pipeline.GetCardRow`, `Pipeline.GetGrowthFormRow` (legacy api — actually `GetFinalFormAsset`), Save/Load `FGrowthState` 영속화
- **Owned types**: `FGrowthState` struct, `FGrowthProcessResult`, `FGrowthMaterialHints`, `EGrowthStage`, `UGrowthConfigAsset` (CategoryWeightMap 등)
- **Settings**: `UMossGrowthSettings`
- **API**: `GetLastCardDay() const -> int32`, `GetMaterialHints() const -> FGrowthMaterialHints`, `EvaluateFinalForm() -> FName` (CR-5)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Growth Subsystem 골격 + State Machine 초기화 | Logic | Ready | ADR-0011 |
| 002 | CR-1 Atomic Chain — 태그 가산 + LastCardDay + SaveAsync | Logic | Ready | ADR-0005 |
| 003 | CR-4 성장 단계 평가 + FOnDayAdvanced 구독 | Logic | Ready | ADR-0011 |
| 004 | CR-5 Day 21 Final Form 결정 + F2/F3 공식 | Logic | Ready | ADR-0010 |
| 005 | Stage 2 FOnCardProcessedByGrowth 발행 + Day 21 통합 시퀀스 | Integration | Ready | ADR-0005 |
| 006 | CR-6 MaterialHints 계산 + GetMaterialHints() Pull API | Logic | Ready | ADR-0005 |
| 007 | GetLastCardDay() int32 Sentinel Pull API | Logic | Ready | ADR-0012 |
| 008 | FGrowthState 영속화 + Load 복구 (EC-17 재발행) | Integration | Ready | ADR-0009 |
| 009 | Schema Guards + Edge Cases (CR-3 빈 Tags / CR-8 validator / EC coverage) | Logic | Ready | ADR-0010 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-GAE-01~15)
- AC-GAE-02 CR-1 step 2-6 동일 call stack — `co_await`/`AsyncTask`/`Tick` 분기 0건 grep
- AC-GAE-06 Day 21 Final Form 결정 — 3종 FinalForm mock으로 태그 벡터 → 정확한 argmax
- AC-GAE-XX Stage 2 발행이 handler 마지막 statement grep 검증
- EC-6 Resolved 상태에서 모든 이벤트 무시 테스트
- EC-5 `FOnCardOffered` + `FOnDayAdvanced` 동일 프레임 순서 테스트 (CR-1 먼저, CR-4 나중)
- `GetLastCardDay() = 0` sentinel 앱 재시작 복원 테스트

## Next Step

Run `/create-stories feature-growth-accumulation` to break this epic into implementable stories.
