# Story 006: EC-1 Day 21 MinimumGuaranteedDreams Fallback

> **Epic**: feature-dream-trigger
> **Layer**: Feature
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/dream-trigger-system.md` §EC-1 (Day 21 강제 트리거) + §CR-2 Day 21 특별 처리 + §EC-2/EC-3 + AC-DTS-04
- **TR-ID**: TR-dream-005 (EC-1 MinimumGuaranteedDreams fallback)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) (Day 21 Stage 2 순서)
- **Engine Risk**: LOW
- **Engine Notes**: Day 21 카드 건넴 이후, `FOnFinalFormReached` 발행 전 강제 트리거 — Farewell 전환 시 Dream 표시 마지막 기회 (GDD §EC-1 "실행 순서").
- **Control Manifest Rules**:
  - Core Layer §Cross-Cutting Constraints — "꿈 없는 플레이스루 방지" (Pillar 3 최소 보장)

## Acceptance Criteria

1. **AC-DTS-04** (AUTOMATED/BLOCKING) — `DreamsUnlockedCount=0`, `MinimumGuaranteedDreams=3`, `MaxDreamsPerPlaythrough=5`, `CurrentDayIndex == GameDurationDays(21)` + `FOnCardProcessedByGrowth` 수신 → 태그 조건·희소성 게이트·EarliestDay 무관하게 **강제 트리거 1회**, `FOnDreamTriggered` 1회, `DreamsUnlockedCount == 1`
2. EC-1 선택 기준: 미열람 꿈 중 TagScore 최대, 모두 동일/0이면 FName 사전순 첫 번째, 미열람 없으면 `DefaultDreamId` 폴백, DefaultDreamId 비어있으면 강제 포기 + 로그
3. **EC-1 1회 제약**: Day 21 강제 트리거는 **1회만** — DreamsUnlockedCount < MinimumGuaranteedDreams 여전해도 추가 강제 없음
4. **EC-2**: `DreamsUnlockedCount >= MaxDreamsPerPlaythrough` + `DayIndex == 21` → Day 21 강제 트리거 미적용 (상한 이미 도달)
5. **EC-3** (Dream Exhaustion): `UnlockedDreamIds.Num() >= GetAllDreamAssets().Num()` → 후보 비음, 평가 종료, 미발행
6. Day 21 강제 실행 순서 (EC-1 GDD "실행 순서"): `FOnCardProcessedByGrowth` 수신 → CR-1 Day 21 guard 완화 (본 story 처리) → 강제 트리거 → `FOnFinalFormReached` 발행 전 완료

## Implementation Notes

- **Day 21 분기 처리** (GDD §CR-2 + §EC-1):
  ```cpp
  void UDreamTriggerSubsystem::OnCardProcessedByGrowthHandler(
      const FGrowthProcessResult& Result) {
      if (GSM->GetCurrentState() == EGameState::Farewell) return;
      if (CurrentDayIndex == 1) return;

      // Day 21 강제 트리거 분기 (EC-1)
      if (Result.bIsDay21Complete && FDreamState.DreamsUnlockedCount < DreamSettings->MinimumGuaranteedDreams
          && FDreamState.DreamsUnlockedCount < DreamSettings->MaxDreamsPerPlaythrough) {  // EC-2
          ForceGuaranteedDreamTrigger();
          return;
      }

      // 일반 경로 — Story 003 가드 체인
      if (Result.bIsDay21Complete) return;
      // ... (Story 003)
  }

  void UDreamTriggerSubsystem::ForceGuaranteedDreamTrigger() {
      TArray<const UDreamDataAsset*> Unseen;
      for (auto* Asset : Pipeline->GetAllDreamAssets()) {
          if (!FDreamState.UnlockedDreamIds.Contains(Asset->DreamId)) Unseen.Add(Asset);
      }
      const UDreamDataAsset* Selected = nullptr;
      if (Unseen.Num() > 0) {
          // TagScore argmax → 사전순 tiebreak
          Unseen.Sort(/* ... */);
          Selected = Unseen[0];
      } else if (!DreamSettings->DefaultDreamId.IsNone()) {
          Selected = Pipeline->GetDreamAsset(DreamSettings->DefaultDreamId);
      } else {
          UE_LOG(LogDream, Warning, TEXT("Day 21 Guaranteed failed — no unseen dream + DefaultDreamId empty"));
          return;
      }
      // CR-4 atomic chain (Story 004 경로 재사용)
      ApplyDreamSelection(Selected);
  }
  ```
- **EC-3 Dream Exhaustion**: 후보 0개 → 평가 종료 (Story 004 경로 자연 처리)
- `DefaultDreamId` knob (ADR-0011) — 기본 `NAME_None`, 디자이너 설정 권고

## Out of Scope

- `FOnFinalFormReached` 발행 순서 (feature-growth-accumulation Story 005 handles)
- Farewell 전환 (core-game-state-machine)
- MVP 콘텐츠 작성 (OQ-DTS-4 producer)

## QA Test Cases

**Given** `DreamsUnlockedCount=0, MinimumGuaranteedDreams=3, MaxDreamsPerPlaythrough=5, DayIndex=21` + `Result.bIsDay21Complete=true`, **When** Handler, **Then** 강제 트리거 1회, FOnDreamTriggered, DreamsUnlockedCount=1 (AC-DTS-04).

**Given** `DreamsUnlockedCount=5, DayIndex=21` (cap reached), **When** Handler, **Then** Day 21 강제 스킵, 미발행 (EC-2).

**Given** 모든 꿈 이미 UnlockedDreamIds에 있음 + DefaultDreamId 비어있음 + DayIndex=21 + guaranteed 미달, **When** ForceGuaranteedDreamTrigger, **Then** 미발행 + LogDream Warning (EC-3 + DefaultDreamId empty).

**Given** Day 21 guaranteed + 미열람 2개 + Score 동점, **When** argmax, **Then** FName 사전순 첫 번째 선택.

## Test Evidence

- **Integration test**: `tests/integration/Dream/test_day21_guaranteed.cpp` (AC-DTS-04)
- EC-2/EC-3 독립 unit tests

## Dependencies

- Story 001-004 (Dream Trigger core)
- `feature-growth-accumulation` Story 005 (bIsDay21Complete flag in FGrowthProcessResult)
- `foundation-data-pipeline` (GetAllDreamAssets, GetDreamAsset)
