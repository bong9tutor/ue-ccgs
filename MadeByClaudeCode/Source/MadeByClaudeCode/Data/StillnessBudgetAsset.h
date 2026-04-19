// Copyright Moss Baby
//
// StillnessBudgetAsset.h — Stillness 예산 slot 한계 자산
//
// Story-1-4: Data Pipeline Schema Types
// ADR-0002: Data Pipeline 컨테이너 선택 — Stillness Budget = UPrimaryDataAsset (single instance)
// ADR-0011: UDeveloperSettings 예외 — GDD가 PrimaryDataAsset으로 명시 → 유지
//
// 이 자산은 단일 인스턴스로 사용.
// Pipeline Initialize 시 1회 pull + 캐싱.
// multiple instance 금지 (ADR-0002 §Decision).
//
// GetPrimaryAssetId() → FPrimaryAssetId("StillnessBudget", GetFName())
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "StillnessBudgetAsset.generated.h"

/**
 * UStillnessBudgetAsset — Stillness 예산 slot 한계 (단일 인스턴스).
 *
 * GDD: design/gdd/stillness-budget.md §Tuning Knobs
 * 모션·파티클·사운드 동시 재생 한계를 정의.
 * bReducedMotionEnabled 접근성 플래그 포함.
 *
 * ADR-0011 예외: GDD가 PrimaryDataAsset으로 명시 → UDeveloperSettings 미사용.
 * 미래 revision에서 UDeveloperSettings 전환 검토 가능 (ADR-0011 §Exceptions 참조).
 *
 * PrimaryAssetId: FPrimaryAssetId("StillnessBudget", GetFName())
 */
UCLASS(BlueprintType)
class MADEBYCLAUDECODE_API UStillnessBudgetAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * 동시 재생 가능한 모션(애니메이션) 슬롯 수.
     * ClampMin/Max [1, 6]. 기본값 2.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slots",
              meta=(ClampMin="1", ClampMax="6"))
    int32 MotionSlots = 2;

    /**
     * 동시 재생 가능한 파티클 이펙트 슬롯 수.
     * ClampMin/Max [1, 6]. 기본값 2.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slots",
              meta=(ClampMin="1", ClampMax="6"))
    int32 ParticleSlots = 2;

    /**
     * 동시 재생 가능한 사운드 슬롯 수.
     * ClampMin/Max [1, 8]. 기본값 3.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slots",
              meta=(ClampMin="1", ClampMax="8"))
    int32 SoundSlots = 3;

    /**
     * 접근성 모션 감소 활성화 플래그.
     * true 시 모션 이펙트를 최소화. 기본값 false.
     * Pillar 1: Quiet Presence — 접근성 고려.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Accessibility")
    bool bReducedMotionEnabled = false;

    /**
     * PrimaryAssetId 반환 — UAssetManager 등록 지원.
     * ADR-0002: PrimaryAssetType = "StillnessBudget", Name = 에셋 FName.
     * DefaultEngine.ini PrimaryAssetTypesToScan 등록 필수 (Story 002).
     */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId("StillnessBudget", GetFName());
    }
};
