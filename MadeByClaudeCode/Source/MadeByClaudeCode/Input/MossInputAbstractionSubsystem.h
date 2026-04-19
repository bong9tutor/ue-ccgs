// Copyright Moss Baby
//
// MossInputAbstractionSubsystem.h — Input Abstraction Layer 메인 서브시스템 선언
//
// Story-1-12: UMossInputAbstractionSubsystem + UMossInputAbstractionSettings + IMC 등록
// Story-1-13: EInputMode auto-detect (Formula 1 hysteresis) + OnInputModeChanged delegate
//             + APlayerController::SetShowMouseCursor 전환
// Story-1-17: Offer Hold Formula 2 — ApplySettingsToMappingContext
//             Settings knob → UInputTriggerHold::HoldTimeThreshold 반영
//             에셋 null 시 no-op (TD-008 대기 상태 안전)
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/input-abstraction-layer.md §Core Rules / §Lifecycle / §Formula 1 / §Formula 2
//
// AC 커버리지:
//   AC-1: UMossInputAbstractionSettings 3 knobs — 별도 헤더 (MossInputAbstractionSettings.h)
//   AC-2: UMossInputAbstractionSubsystem 뼈대 (Initialize/Deinitialize + 6 Action 로드)
//   AC-3: Subsystem Initialize에서 IMC_MouseKeyboard + IMC_Gamepad 등록
//   AC-4: EInputMode enum (Mouse, Gamepad)
//   AC-5: 6 UInputAction + 2 UInputMappingContext UPROPERTY TObjectPtr 보관
//   Story-1-13 AC: FOnInputModeChanged delegate + Formula 1 auto-detect + cursor 전환
//   Story-1-17 AC: ApplySettingsToMappingContext null-safe +
//                  UInputTriggerHold::HoldTimeThreshold 적용
//
// Story-1-11 에셋 미생성 정책:
//   - LoadObject 반환값 null 허용 — crash 없이 동작
//   - bMappingContextRegistered == false 유지 (에셋 생성 후 자동으로 true로 전환)
//   - TD-008: Story-1-11 에셋 UE Editor 수동 생성 대기 중

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "MossInputAbstractionSubsystem.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EInputMode — 현재 입력 모드
//
// GDD §AC-4: Mouse / Gamepad 두 상태.
// Story-1-13: Formula 1 hysteresis auto-detect 구현 완료.
// uint8 기반 — stable integer 보장 (0=Mouse, 1=Gamepad).
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EInputMode — 현재 입력 장치 모드.
 *
 * Mouse: 마우스/키보드 입력이 활성 상태.
 * Gamepad: 게임패드 입력이 활성 상태.
 *
 * uint8 기반 — stable integer 보장 (0=Mouse, 1=Gamepad).
 */
UENUM(BlueprintType)
enum class EInputMode : uint8
{
    /** 마우스/키보드 입력 모드 (기본값) */
    Mouse   = 0,
    /** 게임패드 입력 모드 */
    Gamepad = 1
};

// ─────────────────────────────────────────────────────────────────────────────
// FOnInputModeChanged — Input 모드 전환 Multicast Delegate
//
// Story-1-13 GDD §Formula 1 + §Rule 3:
//   모드 전환 시 1회 Broadcast. 구독자는 NewMode 인자로 전환된 모드 수신.
//   Pillar 1: 알림·모달 없음 — delegate 구독자가 UI 커서 전환 등 내부 처리만 수행.
//
// 관용 위치: class 외부 global scope (UE 표준 패턴).
// ─────────────────────────────────────────────────────────────────────────────

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInputModeChanged, EInputMode);

// ─────────────────────────────────────────────────────────────────────────────
// UMossInputAbstractionSubsystem
// ─────────────────────────────────────────────────────────────────────────────

