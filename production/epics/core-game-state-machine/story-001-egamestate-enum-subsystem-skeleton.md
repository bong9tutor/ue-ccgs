# Story 001 — `EGameState` enum + `UMossGameStateSubsystem` 골격

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Rule 1 (상태 열거) + §Detailed Design
- **TR-ID**: TR-gsm-001 (골격 — 후속 stories의 기반)
- **Governing ADR**: ADR-0004 §Key Interfaces (Subsystem class skeleton)
- **Engine Risk**: LOW (골격 정의만 — Lumen/MPC 구동은 story 002+)
- **Engine Notes**: `UGameInstanceSubsystem` 기반 — Time System과 동일 레벨에서 `GetGameInstance()->GetSubsystem<>()` 대칭 통신. `GSM GDD §Rule 1` 에 명시됨. `UGameInstanceSubsystem`은 Tick 가상 함수 제공 안 함 — MPC Lerp는 후속 story에서 `FTSTicker` 채택.
- **Control Manifest Rules**: Core Layer Rules §Required Patterns — `UMossGameStateSubsystem` 클래스 정의 시 naming convention (`UMossGameStateSubsystem`). Global Rules §Naming Conventions — `EGameState` enum은 `E` prefix + PascalCase.

## Acceptance Criteria

- **AC-GSM-01** [`CODE_REVIEW`/BLOCKING] — `EGameState` 열거형 정의 코드에 대해 정적 분석(grep + static_assert) 실행 시 값이 정확히 6개(Menu=0, Dawn=1, Offering=2, Waiting=3, Dream=4, Farewell=5). `LastPersistedState` 기록 코드 경로는 `Waiting`과 `Farewell` 진입 시에만 존재.

## Implementation Notes

1. **`EGameState` 정의** (`Source/MadeByClaudeCode/Core/GameState/EGameState.h`):
   - `UENUM(BlueprintType)` + `enum class EGameState : uint8 { Menu=0, Dawn=1, Offering=2, Waiting=3, Dream=4, Farewell=5 };`
   - 명시적 숫자 할당 — 세이브 스키마 호환 (story 010에서 영속화)
   - `static_assert(static_cast<uint8>(EGameState::Farewell) == 5, "EGameState enum drift");` 추가로 CI 정적 검증
2. **`UMossGameStateSubsystem` 골격** (`Source/MadeByClaudeCode/Core/GameState/MossGameStateSubsystem.h/.cpp`):
   - `UCLASS()` + `public UGameInstanceSubsystem`
   - `Initialize(FSubsystemCollectionBase&)` / `Deinitialize()` override
   - Private: `EGameState CurrentState = EGameState::Menu;` + `EGameState TargetState = EGameState::Menu;`
   - `UPROPERTY(BlueprintAssignable) FOnGameStateChanged OnGameStateChanged;` — delegate 선언은 story 004에서 구체화
   - Getter: `EGameState GetCurrentState() const`
3. **Delegate 타입 forward declare**: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGameStateChanged, EGameState, OldState, EGameState, NewState);` — GDD §Downstream Consumers Implementation Note.

## Out of Scope

- 상태 전환 로직 (story 002)
- MPC / Light Actor 구동 (story 003-004)
- 세이브/로드 통합 (story 010)
- Compound event boundary (story 011)

## QA Test Cases

**Test Case 1 — Enum integrity (AC-GSM-01)**
- **Setup**: `Source/MadeByClaudeCode/Core/GameState/EGameState.h` 파일 존재.
- **Verify**:
  - `grep "Menu = 0"`, `grep "Farewell = 5"` 매치 각 1건.
  - `static_assert(static_cast<uint8>(EGameState::Farewell) == 5, ...)` 존재.
  - 컴파일 성공 (Windows Editor + Shipping target).
- **Pass**: 모든 grep 조건 충족 + 컴파일 성공.

**Test Case 2 — Subsystem registration**
- **Setup**: Editor 실행 후 console에서 `GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>()` 호출.
- **Verify**: 반환값 non-null + `GetCurrentState()` 호출 시 `EGameState::Menu` 반환.
- **Pass**: 두 조건 모두 충족.

## Test Evidence

- [ ] `tests/unit/game-state/egamestate_enum_test.cpp` — static_assert + 값 검증
- [ ] `tests/unit/game-state/subsystem_skeleton_test.cpp` — Subsystem registration + default state

## Dependencies

- **Depends on**: Foundation Save/Load skeleton (for `FSessionRecord` reference in story 010) — 현재는 forward declare만
- **Unlocks**: story-002 (transition table), story-003 (MPC tick), story-004 (delegate publish)
