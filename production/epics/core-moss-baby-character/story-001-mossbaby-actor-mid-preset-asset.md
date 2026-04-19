# Story 001 — `AMossBaby` Actor + MID 세팅 + `UMossBabyPresetAsset` 스키마

> **Epic**: [core-moss-baby-character](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md) §CR-1 (성장 단계 시각 매핑) + §CR-2 (머티리얼 파라미터 세트) + §Visual/Audio Requirements
- **TR-ID**: TR-mbc-001 (AC-MBC-03 Material param 리터럴 금지 — `UMossBabyPresetAsset` 전용)
- **Governing ADR**: ADR-0011 §Exceptions — `UMossBabyPresetAsset : UPrimaryDataAsset` 유지 (per-instance 콘텐츠).
- **Engine Risk**: LOW (Lumen 영향 상속 — MBC 머티리얼이 GSM MPC 자동 참조. 직접 제어 없음)
- **Engine Notes**: MVP에서 Static Mesh + MaterialInstanceDynamic. 블렌드 셰이프·스켈레탈 리깅 범위 밖. 메시 교체 `SetStaticMesh()` 즉시 반환, 시각 반영은 다음 렌더 프레임. Lumen Surface Cache 재빌드로 1-2프레임 GI 흐려짐 가능 (소형 씬 허용).
- **Control Manifest Rules**: Cross-Layer Rules §Exceptions — `UMossBabyPresetAsset` 유지 (per-instance 콘텐츠). Global Rules §Naming Conventions — `A` prefix Actor (`AMossBaby`), `U` prefix UObject (`UMossBabyPresetAsset`).

## Acceptance Criteria

- **AC-MBC-03** [`CODE_REVIEW`/BLOCKING] — 6개 머티리얼 파라미터 정의 후 Source/ 하위 *.h, *.cpp grep으로 (1) SSS_Intensity, HueShift, EmissionStrength, Roughness, DryingFactor, AmbientMoodBlend 리터럴 float 초기화(= 0.xx 등) 검색, (2) `UMossBabyPresetAsset` 외부에서 `SetScalarParameterValue` 직접 호출 검색 시 위 패턴 0건 (CR-2).

## Implementation Notes

1. **`AMossBaby` Actor** (`Source/MadeByClaudeCode/Core/MossBaby/MossBaby.h/.cpp`):
   - `UCLASS()` + `public AActor`.
   - `UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> MeshComponent;`
   - `UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;`
   - `BeginPlay()`에서 `MID = MeshComponent->CreateDynamicMaterialInstance(0, BaseMaterial);`
   - `EndPlay(EEndPlayReason)`에서 `MID = nullptr` (GC 안전).
2. **`UMossBabyPresetAsset : UPrimaryDataAsset`** (`Source/MadeByClaudeCode/Core/MossBaby/MossBabyPresetAsset.h`):
   - `UCLASS()` + `public UPrimaryDataAsset`.
   - `USTRUCT FMossBabyMaterialPreset` = GDD §CR-2 6 parameters × 5 columns (Sprout, Growing, Mature, Final, Drying=1).
   - `UPROPERTY(EditAnywhere) TMap<EGrowthStage, FMossBabyMaterialPreset> StagePresets;` 
   - `UPROPERTY(EditAnywhere) FMossBabyMaterialPreset QuietRestPreset;` — Drying=1 컬럼.
   - `GetPrimaryAssetId()` → `FPrimaryAssetId("MossBabyPreset", AssetName)`.
3. **파라미터 setter 경유 — `ApplyPreset` API**:
   - `void AMossBaby::ApplyPreset(const FMossBabyMaterialPreset& Preset);` 
   - 내부 `MID->SetScalarParameterValue("SSS_Intensity", Preset.SSS);` 등 6개 호출.
   - **다른 경로로 `SetScalarParameterValue` 호출 금지** (AC-MBC-03 grep) — CI 검증.
4. **정적 메시 기본 레퍼런스** (Sprout):
   - `UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> SproutMesh;` — Hard reference 대신 soft로 패키지 간 sync 보호. BeginPlay 시 `StreamableManager::LoadSynchronous` 호출.

## Out of Scope

- State machine (story 002)
- Lerp 구현 (story 003)
- DryingFactor 계산 (story 005)
- Growth Stage transitions (story 004)

## QA Test Cases

**Test Case 1 — AC-MBC-03 Literal grep**
- **Setup (1) Literal init grep**: `grep -rnE "(SSS_Intensity|HueShift|EmissionStrength|Roughness|DryingFactor|AmbientMoodBlend).*=.*[0-9]+\.[0-9]+" Source/MadeByClaudeCode/Core/MossBaby/ --include="*.cpp" --include="*.h"`
- **Setup (2) MID setter grep**: `grep -rn "SetScalarParameterValue\|SetVectorParameterValue" Source/MadeByClaudeCode/Core/MossBaby/` 
- **Verify**: (1) 매치 0건. (2) 매치는 `AMossBaby::ApplyPreset` 구현부에만 한정 (다른 클래스/함수 0건).
- **Pass**: 두 grep 조건 모두 충족.

**Test Case 2 — MID creation lifecycle**
- **Setup**: PIE 실행 → `AMossBaby` spawn → `BeginPlay` 완료.
- **Verify**: `MID` non-null. `MeshComponent->GetMaterial(0) == MID`.
- **Pass**: 둘 다 true.

**Test Case 3 — Preset Asset schema validation**
- **Setup**: Editor에서 `UMossBabyPresetAsset` 인스턴스 생성 → `StagePresets[EGrowthStage::Sprout]` 편집.
- **Verify**: 6 파라미터 UPROPERTY 모두 편집 가능. `ClampMin`/`ClampMax` 범위 (GDD §CR-2 범위).
- **Pass**: Editor UI 확인.

## Test Evidence

- [ ] `tests/unit/moss-baby/preset_asset_schema_test.cpp` — TMap 편집 + GetPrimaryAssetId
- [ ] `tests/unit/moss-baby/mid_lifecycle_test.cpp` — BeginPlay/EndPlay
- [ ] `tests/unit/moss-baby/ac_mbc_03_grep.md` — AC-MBC-03 grep 명령 문서화

## Dependencies

- **Depends on**: Foundation Data Pipeline (for PrimaryAssetTypesToScan registration)
- **Unlocks**: story-002 (state machine), story-003 (Lerp), story-004 (growth transitions)
