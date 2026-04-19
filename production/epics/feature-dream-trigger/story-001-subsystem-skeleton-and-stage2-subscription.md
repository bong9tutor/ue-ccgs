# Story 001: Dream Trigger Subsystem 골격 + Stage 2 구독 전용

> **Epic**: feature-dream-trigger
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/dream-trigger-system.md` §States and Transitions + §Interactions §1 (Stage 2 subscriber) + §Implementation Notes
- **TR-ID**: TR-dream-002 (Stage 2 구독 전환)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) — **Dream Trigger는 Stage 2 `FOnCardProcessedByGrowth` 구독 only** (Stage 1 직접 구독 금지)
- **Engine Risk**: LOW
- **Engine Notes**: `UGameInstanceSubsystem::Initialize()` 내 `Collection.InitializeDependency<UDataPipelineSubsystem>()` + Growth Subsystem 의존. `FOnGameStateChanged` 구독도 함께 등록 (PendingDreamId 클리어 — OQ-DTS-2).
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — "Dream Trigger는 Stage 2 구독 only — `FOnCardProcessedByGrowth` 수신 시 CR-1 평가 시작"
  - Cross-Layer §Required Patterns (Tuning Knob) — `UMossDreamTriggerSettings : UDeveloperSettings`
  - Global Rules §Naming Conventions — `UDreamTriggerSubsystem`, `EDreamState`, `FDreamState` struct

## Acceptance Criteria

1. `UDreamTriggerSubsystem : UGameInstanceSubsystem` 클래스 정의 + `Source/MadeByClaudeCode/Dream/` 배치
2. `FDreamState` USTRUCT — `UnlockedDreamIds: TArray<FName>`, `LastDreamDay: int32 (0=미수신)`, `DreamsUnlockedCount: int32`, `PendingDreamId: FName (NAME_None=대기 없음)`
3. Internal state machine `EDreamInternalState { Dormant, Evaluating, Triggered }` enum
4. `FOnDreamTriggered(FName DreamId)` delegate 선언 — `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam`, `UPROPERTY(BlueprintAssignable)`
5. `Initialize()` 내 Growth Subsystem `OnCardProcessedByGrowth.AddDynamic(this, &OnCardProcessedByGrowthHandler)` 구독 (Stage 2 ONLY)
6. `Initialize()` 내 GSM `OnGameStateChanged.AddDynamic(this, &OnGameStateChangedHandler)` 구독 (PendingDreamId 클리어 — OQ-DTS-2 RESOLVED)
7. **ADR-0005 준수**: `grep "OnCardOffered.AddDynamic" DreamTrigger*.cpp` = 0건 (Stage 1 구독 금지)
8. `Deinitialize()` 모든 delegate 해제
9. `UMossDreamTriggerSettings : UDeveloperSettings` — `MinDreamIntervalDays (Full:4, MVP:2)`, `MaxDreamsPerPlaythrough (Full:5, MVP:2)`, `MinimumGuaranteedDreams (Full:3, MVP:1)`, `EarliestDreamDay (2)`, `DefaultDreamId (NAME_None)`

## Implementation Notes

- **Stage 2 subscription** (ADR-0005 + GDD §Interactions §1):
  ```cpp
  void UDreamTriggerSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
      Super::Initialize(Collection);
      Collection.InitializeDependency<UDataPipelineSubsystem>();
      Collection.InitializeDependency<UGrowthAccumulationSubsystem>();

      auto* Growth = GetGameInstance()->GetSubsystem<UGrowthAccumulationSubsystem>();
      Growth->OnCardProcessedByGrowth.AddDynamic(this,
          &UDreamTriggerSubsystem::OnCardProcessedByGrowthHandler);

      auto* GSM = GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>();
      GSM->OnGameStateChanged.AddDynamic(this,
          &UDreamTriggerSubsystem::OnGameStateChangedHandler);
  }
  ```
- **OQ-DTS-2 RESOLVED 구현**: `OldState == EGameState::Dream`인 전환 수신 시 → `FDreamState.PendingDreamId = NAME_None` + SaveAsync(EDreamReceived). 본 story에서는 handler skeleton만, 상세 PendingDreamId 관리는 Story 005.
- **ADR-0011 ConfigRestartRequired** 적용 — Initialize에서 한 번 읽는 knob

## Out of Scope

- F1 TagScore 계산 (Story 002)
- 희소성 게이트 (Story 003)
- CR-4 Dream Selection (Story 004)
- EC-1 Day 21 강제 트리거 (Story 006)
- PendingDreamId 복구 (Story 005)

## QA Test Cases

**Given** Fresh GameInstance, **When** Dream Trigger Initialize, **Then** Dormant 상태, Growth + GSM 의존 선언 완료, OnCardProcessedByGrowth 구독 등록.

**Given** DreamTrigger 구현 소스, **When** `grep "OnCardOffered.AddDynamic" Source/MadeByClaudeCode/Dream/`, **Then** 매치 0건 (ADR-0005 Stage 1 구독 금지).

**Given** DreamTrigger 구현 소스, **When** `grep "OnCardProcessedByGrowth.AddDynamic" Source/MadeByClaudeCode/Dream/`, **Then** 매치 1건 (ADR-0005 Stage 2 구독).

**Given** Project Settings → Game → Moss Baby → Dream Trigger, **When** 에디터 열람, **Then** MinDreamIntervalDays/MaxDreamsPerPlaythrough/MinimumGuaranteedDreams/EarliestDreamDay/DefaultDreamId 필드 표시.

## Test Evidence

- **Unit test**: `tests/unit/Dream/test_subsystem_initialize.cpp`
- **Grep gate**: Stage 1 구독 0건, Stage 2 구독 1건

## Dependencies

- `foundation-data-pipeline`
- `feature-growth-accumulation` Story 005 (FOnCardProcessedByGrowth delegate 선언)
- `core-game-state-machine` (FOnGameStateChanged delegate)
- `foundation-save-load` (FDreamState slice in UMossSaveData)
