# Story 001: Growth Subsystem 골격 + State Machine 초기화

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2–3시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §States and Transitions + §CR-7 (FGrowthState)
- **TR-ID**: TR-growth-001 (부분 — Subsystem skeleton prerequisite)
- **Governing ADR**: [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) (`UMossGrowthSettings`)
- **Engine Risk**: LOW — `UGameInstanceSubsystem` 표준 패턴
- **Engine Notes**: UE 5.6 `UGameInstanceSubsystem::Initialize()` 내 `Collection.InitializeDependency<UDataPipelineSubsystem>()` + Save/Load 선행 의존 필수. Deinitialize에서 delegate 해제.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — Stage 1 `FOnCardOffered` 구독 + Stage 2 `FOnCardProcessedByGrowth` 발행 보유자
  - Cross-Layer §Required Patterns (Tuning Knob) — `UMossGrowthSettings : UDeveloperSettings` 시스템 전역 상수
  - Global Rules §Naming Conventions — `UGrowthAccumulationSubsystem` 클래스명, `FGrowthState` struct

## Acceptance Criteria

1. `UGrowthAccumulationSubsystem : UGameInstanceSubsystem` 클래스 정의 + `Source/MadeByClaudeCode/Growth/` 배치
2. `FGrowthState` USTRUCT 정의 — `TagVector: TMap<FName, float>`, `CurrentStage: EGrowthStage`, `LastCardDay: int32`, `TotalCardsOffered: int32`, `FinalFormId: FName` (GDD §CR-7)
3. `EGrowthStage { Sprout, Growing, Mature, FinalForm }` enum 정의
4. Internal state machine `EGrowthInternalState { Uninitialized, Accumulating, FormDetermination, Resolved }` 정의
5. `Initialize()`에서 Data Pipeline + Save/Load 의존 선언, `FOnLoadComplete` 구독 → FGrowthState 복원 → Accumulating 또는 Resolved 상태 진입
6. `Deinitialize()`에서 모든 delegate 해제
7. `UMossGrowthSettings : UDeveloperSettings` 정의 — `GrowingDayThreshold (3)`, `MatureDayThreshold (5)`, `DefaultFormId ("Default")`
8. 모든 struct 필드 `UPROPERTY()` 표기 (GC 안전성)

## Implementation Notes

- `UGameInstanceSubsystem::Initialize` override 내에서만 `Collection.InitializeDependency` 호출 (Control Manifest Foundation Layer §Required Patterns)
- `FGrowthState`는 `UMossSaveData`의 sub-struct — 직렬화는 Save/Load 소유
- Uninitialized → Accumulating 전환 조건: `FOnLoadComplete` 수신 + `FinalFormId.IsNone()`
- Uninitialized → Resolved 직행: `FOnLoadComplete` + `!FinalFormId.IsNone()` (EC-17)
- ADR-0011 `ConfigRestartRequired` convention — Initialize에서 한 번 읽는 knob (GrowingDayThreshold, MatureDayThreshold)에 `meta=(ConfigRestartRequired="true")` 적용

## Out of Scope

- CR-1 태그 가산 로직 (Story 002)
- CR-4 단계 평가 로직 (Story 003)
- CR-5 Final Form 결정 로직 (Story 004)
- Stage 2 delegate 발행 (Story 005)
- Load 복구 재발행 EC-17 (Story 008)

## QA Test Cases

**Given** Fresh GameInstance, **When** Growth Subsystem Initialize 호출, **Then** Uninitialized 상태로 대기, Data Pipeline + Save/Load 의존 선언 완료.

**Given** Save/Load 복원 완료 (`FGrowthState` 기본값, FinalFormId 비어있음), **When** `FOnLoadComplete` 수신, **Then** Accumulating 상태로 전환, CurrentStage = Sprout.

**Given** Save/Load 복원 완료 (`FinalFormId = "FormA"`), **When** `FOnLoadComplete` 수신, **Then** Resolved 상태로 직행 (EC-17 preview).

**Given** Project Settings → Game → Moss Baby → Growth, **When** 에디터 열람, **Then** `GrowingDayThreshold`, `MatureDayThreshold`, `DefaultFormId` 필드 표시 + ConfigRestartRequired 마킹.

## Test Evidence

- **Unit test**: `tests/unit/Growth/test_subsystem_initialize.cpp`
- **Manifest rule grep**: `grep "class UGrowthAccumulationSubsystem.*UGameInstanceSubsystem" Source/MadeByClaudeCode/Growth/` = 1건
- **UDeveloperSettings check**: `grep "class UMossGrowthSettings.*UDeveloperSettings" Source/MadeByClaudeCode/Settings/` = 1건

## Dependencies

- `foundation-data-pipeline` Epic (UDataPipelineSubsystem Initialize + FOnLoadComplete)
- `foundation-save-load` Epic (UMossSaveData + FGrowthState slice)