/**
 * UMossInputAbstractionSubsystem — Moss Baby의 Enhanced Input 래핑 계층.
 *
 * UGameInstanceSubsystem 기반. GameInstance 직접 수정 금지 — 별도 클래스.
 * 6개 Input Action + 2개 IMC를 로드하고 PlayerController의
 * UEnhancedInputLocalPlayerSubsystem에 IMC를 등록.
 *
 * Story-1-11 에셋 미생성 상태에서도 null 허용으로 crash 없이 동작.
 * APlayerController가 Initialize 시점에 없을 경우 IMC 등록은 지연됨
 * (Production에서는 PlayerSpawn 이벤트 구독으로 보완 — Story-1-20).
 *
 * Story-1-12: 뼈대 선언 + 6 Action + 2 IMC load + IMC 등록.
 * Story-1-13: EInputMode auto-detect (Formula 1 hysteresis) + FOnInputModeChanged delegate
 *             + APlayerController::SetShowMouseCursor 모드 연동.
 */
UCLASS()
class MADEBYCLAUDECODE_API UMossInputAbstractionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── Lifecycle (UGameInstanceSubsystem overrides) ──────────────────────────

    /**
     * Input Asset 로드 + IMC 등록 시도.
     * APlayerController가 없을 경우 IMC 등록은 지연 (bMappingContextRegistered == false).
     * Super 호출 필수.
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * OnInputModeChanged delegate Clear + IMC 등록 플래그 초기화 + Super 호출.
     */
    virtual void Deinitialize() override;

    // ── State Accessors ───────────────────────────────────────────────────────

    /**
     * 현재 입력 모드 반환.
     * Story-1-13~: Formula 1 hysteresis auto-detect에 의해 갱신됨.
     */
    EInputMode GetCurrentInputMode() const { return CurrentMode; }

    /**
     * IMC 등록 완료 여부.
     * true: IMC_MouseKeyboard 또는 IMC_Gamepad 중 하나 이상 등록 완료.
     * false: PlayerController 없거나 에셋 null (Story-1-11 대기 상태).
     */
    bool IsMappingContextRegistered() const { return bMappingContextRegistered; }

    // ── Story-1-17: Settings → IMC Hold Trigger 반영 ─────────────────────────

    /**
     * Settings knob(OfferDragThresholdSec / OfferHoldDurationSec)을
     * 각 IMC의 IA_OfferCard 매핑에 연결된 UInputTriggerHold::HoldTimeThreshold에 적용.
     *
     * 호출 시점: Initialize() 내 LoadInputAssets() 직후.
     * null-safe: Settings 또는 IA_OfferCard / IMC 중 어느 하나라도 null이면 즉시 return (no-op).
     * TD-008: Story-1-11 에셋 생성 전에는 IA_OfferCard null → 경고 로그 + no-op.
     *
     * Formula 2: OfferTriggered = HoldElapsed >= HoldThreshold (inclusive `>=`)
     * Formula 1 strict `>` 와 대비 — UInputTriggerHold 엔진 구현이 `>=`를 사용함.
     */
    void ApplySettingsToMappingContext();

    // ── Story-1-13: Input Mode Auto-Detect (Formula 1 Hysteresis) ─────────────

    /**
     * 마우스 포인터 이동 이벤트 처리 후크.
     *
     * Formula 1: ShouldSwitchToMouse = (Delta > InputModeMouseThreshold) AND (CurrentMode == Gamepad)
     * strict `>` — 3.0px 정확히 이동 시 전환 미발생, 3.001px 초과 시 전환 발생.
     *
     * 최초 호출 시 LastMousePosition 초기화만 수행 (위치 확립, 전환 없음).
     * 이후 호출 시 delta 계산 → Formula 1 판정 → 필요 시 TransitionToMode(Mouse).
     *
     * @param NewMousePosition  현재 프레임의 마우스 위치 (화면 좌표계).
     */
    void OnMouseMoved(const FVector2D& NewMousePosition);

    /**
     * 게임패드 입력 수신 이벤트 처리 후크.
     *
     * 현재 모드가 Mouse일 때만 TransitionToMode(Gamepad) 호출.
     * 즉시 전환 (프레임 내, FTimer/Delay 없음) — GDD §Rule 3.
     */
    void OnGamepadInputReceived();

    /**
     * 입력 모드 전환 Multicast Delegate.
     *
     * TransitionToMode 내 SetShowMouseCursor 이후 마지막에 Broadcast.
     * Pillar 1: 알림·모달 없음 — delegate 구독자가 필요한 내부 처리만 수행.
     *
     * Subscribe: OnInputModeChanged.AddWeakLambda(this, [](EInputMode NewMode){ ... });
     */
    FOnInputModeChanged OnInputModeChanged;

    // ── Input Asset References ─────────────────────────────────────────────
    //
    // 6 Input Action + 2 Input Mapping Context.
    // Story-1-11 에셋 생성 전: 모두 null (crash 없이 동작).
    // Story-1-11 에셋 생성 후: Initialize() 시 LoadObject로 채워짐.
    //
    // UPROPERTY() 필수 — GC가 이 UObject 참조를 추적해야 함.
    // TObjectPtr<> 필수 (UE 5.0+ 권장 — deprecated TSubobjectPtr 대체).

    /** IA_PointerMove — 마우스 커서 / 게임패드 스틱 이동 */
    UPROPERTY()
    TObjectPtr<UInputAction> IA_PointerMove;

    /** IA_Select — 카드 선택 / 확인 */
    UPROPERTY()
    TObjectPtr<UInputAction> IA_Select;

    /** IA_OfferCard — 카드 건넴 제스처 시작 */
    UPROPERTY()
    TObjectPtr<UInputAction> IA_OfferCard;

    /** IA_NavigateUI — UI 방향 이동 (게임패드 D-Pad / 키보드 방향키) */
    UPROPERTY()
    TObjectPtr<UInputAction> IA_NavigateUI;

    /** IA_OpenJournal — 꿈 일기 열기/닫기 토글 */
    UPROPERTY()
    TObjectPtr<UInputAction> IA_OpenJournal;

    /** IA_Back — UI 뒤로가기 / 취소 */
    UPROPERTY()
    TObjectPtr<UInputAction> IA_Back;

    /** IMC_MouseKeyboard — 마우스/키보드 IMC. Priority 1로 등록. */
    UPROPERTY()
    TObjectPtr<UInputMappingContext> IMC_MouseKeyboard;

    /** IMC_Gamepad — 게임패드 IMC. Priority 1로 등록 (구조만 — binding은 VS). */
    UPROPERTY()
    TObjectPtr<UInputMappingContext> IMC_Gamepad;

    // ── Test Seams (WITH_AUTOMATION_TESTS guard) ──────────────────────────────

