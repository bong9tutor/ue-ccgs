// Copyright Moss Baby
//
// MossFinalFormData.h — UMossFinalFormAsset의 read-only view struct
//
// Story-1-5: FMossFinalFormData 선언
// ADR-0010: FFinalFormRow 저장 형식 — 외부에는 struct view만 노출, UObject 직접 참조 회피
// ADR-0002: FinalForm = UMossFinalFormAsset (UPrimaryDataAsset per-form)
//
// 라이프사이클 격리 원칙 (ADR-0010):
//   UMossFinalFormAsset(UObject)을 직접 반환하면 GC 수명에 caller가 종속됨.
//   FMossFinalFormData는 Plain Old Data 수준의 view struct로, UObject 수명과 무관.
//   Downstream 시스템은 FMossFinalFormData만 보관 — UMossFinalFormAsset 포인터 직접 캐싱 금지.
//
// FromAsset() 정적 팩토리:
//   UMossFinalFormAsset → FMossFinalFormData 변환.
//   nullptr 안전 (Asset == nullptr 시 기본값 반환).
//   Story 1-6에서 실제 카탈로그 등록 후 변환 검증 예정.
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#pragma once
#include "CoreMinimal.h"
#include "Data/MossFinalFormAsset.h"
#include "MossFinalFormData.generated.h"

/**
 * FMossFinalFormData — UMossFinalFormAsset의 read-only view struct.
 *
 * ADR-0010: 외부(Downstream)에는 이 struct view만 노출.
 * UMossFinalFormAsset(UObject) 직접 참조 회피 → 라이프사이클 격리.
 *
 * 사용 예:
 *   TOptional<FMossFinalFormData> FormData = Pipeline->GetGrowthFormRow(FormId);
 *   if (FormData.IsSet())
 *   {
 *       float Weight = FormData->RequiredTagWeights.FindRef(TagName);
 *   }
 *
 * Growth F3 Score Formula (GDD §Growth Accumulation Engine):
 *   Score = Σ V_norm[tag] × RequiredTagWeights[tag]  (태그 벡터 내적)
 */
USTRUCT(BlueprintType)
struct MADEBYCLAUDECODE_API FMossFinalFormData
{
    GENERATED_BODY()

    /** 형태 고유 식별자. Pipeline FormRegistry 키와 동일. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Form")
    FName FormId;

    /**
     * 형태 달성을 위한 이상적 태그 분포.
     * Growth F3 Score Formula에서 내적 계산에 사용.
     * 각 태그 가중치 범위 [0.0, 1.0]. 합계 1.0 권고.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Form")
    TMap<FName, float> RequiredTagWeights;

    /** 형태 표시 이름 (UI 렌더링용). FText — 로컬라이제이션 지원. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Form")
    FText DisplayName;

    /**
     * 형태별 Static Mesh 경로 (Soft Reference).
     * FinalReveal 시 SM_MossBaby_Final_[ID] 교체에 사용.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Form")
    FSoftObjectPath MeshPath;

    /**
     * 형태별 머티리얼 프리셋 경로 (Soft Reference).
     * FinalReveal 시 머티리얼 전환에 사용.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Form")
    FSoftObjectPath MaterialPresetPath;

    /**
     * UMossFinalFormAsset → FMossFinalFormData 변환 팩토리 (ADR-0010).
     *
     * nullptr 안전: Asset == nullptr 시 기본값(FormId == NAME_None) struct 반환.
     * UObject 수명에 무관한 순수 data copy.
     *
     * @param Asset  변환할 UMossFinalFormAsset. nullptr 허용.
     * @return       복사된 FMossFinalFormData view.
     */
    static FMossFinalFormData FromAsset(const UMossFinalFormAsset* Asset)
    {
        FMossFinalFormData Data;
        if (Asset)
        {
            Data.FormId               = Asset->FormId;
            Data.RequiredTagWeights   = Asset->RequiredTagWeights;
            Data.DisplayName          = Asset->DisplayName;
            Data.MeshPath             = Asset->MeshPath;
            Data.MaterialPresetPath   = Asset->MaterialPresetPath;
        }
        return Data;
    }
};
