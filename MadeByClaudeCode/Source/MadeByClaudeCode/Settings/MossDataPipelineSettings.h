// Copyright Moss Baby
//
// MossDataPipelineSettings.h — Data Pipeline System 전역 Tuning Knobs
//
// Story-1-4: Data Pipeline Schema Types
// ADR-0011: Tuning Knob 노출 방식 — UDeveloperSettings 정식 채택
// ADR-0002: Data Pipeline 컨테이너 선택 §Performance Budget Knobs
//
// ADR-0011 §Naming Convention:
//   U[SystemName]Settings : public UDeveloperSettings
//   GetCategoryName() → "Moss Baby"
//   GetSectionName()  → "Data Pipeline"
//
// ADR-0011 §ConfigRestartRequired convention:
//   - Subsystem Initialize()에서 캐싱하는 knob → ConfigRestartRequired="true"
//   - 매 호출 시점 read 또는 이벤트 시점 read → hot reload 자연 지원 (meta 불필요)
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.
//
// ini 저장 위치: Config/DefaultGame.ini
//   [/Script/MadeByClaudeCode.MossDataPipelineSettings]
//
// 사용 예 (런타임):
//   const auto* S = UMossDataPipelineSettings::Get();
//   float Budget = S->MaxInitTimeBudgetMs;

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MossDataPipelineSettings.generated.h"

/**
 * EDuplicateCardIdPolicy — 중복 CardId 발견 시 처리 정책.
 *
 * GDD: design/gdd/data-pipeline.md §Behavior Policy Knobs
 */
UENUM(BlueprintType)
enum class EDuplicateCardIdPolicy : uint8
{
    /** 중복 발견 시 경고 로그만 출력하고 계속 진행. */
    WarnOnly            UMETA(DisplayName = "Warn Only"),

    /** 중복 발견 시 경고 로그 출력 + 최초 등록 카드만 유지 (열화 폴백). */
    DegradedFallback    UMETA(DisplayName = "Degraded Fallback"),
};