#if WITH_AUTOMATION_TESTS
    /**
     * [테스트 전용] CurrentMode 강제 설정.
     * auto-detect 로직과 독립적으로 모드 주입 용도.
     * @param Mode  설정할 EInputMode 값.
     */
    void TestingSetCurrentMode(EInputMode Mode) { CurrentMode = Mode; }

    /**
     * [테스트 전용] 단일 IMC에 대한 ApplyHoldThresholdToIMC 직접 호출.
     * Mock IMC 주입 후 HoldTimeThreshold 적용 결과 검증용.
     * @param IMC              대상 UInputMappingContext (null 허용 — 0 반환).
     * @param TargetAction     Hold trigger를 탐색할 UInputAction (null 허용 — 0 반환).
     * @param TargetHoldThreshold  적용할 HoldTimeThreshold 값(초).
     * @return 변경된 UInputTriggerHold 개수.
     */
    int32 TestingApplySettingsToSingleIMC(UInputMappingContext* IMC,
                                          const UInputAction* TargetAction,
                                          float TargetHoldThreshold);

    /**
     * [테스트 전용] 현재 로드된 Action 수 반환 (6개 중).
     * IMC는 포함하지 않음 (Action만 카운트).
     * @return IA_PointerMove ~ IA_Back 중 non-null 수 (0–6).
     */
    int32 TestingCountLoadedActions() const;

    /**
     * [테스트 전용] Action 이름으로 Mock UInputAction 주입.
     * Story-1-11 에셋 없이 테스트할 때 사용.
     * @param ActionName  "IA_PointerMove", "IA_Select" 등 FName 키.
     * @param Action      주입할 UInputAction 포인터 (NewObject로 생성).
     */
    void TestingInjectAction(FName ActionName, UInputAction* Action);

    /**
     * [테스트 전용] LastMousePosition 값 반환.
     * Formula 1 delta 계산 검증에 사용.
     */
    FVector2D TestingGetLastMousePosition() const { return LastMousePosition; }

    /**
     * [테스트 전용] bMousePositionInitialized 값 반환.
     * 최초 호출 초기화 경로 검증에 사용.
     */
    bool TestingIsMousePositionInitialized() const { return bMousePositionInitialized; }

    /**
     * [테스트 전용] LastMousePosition + bMousePositionInitialized 리셋.
     * 테스트 간 독립성 보장을 위해 각 테스트 시작 전 호출.
     */
    void TestingResetMousePosition()
    {
        bMousePositionInitialized = false;
        LastMousePosition = FVector2D::ZeroVector;
    }
