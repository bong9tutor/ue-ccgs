// Copyright Moss Baby
//
// DreamDataAsset.h — 개별 꿈 자산 타입
//
// Story-1-4: Data Pipeline Schema Types
// ADR-0002: Data Pipeline 컨테이너 선택 — Dream = UPrimaryDataAsset per dream
// ADR-0011: UDeveloperSettings 예외 — per-instance 트리거 임계는 UPrimaryDataAsset 유지
//
// C2 Schema Gate (ADR-0002):
//   모든 Dream 트리거 임계를 코드에 하드코딩 금지.
//   TriggerThreshold, TriggerTagWeights, EarliestDay는 반드시 UPROPERTY(EditAnywhere)로 노출.
//   Pillar 3 보호 — 꿈은 예측 불가능해야 하며, 임계값이 에셋 단위로 다를 수 있다.
//
// GetPrimaryAssetId() → FPrimaryAssetId("DreamData", DreamId)
// DefaultEngine.ini PrimaryAssetTypesToScan 등록 필수 (Story 002 — Out of Scope).
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/GrowthStage.h"
#include "DreamDataAsset.generated.h"

/**
 * UDreamDataAsset — 개별 꿈 자산 (ADR-0002 C2 schema gate).
 *
 * GDD: design/gdd/dream-trigger-system.md
 * 각 꿈은 별도 .uasset으로 Content/Data/Dreams/ 에 저장.
 * 텍스트 작가 + 트리거 조건을 자산 단위로 병행 편집 가능.
 *
 * C2 schema gate: 모든 트리거 임계는 UPROPERTY로 외부 노출 필수 (하드코딩 금지).
 * TriggerThreshold, TriggerTagWeights, EarliestDay — 꿈별로 다른 값 허용.
 *
 * PrimaryAssetId: FPrimaryAssetId("DreamData", DreamId)
 */
UCLASS(BlueprintType)
class MADEBYCLAUDECODE_API UDreamDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * 꿈 고유 식별자. PrimaryAssetId 생성 + Pipeline ContentRegistry 키로 사용.
     * AssetRegistrySearchable: 에디터에서 DreamId로 검색 가능.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, AssetRegistrySearchable, Category="Dream")
    FName DreamId;

    /**
     * 꿈 본문 텍스트. 텍스트 작가가 자산 단위로 편집.
     * FText — 로컬라이제이션 지원.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dream")
    FText BodyText;

    /**
     * 꿈 발화 트리거 태그 가중치 맵 (C2 schema gate — UPROPERTY 노출 필수).
     * 각 태그가 이 꿈을 발화시키는 데 기여하는 가중치.
     * ClampMin/Max [0.0, 1.0].
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dream Trigger",
              meta=(ClampMin="0.0", ClampMax="1.0"))
    TMap<FName, float> TriggerTagWeights;

    /**
     * 꿈 발화 임계값 (C2 schema gate — UPROPERTY 노출 필수, 하드코딩 금지).
     * TagVector 내적 점수가 이 임계를 초과할 때 발화 후보.
     * 기본값 0.6. ClampMin/Max [0.0, 1.0].
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dream Trigger",
              meta=(ClampMin="0.0", ClampMax="1.0"))
    float TriggerThreshold = 0.6f; // C2 schema gate — UPROPERTY only

    /**
     * 이 꿈이 발화될 수 있는 최소 성장 단계.
     * 임시 include: Data/GrowthStage.h (core-moss-baby-character epic 진입 시 이동 예정).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dream Trigger")
    EGrowthStage RequiredStage;

    /**
     * 발화 가능 최조 일수 (C2 schema gate — UPROPERTY 노출 필수).
     * DayIndex >= EarliestDay여야 발화 후보.
     * ClampMin/Max [1, 21].
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dream Trigger",
              meta=(ClampMin="1", ClampMax="21"))
    int32 EarliestDay = 2;

    /**
     * PrimaryAssetId 반환 — UAssetManager type-bulk 로드 지원.
     * ADR-0002: PrimaryAssetType = "DreamData", Name = DreamId.
     * DefaultEngine.ini PrimaryAssetTypesToScan 등록 필수 (Story 002).
     */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("DreamData", DreamId);
    }
};
