// Copyright Moss Baby
//
// MossInputAbstractionSettings.h — Input Abstraction Layer Tuning Knobs
//
// Story-1-12: UMossInputAbstractionSubsystem + UMossInputAbstractionSettings + IMC 등록
// ADR-0011: Tuning Knob 노출 방식 — UDeveloperSettings 정식 채택
// GDD: design/gdd/input-abstraction-layer.md §Tuning Knobs
//
// ADR-0011 §Naming Convention:
//   U[SystemName]Settings : public UDeveloperSettings
//   GetCategoryName() → "Moss Baby"
//   GetSectionName()  → "Input Abstraction"
//
// ADR-0011 §ConfigRestartRequired 정책:
//   - OfferDragThresholdSec: UInputTriggerHold 에셋의 HoldTimeThreshold 재설정 필요
//     → ConfigRestartRequired="true"
//   - InputModeMouseThreshold, OfferHoldDurationSec: 런타임 read → hot reload 자연 지원
//
// ini 저장 위치: Config/DefaultGame.ini
//   [/Script/MadeByClaudeCode.MossInputAbstractionSettings]
//
// 사용 예 (런타임):
//   const auto* S = UMossInputAbstractionSettings::Get();
//   float Threshold = S->InputModeMouseThreshold;

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MossInputAbstractionSettings.generated.h"

/**
 * UMossInputAbstractionSettings — Input Abstraction Layer 전역 Tuning Knobs
 *
 * Project Settings → Game → Moss Baby → Input Abstraction 에서 편집.
 * 3개 노브는 GDD §Tuning Knobs 표와 1:1 대응.
 * ADR-0011: 시스템 전역 상수는 UDeveloperSettings, per-instance 데이터는 UPrimaryDataAsset.
 *
 * Story-1-12 범위: 3 knob 정의 + CDO accessor.
 * Story-1-13에서 InputModeMouseThreshold 실제 Mouse/Gamepad 전환 판단에 사용 예정.
 * Story-1-17에서 OfferHoldDurationSec Offer Hold Formula 2 경계값으로 사용 예정.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Input Abstraction"))
class MADEBYCLAUDECODE_API UMossInputAbstractionSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // ── Input Mode Detection ──────────────────────────────────────────────────

    /**
     * Mouse 입력 모드 판단 임계값 (픽셀).
     * 포인터 이동 거리가 이 값 이상이면 Mouse 모드로 전환 판단.
     * Story-1-13에서 EInputMode 자동 전환(Formula 1 hysteresis)에 사용.
     *
     * 안전 범위: [1.0, 10.0] 픽셀
     *   값 내리면 → 게임패드 미세 진동에도 Mouse 모드로 오전환 위험
     *   값 올리면 → 빠른 마우스 이동도 Gamepad 모드로 남는 문제
     *
     * ADR-0011: 매 호출 시점 read → hot reload 자연 지원
     */
    UPROPERTY(Config, EditAnywhere, Category="Input Mode",
              meta=(ClampMin="1.0", ClampMax="10.0", Units="Pixels",
                    ToolTip="마우스 입력 모드 판단 임계값(픽셀). 이동 거리 >= 이 값이면 Mouse 모드. Story-1-13 Formula 1에 사용."))
    float InputModeMouseThreshold = 3.0f;

    // ── Offer Gesture ─────────────────────────────────────────────────────────

    /**
     * 카드 건넴 제스처 — 드래그 시작 판단 임계 시간(초).
     * 마우스 버튼 누른 상태로 이 시간 이상 경과하면 드래그 시작으로 인식.
     * UInputTriggerHold 에셋의 HoldTimeThreshold 값과 연동됨.
     *
     * 안전 범위: [0.08, 0.3] 초
     *   값 내리면 → 단순 클릭도 드래그로 오인식 위험
     *   값 올리면 → 드래그 반응 느려짐 (UX 저하)
     *
     * ADR-0011: UInputTriggerHold 에셋 HoldTimeThreshold 재설정 필요 → ConfigRestartRequired
     * Story-1-17: Offer Hold Formula 2 경계값으로 추가 활용 예정.
     */
    UPROPERTY(Config, EditAnywhere, Category="Offer Gesture",
              meta=(ClampMin="0.08", ClampMax="0.3", Units="Seconds",
                    ConfigRestartRequired="true",
                    ToolTip="카드 드래그 시작 임계 시간(초). UInputTriggerHold.HoldTimeThreshold 재설정 필요 (ConfigRestartRequired). Story-1-17 Formula 2 참조."))
    float OfferDragThresholdSec = 0.15f;

    /**
     * 카드 건넴 제스처 — Hold 확정 임계 시간(초).
     * 이끼 아이 위에서 이 시간 이상 Hold 유지 시 건넴 확정.
     * Story-1-17에서 Offer Hold Formula 2 실제 임계 로직에 사용 예정.
     *
     * 안전 범위: [0.2, 1.5] 초
     *   값 내리면 → 의도치 않은 빠른 건넴 확정 위험
     *   값 올리면 → 건넴 확정 지연으로 UX 저하
     *
     * ADR-0011: Story-1-17에서 캐싱 여부 결정 예정 → 현재 ConfigRestartRequired 미적용
     */
    UPROPERTY(Config, EditAnywhere, Category="Offer Gesture",
              meta=(ClampMin="0.2", ClampMax="1.5", Units="Seconds",
                    ToolTip="Hold 확정 임계 시간(초). Story-1-17 Offer Hold Formula 2에 사용 예정."))
    float OfferHoldDurationSec = 0.5f;

    // ── Static Accessor ───────────────────────────────────────────────────────

    /** 런타임 + 에디터 어디서나 사용 가능한 CDO accessor. */
    static const UMossInputAbstractionSettings* Get()
    {
        return GetDefault<UMossInputAbstractionSettings>();
    }

    // ── UDeveloperSettings overrides ─────────────────────────────────────────

    /** Project Settings 카테고리 — "Moss Baby" */
    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }

    /** Project Settings 섹션 — "Input Abstraction" */
    virtual FName GetSectionName() const override { return TEXT("Input Abstraction"); }
};
