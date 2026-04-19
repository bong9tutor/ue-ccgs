# Story 007: 동일 프레임 + 멀티데이 + 재생성 가드 Edge Cases

> **Epic**: feature-card-system
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 1.5시간

## Context

- **GDD Reference**: `design/gdd/card-system.md` §CR-CS-2 (재생성 조건) + EC-CS-3/6/10/11/19 + AC-CS-03/13b
- **TR-ID**: TR-card-003 (Eager 재생성 가드)
- **Governing ADR**: [ADR-0006](../../../docs/architecture/adr-0006-card-pool-regeneration-timing.md)
- **Engine Risk**: LOW
- **Engine Notes**: `FOnDayAdvanced`와 `ConfirmOffer` 동일 프레임 처리는 UE Multicast Delegate 동기 순차 — Input→Game 처리 순서에 의존.

## Acceptance Criteria

1. **AC-CS-03** (AUTOMATED/BLOCKING) — `FOnDayAdvanced(5)` 후 DailyHand 구성 완료, 동일 `FOnDayAdvanced(5)` 재수신 → DailyHand 불변, 샘플링 미재실행 (EC-CS-11)
2. EC-CS-10: `FOnDayAdvanced`가 복수 일차 점프 (예: Day 3→8) → 최종 DayIndex 기준 단 1회 샘플링, 중간 날짜 별도 처리 없음 (GSM E8과 일관)
3. EC-CS-6: `FOnDayAdvanced`와 `ConfirmOffer` 동일 프레임 처리 — 양쪽 경로 테스트:
   - ConfirmOffer 먼저 → 현재 DailyHand로 정상 건넴 → FOnDayAdvanced로 재생성
   - FOnDayAdvanced 먼저 → 새 DailyHand 생성 → ConfirmOffer는 이전 CardId로 게이트 2 실패
4. EC-CS-3: 모든 카드 태그가 동일 계절 (동질적 카탈로그) → BaseWeight 동일 → 런타임 크래시 없음, 에디터 경고 권고 (비 BLOCKING)
5. **AC-CS-13b** (AUTOMATED/BLOCKING) — CurrentDayIndex 초기값 = -1, `FOnDayAdvanced(0)` → `NewDayIndex < 1` guard → 샘플링 스킵, `UE_LOG(Warning)` 1회 (EC-CS-19)

## Implementation Notes

- **CR-CS-2 재생성 가드**:
  ```cpp
  if (NewDayIndex < 1) { UE_LOG(Warning, ...); return; }  // EC-CS-19
  if (NewDayIndex <= CurrentDayIndex) return;  // EC-CS-11 중복
  ```
- **EC-CS-10 멀티-day jump**: 중간 날짜 샘플링 없음 — 최종 DayIndex만 사용 (F-CS-1 계절 계산 입력)
- **EC-CS-6 동일 프레임**: UE delegate 동기 처리 — Input → Game 순서에 의존. 테스트는 양 경로 각각 독립 검증
- **EC-CS-3 동질적 카탈로그**: `BaseWeight` 모두 InSeasonWeight → 가중치 효과 소멸. 런타임 정상 동작 보장 (에디터 경고는 Data Pipeline 측 validator 책임)

## Out of Scope

- Data Pipeline 에디터 경고 (foundation-data-pipeline)
- GSM E8 Dawn 1회 보장 (core-game-state-machine)

## QA Test Cases

**Given** `FOnDayAdvanced(5)` 후 DailyHand 구성 완료, **When** 동일 `FOnDayAdvanced(5)` 재수신, **Then** DailyHand 불변, 샘플링 미재실행, CurrentState 변화 없음 (AC-CS-03).

**Given** `CurrentDayIndex=3`, **When** `FOnDayAdvanced(8)` (5-day jump), **Then** 최종 DayIndex=8 기준 1회 샘플링, 중간 날짜 이벤트 없음 (EC-CS-10).

**Given** 동일 프레임 `FOnDayAdvanced(4) → ConfirmOffer("OldCard")`, **When** delegate 순서대로, **Then** 새 DailyHand 생성 → ConfirmOffer는 "OldCard" 미포함 → false (EC-CS-6 역순).

**Given** `CurrentDayIndex=-1`, **When** `FOnDayAdvanced(0)`, **Then** 샘플링 스킵, `UE_LOG(Warning)` 1회, DailyHand 미변경 (AC-CS-13b / EC-CS-19).

**Given** 모든 카드 Tags=["Season.Spring"], **When** Day 1 샘플링, **Then** 크래시 없음, 3장 선택 성공 (EC-CS-3).

## Test Evidence

- **Unit test**: `tests/unit/Card/test_edge_cases.cpp`
- 각 EC별 독립 함수

## Dependencies

- Story 001 (Subsystem + state machine)
- Story 002 (Eager 샘플링)
- Story 003 (ConfirmOffer)
