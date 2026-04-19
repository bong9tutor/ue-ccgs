# Story 003: CR-2 희소성 게이트 (Cooldown + Cap + Day 1 + Day 21 Guards)

> **Epic**: feature-dream-trigger
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/dream-trigger-system.md` §CR-1 Guards + §CR-2 희소성 가드 + §F2 DaysSinceLastDream + AC-DTS-01/02/03/15/18
- **TR-ID**: TR-dream-004 (rarity gates — Pillar 3 수학적 수호)
- **Governing ADR**: [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) (MinDreamIntervalDays / MaxDreamsPerPlaythrough)
- **Engine Risk**: LOW
- **Engine Notes**: 평가는 동기 실행 — `OnCardProcessedByGrowthHandler` 내에서 전체 가드 체인 완료.
- **Control Manifest Rules**:
  - Core Layer §Cross-Cutting Constraints — "Pillar 3 (말할 때만 말한다)" — Dream Trigger 희소성 게이트가 수학적 수호

## Acceptance Criteria

1. **AC-DTS-01** (AUTOMATED/BLOCKING) — `LastDreamDay=5`, `CurrentDayIndex=8`, `MinDreamIntervalDays=4` → `DaysSinceLastDream=3` < 4 → 게이트 A 실패 → `FOnDreamTriggered` 미발행, `DreamsUnlockedCount` 불변
2. **AC-DTS-02** (AUTOMATED/BLOCKING) — `LastDreamDay=4`, `CurrentDayIndex=8`, `MinDreamIntervalDays=4` → `DaysSinceLastDream=4` == 4 → 게이트 A 통과 → TagScore 조건 충족 시 트리거 가능
3. **AC-DTS-03** (AUTOMATED/BLOCKING) — `DreamsUnlockedCount=5`, `MaxDreamsPerPlaythrough=5` + TagScore 조건 충족 → 게이트 B 실패 → 미발행, 카운트 불변 (5 초과 없음)
4. **AC-DTS-15** (AUTOMATED/BLOCKING) — `EGameState == Farewell`, `FOnCardProcessedByGrowth` 수신 → 평가 스킵 (EC-6)
5. **AC-DTS-18** (AUTOMATED/BLOCKING) — `CurrentDayIndex - LastDreamDay`가 음수 (BACKWARD_GAP) → `max(0, ...)` 가드로 음수 방지, 게이트 A 실패 (EC-9)
6. CR-1 Day 1 guard: `DayIndex == 1` → 평가 스킵 (첫 경험 보호)
7. CR-1 Day 21 (`Result.bIsDay21Complete == true`) → 평가 스킵 (GSM Farewell P0 진입 예정 — Story 006 Day 21 강제 트리거는 별도 경로)
8. 게이트 순서 (GDD §CR-1): EGameState → Day 1 → bIsDay21Complete → 게이트 A → 게이트 B → 후보 수집

## Implementation Notes

- **Handler 가드 체인** (GDD §CR-1):
  ```cpp
  void UDreamTriggerSubsystem::OnCardProcessedByGrowthHandler(
      const FGrowthProcessResult& Result) {
      // Guard 1: GameState
      if (GSM->GetCurrentState() == EGameState::Farewell) return;  // AC-DTS-15 EC-6
      // Guard 2: Day 1 first-experience protection
      if (CurrentDayIndex == 1) return;
      // Guard 3: Day 21 Final Form 진입 중
      if (Result.bIsDay21Complete) return;  // Story 006 Day 21 강제 경로는 별도
      // Guard A: Cooldown (F2)
      const int32 DaysSince = FMath::Max(0, CurrentDayIndex - FDreamState.LastDreamDay);  // AC-DTS-18
      if (DaysSince < DreamSettings->MinDreamIntervalDays) return;
      // Guard B: Cap
      if (FDreamState.DreamsUnlockedCount >= DreamSettings->MaxDreamsPerPlaythrough) return;

      // 후보 수집 + 선택 (Story 004)
      EvaluateAndSelect();
  }
  ```
- **F2 `max(0, ...)` 가드** (AC-DTS-18): EC-9 BACKWARD_GAP 시 음수 방지
- 평가 실패의 침묵 (GDD §CR-1): 어떤 이벤트도 발행 안 함 (Pillar 3)

## Out of Scope

- F1 TagScore (Story 002)
- 꿈 선택 + argmax (Story 004)
- CR-1 atomic 저장 + Broadcast (Story 005)
- Day 21 강제 트리거 EC-1 (Story 006)

## QA Test Cases

**Given** `LastDreamDay=5, CurrentDayIndex=8, MinDreamIntervalDays=4`, **When** `FOnCardProcessedByGrowth` 수신, **Then** 게이트 A 실패, 미발행 (AC-DTS-01).

**Given** `LastDreamDay=4, CurrentDayIndex=8, MinDreamIntervalDays=4`, **When** Handler, **Then** 게이트 A 통과 → 후보 수집 단계로 진입.

**Given** `DreamsUnlockedCount=5, MaxDreams=5`, **When** Handler, **Then** 게이트 B 실패, 미발행 (AC-DTS-03).

**Given** Farewell 상태, **When** `FOnCardProcessedByGrowth` 수신, **Then** 즉시 return, 미발행 (AC-DTS-15).

**Given** `Result.bIsDay21Complete = true`, **When** Handler, **Then** 평가 스킵 (Day 21 별도 경로).

**Given** `CurrentDayIndex=3, LastDreamDay=5` (BACKWARD_GAP), **When** DaysSince 계산, **Then** `max(0, -2) = 0` < 4 → 게이트 A 실패 (AC-DTS-18).

**Given** `CurrentDayIndex=1`, **When** Handler, **Then** Day 1 guard → 평가 스킵.

## Test Evidence

- **Unit test**: `tests/unit/Dream/test_rarity_gates.cpp`
- Guard 순서별 독립 테스트

## Dependencies

- Story 001 (Subsystem + FDreamState)
- Story 002 (F1 TagScore — 후보 수집 진입 검증)
- `core-game-state-machine` (GetCurrentState / EGameState)
