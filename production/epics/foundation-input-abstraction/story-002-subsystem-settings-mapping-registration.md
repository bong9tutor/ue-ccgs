# Story 002: UMossInputAbstractionSubsystem + UMossInputAbstractionSettings + IMC 등록

> **Epic**: Input Abstraction Layer
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/input-abstraction-layer.md`
**Requirement**: `TR-input-002` (`UEnhancedInputLocalPlayerSubsystem` wrapping), Tuning Knob exposure
**ADR Governing Implementation**: ADR-0011 (UMossInputAbstractionSettings knob exposure)
**ADR Decision Summary**: `UMossInputAbstractionSettings : UDeveloperSettings` 3 knobs (`InputModeMouseThreshold = 3.0px`, `OfferDragThresholdSec = 0.15s`, `OfferHoldDurationSec = 0.5s`). Subsystem이 `APlayerController`의 `UEnhancedInputLocalPlayerSubsystem`에 IMC 등록.
**Engine**: UE 5.6 | **Risk**: LOW
**Engine Notes**: `UEnhancedInputLocalPlayerSubsystem::AddMappingContext(IMC, Priority)`. `APlayerController::SetShowMouseCursor(true/false)` 모드에 따라 전환 (Story 004). `UInputTriggerHold`의 `HoldTimeThreshold`는 에셋 시점에 설정됨 — Settings에서 읽으려면 별도 hook 또는 에셋 편집.

**Control Manifest Rules (Foundation + Cross-Layer)**:
- Required: Cross-Layer "`UMossInputAbstractionSettings : UDeveloperSettings`" with 3 knobs (ADR-0011)

---

## Acceptance Criteria

*From GDD §Tuning Knobs + Upstream Dependencies:*

- [ ] `UMossInputAbstractionSettings : UDeveloperSettings` 정의 — knobs:
  - `InputModeMouseThreshold` (float, default 3.0, range [1, 10], units pixels)
  - `OfferDragThresholdSec` (float, default 0.15, range [0.08, 0.3], units seconds, `ConfigRestartRequired=true` — IA_OfferCard Hold 재설정 필요)
  - `OfferHoldDurationSec` (float, default 0.5, range [0.2, 1.5], units seconds)
- [ ] `UMossInputAbstractionSubsystem : UGameInstanceSubsystem` 뼈대 (Initialize/Deinitialize, 6 Action 로드)
- [ ] Subsystem Initialize에서 `APlayerController`의 `UEnhancedInputLocalPlayerSubsystem`에 `IMC_MouseKeyboard` 등록 (priority 1). `IMC_Gamepad`는 loaded 상태만 (실제 binding은 VS)
- [ ] `EInputMode` enum 정의: `Mouse`, `Gamepad`. Story 003에서 delegate 추가.
- [ ] 6 UInputAction + 2 UInputMappingContext soft reference 보관 (`UPROPERTY() TObjectPtr<UInputAction> IA_PointerMove;` 등)

---

## Implementation Notes

*Derived from GDD §Upstream Dependencies + ADR-0011 §Key Interfaces:*

```cpp
// Source/MadeByClaudeCode/Settings/MossInputAbstractionSettings.h
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Input Abstraction"))
class UMossInputAbstractionSettings : public UDeveloperSettings {
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Input Mode",
              meta=(ClampMin="1.0", ClampMax="10.0", Units="Pixels"))
    float InputModeMouseThreshold = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category="Offer Gesture",
              meta=(ClampMin="0.08", ClampMax="0.3", Units="Seconds", ConfigRestartRequired="true"))
    float OfferDragThresholdSec = 0.15f;

    UPROPERTY(Config, EditAnywhere, Category="Offer Gesture",
              meta=(ClampMin="0.2", ClampMax="1.5", Units="Seconds", ConfigRestartRequired="true"))
    float OfferHoldDurationSec = 0.5f;

    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }
    virtual FName GetSectionName() const override { return TEXT("Input Abstraction"); }
    static const UMossInputAbstractionSettings* Get() { return GetDefault<UMossInputAbstractionSettings>(); }
};

// Source/MadeByClaudeCode/Input/MossInputAbstractionSubsystem.h
UENUM(BlueprintType)
enum class EInputMode : uint8 { Mouse, Gamepad };

