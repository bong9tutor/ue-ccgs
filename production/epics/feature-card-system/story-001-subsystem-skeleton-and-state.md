# Story 001: Card Subsystem 골격 + State Machine + FOnCardOffered 정의

> **Epic**: feature-card-system
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/card-system.md` §States and Transitions + §CR-CS-4 `FOnCardOffered` 발행
- **TR-ID**: TR-card-001 (Stage 1 publisher)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) — Card는 Stage 1 `FOnCardOffered` 유일 publisher
- **Engine Risk**: LOW
- **Engine Notes**: `UGameInstanceSubsystem::Initialize()` 내 `InitializeDependency<UDataPipelineSubsystem>()` 호출 필수. `Source/MadeByClaudeCode/Card/` 배치.
- **Control Manifest Rules**:
  - Feature Layer §Forbidden Approaches — "Never broadcast `FOnCardOffered` outside Card System"
  - Cross-Layer §Required Patterns (Tuning Knob) — `UMossCardSystemSettings : UDeveloperSettings`
  - Global Rules §Naming Conventions — `UCardSystemSubsystem`, `ECardSystemState`

## Acceptance Criteria

1. `UCardSystemSubsystem : UGameInstanceSubsystem` 클래스 정의 + `Source/MadeByClaudeCode/Card/` 배치
2. `ECardSystemState { Uninitialized, Preparing, Ready, Offered }` enum 정의 (GDD §States)
3. `FOnCardOffered` delegate 선언: `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardOffered, FName, CardId)`
4. `FOnCardOffered` `UPROPERTY(BlueprintAssignable)` 필드 + Card System이 **유일 writer** — grep `"OnCardOffered.Broadcast" Source/MadeByClaudeCode/` = Card 파일만 매치
5. `Initialize()`에서 Data Pipeline 의존 선언 + `FOnDayAdvanced` 구독 + FOnCardProcessedByGrowth는 구독하지 않음 (Card는 Stage 1 단독 publisher)
6. `Deinitialize()`에서 모든 delegate 해제
7. `UMossCardSystemSettings : UDeveloperSettings` 정의 — `HandSize (3)`, `InSeasonWeight (3.0)`, `OffSeasonWeight (1.0)`, `ConsecutiveDaySuppression (0.5)` + `ConfigRestartRequired` 메타
8. 비영속 필드: `DailyHand: TArray<FName>`, `PreviousDayHand: TArray<FName>`, `CurrentDayIndex: int32 (초기값 -1)`, `CurrentState: ECardSystemState`, `bHandOffered: bool`

## Implementation Notes

- **비영속 설계 (CR-CS-5)**: DailyHand는 세이브 안 함. `PlaythroughSeed`만 `FSessionRecord`에 영속화 (Save/Load 소유).
- **CurrentDayIndex 초기값 -1** (EC-CS-19 방어): `FOnDayAdvanced(0)` 방어 guard에 필요
- ADR-0005 준수: Card는 `FOnCardProcessedByGrowth` (Stage 2) 구독 금지 — Card는 Stage 1 publisher only
- `FOnDayAdvanced(int32)` 구독자 — Time System 경유 (GSM 경유 불필요, GSM §Interaction 4)

## Out of Scope

- Eager 샘플링 구현 (Story 002)
- ConfirmOffer 검증 게이트 (Story 003)
- F-CS-1/2/3 공식 (Story 004)
- bHandOffered 복원 (Story 005)
- GSM Prepare/Ready 프로토콜 (Story 006)

## QA Test Cases

**Given** Fresh GameInstance, **When** Card Subsystem Initialize, **Then** Uninitialized 상태, Pipeline 의존 선언 완료, FOnDayAdvanced 구독 등록.

**Given** Card System 구현 소스, **When** `grep "OnCardOffered.Broadcast" Source/MadeByClaudeCode/`, **Then** Card System 파일만 매치 (Feature Layer §Forbidden Approaches).

**Given** Card 구현 소스, **When** `grep "OnCardProcessedByGrowth.AddDynamic" Source/MadeByClaudeCode/Card/`, **Then** 매치 0건 (ADR-0005 Card는 Stage 2 미구독).

**Given** Project Settings → Game → Moss Baby → Card System, **When** 에디터 열람, **Then** HandSize/InSeasonWeight/OffSeasonWeight 필드 표시.

## Test Evidence

- **Unit test**: `tests/unit/Card/test_subsystem_initialize.cpp`
- **Grep gate**: Card is unique writer of FOnCardOffered (AC-CS-17 관련)

## Dependencies

- `foundation-data-pipeline` (UDataPipelineSubsystem)
- `foundation-time-session` (FOnDayAdvanced publisher)
