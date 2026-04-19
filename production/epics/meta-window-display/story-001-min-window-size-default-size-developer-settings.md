# Story 001 — `UMossWindowDisplaySettings` + 최소/기본 창 크기 OS 강제

> **Epic**: [meta-window-display](EPIC.md)
> **Layer**: Meta
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/window-display-management.md](../../../design/gdd/window-display-management.md) §C Core Rules Rule 2 (최소 800×600) + Rule 3 (기본 1024×768) + §E EC-08 최소 크기 OS 강제 + §G Tuning Knobs (`MinWindowWidth/Height`, `DefaultWindowWidth/Height`) + §Implementation Notes §1
- **TR-ID**: TR-window-002 (UDeveloperSettings knob exposure) + TR-window-008 (EC-08 800×600 최소 크기 OS 강제)
- **Governing ADR**: [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) — `UMossWindowDisplaySettings : UDeveloperSettings`
- **Engine Risk**: LOW — `SWindow::FArguments::MinWidth/MinHeight` + `UDeveloperSettings` UE 5.6 stable
- **Engine Notes**: `SWindow::FArguments`에 `MinWidth(800).MinHeight(600)` + `SetSizingRule(ESizingRule::UserSized)` 설정 필수 (GDD §Implementation Notes §1). UE 창 생성 타이밍: `FEngineLoop::PreInit` 후, `UGameInstance::Init` 전. 커스텀 창 생성 대신 UE 기본 창 생성 훅 (`GameUserSettings` 확장 또는 `GameViewportClient` override) 경유 권장.
- **Control Manifest Rules**:
  - Meta Layer Rules §Required Patterns — "`SWindow::FArguments::MinWidth/MinHeight` = 800/600" (window-display-management.md source)
  - Cross-Layer Rules §Required Patterns — "시스템 전역 상수 Tuning Knob은 `U[SystemName]Settings : UDeveloperSettings`" (ADR-0011)
  - Cross-Layer Rules §Required Patterns — Hot reload 불가 knob에 `meta=(ConfigRestartRequired=true)` (ADR-0011)

## Acceptance Criteria

- **AC-WD-01** / **TR-window-008** [`MANUAL`/ADVISORY] — 창 모드 리사이즈 핸들로 700×500 줄이려 시도 → 창 800×600 이하 축소 차단. 실제 창 크기 `Width >= 800 AND Height >= 600`.
- **AC-WD-02** [`AUTOMATED`/BLOCKING] — 세이브 데이터 없이 첫 실행 → 메인 루프 진입 시 창 크기 `1024×768` (±1px).
- **AC-WD-Settings-01** / **TR-window-002** [`CODE_REVIEW`/BLOCKING] — `UMossWindowDisplaySettings` 클래스가 `UDeveloperSettings` 상속, `UCLASS(Config=Game, DefaultConfig)` 매크로, `GetCategoryName() = "Moss Baby"`, `GetSectionName() = "Window & Display"` 리턴.

## Implementation Notes

1. **`UMossWindowDisplaySettings` 정의** (`Source/MadeByClaudeCode/Settings/MossWindowDisplaySettings.h`) — ADR-0011 §Key Interfaces 인용:
   ```cpp
   UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Window & Display"))
   class UMossWindowDisplaySettings : public UDeveloperSettings {
       GENERATED_BODY()
   public:
       // TR-window-008 최소 크기 (Restart Required — 창 생성 시점에만 사용)
       UPROPERTY(Config, EditAnywhere, Category="Window Size",
                 meta=(ClampMin="640", ClampMax="1280", ConfigRestartRequired=true))
       int32 MinWindowWidth = 800;

       UPROPERTY(Config, EditAnywhere, Category="Window Size",
                 meta=(ClampMin="480", ClampMax="960", ConfigRestartRequired=true))
       int32 MinWindowHeight = 600;

       UPROPERTY(Config, EditAnywhere, Category="Window Size",
                 meta=(ClampMin="800", ClampMax="1920", ConfigRestartRequired=true))
       int32 DefaultWindowWidth = 1024;

       UPROPERTY(Config, EditAnywhere, Category="Window Size",
                 meta=(ClampMin="600", ClampMax="1080", ConfigRestartRequired=true))
       int32 DefaultWindowHeight = 768;

       UPROPERTY(Config, EditAnywhere, Category="Always On Top")
       bool bAlwaysOnTopDefault = false;

       UPROPERTY(Config, EditAnywhere, Category="DPI",
                 meta=(ClampMin="0.0", ClampMax="2.0"))
       float DPIScaleOverride = 0.0f;

       UPROPERTY(Config, EditAnywhere, Category="Focus",
                 meta=(ClampMin="0", ClampMax="200", Units="Milliseconds"))
       int32 FocusEventDebounceMsec = 50;

       UPROPERTY(Config, EditAnywhere, Category="Performance")
       bool bDisableRenderWhenMinimized = true;

       UPROPERTY(Config, EditAnywhere, Category="Scene Aspect",
                 meta=(ClampMin="3", ClampMax="16"))
       int32 SceneAspectRatioNumerator = 4;

       UPROPERTY(Config, EditAnywhere, Category="Scene Aspect",
                 meta=(ClampMin="3", ClampMax="9"))
       int32 SceneAspectRatioDenominator = 3;

       // UDeveloperSettings overrides
       virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }
       virtual FName GetSectionName() const override { return TEXT("Window & Display"); }

       static const UMossWindowDisplaySettings* Get() {
           return GetDefault<UMossWindowDisplaySettings>();
       }
   };
   ```

