# Story 002: Eager 샘플링 + F-CS-1 계절 + F-CS-2 가중치

> **Epic**: feature-card-system
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3시간

## Context

- **GDD Reference**: `design/gdd/card-system.md` §CR-CS-1 (Eager 샘플링) + §F-CS-1 (SeasonForDay) + §F-CS-2 (CardWeight) + AC-CS-01/02/13/13b/14/15a/15b/20a
- **TR-ID**: TR-card-003 (Eager), TR-card-006 (확률 수렴)
- **Governing ADR**: [ADR-0006](../../../docs/architecture/adr-0006-card-pool-regeneration-timing.md) — Eager 채택, `FOnDayAdvanced` 수신 즉시 샘플링
- **Engine Risk**: LOW
- **Engine Notes**: `FRandomStream` 외부 주입 가능하게 설계 — 테스트 시드 제어. `HashCombine(PlaythroughSeed, DayIndex)` 결정적 시드.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — "Card 풀 샘플링은 Eager — `FOnDayAdvanced(NewDayIndex)` 수신 즉시 `RegenerateDailyHand` 호출 + Ready 상태 진입"
  - Feature Layer §Required Patterns — "`FRandomStream` 외부 주입 — `PlaythroughSeed + DayIndex` HashCombine으로 결정적 시드"
  - Feature Layer §Forbidden Approaches — "Never lazy-sample card pool on Offering entry"
  - Feature Layer §Performance Guardrails — "Card 샘플링 CPU cost: MVP ≈ 0.1ms, Full 40장 < 1ms"

## Acceptance Criteria

1. **AC-CS-01** (AUTOMATED/BLOCKING) — 카탈로그 10장 로드 완료, `FOnDayAdvanced(1)` 수신 후 `GetDailyHand()` → 원소 수 3, 모두 유효 FName, 중복 없음
2. **AC-CS-02** (AUTOMATED/BLOCKING) — `GameDurationDays=21`, DayIndex=7에서 F-CS-1 실행 → `Season.Summer`, 해당 계절 카드 `BaseWeight = InSeasonWeight(3.0)`, 비해당 `BaseWeight = OffSeasonWeight(1.0)`. SegmentLength 동적 도출 (하드코딩 금지)
3. **AC-CS-13** (AUTOMATED/BLOCKING) — `GameDurationDays=21`, DayIndex={1,6,7,12,17,21} 각각 F-CS-1 → {Spring, Spring, Summer, Autumn, Winter, Winter}
4. **AC-CS-13b** (AUTOMATED/BLOCKING) — `CurrentDayIndex` 초기값 = -1, `FOnDayAdvanced(0)` 수신 → 샘플링 스킵 (`NewDayIndex < 1` guard), `UE_LOG(Warning)` 1회
5. **AC-CS-14** (AUTOMATED/BLOCKING) — Spring, `InSeasonWeight=3.0`, `OffSeasonWeight=1.0`, `ConsecutiveDaySuppression=0.5`, `PreviousDayHand={"Card_S1","Card_S2"}`: Card_S1 (Spring, 억제)=1.5, Card_S3 (Spring, 미억제)=3.0, Card_Su1 (Summer, 미억제)=1.0
6. **AC-CS-15a** (AUTOMATED/BLOCKING) — F-CS-3 기대 확률 결정성: 억제 없는 Spring P ∈ [0.45, 0.63], 억제 Spring P ∈ [0.22, 0.38], 비제철 P ∈ [0.16, 0.28]
7. **AC-CS-15b** (AUTOMATED/BLOCKING) — 고정 시드 `PlaythroughSeed=12345` + 10,000회 반복 샘플링 → F-CS-3 기대 확률 ±5%p 수렴
8. **AC-CS-20a** (AUTOMATED/BLOCKING) — `GameDurationDays=3` (범위 밖) PostLoad 검증 → `UE_LOG(Error)` + 기본값(21) 폴백
9. Full 40장 샘플링 < 1ms (ADR-0006 §Validation Criteria)

## Implementation Notes

