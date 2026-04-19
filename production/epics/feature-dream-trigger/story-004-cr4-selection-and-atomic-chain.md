# Story 004: CR-4 Dream Selection (argmax) + CR-1 Atomic Chain

> **Epic**: feature-dream-trigger
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/dream-trigger-system.md` §CR-4 Dream Selection + AC-DTS-07/08/11/12
- **TR-ID**: TR-dream-003 (CR-1 atomic — FDreamState + SaveAsync + FOnDreamTriggered)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) (Stage 2 → CR-1 atomic)
- **Engine Risk**: LOW
- **Engine Notes**: CR-4 갱신(1-3) + `SaveAsync(EDreamReceived)` + `FOnDreamTriggered` 발행은 동일 call stack — `co_await`/`AsyncTask`/`Tick` 분기 금지.
- **Control Manifest Rules**:
  - Foundation Layer §Required Patterns — "Save/Load는 per-trigger atomicity만 보장"
  - Feature Layer §Required Patterns — "Growth `OnCardOfferedHandler`의 마지막 statement" (유사 패턴 — Dream은 `OnCardProcessedByGrowthHandler`)

## Acceptance Criteria

1. **AC-DTS-07** (AUTOMATED/BLOCKING) — 후보 2개: Dream "A" Score=0.55, Dream "B" Score=0.72, 모두 미열람 → `SelectedDream.DreamId == "B"` (argmax)
2. **AC-DTS-08** (AUTOMATED/BLOCKING) — 후보 2개 Score=0.60 동점 → `SelectedDream == "A"` ("A" < "B" FName 사전순)
3. **AC-DTS-11** (AUTOMATED/BLOCKING) — 꿈 트리거 완료 후: `UnlockedDreamIds`에 선택 DreamId 추가, `LastDreamDay == CurrentDayIndex`, `DreamsUnlockedCount` += 1, `SaveAsync(EDreamReceived)` 1회 호출
4. **AC-DTS-12** (CODE_REVIEW/BLOCKING) — CR-4 구현 소스에서 `FDreamState` 갱신(1-3) → `SaveAsync` → `FOnDreamTriggered` 발행 순서가 단일 call stack, `co_await`/`AsyncTask` 개입 없음
5. CR-4 실행 순서:
   1. `FDreamState.UnlockedDreamIds.Add(SelectedDream.DreamId)`
   2. `FDreamState.LastDreamDay = CurrentDayIndex`
   3. `FDreamState.DreamsUnlockedCount += 1`
   4. `FDreamState.PendingDreamId = SelectedDream.DreamId`
   5. `SaveAsync(ESaveReason::EDreamReceived)` (동일 함수 스택)
   6. `OnDreamTriggered.Broadcast(SelectedDream.DreamId)` (마지막 statement)

## Implementation Notes

- **CR-4 구현** (GDD §CR-4 + §Interactions §4 원자성):
  ```cpp
  void UDreamTriggerSubsystem::EvaluateAndSelect() {
      TArray<const UDreamDataAsset*> Candidates;
      for (auto* Asset : Pipeline->GetAllDreamAssets()) {
          if (IsCandidate(Asset)) Candidates.Add(Asset);  // Story 002
      }
      if (Candidates.Num() == 0) return;  // 평가 종료

      // argmax + 사전순 타이브레이커 (AC-DTS-07/08)
      Candidates.Sort([this](const UDreamDataAsset& A, const UDreamDataAsset& B) {
          const float ScoreA = ComputeTagScore(&A, Growth->GetTagVector());
          const float ScoreB = ComputeTagScore(&B, Growth->GetTagVector());
          if (FMath::Abs(ScoreA - ScoreB) < KINDA_SMALL_NUMBER) {
              return A.DreamId.LexicalLess(B.DreamId);  // 동점 사전순
          }
          return ScoreA > ScoreB;  // 내림차순
      });
      const UDreamDataAsset* Selected = Candidates[0];

      // CR-4 Atomic Chain
      FDreamState.UnlockedDreamIds.Add(Selected->DreamId);  // 1
      FDreamState.LastDreamDay = CurrentDayIndex;  // 2
      FDreamState.DreamsUnlockedCount += 1;  // 3
      FDreamState.PendingDreamId = Selected->DreamId;  // PendingDreamId 갱신
      GetSaveLoad()->SaveAsync(ESaveReason::EDreamReceived);  // 5
      OnDreamTriggered.Broadcast(Selected->DreamId);  // 6 — 마지막 statement
  }
  ```
- **Atomic 계약** (AC-DTS-12): `co_await`, `AsyncTask`, `Tick` 분기 grep = 0건
- Broadcast 전에 SaveAsync 호출 순서 필수 — 앱이 중간에 종료되어도 `PendingDreamId`로 재시도 가능 (EC-5, Story 005)

## Out of Scope

- PendingDreamId 복구 + 재발행 (Story 005 — EC-5)
- Day 21 EC-1 강제 트리거 (Story 006)
- EC-8 GetAllDreamAssets 빈 배열 (Story 007)

## QA Test Cases

**Given** 후보 2개 Dream A(Score=0.55) + Dream B(Score=0.72), **When** EvaluateAndSelect, **Then** FOnDreamTriggered("B") 발행 (AC-DTS-07).

**Given** 후보 Dream A + B 동점 Score=0.60, **When** argmax, **Then** Broadcast("A") — 사전순 (AC-DTS-08).

**Given** 평가 진입 + 후보 1개 선택됨, **When** CR-4 실행 완료, **Then** UnlockedDreamIds에 추가, LastDreamDay=CurrentDayIndex, DreamsUnlockedCount+=1, SaveAsync 1회 (AC-DTS-11).

**Given** CR-4 구현 소스, **When** `grep -E "co_await|AsyncTask|Tick" EvaluateAndSelect`, **Then** 매치 0건 (AC-DTS-12).

## Test Evidence

- **Unit test**: `tests/unit/Dream/test_cr4_selection.cpp` + `test_atomic_chain.cpp`
- **CODE_REVIEW gate**: atomic chain grep 0건

## Dependencies

- Story 001 (Subsystem + FDreamState)
- Story 002 (IsCandidate + ComputeTagScore)
- Story 003 (rarity gates — 진입 조건)
- `foundation-save-load` (SaveAsync + ESaveReason::EDreamReceived)