2. **창 생성 시 Min/Default 적용** — `FMossWindowManager` 또는 `UMossWindowSubsystem::Initialize`:
   ```cpp
   void UMossWindowSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
       Super::Initialize(Collection);
       const auto* Settings = UMossWindowDisplaySettings::Get();

       TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
       if (!Window.IsValid()) return;

       // AC-WD-01 최소 크기 OS 강제 (GDD EC-08 + Rule 2)
       FWindowSizeLimits Limits;
       Limits.SetMinWidth((float)Settings->MinWindowWidth);
       Limits.SetMinHeight((float)Settings->MinWindowHeight);
       Window->SetSizeLimits(Limits);

       // AC-WD-02 기본 크기 (Save/Load FWindowState 없을 때만)
       if (!HasSavedWindowState()) {
           Window->Resize(FVector2D(Settings->DefaultWindowWidth, Settings->DefaultWindowHeight));
       }
   }
   ```

3. **`HasSavedWindowState`**:
   - `UMossSaveSubsystem::GetSaveData()->WindowState.Size != FVector2D::ZeroVector` 확인 — Fresh Start 판정.

## Out of Scope

- `FWindowState` struct 정의 + Save/Load 통합 (story 003)
- DPI 스케일링 (story 002)
- Always-on-top (story 004)
- Focus 이벤트 (story 005)
- 화면 밖 복구 EC-07 (story 006)

## QA Test Cases

**Test Case 1 — UDeveloperSettings 구조 (AC-WD-Settings-01)**
- **Setup**: `Source/MadeByClaudeCode/Settings/MossWindowDisplaySettings.h`.
- **Verify**:
  - `grep "class UMossWindowDisplaySettings.*public UDeveloperSettings"` 매치 1건.
  - `grep "UCLASS(Config=Game, DefaultConfig"` 매치 1건.
  - `grep "GetCategoryName.*Moss Baby"` 매치 1건.
  - `grep "GetSectionName.*Window & Display"` 매치 1건.
  - `grep "ConfigRestartRequired=true"` 매치 ≥ 4건 (Min/Default Width/Height).
- **Pass**: 5 조건 충족.

**Test Case 2 — 기본 창 크기 (AC-WD-02)**
- **Setup**: SaveData 없는 Fresh Start. Mock engine 초기화.
- **Verify**:
  - 첫 프레임에서 `Window->GetSizeInScreen() ≈ (1024, 768)` (±1px).
- **Pass**: 조건 충족.

**Test Case 3 — 최소 창 크기 강제 (AC-WD-01 MANUAL)**
- Evidence: `production/qa/evidence/ac-wd-01-min-size.md` 체크리스트.
- Steps:
  - 게임 실행, 창 모드.
  - 창 리사이즈 핸들로 700×500 시도.
  - 실측: `FPlatformApplicationMisc::GetWorkArea()` 또는 창 geometry API.
- Pass: 크기 `>= 800×600`.

**Test Case 4 — ini 외부화 확인**
- **Setup**: Editor 저장 후 `Config/DefaultGame.ini` 열기.
- **Verify**: `[/Script/MadeByClaudeCode.MossWindowDisplaySettings]` 섹션 존재 + `MinWindowWidth=800` 등 knob 값 평문.
- **Pass**: 조건 충족.

## Test Evidence

- [ ] `tests/unit/meta/window_settings_class_test.cpp` — AC-WD-Settings-01 (grep + 컴파일).
- [ ] `tests/integration/meta/window_default_size_test.cpp` — AC-WD-02.
- [ ] `production/qa/evidence/ac-wd-01-min-size.md` — AC-WD-01 (MANUAL).

## Dependencies

- **Depends on**: 없음 (Meta layer bootstrap).
- **Unlocks**: story-002 (DPI), story-003 (FWindowState + Save/Load), story-004 (Always-on-top), story-005 (Focus), story-006 (EC-07 recovery).