UCLASS()
class UMossInputAbstractionSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    EInputMode GetCurrentInputMode() const { return CurrentMode; }

    // Story 003 추가: OnInputModeChanged delegate

    UPROPERTY() TObjectPtr<UInputAction> IA_PointerMove;
    UPROPERTY() TObjectPtr<UInputAction> IA_Select;
    UPROPERTY() TObjectPtr<UInputAction> IA_OfferCard;
    UPROPERTY() TObjectPtr<UInputAction> IA_NavigateUI;
    UPROPERTY() TObjectPtr<UInputAction> IA_OpenJournal;
    UPROPERTY() TObjectPtr<UInputAction> IA_Back;
    UPROPERTY() TObjectPtr<UInputMappingContext> IMC_MouseKeyboard;
    UPROPERTY() TObjectPtr<UInputMappingContext> IMC_Gamepad;

private:
    void RegisterMappingContext(APlayerController* PC);
    EInputMode CurrentMode = EInputMode::Mouse;
};

// .cpp
void UMossInputAbstractionSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    // Soft paths — configurable via .ini (FSoftObjectPath) or hardcoded here
    IA_PointerMove = LoadObject<UInputAction>(nullptr, TEXT("/Game/Input/Actions/IA_PointerMove.IA_PointerMove"));
    // ... 나머지 5 Action load
    IMC_MouseKeyboard = LoadObject<UInputMappingContext>(nullptr,
        TEXT("/Game/Input/Contexts/IMC_MouseKeyboard.IMC_MouseKeyboard"));
    IMC_Gamepad = LoadObject<UInputMappingContext>(nullptr,
        TEXT("/Game/Input/Contexts/IMC_Gamepad.IMC_Gamepad"));

    // APlayerController 접근 — 생성 시점에 BindToGameInstance 또는 LocalPlayerAdded 이벤트 구독
    if (auto* World = GetGameInstance()->GetWorld()) {
        if (auto* PC = World->GetFirstPlayerController()) {
            RegisterMappingContext(PC);
        }
    }
}

void UMossInputAbstractionSubsystem::RegisterMappingContext(APlayerController* PC) {
    if (auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())) {
        Subsystem->AddMappingContext(IMC_MouseKeyboard, 1);
        Subsystem->AddMappingContext(IMC_Gamepad, 1);  // MVP 구조만 — binding은 VS
    }
}
```

- `LoadObject` 대신 `FSoftObjectPath` + `LoadSynchronous` 권장 (current-best-practices.md). 여기서는 GDD §Upstream Dep `UEnhancedInputLocalPlayerSubsystem` 래핑만 목표
- `Source/MadeByClaudeCode/Input/` 디렉터리

---

## Out of Scope

- Story 003: Input mode auto-detect (Formula 1 hysteresis) + `OnInputModeChanged` delegate
- Story 004: Offer Hold boundary tests (Formula 2)
- Story 005: Mouse-only/Hover-only MANUAL + Gamepad disconnect silent

---

## QA Test Cases

**For Logic story:**
- **Subsystem lifecycle**
  - Given: GameInstance init, PlayerController 존재
  - When: Subsystem `Initialize()`
  - Then: `IMC_MouseKeyboard` + `IMC_Gamepad` 모두 등록 (PIE에서 IMC 목록 쿼리 확인)
- **UDeveloperSettings CDO access**
  - Given: `UMossInputAbstractionSettings` 정의
  - When: `UMossInputAbstractionSettings::Get()->OfferDragThresholdSec`
  - Then: 0.15 (default)
  - Edge cases: Project Settings → Moss Baby → Input Abstraction 렌더링 수동 확인
- **6 Action + 2 IMC load**
  - Given: Content 경로에 에셋 존재
  - When: Subsystem Initialize
  - Then: `IA_PointerMove`, `IA_Select`, ..., `IMC_MouseKeyboard`, `IMC_Gamepad` 모두 non-null

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/input/subsystem_skeleton_test.cpp` (AUTOMATED — asset load + IMC 등록 검증)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (6 Action + 2 IMC 에셋)
- Unlocks: Story 003, 004