/**
 * UMossDataPipelineSettings — Data Pipeline System 전역 Tuning Knobs
 *
 * Project Settings → Game → Moss Baby → Data Pipeline 에서 편집.
 * 9개 노브는 GDD §G.2/G.3/G.4 Performance Budget + Behavior Policy + Threshold Scaling.
 * ADR-0011: 시스템 전역 상수는 UDeveloperSettings, per-instance 데이터는 UPrimaryDataAsset.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Data Pipeline"))
class MADEBYCLAUDECODE_API UMossDataPipelineSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // ── Performance Budget ────────────────────────────────────────────────────

    /**
     * Pipeline 초기화 최대 허용 시간(밀리초).
     * UDataPipelineSubsystem::Initialize()의 시간 예산 상한.
     * ADR-0011: Subsystem Initialize()에서 캐싱 → ConfigRestartRequired.
     *
     * 안전 범위: [10.0, 200.0] ms
     *   너무 낮으면 → 정상 로드도 예산 초과 경고
     *   너무 높으면 → 긴 로드가 감지되지 않음
     * GDD §D2 Formula: MVP ≈ 2.7ms, Full ≈ 8.2ms 기준. 50ms 여유 충분.
     */
    UPROPERTY(Config, EditAnywhere, Category="Performance Budget",
              meta=(ClampMin="10.0", ClampMax="200.0", Units="Milliseconds",
                    ConfigRestartRequired="true",
                    ToolTip="Pipeline 초기화 최대 허용 시간(ms). D2 Formula 기준 50ms 여유."))
    float MaxInitTimeBudgetMs = 50.0f;

    /**
     * 카드 카탈로그 최대 등록 수.
     * ADR-0011: Subsystem Initialize()에서 캐싱 → ConfigRestartRequired.
     *
     * 안전 범위: [10, 1000]
     *   MVP 10 / Full 40 기준. 200은 Full Vision 5배 여유.
     */
    UPROPERTY(Config, EditAnywhere, Category="Performance Budget",
              meta=(ClampMin="10", ClampMax="1000",
                    ConfigRestartRequired="true",
                    ToolTip="카드 카탈로그 최대 등록 수. MVP=10, Full=40 기준."))
    int32 MaxCatalogSizeCards = 200;

    /**
     * 꿈 카탈로그 최대 등록 수.
     * ADR-0011: Subsystem Initialize()에서 캐싱 → ConfigRestartRequired.
     *
     * 안전 범위: [10, 500]
     *   MVP 5 / Full 50 기준. 100은 Full Vision 2배 여유.
     */
    UPROPERTY(Config, EditAnywhere, Category="Performance Budget",
              meta=(ClampMin="10", ClampMax="500",
                    ConfigRestartRequired="true",
                    ToolTip="꿈 카탈로그 최대 등록 수. MVP=5, Full=50 기준."))
    int32 MaxCatalogSizeDreams = 100;

    /**
     * 최종 형태 카탈로그 최대 등록 수.
     * ADR-0011: Subsystem Initialize()에서 캐싱 → ConfigRestartRequired.
     *
     * 안전 범위: [1, 64]
     *   MVP 1 / Full 8-12 기준. 16은 Full Vision 여유.
     */
    UPROPERTY(Config, EditAnywhere, Category="Performance Budget",
              meta=(ClampMin="1", ClampMax="64",
                    ConfigRestartRequired="true",
                    ToolTip="최종 형태 카탈로그 최대 등록 수. MVP=1, Full=8-12 기준."))
    int32 MaxCatalogSizeForms = 16;

    // ── Threshold Scaling ─────────────────────────────────────────────────────

    /**
     * 카탈로그 용량 경고 임계 배수.
     * 실제 등록 수가 MaxCatalogSize × WarnMultiplier 초과 시 UE_LOG Warning.
     * ADR-0011: 등록 시점 read → hot reload 자연 지원.
     *
     * 안전 범위: [1.0, 1.2]
     */
    UPROPERTY(Config, EditAnywhere, Category="Threshold Scaling",
              meta=(ClampMin="1.0", ClampMax="1.2",
                    ToolTip="카탈로그 용량 경고 배수 (MaxCatalogSize × 이 값 초과 시 Warning 로그)."))
    float CatalogOverflowWarnMultiplier = 1.05f;

    /**
     * 카탈로그 용량 오류 임계 배수.
     * 실제 등록 수가 MaxCatalogSize × ErrorMultiplier 초과 시 UE_LOG Error.
     * ADR-0011: 등록 시점 read → hot reload 자연 지원.
     *
     * 안전 범위: [1.2, 1.8]
     */
    UPROPERTY(Config, EditAnywhere, Category="Threshold Scaling",
              meta=(ClampMin="1.2", ClampMax="1.8",
                    ToolTip="카탈로그 용량 오류 배수 (MaxCatalogSize × 이 값 초과 시 Error 로그)."))
    float CatalogOverflowErrorMultiplier = 1.5f;

    /**
     * 카탈로그 용량 치명적 오류 임계 배수.
     * 실제 등록 수가 MaxCatalogSize × FatalMultiplier 초과 시 UE_LOG Fatal.
     * ADR-0011: 등록 시점 read → hot reload 자연 지원.
     *
     * 안전 범위: [1.5, 3.0]
     */
    UPROPERTY(Config, EditAnywhere, Category="Threshold Scaling",
              meta=(ClampMin="1.5", ClampMax="3.0",
                    ToolTip="카탈로그 용량 치명 배수 (MaxCatalogSize × 이 값 초과 시 Fatal 로그)."))
    float CatalogOverflowFatalMultiplier = 2.0f;

    // ── Behavior Policy ───────────────────────────────────────────────────────

    /**
     * 중복 CardId 발견 시 처리 정책.
     * WarnOnly (기본): 경고 로그만 출력.
     * DegradedFallback: 최초 등록 카드만 유지 + 경고 로그.
     * ADR-0011: 등록 시점 read → hot reload 자연 지원.
     */
    UPROPERTY(Config, EditAnywhere, Category="Behavior Policy",
              meta=(ToolTip="중복 CardId 발견 시 처리 정책. WarnOnly(기본) / DegradedFallback."))
    EDuplicateCardIdPolicy DuplicateCardIdPolicy = EDuplicateCardIdPolicy::WarnOnly;

    /**
     * FText 빈 문자열 가드 활성화.
     * true 시 DisplayName / Description이 빈 FText인 카드·꿈 등록 시 Warning 로그.
     * ADR-0011: 등록 시점 read → hot reload 자연 지원.
     */
    UPROPERTY(Config, EditAnywhere, Category="Behavior Policy",
              meta=(ToolTip="빈 FText 필드 가드. true 시 빈 DisplayName/Description 등록 시 Warning."))
    bool bFTextEmptyGuard = true;

    // ── Static Accessor ───────────────────────────────────────────────────────

    /** 런타임 + 에디터 어디서나 사용 가능한 CDO accessor. */
    static const UMossDataPipelineSettings* Get()
    {
        return GetDefault<UMossDataPipelineSettings>();
    }

    // ── UDeveloperSettings overrides ─────────────────────────────────────────

    /** Project Settings 카테고리 — "Moss Baby" */
    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }

    /** Project Settings 섹션 — "Data Pipeline" */
    virtual FName GetSectionName() const override { return TEXT("Data Pipeline"); }
};
