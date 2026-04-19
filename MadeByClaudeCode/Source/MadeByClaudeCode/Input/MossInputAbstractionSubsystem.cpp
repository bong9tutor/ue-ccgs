// Copyright Moss Baby
//
// MossInputAbstractionSubsystem.cpp — Input Abstraction Layer 서브시스템 구현
//
// Story-1-12: UMossInputAbstractionSubsystem + UMossInputAbstractionSettings + IMC 등록
// Story-1-13: EInputMode auto-detect (Formula 1 hysteresis) + OnInputModeChanged delegate
//             + APlayerController::SetShowMouseCursor 전환 (Pillar 1: 알림·모달 없음)
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/input-abstraction-layer.md §Formula 1 / §Rule 3
//
// Story-1-17: OfferHoldDurationSec Offer Hold Formula 2 연동 구현 완료.
// Story-1-20에서 PlayerSpawn 이벤트 구독으로 지연된 IMC 등록 보완 예정.
//
// Story-1-11 에셋 미생성 정책:
//   LoadObject 반환 null 허용 → 경고 로그만 출력, crash 없음.
//   에셋 생성 후 에디터 재시작(또는 PIE 재실행) 시 자동으로 load 성공.

#include "Input/MossInputAbstractionSubsystem.h"
#include "Settings/MossInputAbstractionSettings.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputTriggers.h"

// LogMossInput: Input Abstraction 전용 로그 카테고리
// 필터: -LogMossInput Verbose → 에셋 로드 상세 / Quiet → 경고만 출력
DEFINE_LOG_CATEGORY_STATIC(LogMossInput, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Asset Path Constants
//
// Story-1-11 에셋 생성 후 이 경로에 에셋이 위치해야 load 성공.
// Editor에서 에셋 경로 변경 시 이 상수도 함께 업데이트 필요.
// ─────────────────────────────────────────────────────────────────────────────

namespace MossInputAssetPaths
{
    /** /Game/Input/Actions/ — IA_* 에셋 디렉토리 */
    constexpr const TCHAR* IA_PointerMove  = TEXT("/Game/Input/Actions/IA_PointerMove.IA_PointerMove");
    constexpr const TCHAR* IA_Select       = TEXT("/Game/Input/Actions/IA_Select.IA_Select");
    constexpr const TCHAR* IA_OfferCard    = TEXT("/Game/Input/Actions/IA_OfferCard.IA_OfferCard");
    constexpr const TCHAR* IA_NavigateUI   = TEXT("/Game/Input/Actions/IA_NavigateUI.IA_NavigateUI");
    constexpr const TCHAR* IA_OpenJournal  = TEXT("/Game/Input/Actions/IA_OpenJournal.IA_OpenJournal");
    constexpr const TCHAR* IA_Back         = TEXT("/Game/Input/Actions/IA_Back.IA_Back");

    /** /Game/Input/Contexts/ — IMC_* 에셋 디렉토리 */
    constexpr const TCHAR* IMC_MouseKeyboard = TEXT("/Game/Input/Contexts/IMC_MouseKeyboard.IMC_MouseKeyboard");
    constexpr const TCHAR* IMC_Gamepad       = TEXT("/Game/Input/Contexts/IMC_Gamepad.IMC_Gamepad");
}

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UMossInputAbstractionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 1. Input Asset 로드 (Story-1-11 에셋 없으면 null — crash 없음)
    LoadInputAssets();

    // 2. Settings knob → IMC Hold Trigger 반영 (Story-1-17)
    //    LoadInputAssets 직후 호출 — 에셋 null 시 no-op (TD-008 상태 안전)
    ApplySettingsToMappingContext();

    // 3. IMC 등록 시도
    //    APlayerController는 GameInstance 초기화 시점에 아직 없을 수 있음.
    //    (GameInstance::Init은 World / Controller 생성보다 먼저 호출될 수 있음)
    //
    //    PlayerController 없을 경우:
    //      - bMappingContextRegistered = false 유지
    //      - 경고 로그 출력
    //      - Production에서는 Story-1-20에서 FGameModeEvents::GameModePostLoginEvent
    //        또는 ULocalPlayer::PlayerAdded 이벤트로 지연 등록 보완 예정
    UGameInstance* GI = GetGameInstance();
    if (GI)
    {
        UWorld* World = GI->GetWorld();
        if (World)
        {
            APlayerController* PC = World->GetFirstPlayerController();
            if (PC)
            {
                RegisterMappingContext(PC);
            }
            else
            {
                UE_LOG(LogMossInput, Log,
                    TEXT("[MossInput] PlayerController not available at Initialize — "
                         "IMC registration deferred until player spawn (Story-1-20 보완 예정)"));
            }
        }
        else
        {
            UE_LOG(LogMossInput, Log,
                TEXT("[MossInput] World not available at Initialize — "
                     "IMC registration deferred"));
        }
    }
}

