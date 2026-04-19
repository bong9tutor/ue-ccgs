# Story 006: CR-6 MaterialHints 계산 + GetMaterialHints() Pull API

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §CR-6 + §F4 + AC-GAE-08/09/15
- **TR-ID**: TR-growth-001 (CR-1 step 5 MaterialHints 재계산)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) — "Never call Growth pull API from MBC inside Growth Handler window" (Layer Boundary)
- **Engine Risk**: LOW
- **Engine Notes**: Growth는 **raw 오프셋**만 반환. MBC가 프리셋 + 오프셋 합산 + clamp.
- **Control Manifest Rules**:
  - Feature Layer §Forbidden Approaches — "Never call Growth pull API from MBC inside Growth Handler window. `GetMaterialHints()`는 Reacting 후 별도 이벤트에서 pull"
  - Cross-Layer §Forbidden Approaches — "Hardcoded 문자열 경로 금지" — `HueShiftOffsetMap` + `SSSOffsetMap`은 `UGrowthConfigAsset` 외부화

## Acceptance Criteria

1. **AC-GAE-08** (AUTOMATED/BLOCKING) — `TagVector = {Season.Spring: 6.0, Season.Summer: 2.0, Element.Water: 1.0, Emotion.Calm: 1.0}` + HueShift_offset[Season.Spring]=+0.02, SSS_offset[Season.Spring]=+0.05, `GetMaterialHints()` 호출 시: DominantCategory == Season, DominantTag == Season.Spring, DominanceRatio ≈ 0.80 (±1e-5), HueShiftOffset ≈ 0.016 (±1e-5), SSSOffset ≈ 0.040 (±1e-5). **Raw 오프셋 반환 — MBC 프리셋·clamp 미포함**
2. **AC-GAE-09** (AUTOMATED/BLOCKING) — `TotalWeight == 0` → HueShiftOffset == 0.0, SSSOffset == 0.0, DominanceRatio == 0.0
3. **AC-GAE-15** (AUTOMATED/BLOCKING) — DominanceRatio = 1.0 + offset[Spring]={0.02, 0.05} → HueShiftOffset = 0.02, SSSOffset = 0.05 (Growth raw 반환)
4. `FGrowthMaterialHints` struct: `float HueShiftOffset`, `float SSSOffset`, `FName DominantCategory`, `float DominanceRatio`
5. `GetMaterialHints() const -> FGrowthMaterialHints` public API
6. EC-14: DominantCategory/DominantTag 동점 시 FName 사전순 첫 번째
7. CR-1 step 5에서 MaterialHints 재계산 호출 (캐싱) — 카드 건넴마다 갱신

## Implementation Notes

- **F4 공식 구현**:
  ```cpp
  FGrowthMaterialHints UGrowthAccumulationSubsystem::GetMaterialHints() const {
      FGrowthMaterialHints Out;
      if (TotalWeight < KINDA_SMALL_NUMBER) return Out;  // AC-GAE-09

      // CategoryWeight 집계
      TMap<FName, float> CategoryWeight;
      for (const auto& Pair : FGrowthState.TagVector) {
          FName Category = ParseCategory(Pair.Key);  // "Season.Spring" → "Season"
          CategoryWeight.FindOrAdd(Category) += Pair.Value;
      }

      // DominantCategory = argmax, 사전순 타이브레이커 (EC-14)
      Out.DominantCategory = LexicallyFirstArgMax(CategoryWeight);
      Out.DominanceRatio = CategoryWeight[Out.DominantCategory] / TotalWeight;

      // DominantTag within category
      FName DominantTag = LexicallyFirstArgMaxInCategory(FGrowthState.TagVector, Out.DominantCategory);

      // Offset lookup (UGrowthConfigAsset)
      const auto* Config = GetGrowthConfig();
      const float HueOffset = Config->HueShiftOffsetMap.FindRef(DominantTag);  // missing → 0.0f
      const float SSSOffset = Config->SSSOffsetMap.FindRef(DominantTag);

      Out.HueShiftOffset = HueOffset * Out.DominanceRatio;
      Out.SSSOffset = SSSOffset * Out.DominanceRatio;
      return Out;
  }
  ```
- **Raw 오프셋만 반환**: `HueShift_preset + HueShift_offset × DominanceRatio` 중 `HueShift_offset × DominanceRatio` 만. MBC가 프리셋 + clamp 책임 (GDD §CR-6 + AC-GAE-15)
- **Layer Boundary (ADR-0005 §Forbidden)**: MBC는 Reacting 진입 후 **별도 이벤트**에서 pull — Growth `OnCardOfferedHandler` 중에 호출 불가
- CR-1 step 5: MaterialHints는 매 카드 건넴마다 재계산. 본 story에서는 `GetMaterialHints()` compute-on-demand 방식 허용 — 추후 최적화 시 캐싱 고려
- `HueShiftOffsetMap` / `SSSOffsetMap`은 `UGrowthConfigAsset` (per-content — ADR-0011 UDeveloperSettings 예외)

## Out of Scope

- MBC 측 프리셋 + 오프셋 합산 + clamp (core-moss-baby-character epic)
- Element/Emotion 오프셋 확장 (OQ-GAE-3 — Full scope)
- Cache invalidation 최적화

## QA Test Cases

**Given** `TagVector = {Season.Spring: 6.0, Season.Summer: 2.0, Element.Water: 1.0, Emotion.Calm: 1.0}` + Config offsets, **When** `GetMaterialHints()`, **Then** DominantCategory==Season, DominantTag==Season.Spring, DominanceRatio≈0.80, HueShiftOffset≈0.016, SSSOffset≈0.040 (AC-GAE-08).

**Given** `TotalWeight == 0`, **When** `GetMaterialHints()`, **Then** 모든 필드 0.0 (AC-GAE-09).

**Given** `TagVector = {Season.Spring: 2.0, Element.Water: 2.0}` (DominantCategory 동점), **When** GetMaterialHints, **Then** `DominantCategory == "Element"` (사전순 "Element" < "Season") (EC-14).

**Given** `TagVector = {Season.Spring: 3.0, Season.Summer: 3.0}` (카테고리 내 DominantTag 동점), **When** GetMaterialHints, **Then** DominantTag == "Season.Spring" (사전순) (EC-14 확장).

## Test Evidence

- **Unit test**: `tests/unit/Growth/test_f4_material_hints.cpp`
- DominantCategory/Tag tiebreaker 테스트 필수

## Dependencies

- Story 001 (Subsystem 골격)
- Story 002 (CR-1 — step 5 호출 지점)
- `foundation-data-pipeline` (UGrowthConfigAsset loading)
