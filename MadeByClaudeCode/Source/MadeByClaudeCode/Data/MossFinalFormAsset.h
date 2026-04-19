// Copyright Moss Baby
//
// MossFinalFormAsset.h — per-final-form 자산 타입
//
// Story-1-4: Data Pipeline Schema Types
// ADR-0010: FFinalFormRow 저장 형식 — UPrimaryDataAsset 격상
// ADR-0002: Data Pipeline 컨테이너 선택 (FinalForm은 ADR-0010에서 DataTable → DataAsset 격상)
//
// 격상 이유 (ADR-0010 §Context):
//   UE DataTable 에디터는 USTRUCT 내 TMap 인라인 편집 미지원.
//   RequiredTagWeights: TMap<FName, float>는 UPrimaryDataAsset에서만 인라인 편집 가능.
//   또한 각 형태별 Mesh/Material 경로(SoftPath)가 존재하므로 DataAsset이 자연스러운 형태.
//
// GetPrimaryAssetId() → FPrimaryAssetId("FinalForm", FormId)
// DefaultEngine.ini PrimaryAssetTypesToScan 등록 필수 (Story 002 — Out of Scope).
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MossFinalFormAsset.generated.h"

/**
 * UMossFinalFormAsset — per-final-form 자산 (ADR-0010).
 *
 * GDD: design/gdd/growth-accumulation-engine.md §CR-8 / §FinalForm
 * 각 최종 형태는 별도 .uasset으로 Content/Data/FinalForms/ 에 저장.
 *
 * TMap<FName, float> RequiredTagWeights — 태그 벡터 인라인 편집 가능.
 * Growth F3 Score Formula: Σ V_norm[tag] × R[tag] (내적 계산).
 * RequiredTagWeights 합계 1.0 권고 (UEditorValidatorBase 경고 — Growth sprint 구현 예정).
 *
 * PrimaryAssetId: FPrimaryAssetId("FinalForm", FormId)
 */
UCLASS(BlueprintType)
class MADEBYCLAUDECODE_API UMossFinalFormAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * 형태 고유 식별자. PrimaryAssetId 생성 + Pipeline ContentRegistry 키로 사용.
     * AssetRegistrySearchable: 에디터에서 FormId로 검색 가능.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category="Form")
    FName FormId;

    /**
     * 형태의 이상적 태그 분포.
     * Growth F3 Score Formula에서 내적 계산에 사용.
     * ClampMin/Max [0.0, 1.0] — 각 태그 가중치 범위 제한.
     * ADR-0010: DataAsset이라 TMap 인라인 편집 가능 (DataTable에서는 불가).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Form",
              meta=(ClampMin="0.0", ClampMax="1.0"))
    TMap<FName, float> RequiredTagWeights;

    /** 형태 표시 이름 (UI 렌더링용). FText — 로컬라이제이션 지원. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Form")
    FText DisplayName;

    /**
     * 형태별 고유 Static Mesh 경로 (Soft Reference — 필요 시 로드).
     * MBC GDD CR-1 FinalReveal: SM_MossBaby_Final_[ID] 교체 시 참조.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Form",
              meta=(AllowedClasses="StaticMesh"))
    FSoftObjectPath MeshPath;

    /**
     * 형태별 머티리얼 프리셋 경로 (Soft Reference — 필요 시 로드).
     * MBC GDD CR-1 FinalReveal: 머티리얼 전환 시 참조.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Form",
              meta=(AllowedClasses="MaterialInstance"))
    FSoftObjectPath MaterialPresetPath;

    /**
     * PrimaryAssetId 반환 — UAssetManager type-bulk 로드 지원.
     * ADR-0010: PrimaryAssetType = "FinalForm", Name = FormId.
     * DefaultEngine.ini PrimaryAssetTypesToScan 등록 필수 (Story 002).
     */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("FinalForm", FormId);
    }
};
