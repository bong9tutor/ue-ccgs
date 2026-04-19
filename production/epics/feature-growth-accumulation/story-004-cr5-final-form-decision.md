# Story 004: CR-5 Day 21 Final Form 결정 + F2/F3 공식

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §CR-5 + §F2/F3 + AC-GAE-06/07/13/14/21/22
- **TR-ID**: TR-growth-002 (FinalForm storage), TR-growth-005 (F3 Score), TR-growth-006 (Day 21 accuracy)
- **Governing ADR**: [ADR-0010](../../../docs/architecture/adr-0010-ffinalformrow-storage-format.md) (`UMossFinalFormAsset : UPrimaryDataAsset`)
- **Engine Risk**: LOW (inherits MEDIUM from ADR-0002 cooked AssetManager)
- **Engine Notes**: `UAssetManager::GetPrimaryAssetIdList("FinalForm")` + sync load 완료 후 `FMossFinalFormData::FromAsset` 변환. `FMossFinalFormData` read-only view struct — Growth가 UObject 직접 노출 받지 않음.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — `FFinalFormRow` 격상 = `UMossFinalFormAsset : UPrimaryDataAsset` per-form
  - Feature Layer §Forbidden Approaches — "Never store `FFinalFormRow` as `UDataTable` rows" (ADR-0010)
  - Foundation Layer §Required Patterns — Pipeline pull API sync return

## Acceptance Criteria

1. **AC-GAE-06** (AUTOMATED/BLOCKING) — `TagVector = {Season.Spring: 3.0, Emotion.Calm: 2.0, Element.Water: 1.0}`, FormA `{Season.Spring: 0.70, Element.Water: 0.20, Emotion.Calm: 0.10}`, FormB `{Element.Water: 0.60, Emotion.Calm: 0.30, Season.Winter: 0.10}`, `FOnDayAdvanced(GameDurationDays)` 수신 → Score(FormA) ≈ 0.417 (±1e-4), Score(FormB) ≈ 0.201 (±1e-4), `FinalFormId == "FormA"`, `FOnFinalFormReached("FormA")` 1회
2. **AC-GAE-07** (AUTOMATED/BLOCKING) — `TotalCardsOffered == 0` + `DefaultFormId = "Default"` → `FinalFormId == "Default"`, `FOnFinalFormReached("Default")` 발행 (EC-1)
3. **AC-GAE-13** (AUTOMATED/BLOCKING) — F2 정규화: `TagVector = {3.0, 2.0, 1.0}` → V_norm ≈ {0.500, 0.333, 0.167}, 합 = 1.0 (±1e-5)
4. **AC-GAE-14** (AUTOMATED/BLOCKING) — F2 + F3 파이프라인 end-to-end: Score 값 AC-GAE-06과 동일
5. **AC-GAE-21** (AUTOMATED/BLOCKING) — EC-4: Score = 0.0 동점 시 FName 사전순 첫 번째 ("FormX")
6. **AC-GAE-22** (AUTOMATED/BLOCKING) — EC-12: Score 정확 동점 시 사전순 ("FormA")
7. `UMossFinalFormAsset` 정의: `FName FormId`, `TMap<FName, float> RequiredTagWeights`, `FText DisplayName`, `FSoftObjectPath MeshPath`, `FSoftObjectPath MaterialPresetPath`, `GetPrimaryAssetId() override`
8. `FMossFinalFormData::FromAsset(const UMossFinalFormAsset&)` view struct 변환 함수

## Implementation Notes

- **`UMossFinalFormAsset` 정의** (ADR-0010):
  ```cpp
  UCLASS(BlueprintType)
  class UMossFinalFormAsset : public UPrimaryDataAsset {
      UPROPERTY(EditAnywhere, AssetRegistrySearchable) FName FormId;
      UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="1.0"))
      TMap<FName, float> RequiredTagWeights;
      // ...
      virtual FPrimaryAssetId GetPrimaryAssetId() const override {
          return FPrimaryAssetId("FinalForm", FormId);
      }
  };
  ```
