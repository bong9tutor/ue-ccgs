# Story 006: Day 21 통합 (AC-CS-16) + FName/FGuid 계약 (AC-CS-17)

> **Epic**: feature-card-system
> **Layer**: Feature
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3시간

## Context

- **GDD Reference**: `design/gdd/card-system.md` §EC-CS-15/16/17 + AC-CS-16/17 + §CR-CS-6 C1 schema guard (AC-CS-10)
- **TR-ID**: TR-card-001 (Day 21 ordering), TR-card-004 (FGuid → FName migration)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) — **핵심 Day 21 통합 story**
- **Engine Risk**: LOW
- **Engine Notes**: UE Multicast Delegate 등록 순서 비공개 계약 — ADR-0005 Stage 2 패턴으로 회피. Growth + Card + GSM 3 epic 교차.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — "2단계 Delegate 패턴" (Stage 1/Stage 2 역할 분리)
  - Core Layer §Required Patterns — "GSM이 Offering→Waiting / Farewell P0 전환은 `FOnCardProcessedByGrowth` (Stage 2)만 트리거"
  - Core Layer §Forbidden Approaches — "Never subscribe GSM to Stage 1 `FOnCardOffered`" (ADR-0005 Migration 완료)

## Acceptance Criteria

1. **AC-CS-16** (INTEGRATION/BLOCKING) — DayIndex=21, Offering 상태, Growth가 `FOnCardOffered` 구독(Stage 1), GSM은 `FOnCardProcessedByGrowth` 구독(Stage 2), `ConfirmOffer(CardId)` → Stage 1 발행 → 처리 순서 (C++ 호출 스택으로 강제):
   1. Growth CR-1 태그 가산 + CR-5 EvaluateFinalForm
   2. `FOnFinalFormReached(FormId)` 발행
   3. `SaveAsync(ECardOffered)`
   4. **Stage 2 `FOnCardProcessedByGrowth.Broadcast(Result)`**
   5. GSM Farewell P0 전환
   - `FOnDayAdvanced` Farewell 후 미발행. *ADR-0005 BLOCKER 해소 2026-04-19*
2. **AC-CS-17** (CODE_REVIEW/BLOCKING) — Card System, Growth, GSM 구현 파일에서 CardId 컨텍스트 `FGuid` grep → 매치 0건, FName만 사용
3. **AC-CS-10** (CODE_REVIEW/BLOCKING) — `FGiftCardRow` 구조체 정의 파일에서 `int32`/`float`/`double` 수치 효과 필드 grep = 0건, `FOnCardOffered` 시그니처 `FName` 단일 인자
4. EC-CS-15 — Day 21 정상 건넴: F-CS-1 → Winter, `ConfirmOffer` 정상, FOnCardOffered 발행 → Growth CR-1 → Stage 2 → GSM Farewell P0, FOnDayAdvanced Farewell 후 미발행
5. EC-CS-16 — Day 21 Offering 중 LONG_GAP_SILENT 수신 (Time System): GSM E7 Card Hand UI 강제 Hide → Farewell P0, FOnCardOffered 미발행, 21일차 태그 없이 최종 형태 결정

## Implementation Notes

- **Day 21 integration 테스트는 3 epic 연동**:
  - feature-card-system: ConfirmOffer → FOnCardOffered 발행 (본 epic)
  - feature-growth-accumulation Story 005: Stage 2 발행 시퀀스
  - core-game-state-machine: FOnCardProcessedByGrowth 구독자 + Farewell P0 전환
- **timestamp 검증 방식** (ADR-0005 §Validation Criteria):
  - `FPlatformTime::Seconds()`로 각 단계 스탬프
  - Growth CR-1 start < FOnFinalFormReached < SaveAsync < Stage 2 broadcast < GSM Farewell 진입 순서 검증
- **AC-CS-17 grep** (FGuid migration 2026-04-18 RESOLVED):
  ```bash
  grep -rE "FGuid.*CardId|CardId.*FGuid" Source/MadeByClaudeCode/{Card,Growth,GameState}/
  # 매치 0건 보장
  ```
- **AC-CS-10 C1 schema guard**: `FGiftCardRow`는 허용 타입만 (FName, TArray<FName>, FText, FSoftObjectPath) — Control Manifest Foundation Layer §Forbidden
- **EC-CS-17 RESOLVED by ADR-0005**: 본 story는 ADR-0005 통합 검증

## Out of Scope

- GSM Farewell P0 전환 구현 (core-game-state-machine)
- Growth Stage 2 broadcast (feature-growth-accumulation Story 005)
- LONG_GAP_SILENT 처리 (foundation-time-session)

## QA Test Cases

**Given** DayIndex=21, Offering, Card+Growth+GSM 3 subsystem 실제 결합, **When** `ConfirmOffer("Card_Day21")`, **Then** 5단계 순서 timestamps 검증 (Growth CR-1 < FOnFinalFormReached < SaveAsync < Stage 2 < Farewell 진입), `FSessionRecord.FinalFormId` 저장 완료 후 Farewell commit (AC-CS-16).

**Given** Card/Growth/GSM 구현 소스, **When** `grep "FGuid" Source/MadeByClaudeCode/{Card,Growth,GameState}/`, **Then** CardId 컨텍스트 0건 (AC-CS-17).

**Given** `FGiftCardRow` 정의 파일, **When** `grep -E "int32|float|double" FGiftCardRow`, **Then** 수치 효과 필드 0건 (AC-CS-10 C1).

**Given** Day 21 Offering 중 LONG_GAP_SILENT, **When** GSM E7 발동, **Then** Card Hand UI Hide, FOnCardOffered 미발행, 21일차 태그 미반영 최종 형태 결정 (EC-CS-16).

## Test Evidence

- **Integration test**: `tests/integration/Day21Sequence/test_card_to_growth_to_gsm.cpp` (AC-CS-16)
- **CODE_REVIEW grep**: 
  - `grep -rE "FGuid.*CardId" Source/MadeByClaudeCode/` = 0건 (AC-CS-17)
  - FGiftCardRow 수치 필드 grep = 0건 (AC-CS-10)
- timestamp 로그 분석 스크립트

## Dependencies

- Story 001-005 (Card System 전체)
- `feature-growth-accumulation` Story 002, 004, 005 (CR-1, CR-5, Stage 2 broadcast)
- `core-game-state-machine` Story (Stage 2 subscriber + Farewell P0)
- `foundation-time-session` (LONG_GAP_SILENT for EC-CS-16)
