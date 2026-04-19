# Epic: Card System

> **Layer**: Feature
> **GDD**: [design/gdd/card-system.md](../../../design/gdd/card-system.md)
> **Architecture Module**: Feature / Card — 일일 3장 제시 + `FOnCardOffered` 발행 + 시즌 가중치 샘플링
> **Status**: Ready
> **Stories**: 7 stories created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Card System은 `FOnDayAdvanced` 수신 즉시 Eager 샘플링으로 일일 3장 카드 풀을 구성(ADR-0006)하고, Card Hand UI의 `ConfirmOffer(FName)` 호출로 Stage 1 `FOnCardOffered` 이벤트를 발행하는 단일 책임 시스템이다. Drag-to-offer 입력 로직은 Card Hand UI 소유(R2 재정의). `FRandomStream` 외부 주입 + `HashCombine(PlaythroughSeed, DayIndex)`로 결정적 샘플링. F-CS-1(계절 결정) + F-CS-2(카드 가중치) + F-CS-3(확률) 공식. C1 schema gate로 `FGiftCardRow`에 수치 효과 필드 금지(Pillar 2). Day 21 통합 AC-CS-16 + FGuid→FName 마이그레이션 완료(AC-CS-17).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) | Card가 Stage 1 `FOnCardOffered` 유일 publisher. Stage 2는 Growth 소유 | LOW |
| [ADR-0006](../../../docs/architecture/adr-0006-card-pool-regeneration-timing.md) | Eager — `FOnDayAdvanced` 수신 즉시 샘플링 + Ready 상태 진입 | LOW |
| [ADR-0012](../../../docs/architecture/adr-0012-growth-getlastcardday-api.md) | `bHandOffered` 복원에 Growth `GetLastCardDay()` 사용 | LOW |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossCardSystemSettings` — MaxCardsPerDay 등. `UCardSystemConfigAsset` 시즌 가중치는 예외 (per-content) | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-card-001 | OQ-CS-3 `FOnCardOffered` 순서 보장 (Day 21 race) | ADR-0005 ✅ |
| TR-card-002 | OQ-CS-2 `GetLastCardDay()` API 형태 | ADR-0012 ✅ |
| TR-card-003 | OQ-CS-5 카드 풀 Eager vs Lazy | ADR-0006 ✅ |
| TR-card-004 | AC-CS-17 FGuid → FName 마이그레이션 (CODE_REVIEW) | GDD cross-review D-1 ✅ |
| TR-card-005 | CR-CS-4 `FOnCardOffered` 하루 최대 1회 (EC-CS-9 이중 호출 방지) | GDD contract ✅ |
| TR-card-006 | F-CS-3 시즌 가중치 확률 수렴 (±5%p 기대값) | GDD contract ✅ |

## Key Interfaces

- **Publishes**: `FOnCardOffered(FName CardId)` (Stage 1)
- **Consumes**: `FOnDayAdvanced`, `Pipeline.GetCardRow`, `Pipeline.GetAllCardIds`, GSM `OnPrepareRequested()`
- **Owned types**: `ECardSystemState` (Uninitialized/Preparing/Ready/Offered), `UCardSystemConfigAsset` (시즌 가중치), `FGiftCardRow` (field 의미 owner — data-pipeline.md forward-declare)
- **Settings**: `UMossCardSystemSettings`
- **API**: `GetDailyHand() -> TArray<FName>`, `ConfirmOffer(FName) -> bool`, `OnCardSystemReady(bool bDegraded)` (GSM 응답)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Card Subsystem 골격 + State Machine + FOnCardOffered 정의 | Logic | Ready | ADR-0005 |
| 002 | Eager 샘플링 + F-CS-1 계절 + F-CS-2 가중치 | Logic | Ready | ADR-0006 |
| 003 | ConfirmOffer 검증 게이트 + Stage 1 Broadcast | Logic | Ready | ADR-0005 |
| 004 | bHandOffered 복원 (GetLastCardDay sentinel) | Logic | Ready | ADR-0012 |
| 005 | GSM Prepare/Ready 프로토콜 + Degraded Fallback | Integration | Ready | ADR-0006 |
| 006 | Day 21 통합 (AC-CS-16) + FName/FGuid 계약 (AC-CS-17) | Integration | Ready | ADR-0005 |
| 007 | 동일 프레임 + 멀티데이 + 재생성 가드 Edge Cases | Logic | Ready | ADR-0006 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-CS-01~21)
- AC-CS-16 INTEGRATION — Day 21 순서 (Growth CR-1 먼저, GSM Farewell 나중) 타임스탬프 검증
- AC-CS-17 CODE_REVIEW — `FGuid` grep in Card/Growth/GSM 파일 = 0건
- AC-CS-15b AUTOMATED — 10,000회 샘플링 확률 ±5%p 수렴 (고정 시드 `PlaythroughSeed=12345`)
- EC-CS-9 이중 `ConfirmOffer` 거부 + `FOnCardOffered` 1회 발행 테스트
- EC-CS-13 `bHandOffered` 복원 — `GetLastCardDay() == CurrentDayIndex` 경로
- `FGiftCardRow` 수치 효과 필드 grep = 0건 (C1 schema gate)
- Eager 샘플링 < 1ms Full 40장 (ADR-0006)

## Next Step

Run `/create-stories feature-card-system` to break this epic into implementable stories.
