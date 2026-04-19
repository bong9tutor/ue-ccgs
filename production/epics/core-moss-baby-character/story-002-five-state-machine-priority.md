# Story 002 — 5-State 상태 머신 + 우선순위 (FinalReveal > GrowthTransition > Reacting > Drying/Idle)

> **Epic**: [core-moss-baby-character](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md) §States and Transitions
- **TR-ID**: TR-mbc-002 (Priority: FinalReveal > GrowthTransition > Reacting > Drying/Idle)
- **Governing ADR**: N/A — GDD contract.
- **Engine Risk**: LOW
- **Engine Notes**: 상태 우선순위 원칙: "더 큰 변화가 더 작은 변화를 흡수한다". 최종 형태 공개(FinalReveal)는 21일 절정 — 모든 것에 우선.
- **Control Manifest Rules**: Cross-Cutting — Tick 남용 금지, 이벤트/타이머 우선.

## Acceptance Criteria

- **AC-MBC-10** [`INTEGRATION`/BLOCKING] — GrowthTransition 중 FinalReveal 도착 시 GrowthTransition 대기 없이 즉시 FinalReveal 진입 + 중간 메시 미렌더링 (E2).
- **AC-MBC-13** [`INTEGRATION`/BLOCKING] — Reacting 상태 활성 중 GrowthTransition 이벤트 도착 시 Reacting 즉시 중단 + Stillness Budget Release 확인 + GrowthTransition 진입. Reacting의 잔여 Emission/SSS 감쇠는 중단됨 (E3).

## Implementation Notes

1. **`EMossCharacterState` enum** (`Source/MadeByClaudeCode/Core/MossBaby/EMossCharacterState.h`):
   - `enum class EMossCharacterState : uint8 { Idle=0, Reacting=1, GrowthTransition=2, QuietRest=3, FinalReveal=4 };`
2. **Priority 레벨** (inline helper):
   - `static uint8 GetStatePriority(EMossCharacterState S)`: FinalReveal=4, GrowthTransition=3, Reacting=2, QuietRest=1, Idle=0. 숫자 큰 것이 우선.
3. **`AMossBaby::RequestStateChange(EMossCharacterState NewState)`**:
   - `if (GetStatePriority(NewState) < GetStatePriority(CurrentState)) { return false; }` — 낮은 우선순위 reject.
   - `if (GetStatePriority(NewState) > GetStatePriority(CurrentState)) { ForceInterrupt(); }` — 높은 우선순위 인터럽트.
   - `EnterState(NewState);` — Entry action + current state 업데이트.
4. **`ForceInterrupt()` — E2/E3 처리**:
   - `if (CurrentState == EMossCharacterState::Reacting) { ReactingStillnessHandle.Release(); CancelReactingDecayTimer(); }`
   - `if (CurrentState == EMossCharacterState::GrowthTransition) { CancelGrowthLerp(); }` — 중간 메시 미렌더링 — target 머티리얼 값은 그대로 유지하여 새 Lerp의 V_start로 사용.
5. **Idle/QuietRest 구분**:
   - Idle: 기본 상태, 브리딩 활성 (story 006).
   - QuietRest: DayGap ≥ 2 시 진입 (story 005).

## Out of Scope

- Reacting 지수 감쇠 Formula 4 (story 003)
- GrowthTransition Lerp (story 004)
- QuietRest DryingFactor 계산 (story 005)
- 브리딩 사인파 (story 006)
- Stillness Budget Request/Release API (Feature epic)

## QA Test Cases

**Test Case 1 — AC-MBC-10 GrowthTransition → FinalReveal interrupt**
- **Given**: `CurrentState = GrowthTransition` (메시 Sprout→Growing Lerp 진행 중, 진행률 50%).
- **When**: `RequestStateChange(FinalReveal)` 호출.
- **Then**: `CurrentState = FinalReveal`. GrowthTransition Lerp 즉시 중단. 중간 메시 (Growing) 미 렌더링 — Lerp 취소 확인. `MeshComponent->GetStaticMesh()` 이 `SM_MossBaby_Final_[ID]`로 교체.

**Test Case 2 — AC-MBC-13 Reacting → GrowthTransition**
- **Given**: `CurrentState = Reacting`, `ReactingStillnessHandle.IsValid() == true`, Reacting 감쇠 타이머 진행 중.
- **When**: Growth emits `FOnGrowthStageChanged(Growing)` → MBC handler → `RequestStateChange(GrowthTransition)`.
- **Then**: 
  - `CurrentState = GrowthTransition`.
  - `ReactingStillnessHandle.IsValid() == false` (Release 호출).
  - Reacting 감쇠 타이머 cancel 확인.
  - 다음 frame 이후 Emission boost 없음 (감쇠 중단).

**Test Case 3 — Low priority reject**
- **Given**: `CurrentState = GrowthTransition`.
- **When**: `RequestStateChange(Reacting)` 호출.
- **Then**: Return `false`. `CurrentState` 여전히 `GrowthTransition`.

## Test Evidence

- [ ] `tests/unit/moss-baby/priority_table_test.cpp` — GetStatePriority 5×5 matrix
- [ ] `tests/integration/moss-baby/interrupt_transitions_test.cpp` — E2/E3 시나리오
- [ ] `tests/unit/moss-baby/low_priority_reject_test.cpp` — reject path

## Dependencies

- **Depends on**: story-001 (MossBaby actor)
- **Unlocks**: story-003 (Reacting entry), story-004 (GrowthTransition entry), story-005 (QuietRest), story-007 (FinalReveal)
