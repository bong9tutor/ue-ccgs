# Story 009: Schema Guards + Edge Cases (CR-3 빈 Tags / CR-8 validator / EC coverage)

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §CR-3 (빈 Tags guard) + §EC 전반 + AC-GAE-03/11a/11b
- **TR-ID**: TR-growth-001/002 (edge cases)
- **Governing ADR**: [ADR-0010](../../../docs/architecture/adr-0010-ffinalformrow-storage-format.md) (FinalForm Validator)
- **Engine Risk**: LOW
- **Engine Notes**: `UEditorValidatorBase`는 `WITH_EDITOR` 가드 필수. 패키지 빌드 가드를 위해 런타임 `Initialize` 병행 검증 필수.
- **Control Manifest Rules**:
  - Foundation Layer §Forbidden Approaches — "Never define `int32`/`float`/`double` 수치 효과 필드 on `FGiftCardRow`" (C1 schema — Pipeline 소유이지만 Growth가 소비)
  - Global Rules §Cross-Cutting Constraints — "Editor-only 기능 런타임 호출 금지 — `#if WITH_EDITOR` 가드 필수"

## Acceptance Criteria

1. **AC-GAE-03** (AUTOMATED/BLOCKING) — `FGiftCardRow.Tags.Num() == 0` 행 검증 시:
   - 에디터 (`UEditorValidatorBase`): `EDataValidationResult::Invalid` (저장 차단)
   - 런타임 Initialize: `UE_LOG(Error)` + 해당 행 카탈로그 등록 스킵
2. **AC-GAE-11a** (AUTOMATED/BLOCKING) — `UMossFinalFormAsset.RequiredTagWeights` 비어있거나 합계=0인 자산 검증:
   - 에디터: `EDataValidationResult::Invalid`
   - 런타임: `UE_LOG(Error)` + Score 계산 스킵 (Default Form 폴백 합류)
3. **AC-GAE-11b** (AUTOMATED/ADVISORY) — `RequiredTagWeights` 합계 > 0이나 합계 ≠ 1.0 (±1e-3) → `Valid` + Warning
4. EC-11 가드: `GetLastCardDay()` consumer의 `max(0, DayIndex - GetLastCardDay())` 패턴 — MBC/Dream에서 호출 시 음수 방어 문서화 (본 story에서는 Growth-side는 행위 없음, cross-system contract 확인만)
5. EC-2 (단일 카드 1태그): `TagVector = {TagX: 1.0}` → V_norm = {1.0} → 정상 경로 (AC-GAE-06 기초 위)
6. EC-3 (모든 카드 동일 태그): `TagVector = {TagX: N}` → V_norm = {1.0} → Score 최대 Form 선택

## Implementation Notes

- **`UEditorValidatorBase` 2 subclass** (ADR-0010 + GDD §CR-3):
  ```cpp
  #if WITH_EDITOR
  UCLASS()
  class UGiftCardRowValidator : public UEditorValidatorBase {
      virtual EDataValidationResult ValidateLoadedAsset_Implementation(
          UObject* InAsset, TArray<FText>& ValidationErrors) override;
  };

  UCLASS()
  class UMossFinalFormAssetValidator : public UEditorValidatorBase {
      virtual EDataValidationResult ValidateLoadedAsset_Implementation(
          UObject* InAsset, TArray<FText>& ValidationErrors) override;
  };
  #endif
  ```
- **런타임 validator** (`Initialize` 내):
  ```cpp
  void UGrowthAccumulationSubsystem::ValidateFinalFormsAtRuntime() {
      TArray<FMossFinalFormData> Forms = Pipeline->GetAllGrowthFormRows();
      for (const auto& Form : Forms) {
          float Sum = 0;
          for (auto& Pair : Form.RequiredTagWeights) Sum += Pair.Value;
          if (Form.RequiredTagWeights.Num() == 0 || Sum < KINDA_SMALL_NUMBER) {
              UE_LOG(LogGrowth, Error, TEXT("Invalid FinalForm: %s"), *Form.FormId.ToString());
              // Score 계산 시 스킵 — EC-9와 동일 경로
          }
      }
  }
  ```
- EC-13: `RequiredTagWeights` 합 ≠ 1.0 런타임 fallback 불필요 (F3 argmax 상대 비교 유지)
- Pipeline `GetAllCardIds()` / `GetCardRow()` 일관성은 Data Pipeline R3 fail-close 계약 책임 (EC-8/EC-18)

## Out of Scope

- `UEditorValidatorBase` framework setup (`foundation-data-pipeline` 또는 프로젝트 공통)
- Pipeline fail-close 구현 (foundation-data-pipeline)
- MBC EC-11 `max(0, ...)` 가드 구현 (core-moss-baby-character — cross-system contract만)

## QA Test Cases

**Given** 에디터 + `FGiftCardRow`에 `Tags = []` 행, **When** `UGiftCardRowValidator` 실행, **Then** `EDataValidationResult::Invalid` (AC-GAE-03 에디터).

**Given** 런타임 Initialize + DataTable에 `Tags = []` 행 포함, **When** `ValidateCardRowsAtRuntime`, **Then** `UE_LOG(Error)` 1회 + 해당 행 카탈로그 등록 스킵.

**Given** `UMossFinalFormAsset`의 `RequiredTagWeights = {}`, **When** 에디터 저장, **Then** `Invalid` (AC-GAE-11a).

**Given** `RequiredTagWeights = {Season.Spring: 0.7, Emotion.Calm: 0.4}` (합 1.1), **When** 에디터 저장, **Then** `Valid` + Warning "합계 1.0 권고" (AC-GAE-11b).

**Given** 단일 카드 `Tags=[Season.Spring]`, **When** EvaluateFinalForm, **Then** V_norm = {Season.Spring: 1.0}, 정상 Score 계산 (EC-2).

## Test Evidence

- **Unit test**: `tests/unit/Growth/test_schema_guards.cpp`
- **Editor test**: `tests/editor/test_validators.cpp` (WITH_EDITOR만)

## Dependencies

- Story 001 (Subsystem 골격)
- Story 004 (CR-5 + UMossFinalFormAsset)
- `foundation-data-pipeline` (DataTable + Asset Manager)