- **`DefaultEngine.ini`**: `[/Script/Engine.AssetManagerSettings] +PrimaryAssetTypesToScan=(PrimaryAssetType="FinalForm", ...)` (ADR-0010)
- **F2 정규화** (TotalWeight 가드): `if (TotalWeight < KINDA_SMALL_NUMBER) return DefaultFormId;`
- **F3 Score 계산**: 각 Form의 `RequiredTagWeights`에 대해 `Sum(V_norm[tag] * R[tag])` — `TMap::FindRef`는 missing tag 시 0.0f 반환 (EC-4 자연 처리)
- **argmax 동점**: `TArray<FName>` collect → `Sort(FName.LexicalLess)` → first (EC-12, EC-14)
- **`EvaluateFinalForm()` public API** — Growth 소유, CR-5 전용. `FGrowthState.FinalFormId` 기록 + `FOnFinalFormReached.Broadcast(FormId)` 발행
- **호출 경로 분리** (GDD §Interactions §4b): CR-5는 (a) `FOnDayAdvanced(GameDurationDays)` 수신 시 — Day 21 카드 미건넴 경로 / (b) CR-1 내부 Day 21 체크 경로 — Story 005 Stage 2가 `bIsDay21Complete` flag 기준으로 CR-5 수행. 본 story는 핵심 함수 + (a) 경로만 구현, (b) 경로는 Story 005에서 통합.
- `UEditorValidatorBase`로 `RequiredTagWeights` 합계 검증 — AC-GAE-11a/b (별도 story로 분리 가능, 여기서는 minimal implementation)

## Out of Scope

- Stage 2 `FOnCardProcessedByGrowth` broadcast (Story 005)
- Day 21 CR-1 → CR-5 → SaveAsync → Stage 2 통합 시퀀스 (Story 005)
- MaterialHints pull API (Story 006)
- EC-17 로드 복구 재발행 (Story 008)

## QA Test Cases

**Given** Accumulating + `TagVector = {Season.Spring: 3.0, Emotion.Calm: 2.0, Element.Water: 1.0}` + FormA/FormB asset, **When** `EvaluateFinalForm()` 호출, **Then** `FinalFormId == "FormA"`, Score(FormA) ≈ 0.417, Score(FormB) ≈ 0.201 (AC-GAE-06).

**Given** `TotalCardsOffered == 0`, **When** `FOnDayAdvanced(GameDurationDays)`, **Then** `FinalFormId == "Default"`, `FOnFinalFormReached("Default")` 1회 (AC-GAE-07).

**Given** `TagVector = {Emotion.Joy: 3.0, Emotion.Calm: 2.0}` + FormX `{Season.Spring: 0.70, Element.Water: 0.30}` + FormY `{Season.Winter: 0.50, Element.Earth: 0.50}`, **When** EvaluateFinalForm, **Then** Score(FormX) == 0.0, Score(FormY) == 0.0, `FinalFormId == "FormX"` (EC-4 사전순).

**Given** FormA + FormB 동일 RequiredTagWeights, **When** EvaluateFinalForm, **Then** `FinalFormId == "FormA"` (EC-12).

## Test Evidence

- **Unit test**: `tests/unit/Growth/test_cr5_final_form.cpp` + `test_f2_normalization.cpp` + `test_f3_form_score.cpp`
- **Asset validator**: `RequiredTagWeights.Num() > 0` + sum ≈ 1.0 check (AC-GAE-11a)
- **AC-DP-18 [5.6-VERIFY]** shared — FinalForm PrimaryAssetType cooked build check

## Dependencies

- Story 001 (Subsystem 골격)
- Story 003 (CR-4 — Mature 단계 전환 후 CR-5)
- `foundation-data-pipeline` (GetGrowthFormAsset / GetAllGrowthFormAssets API + FinalForm PrimaryAssetType 등록)