#endif

private:
    // ── Internal Helpers ──────────────────────────────────────────────────────

    /**
     * 8개 Input Asset (6 Action + 2 IMC) LoadObject 시도.
     * Story-1-11 에셋 미존재 시 null 반환 — crash 없음.
     * 로드 수를 Warning 로그로 출력 (0 ~ 8).
     */
    void LoadInputAssets();

    /**
     * PlayerController를 통해 EnhancedInputLocalPlayerSubsystem에 IMC 등록.
     * IMC null이면 해당 IMC만 스킵 (partial registration 허용).
     * @param PC  IMC를 등록할 PlayerController. null이면 no-op.
     */
    void RegisterMappingContext(APlayerController* PC);

    // ── Story-1-17: Hold Threshold Helper ────────────────────────────────────

    /**
     * IMC 내 TargetAction에 매핑된 모든 UInputTriggerHold의 HoldTimeThreshold를 ThresholdSec로 설정.
     *
     * 구현 노트:
     *   GetMappings()는 const TArray<FEnhancedActionKeyMapping>& 반환.
     *   FEnhancedActionKeyMapping.Triggers는 TArray<TObjectPtr<UInputTrigger>>.
     *   TObjectPtr<UInputTrigger>가 가리키는 UObject는 포인터 const와 무관하게 mutable.
     *   따라서 Cast<UInputTriggerHold>(Trigger.Get())->HoldTimeThreshold = X 는 valid.
     *
     * @param IMC           대상 UInputMappingContext (null 허용 — 0 반환).
     * @param TargetAction  탐색할 UInputAction (null 허용 — 0 반환).
     * @param ThresholdSec  설정할 HoldTimeThreshold (초).
     * @return 변경된 UInputTriggerHold 인스턴스 수.
     */
    int32 ApplyHoldThresholdToIMC(UInputMappingContext* IMC,
                                  const UInputAction* TargetAction,
                                  float ThresholdSec);

    // ── Story-1-13: Mode Transition ───────────────────────────────────────────

    /**
     * 입력 모드 전환 내부 처리.
     *
     * 1. NewMode == CurrentMode이면 즉시 return (중복 전환 방지 — delegate 미발행).
     * 2. CurrentMode 갱신.
     * 3. APlayerController::SetShowMouseCursor 적용 (Mouse=true, Gamepad=false).
     *    PC null 시 cursor 전환 스킵 (단위 테스트 환경 안전).
     * 4. OnInputModeChanged.Broadcast(NewMode) — 마지막에 (PC 처리 이후).
     *
     * Pillar 1: UI 알림·모달 없음.
     * @param NewMode  전환할 EInputMode 값.
     */
    void TransitionToMode(EInputMode NewMode);

    // ── State ─────────────────────────────────────────────────────────────────

    /** 현재 입력 모드. Formula 1 hysteresis에 의해 갱신됨 (Story-1-13~). */
    EInputMode CurrentMode = EInputMode::Mouse;

    /**
     * IMC 등록 완료 플래그.
     * true: RegisterMappingContext 성공 + 최소 1개 IMC 등록.
     * Deinitialize에서 false로 리셋.
     */
    bool bMappingContextRegistered = false;

    /**
     * 이전 프레임의 마우스 위치.
     * Formula 1 delta 계산 기준점. bMousePositionInitialized == false이면 사용 불가.
     */
    FVector2D LastMousePosition = FVector2D::ZeroVector;

    /**
     * LastMousePosition 초기화 완료 여부.
     * false: 최초 OnMouseMoved 호출 전 — delta 계산 불가 (기준점 없음).
     * true: 최초 호출로 LastMousePosition 확립 완료.
     */
    bool bMousePositionInitialized = false;
};
