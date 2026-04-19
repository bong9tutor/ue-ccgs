// Copyright Moss Baby
//
// MossInputAbstractionSubsystem.h — Input Abstraction Layer 메인 서브시스템 선언
//
// Story-1-12: UMossInputAbstractionSubsystem + UMossInputAbstractionSettings + IMC 등록
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/input-abstraction-layer.md §Core Rules / §Lifecycle
//
// AC 커버리지:
//   AC-1: UMossInputAbstractionSettings 3 knobs — 별도 헤더 (MossInputAbstractionSettings.h)
//   AC-2: UMossInputAbstractionSubsystem 뼈대 (Initialize/Deinitialize + 6 Action 로드)
//   AC-3: Subsystem Initialize에서 IMC_MouseKeyboard + IMC_Gamepad 등록
//   AC-4: EInputMode enum (Mouse, Gamepad) — Story-1-13에서 auto-detect 구현 예정
//   AC-5: 6 UInputAction + 2 UInputMappingContext UPROPERTY TObjectPtr 보관
//
// Story-1-11 에셋 미생성 정책:
//   - LoadObject 반환값 null 허용 — crash 없이 동작
//   - bMappingContextRegistered == false 유지 (에셋 생성 후 자동으로 true로 전환)
//   - TD-008: Story-1-11 에셋 UE Editor 수동 생성 대기 중
//
// Story-1-13에서 추가 예정:
//   - OnInputModeChanged delegate
//   - EInputMode auto-detect (Formula 1 hysteresis)
//   - APlayerController::SetShowMouseCursor 전환
//
// Story-1-17에서 추가 예정:
//   - Offer Hold Formula 2 경계값 적용

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
// Story-1-13에서 실제 auto-detect (Formula 1 hysteresis) 로직 구현 예정.
// 현재(Story-1-12)는 기본값 Mouse로 고정 — Gamepad 전환 조건 미구현.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EInputMode — 현재 입력 장치 모드.
 *
 * Mouse: 마우스/키보드 입력이 활성 상태.
 * Gamepad: 게임패드 입력이 활성 상태.
 *
 * Story-1-12: 열거형 정의만. 실제 전환 로직은 Story-1-13에서 구현.
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
 * Story-1-12(이 파일): 뼈대 선언 + 6 Action + 2 IMC load + IMC 등록.
 * Story-1-13에서 EInputMode auto-detect + OnInputModeChanged delegate 추가 예정.
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
     * IMC 등록 플래그 초기화 + Super 호출.
     * Story-1-13+에서 delegate 해제 추가 예정.
     */
    virtual void Deinitialize() override;

    // ── State Accessors ───────────────────────────────────────────────────────

    /**
     * 현재 입력 모드 반환.
     * Story-1-13 구현 전에는 항상 EInputMode::Mouse.
     */
    EInputMode GetCurrentInputMode() const { return CurrentMode; }

    /**
     * IMC 등록 완료 여부.
     * true: IMC_MouseKeyboard 또는 IMC_Gamepad 중 하나 이상 등록 완료.
     * false: PlayerController 없거나 에셋 null (Story-1-11 대기 상태).
     */
    bool IsMappingContextRegistered() const { return bMappingContextRegistered; }

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
     * Story-1-13 auto-detect 구현 전 테스트에서 모드 주입 용도.
     * @param Mode  설정할 EInputMode 값.
     */
    void TestingSetCurrentMode(EInputMode Mode) { CurrentMode = Mode; }

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

    // ── State ─────────────────────────────────────────────────────────────────

    /** 현재 입력 모드. Story-1-13 이전에는 항상 Mouse로 고정. */
    EInputMode CurrentMode = EInputMode::Mouse;

    /**
     * IMC 등록 완료 플래그.
     * true: RegisterMappingContext 성공 + 최소 1개 IMC 등록.
     * Deinitialize에서 false로 리셋.
     */
    bool bMappingContextRegistered = false;
};
