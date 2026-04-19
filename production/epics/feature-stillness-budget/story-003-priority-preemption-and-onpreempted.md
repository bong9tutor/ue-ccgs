# Story 003: 우선순위 선점 + OnPreempted Delegate + 재진입 가드

> **Epic**: feature-stillness-budget
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/stillness-budget.md` §Rule 5 (선점) + §E5 (선점 체인 방지) + §E6 (동일 우선순위) + §E8 (선점 후 Release) + §E10 (단일 슬롯 루프) + AC-SB-04/05/08/12
- **TR-ID**: TR-stillness-003 (preemption — Narrative > Standard > Background)
- **Governing ADR**: None (GDD contract)
- **Engine Risk**: LOW
- **Engine Notes**: 선점 시점에 핸들을 즉시 무효화 (`bValid = false`) — E8 이중 Release 방어.

## Acceptance Criteria

1. **AC-SB-04** (AUTOMATED) — Motion EffLimit=2, Active=[Background, Standard], `Request(Motion, Narrative)` → Background 슬롯 선점 해제 + Grant 신규 핸들, `OnPreempted` delegate 발행 1회
2. **AC-SB-05** (AUTOMATED) — Motion EffLimit=2, Active=[Standard, Standard], `Request(Motion, Standard)` → Invalid 핸들, 기존 슬롯 선점 없음 (E6 동일 우선순위 선점 금지)
3. **AC-SB-08** (AUTOMATED) — DegradedFallback, 세 채널 `Request()` → 모두 Invalid, 하드코딩 기본값 없음 (Rule 7 fail-close)
4. **AC-SB-12** (AUTOMATED) — `OnPreempted` 핸들러 내에서 동일 채널 `Request()` 재호출 시도 → 재진입 가드 assert 발동 또는 무시 (E5)
5. 선점 순서: `Narrative` 요청 → Background → Standard 순 선점 탐색. `Standard` 요청 → Background만 선점 가능
6. 선점 즉시 기존 핸들 `bValid = false` (E8)

## Implementation Notes

- **TryPreempt 구현** (GDD §Rule 5):
  ```cpp
  FStillnessHandle UStillnessBudgetSubsystem::TryPreempt(
      EStillnessChannel Channel, EStillnessPriority RequestedPriority) {
      // 우선순위 < RequestedPriority인 슬롯 검색 (Rule 5)
      FActiveSlot* Victim = FindLowestPriorityBelow(Channel, RequestedPriority);
      if (!Victim) return FStillnessHandle{};  // Deny (E6 동일/고 우선순위 선점 불가)

      // 선점 실행 — bValid=false로 무효화 (E8)
      Victim->Handle->bValid = false;
      Victim->Handle->OnPreempted.Broadcast();  // OnPreempted 발행

      // 재진입 가드 (E5)
      TGuardValue<bool> Guard(bProcessingPreemption, true);

      // Victim 제거 + 새 슬롯 할당
      RemoveSlot(Channel, Victim->SlotId);
      const int32 SlotId = AllocateSlot(Channel, RequestedPriority);
      FStillnessHandle H; H.Channel = Channel; H.SlotId = SlotId; H.bValid = true;
      return H;
  }

  FStillnessHandle UStillnessBudgetSubsystem::Request(EStillnessChannel Channel, EStillnessPriority Pri) {
      if (bProcessingPreemption) {
          checkf(false, TEXT("E5: Request during OnPreempted handler"));
          return FStillnessHandle{};
      }
      // ... (Story 002 경로 + TryPreempt 통합)
  }
  ```
- **FOnPreempted delegate** — `FStillnessHandle` 내부 필드: `DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPreempted)`
- **E6 동일 우선순위 선점 금지**: `FindLowestPriorityBelow`는 엄격 `<` 비교 (equal priority exclude)
- **E10 단일 슬롯 루프**: 프레임 내 루프는 E5로 방지 — 프레임 간 재요청은 소비자 책임 (이벤트 기반만 허용)

## Out of Scope

- Reduced Motion 통합 (Story 004)
- OnBudgetRestored delegate (Story 005)
- USoundConcurrency (VS sprint)

## QA Test Cases

**Given** Motion EffLimit=2, Active=[Background, Standard], **When** `Request(Motion, Narrative)`, **Then** Background 선점 해제, 신규 핸들 Grant, OnPreempted 발행 (AC-SB-04).

**Given** Motion EffLimit=2, Active=[Standard, Standard], **When** `Request(Motion, Standard)`, **Then** Invalid 핸들, 기존 슬롯 유지 (AC-SB-05, E6).

**Given** DegradedFallback, **When** 3 채널 Request, **Then** 모두 Invalid, no hardcoded defaults (AC-SB-08).

**Given** OnPreempted handler 내에서 `Request(동일 채널)` 시도, **When** 선점 발생, **Then** assert 발동 or no-op (AC-SB-12, E5).

**Given** 선점된 handle (bValid=false), **When** 소비자가 나중에 `Release()` 호출, **Then** no-op (E8).

## Test Evidence

- **Unit test**: `tests/unit/Stillness/test_preemption.cpp` + `test_reentrancy_guard.cpp`
- 각 priority combination 독립 테스트

## Dependencies

- Story 001 (Subsystem + enums)
- Story 002 (Request/Release base)