- **CR-CS-1 구현 (ADR-0006 Eager)**:
  ```cpp
  void UCardSystemSubsystem::OnDayAdvancedHandler(int32 NewDayIndex) {
      if (NewDayIndex < 1) { UE_LOG(Warning, TEXT("Invalid DayIndex: %d"), NewDayIndex); return; }
      if (NewDayIndex <= CurrentDayIndex) return;  // CR-CS-2 가드
      PreviousDayHand = DailyHand;  // 이전 핸드 보존 (억제 가중치)
      CurrentDayIndex = NewDayIndex;
      CurrentState = ECardSystemState::Preparing;
      RegenerateDailyHand(NewDayIndex);
      CurrentState = ECardSystemState::Ready;
      // bHandOffered 복원 체크는 Story 005
  }
  ```
- **F-CS-1 구현**: `SegmentLength = GameDurationDays / 4.0f`, `SeasonIndex = FMath::Clamp(FMath::FloorToInt((DayIndex-1)/SegmentLength), 0, 3)`
- **F-CS-2 구현**: `BaseWeight = Card.Tags.Contains(CurrentSeason) ? InSeasonWeight : OffSeasonWeight`, `SuppressMultiplier = PreviousDayHand.Contains(Card.CardId) ? ConsecutiveDaySuppression : 1.0f`
- **F-CS-3 샘플링** (without replacement, weighted):
  ```cpp
  const uint32 Seed = HashCombine(SessionRecord.PlaythroughSeed, NewDayIndex);
  FRandomStream Stream(Seed);  // 외부 주입 가능
  TArray<FName> Pool = Pipeline->GetAllCardIds();
  for (int32 i = 0; i < HandSize; ++i) {
      float TotalWeight = ComputeTotalWeight(Pool);
      float r = Stream.FRandRange(0.0f, TotalWeight);
      FName Selected = WeightedPick(Pool, r);
      DailyHand.Add(Selected);
      Pool.Remove(Selected);
      if (r >= TotalWeight) Selected = Pool.Last();  // float 경계 fallback
  }
  ```
- **`UCardSystemConfigAsset` PostLoad 검증** (EC-CS-20, AC-CS-20a): `GameDurationDays ∈ {7, 21}`, 나머지 knob clamp
- **결정성**: `FRandomStream` + HashCombine(PlaythroughSeed, DayIndex) → 동일 입력 = 동일 출력
- **성능 guard**: Full 40장 샘플링 CPU < 1ms (ADR-0006)

## Out of Scope

- GSM Prepare/Ready 프로토콜 (Story 006)
- ConfirmOffer 검증 (Story 003)
- bHandOffered 복원 (Story 005)
- Day 21 cross-system integration (Story 007)

## QA Test Cases

**Given** 10장 카탈로그, **When** `FOnDayAdvanced(1)` 수신, **Then** `DailyHand.Num() == 3`, 중복 없음, 상태 Preparing → Ready.

**Given** `GameDurationDays=21`, DayIndex=7, **When** F-CS-1 실행, **Then** Season.Summer, InSeason 카드 Weight=3.0, OffSeason Weight=1.0.

**Given** `CurrentDayIndex=-1`, **When** `FOnDayAdvanced(0)`, **Then** `UE_LOG(Warning)` + 샘플링 스킵, DailyHand 미변경 (AC-CS-13b).

**Given** 고정 시드 `PlaythroughSeed=12345` + 10000회 반복, **When** 샘플링, **Then** 기대 확률 ±5%p 수렴 (AC-CS-15b).

**Given** Full 40장 catalog + Mock RandomStream, **When** 10회 샘플링 측정, **Then** 평균 wall-clock < 1ms (ADR-0006).

## Test Evidence

- **Unit test**: `tests/unit/Card/test_eager_sampling.cpp` + `test_f_cs_1_season.cpp` + `test_f_cs_2_weight.cpp` + `test_f_cs_3_probability.cpp`
- **Performance test**: 40장 x 10회 샘플링 avg < 1ms
- **Statistical test**: 10,000회 ±5%p 수렴 (AC-CS-15b)

## Dependencies

- Story 001 (Subsystem 골격)
- `foundation-data-pipeline` (GetAllCardIds, GetCardRow)
- `foundation-save-load` (PlaythroughSeed in FSessionRecord — OQ-CS-7 implementation)
- `foundation-time-session` (FOnDayAdvanced)