void UMossInputAbstractionSubsystem::Deinitialize()
{
    // Delegate 리스너 전체 해제 — 서브시스템 소멸 시 댕글링 바인딩 방지
    OnInputModeChanged.Clear();

    bMappingContextRegistered = false;

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Private Helpers
// ─────────────────────────────────────────────────────────────────────────────

void UMossInputAbstractionSubsystem::LoadInputAssets()
{
    // LoadObject: 에셋 경로 존재 시 동기 로드, 없으면 null 반환 (crash 없음).
    // Story-1-11 에셋 생성 전에는 전부 null → Warning 로그 출력 후 정상 진행.
    //
    // Note: current-best-practices.md에서는 FSoftObjectPath + LoadSynchronous 권장.
    //       이 Story는 "GDD §Upstream Dep UEnhancedInputLocalPlayerSubsystem 래핑"이
    //       목표이므로 LoadObject 직접 사용. FSoftObjectPath 전환은 Story-1-20에서 검토.

    IA_PointerMove  = LoadObject<UInputAction>(nullptr, MossInputAssetPaths::IA_PointerMove);
    IA_Select       = LoadObject<UInputAction>(nullptr, MossInputAssetPaths::IA_Select);
    IA_OfferCard    = LoadObject<UInputAction>(nullptr, MossInputAssetPaths::IA_OfferCard);
    IA_NavigateUI   = LoadObject<UInputAction>(nullptr, MossInputAssetPaths::IA_NavigateUI);
    IA_OpenJournal  = LoadObject<UInputAction>(nullptr, MossInputAssetPaths::IA_OpenJournal);
    IA_Back         = LoadObject<UInputAction>(nullptr, MossInputAssetPaths::IA_Back);

    IMC_MouseKeyboard = LoadObject<UInputMappingContext>(nullptr, MossInputAssetPaths::IMC_MouseKeyboard);
    IMC_Gamepad       = LoadObject<UInputMappingContext>(nullptr, MossInputAssetPaths::IMC_Gamepad);

    // 로드 수 집계 (0–8). 8 미만이면 Story-1-11 에셋 수동 생성 필요.
    int32 LoadedCount = 0;
    if (IA_PointerMove)       { ++LoadedCount; }
    if (IA_Select)            { ++LoadedCount; }
    if (IA_OfferCard)         { ++LoadedCount; }
    if (IA_NavigateUI)        { ++LoadedCount; }
    if (IA_OpenJournal)       { ++LoadedCount; }
    if (IA_Back)              { ++LoadedCount; }
    if (IMC_MouseKeyboard)    { ++LoadedCount; }
    if (IMC_Gamepad)          { ++LoadedCount; }

    if (LoadedCount < 8)
    {
        UE_LOG(LogMossInput, Warning,
            TEXT("[MossInput] Loaded %d/8 input assets — "
                 "Story-1-11 에셋 UE Editor 수동 생성 대기 중 (TD-008). "
                 "에셋 생성 후 PIE 재실행 시 자동 로드됨."),
            LoadedCount);
    }
    else
    {
        UE_LOG(LogMossInput, Log,
            TEXT("[MossInput] All 8 input assets loaded successfully."));
    }
}

void UMossInputAbstractionSubsystem::RegisterMappingContext(APlayerController* PC)
{
    // null guard — PC 없으면 no-op (호출자에서 이미 확인했지만 방어적 체크)
    if (!PC)
    {
        return;
    }

    ULocalPlayer* LP = PC->GetLocalPlayer();
    if (!LP)
    {
        UE_LOG(LogMossInput, Warning,
            TEXT("[MossInput] LocalPlayer not found on PlayerController — IMC 등록 스킵"));
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* EISubsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP);

    if (!EISubsystem)
    {
        UE_LOG(LogMossInput, Warning,
            TEXT("[MossInput] UEnhancedInputLocalPlayerSubsystem not available — "
                 "EnhancedInput 플러그인 활성화 여부 확인 필요"));
        return;
    }

    // IMC 등록: Priority 1 (GDD AC-3)
    // null IMC는 스킵 — Story-1-11 에셋 미생성 시 partial registration 허용
    if (IMC_MouseKeyboard)
    {
        EISubsystem->AddMappingContext(IMC_MouseKeyboard, /*Priority=*/1);
    }
    if (IMC_Gamepad)
    {
        EISubsystem->AddMappingContext(IMC_Gamepad, /*Priority=*/1);
    }

    // 최소 1개 IMC가 등록되어야 true
    // (에셋 없으면 false — 정상 동작, TD-008 상태 반영)
    bMappingContextRegistered = (IMC_MouseKeyboard != nullptr || IMC_Gamepad != nullptr);

    if (bMappingContextRegistered)
    {
        UE_LOG(LogMossInput, Log,
            TEXT("[MossInput] IMC registration complete — "
                 "MouseKeyboard=%s, Gamepad=%s"),
            IMC_MouseKeyboard ? TEXT("OK") : TEXT("null (TD-008)"),
            IMC_Gamepad       ? TEXT("OK") : TEXT("null (TD-008)"));
    }
    else
    {
        UE_LOG(LogMossInput, Warning,
            TEXT("[MossInput] No IMC registered — both IMC_MouseKeyboard and IMC_Gamepad are null. "
                 "Story-1-11 에셋 생성 후 PIE 재실행 필요."));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Story-1-17: Settings → IMC Hold Trigger 반영
// ─────────────────────────────────────────────────────────────────────────────

void UMossInputAbstractionSubsystem::ApplySettingsToMappingContext()
{
    const UMossInputAbstractionSettings* Settings = UMossInputAbstractionSettings::Get();
    if (!Settings)
    {
        UE_LOG(LogMossInput, Warning,
            TEXT("[MossInput] ApplySettingsToMappingContext: Settings null — 스킵"));
        return;
    }
    if (!IA_OfferCard)
    {
        UE_LOG(LogMossInput, Log,
            TEXT("[MossInput] ApplySettingsToMappingContext: IA_OfferCard null — "
                 "Story-1-11 에셋 대기 중 (TD-008), no-op"));
        return;
    }

    int32 MouseUpdated = 0;
    int32 GamepadUpdated = 0;

    if (IMC_MouseKeyboard)
    {
        MouseUpdated = ApplyHoldThresholdToIMC(
            IMC_MouseKeyboard, IA_OfferCard, Settings->OfferDragThresholdSec);
    }
    if (IMC_Gamepad)
    {
        GamepadUpdated = ApplyHoldThresholdToIMC(
            IMC_Gamepad, IA_OfferCard, Settings->OfferHoldDurationSec);
    }

    UE_LOG(LogMossInput, Log,
        TEXT("[MossInput] ApplySettingsToMappingContext: MouseIMC updated %d, GamepadIMC updated %d"),
        MouseUpdated, GamepadUpdated);
}

int32 UMossInputAbstractionSubsystem::ApplyHoldThresholdToIMC(
    UInputMappingContext* IMC,
    const UInputAction* TargetAction,
    float ThresholdSec)
{
    if (!IMC || !TargetAction) { return 0; }

    int32 UpdatedCount = 0;
    // GetMappings()는 const TArray<FEnhancedActionKeyMapping>& 반환.
    // Mapping.Triggers의 TObjectPtr<UInputTrigger>가 가리키는 UObject는 mutable이므로
    // HoldTimeThreshold 직접 수정 가능 (BlueprintReadWrite).
    const TArray<FEnhancedActionKeyMapping>& ReadMappings = IMC->GetMappings();
    for (const FEnhancedActionKeyMapping& Mapping : ReadMappings)
    {
        if (Mapping.Action != TargetAction) { continue; }
        for (UInputTrigger* Trigger : Mapping.Triggers)
        {
            if (UInputTriggerHold* HoldTrigger = Cast<UInputTriggerHold>(Trigger))
            {
                HoldTrigger->HoldTimeThreshold = ThresholdSec;
                UpdatedCount++;
            }
        }
    }
    return UpdatedCount;
}

// ─────────────────────────────────────────────────────────────────────────────
// Story-1-13: Input Mode Auto-Detect (Formula 1 Hysteresis)
// ─────────────────────────────────────────────────────────────────────────────

void UMossInputAbstractionSubsystem::OnMouseMoved(const FVector2D& NewMousePosition)
{
    // 최초 호출: 기준 위치 확립 (delta 계산 불가 — 전환 없음)
    if (!bMousePositionInitialized)
    {
        LastMousePosition = NewMousePosition;
        bMousePositionInitialized = true;
        return;
    }

    const float Delta = (NewMousePosition - LastMousePosition).Size();
    LastMousePosition = NewMousePosition;

    // Formula 1: strict `>` — 3.0px 정확히 이동 시 전환 미발생, 3.001px 초과 시 전환 발생
    const UMossInputAbstractionSettings* Settings = UMossInputAbstractionSettings::Get();
    if (!Settings)
    {
        UE_LOG(LogMossInput, Warning,
            TEXT("[MossInput] OnMouseMoved: UMossInputAbstractionSettings CDO null — 전환 스킵"));
        return;
    }

    if (CurrentMode == EInputMode::Gamepad && Delta > Settings->InputModeMouseThreshold)
    {
        TransitionToMode(EInputMode::Mouse);
    }
}

void UMossInputAbstractionSubsystem::OnGamepadInputReceived()
{
    // Mouse 모드일 때만 전환 — Gamepad 모드에서 중복 입력은 no-op
    if (CurrentMode == EInputMode::Mouse)
    {
        TransitionToMode(EInputMode::Gamepad);
    }
}

void UMossInputAbstractionSubsystem::TransitionToMode(EInputMode NewMode)
{
    // 동일 모드 재진입 방지 — delegate 미발행
    if (NewMode == CurrentMode)
    {
        return;
    }

    CurrentMode = NewMode;

    // Cursor 가시성 적용 (Pillar 1: 알림·모달 없음 — cursor 전환만)
    // PC null 허용: 단위 테스트 환경(NewObject 직접 생성) 또는 Initialize 전 호출 시 안전
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWorld* World = GI->GetWorld())
        {
            if (APlayerController* PC = World->GetFirstPlayerController())
            {
                PC->SetShowMouseCursor(NewMode == EInputMode::Mouse);
            }
        }
    }

    // Delegate Broadcast — SetShowMouseCursor 이후 마지막에 발행
    // 구독자가 커서 상태에 의존하는 경우 이미 갱신된 후 수신
    OnInputModeChanged.Broadcast(NewMode);

    UE_LOG(LogMossInput, Log,
        TEXT("[MossInput] Mode transition: %s"),
        NewMode == EInputMode::Mouse ? TEXT("Gamepad → Mouse") : TEXT("Mouse → Gamepad"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Test Seams
// ─────────────────────────────────────────────────────────────────────────────

#if WITH_AUTOMATION_TESTS

int32 UMossInputAbstractionSubsystem::TestingApplySettingsToSingleIMC(
    UInputMappingContext* IMC,
    const UInputAction* TargetAction,
    float TargetHoldThreshold)
{
    return ApplyHoldThresholdToIMC(IMC, TargetAction, TargetHoldThreshold);
}

int32 UMossInputAbstractionSubsystem::TestingCountLoadedActions() const
{
    // IMC는 제외 — Action(IA_*) 6개만 카운트
    int32 Count = 0;
    if (IA_PointerMove)  { ++Count; }
    if (IA_Select)       { ++Count; }
    if (IA_OfferCard)    { ++Count; }
    if (IA_NavigateUI)   { ++Count; }
    if (IA_OpenJournal)  { ++Count; }
    if (IA_Back)         { ++Count; }
    return Count;
}

void UMossInputAbstractionSubsystem::TestingInjectAction(FName ActionName, UInputAction* Action)
{
    // ActionName → 해당 UPROPERTY 필드에 Action 주입
    // 6개 Action 이름 분기 — FName 비교는 case-insensitive by default이지만
    // 명시적 동일 케이스 사용 (Engine FName은 대소문자 구별 없이 저장됨)
    if      (ActionName == FName(TEXT("IA_PointerMove")))  { IA_PointerMove  = Action; }
    else if (ActionName == FName(TEXT("IA_Select")))       { IA_Select       = Action; }
    else if (ActionName == FName(TEXT("IA_OfferCard")))    { IA_OfferCard    = Action; }
    else if (ActionName == FName(TEXT("IA_NavigateUI")))   { IA_NavigateUI   = Action; }
    else if (ActionName == FName(TEXT("IA_OpenJournal")))  { IA_OpenJournal  = Action; }
    else if (ActionName == FName(TEXT("IA_Back")))         { IA_Back         = Action; }
    else
    {
        UE_LOG(LogMossInput, Warning,
            TEXT("[MossInput][Test] TestingInjectAction: unknown ActionName '%s'"),
            *ActionName.ToString());
    }
}

#endif // WITH_AUTOMATION_TESTS
