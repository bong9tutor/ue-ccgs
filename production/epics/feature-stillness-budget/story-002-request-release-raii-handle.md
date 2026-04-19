# Story 002: Request/Release API + RAII FStillnessHandle (Move-only)

> **Epic**: feature-stillness-budget
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2.5시간

## Context

- **GDD Reference**: `design/gdd/stillness-budget.md` §Rule 4 슬롯 생명주기 + §Rule 8 공개 API + §F1 (CanGrant) + §F2 (EffLimit) + §E7 이중 Release + AC-SB-01/02/03/11
- **TR-ID**: TR-stillness-002 (RAII + idempotent Release)
- **Governing ADR**: None (GDD contract)
- **Engine Risk**: LOW
- **Engine Notes**: `FStillnessHandle` move-only semantics (복사 금지) — `= delete` copy ctor/assign. Release는 idempotent (`bValid` flag).

## Acceptance Criteria

1. **AC-SB-01** (AUTOMATED) — Motion 채널 EffLimit=2, Active=1, `Request(Motion, Standard)` → 유효 `FStillnessHandle` 반환, Active=2
2. **AC-SB-02** (AUTOMATED) — Motion 채널 EffLimit=2, Active=2, 선점 불가, `Request(Motion, Background)` → Invalid 핸들, Active 변화 없음
3. **AC-SB-03** (AUTOMATED) — Motion Active=2 / Particle Active=0, `Request(Particle, Standard)` → 유효 핸들 (채널 독립성)
4. **AC-SB-11** (AUTOMATED) — 유효 핸들에 `Release()` 2회 연속 호출 → Active 감소 1회만, 두 번째 호출 no-op, 크래시 없음 (E7)
5. `FStillnessHandle` move-only: `FStillnessHandle(const FStillnessHandle&) = delete`, `operator=(const FStillnessHandle&) = delete`. Move ctor/assign 허용
6. RAII: 소멸자에서 `bValid == true`이면 자동 Release 호출
7. F1 `CanGrant = (Active_ch < EffLimit_ch) OR (FreeByPreempt_ch >= 1)` — 선점 판정은 Story 003에서 구현, 이 story는 빈 슬롯 유무만 체크

## Implementation Notes

- **`FStillnessHandle` 정의**:
  ```cpp
  USTRUCT(BlueprintType)
  struct FStillnessHandle {
      GENERATED_BODY()
      UPROPERTY() EStillnessChannel Channel;
      UPROPERTY() int32 SlotId = -1;  // internal slot index
      UPROPERTY() bool bValid = false;
      UPROPERTY() FOnPreempted OnPreempted;  // Story 003

      // Move-only
      FStillnessHandle() = default;
      FStillnessHandle(const FStillnessHandle&) = delete;
      FStillnessHandle& operator=(const FStillnessHandle&) = delete;
      FStillnessHandle(FStillnessHandle&& Other) noexcept { /* ... */ }
      FStillnessHandle& operator=(FStillnessHandle&& Other) noexcept { /* ... */ }
      ~FStillnessHandle() {
          if (bValid) GetSubsystem()->Release(*this);
      }
  };
  ```
- **Request API**:
  ```cpp
  FStillnessHandle UStillnessBudgetSubsystem::Request(
      EStillnessChannel Channel, EStillnessPriority Priority) {
      if (!Asset) return FStillnessHandle{};  // Unavailable (Rule 7)
      const int32 EffLimit = ComputeEffLimit(Channel);  // F2 (Story 004)
      if (ActiveSlots[Channel].Num() < EffLimit) {
          // Grant
          const int32 SlotId = AllocateSlot(Channel, Priority);
          FStillnessHandle H;
          H.Channel = Channel; H.SlotId = SlotId; H.bValid = true;
          return H;
      }
      // 선점 경로 — Story 003에서 구현
      return TryPreempt(Channel, Priority);
  }
  ```
- **Release idempotent (E7)**:
  ```cpp
  void UStillnessBudgetSubsystem::Release(FStillnessHandle& Handle) {
      if (!Handle.bValid) return;  // no-op (이미 released or 선점됨)
      ActiveSlots[Handle.Channel].RemoveAll(/* SlotId match */);
      Handle.bValid = false;
  }
  ```
- **Channel 독립** (AC-SB-03 + Rule 2): 각 채널의 `Active` 집계가 독립

## Out of Scope

- 선점 로직 (Story 003) — 본 story는 빈 슬롯 유무만 체크
- EffLimit 계산 상세 (Story 004 — ReducedMotion 통합)
- OnBudgetRestored delegate (Story 005)

## QA Test Cases

**Given** Motion EffLimit=2, Active=1, **When** `Request(Motion, Standard)`, **Then** 유효 handle, Active=2 (AC-SB-01).

**Given** Motion EffLimit=2, Active=2 (모두 Standard), 선점 불가 (Background 요청), **When** `Request(Motion, Background)`, **Then** Invalid handle, Active=2 (AC-SB-02).

**Given** Motion Active=2, Particle Active=0, **When** `Request(Particle, Standard)`, **Then** 유효 handle (Channel 독립) (AC-SB-03).

**Given** 유효 handle, **When** `Release()` 2회 연속, **Then** Active 감소 1회, 두 번째 no-op, 크래시 없음 (AC-SB-11, E7).

**Given** 유효 handle의 복사 시도, **When** 컴파일, **Then** 컴파일 에러 (copy ctor = delete).

**Given** 로컬 스코프 handle, **When** 스코프 종료, **Then** 소멸자 자동 Release, Active 감소 1회.

## Test Evidence

- **Unit test**: `tests/unit/Stillness/test_request_release_basic.cpp` + `test_raii_handle.cpp`
- **Compile-time check**: move-only semantics via `static_assert`

## Dependencies

- Story 001 (Subsystem + enums)
