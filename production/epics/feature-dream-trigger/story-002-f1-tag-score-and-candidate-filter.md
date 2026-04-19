# Story 002: F1 TagScore 공식 + CR-3 후보 필터링

> **Epic**: feature-dream-trigger
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2.5시간

## Context

- **GDD Reference**: `design/gdd/dream-trigger-system.md` §F1 TagScore + §CR-3 후보 수집 + AC-DTS-05/06/09/10/16
- **TR-ID**: TR-dream-006 (UDreamDataAsset C2 schema gate — 리터럴 금지)
- **Governing ADR**: [ADR-0002](../../../docs/architecture/adr-0002-data-pipeline-container-selection.md) (UDreamDataAsset C2), [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) (Stage 2 최신 TagVector 보장)
- **Engine Risk**: LOW
- **Engine Notes**: Growth `GetTagVector()` pull — Stage 2 구독 덕분에 최신 TagVector 보장.
- **Control Manifest Rules**:
  - Foundation Layer §Forbidden Approaches — "Never hardcode Dream trigger 임계 in code — `UDreamDataAsset`의 UPROPERTY(EditAnywhere) 필드(`TriggerThreshold`, `TriggerTagWeights`)만 사용" (C2 schema gate)

## Acceptance Criteria

1. **AC-DTS-05** (AUTOMATED/BLOCKING) — `TagVector = {Season.Spring: 6, Emotion.Calm: 3, Element.Water: 1}` (TotalWeight=10), Dream "봄비" `TriggerTagWeights = {Season.Spring: 0.70, Emotion.Calm: 0.20, Element.Water: 0.10}`, `TriggerThreshold = 0.50` → `TagScore ≈ 0.509` (±1e-3) ≥ 0.50 → 후보 등록
2. **AC-DTS-06** (AUTOMATED/BLOCKING) — 동일 TagVector, Dream "어둠" `TriggerTagWeights = {Element.Earth: 0.80, Emotion.Melancholy: 0.20}` (태그 없음), `TriggerThreshold = 0.30` → `TagScore == 0.0` < 0.30 → 미등록
3. **AC-DTS-09** (AUTOMATED/BLOCKING) — EGrowthStage.Sprout, Dream "성숙의 꿈" `RequiredStage = Mature`, TagScore 충족 → 후보 미등록 (CR-3 조건 2)
4. **AC-DTS-10** (AUTOMATED/BLOCKING) — 후보 목록에 이미 UnlockedDreamIds에 있는 꿈 포함 → 제외 (CR-3 조건 1)
5. **AC-DTS-16** (AUTOMATED/BLOCKING) — `TotalWeight == 0` → `TagScore == 0.0` (F1 KINDA_SMALL_NUMBER 가드)
6. CR-3 조건 4: `DayIndex >= max(asset.EarliestDay, EarliestDreamDay)` 필터
7. `UDreamDataAsset` 정의 — `FName DreamId`, `FText BodyText`, `TMap<FName, float> TriggerTagWeights`, `float TriggerThreshold`, `EGrowthStage RequiredStage`, `int32 EarliestDay`

## Implementation Notes

- **F1 TagScore 구현** (GDD §F1 + KINDA_SMALL_NUMBER 가드):
  ```cpp
  float UDreamTriggerSubsystem::ComputeTagScore(const UDreamDataAsset* Asset,
                                                const TMap<FName, float>& V) const {
      float TotalWeight = 0;
      for (auto& Pair : V) TotalWeight += Pair.Value;
      if (TotalWeight < KINDA_SMALL_NUMBER) return 0.0f;  // AC-DTS-16

      TMap<FName, float> VNorm;
      for (auto& Pair : V) VNorm.Add(Pair.Key, Pair.Value / TotalWeight);

      float Score = 0;
      for (auto& Pair : Asset->TriggerTagWeights) {
          Score += VNorm.FindRef(Pair.Key) * Pair.Value;  // Missing → 0
      }
      return Score;
  }
  ```
- **CR-3 4-조건 필터**:
  ```cpp
  bool UDreamTriggerSubsystem::IsCandidate(const UDreamDataAsset* Asset) const {
      if (FDreamState.UnlockedDreamIds.Contains(Asset->DreamId)) return false;  // 조건 1
      if (Asset->RequiredStage != EGrowthStage::None &&
          Asset->RequiredStage != Growth->GetCurrentStage()) return false;  // 조건 2
      const float Score = ComputeTagScore(Asset, Growth->GetTagVector());
      if (Score < Asset->TriggerThreshold) return false;  // 조건 3
      const int32 EffectiveEarliest = FMath::Max(DreamSettings->EarliestDreamDay, Asset->EarliestDay);
      if (CurrentDayIndex < EffectiveEarliest) return false;  // 조건 4
      return true;
  }
  ```
- **`UDreamDataAsset` PrimaryAssetType 등록** (DefaultEngine.ini) — `foundation-data-pipeline` Epic에서 등록
- **C2 schema gate**: `TriggerThreshold`, `TriggerTagWeights` 코드 리터럴 grep = 0건 — 모든 값은 DataAsset UPROPERTY

## Out of Scope

- 희소성 게이트 (Story 003)
- Dream Selection argmax (Story 004)
- Day 21 EC-1 fallback (Story 006)
- `UEditorValidatorBase` (TriggerThreshold > 0, TriggerTagWeights.Num() > 0 검증) — minimal implementation here, detailed Story 007

## QA Test Cases

**Given** `TagVector = {Season.Spring:6, Emotion.Calm:3, Element.Water:1}` + Dream "봄비" `TTW = {Season.Spring:0.70, Emotion.Calm:0.20, Element.Water:0.10}` + `TriggerThreshold=0.50`, **When** ComputeTagScore, **Then** `Score ≈ 0.509` ≥ 0.50 → 후보 (AC-DTS-05).

**Given** 동일 TagVector + Dream "어둠" `TTW = {Element.Earth:0.80, Emotion.Melancholy:0.20}`, **When** ComputeTagScore, **Then** `Score == 0.0` < 0.30 → 미등록 (AC-DTS-06).

**Given** Sprout + Dream "성숙" `RequiredStage=Mature`, **When** CR-3 필터, **Then** 미등록 (AC-DTS-09).

**Given** `UnlockedDreamIds=["DreamA"]` + Dream "DreamA" 후보, **When** CR-3 조건 1, **Then** 제외 (AC-DTS-10).

**Given** `TotalWeight == 0`, **When** ComputeTagScore, **Then** `Score == 0.0` (AC-DTS-16).

**Given** Dream System 구현 소스, **When** `grep -E "TriggerThreshold\s*=\s*[0-9]" Source/MadeByClaudeCode/Dream/*.cpp`, **Then** 0건 (C2 schema gate).

## Test Evidence

- **Unit test**: `tests/unit/Dream/test_f1_tag_score.cpp` + `test_cr3_candidate_filter.cpp`
- **C2 grep**: 코드 리터럴 0건

## Dependencies

- Story 001 (Subsystem)
- `feature-growth-accumulation` (GetTagVector, GetCurrentStage pull APIs)
- `foundation-data-pipeline` (GetAllDreamAssets, UDreamDataAsset registration)
