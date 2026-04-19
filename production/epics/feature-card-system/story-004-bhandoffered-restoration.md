# Story 004: bHandOffered 복원 (GetLastCardDay sentinel)

> **Epic**: feature-card-system
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 1.5시간

## Context

- **GDD Reference**: `design/gdd/card-system.md` §CR-CS-5 비영속 복원 + §States and Transitions (앱 재시작 당일 건넴 완료) + EC-CS-12/13/14 + AC-CS-08/09
- **TR-ID**: TR-card-002 (OQ-CS-2 GetLastCardDay)
- **Governing ADR**: [ADR-0012](../../../docs/architecture/adr-0012-growth-getlastcardday-api.md) — `int32 GetLastCardDay() const`, sentinel 0 = 미건넴
- **Engine Risk**: LOW
- **Engine Notes**: Growth `GetLastCardDay()` pull. `LastCard == CurrentDayIndex` 비교 — sentinel 0 자연 처리.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — "Growth `int32 GetLastCardDay() const` API: return 0 = 미건넴 (sentinel)"

## Acceptance Criteria

1. **AC-CS-08** (AUTOMATED/BLOCKING) — 앱 재시작 시뮬, `FGrowthState.LastCardDay=4`, `DayIndex=4`, `FOnDayAdvanced(4)` 수신 → `bHandOffered = true` 복원, `ConfirmOffer()` → `false` (EC-CS-13 중복 건넴 방지)
2. **AC-CS-09** (AUTOMATED/BLOCKING) — 재시작 시뮬, `FGrowthState.LastCardDay=3`, `DayIndex=4`, `FOnDayAdvanced(4)` 후 `ConfirmOffer("Card_A")` (DailyHand 포함) → `bHandOffered = false`, ConfirmOffer 성공, `FOnCardOffered` 정상 발행, `PreviousDayHand = ∅`
3. Ready 진입 직후 `bHandOffered = (GrowthEngine->GetLastCardDay() == CurrentDayIndex)` 체크
4. `bHandOffered == true` 복원 시 `CurrentState = Offered` 전환 (FOnCardOffered **미발행** — 이미 이전 세션에서 발행됨)

## Implementation Notes

- **복원 위치** (CR-CS-1 + CR-CS-5):
  ```cpp
  void UCardSystemSubsystem::OnDayAdvancedHandler(int32 NewDayIndex) {
      // ... (Story 002 Eager 샘플링 경로)
      CurrentState = ECardSystemState::Ready;

      // bHandOffered 복원 (EC-CS-13 ADR-0012)
      auto* Growth = GetGameInstance()->GetSubsystem<UGrowthAccumulationSubsystem>();
      const int32 LastCard = Growth->GetLastCardDay();  // sentinel 0 = 미건넴
      if (LastCard == CurrentDayIndex) {
          bHandOffered = true;
          CurrentState = ECardSystemState::Offered;
          // FOnCardOffered 미발행 — 이전 세션에서 이미 발행됨
      }
  }
  ```
- **EC-CS-14**: `FOnCardOffered` 발행 직후 `SaveAsync` 커밋 전 앱 종료 → `GetLastCardDay()`가 sentinel 0 반환 → EC-CS-12 경로 (미건넴으로 추론, 재건넴 허용). Save/Load per-trigger atomicity 계약상 허용된 1-commit loss.
- **PreviousDayHand**: 재시작 시 비영속이므로 공집합 — 소프트 억제 가중치 차이로 0~1장 다를 수 있으나 대부분 동일 3장 유지 (CR-CS-5 설계 근거)
- **sentinel 0 활용** (ADR-0012): FreshStart시 `LastCardDay=0`, `CurrentDayIndex>=1` → `0 == 1` false → `bHandOffered=false` 자연 처리

## Out of Scope

- Growth `GetLastCardDay()` 구현 (feature-growth-accumulation Story 007)
- Save/Load 라운드트립 세이브 파일 검증 (foundation-save-load)

## QA Test Cases

**Given** 재시작 시뮬 + `GrowthState.LastCardDay=4` + `DayIndex=4`, **When** `FOnDayAdvanced(4)`, **Then** `bHandOffered=true`, CurrentState=Offered, `ConfirmOffer(anyCard)` → false (AC-CS-08).

**Given** 재시작 시뮬 + `GrowthState.LastCardDay=3` + `DayIndex=4`, **When** `FOnDayAdvanced(4)` 후 `ConfirmOffer("Card_A")`, **Then** bHandOffered=false → 성공 → FOnCardOffered 정상 발행, PreviousDayHand 공집합 (AC-CS-09).

**Given** FreshStart (`LastCardDay=0`) + `DayIndex=1`, **When** `FOnDayAdvanced(1)`, **Then** `bHandOffered=false` (sentinel 0 != 1), CurrentState=Ready.

**Given** EC-CS-14 시나리오 (SaveAsync 커밋 전 종료), **When** 재시작 후 `FOnDayAdvanced(N)`, **Then** sentinel 0 → bHandOffered=false → 재건넴 허용.

## Test Evidence

- **Unit test**: `tests/unit/Card/test_bhand_offered_restoration.cpp`
- Growth `GetLastCardDay()` mock으로 test isolation

## Dependencies

- Story 001 (Subsystem)
- Story 002 (Ready 상태 진입)
- `feature-growth-accumulation` Story 007 (GetLastCardDay API)
