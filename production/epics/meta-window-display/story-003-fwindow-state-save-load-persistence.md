# Story 003 — `FWindowState` 구조체 + Save/Load 왕복 + 리사이즈 완료 시 영속화

> **Epic**: [meta-window-display](EPIC.md)
> **Layer**: Meta
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/window-display-management.md](../../../design/gdd/window-display-management.md) §C Rule 3 기본 창 크기 + Rule 8 레이아웃 적응 + §F Dependencies (Save/Load — `FWindowState`) + §Data Ownership (`FWindowState` 구조체 — Window & Display 유일 writer)
- **TR-ID**: (epic DoD AC-WD-03 `FWindowState` Save/Load 왕복 테스트)
- **Governing ADR**: *(직접 없음 — Save/Load GDD `FWindowState` 필드 + architecture §Cross-Layer)*
- **Engine Risk**: LOW — `USTRUCT` + `USaveGame` 필드 추가는 UE 5.6 stable
- **Engine Notes**: `FWindowState` 필드는 `UMossSaveData`에 추가 (Save/Load GDD). Window & Display가 유일 writer — 다른 시스템 read-only. 리사이즈 완료 이벤트는 `FSlateApplication::Get().OnWindowResized` 또는 `SWindow::SetOnResized` 콜백 사용. 리사이즈 도중 매 프레임 저장은 금지 — 완료(mouse release) 시점에만 저장 (`ESaveReason::EWindowStateChanged` 신규 reason 또는 debounce).
- **Control Manifest Rules**:
  - Foundation Layer Rules §Required Patterns — "Save/Load는 per-trigger atomicity만 보장" (ADR-0009)
  - Global Rules §Cross-Cutting — SaveSchemaVersion bump 정책 (필드 추가 시 +1)

## Acceptance Criteria

- **AC-WD-03** [`INTEGRATION`/BLOCKING] — 기본 창 크기 1024×768 → 960×720으로 리사이즈 후 화면 좌상단 이동 → 게임 종료 → 재시작. 재시작 후 창 크기 960×720, 위치 복원 (±5px, OS 데코레이션 차이).
- **AC-WD-FWS-01** [`CODE_REVIEW`/BLOCKING] — `FWindowState` `USTRUCT` 정의에 `FIntPoint Position` + `FIntPoint Size` + `bool bAlwaysOnTop` + `EWindowDisplayMode WindowMode` + `float DPIScaleOverride` 필드 포함. `UPROPERTY()` 표기 (GC 안전성).

## Implementation Notes

1. **`FWindowState` USTRUCT 정의** (`Source/MadeByClaudeCode/Meta/Window/WindowState.h`):
   ```cpp
   UENUM(BlueprintType)
   enum class EWindowDisplayMode : uint8 {
       Windowed = 0,
       BorderlessWindowed = 1,
       Fullscreen = 2
   };

   USTRUCT(BlueprintType)
   struct FWindowState {
       GENERATED_BODY()

       UPROPERTY() FIntPoint Position = FIntPoint::ZeroValue;
       UPROPERTY() FIntPoint Size = FIntPoint(1024, 768);
       UPROPERTY() bool bAlwaysOnTop = false;
       UPROPERTY() EWindowDisplayMode WindowMode = EWindowDisplayMode::Windowed;
       UPROPERTY() float DPIScaleOverride = 0.0f;

       bool IsDefaultInitialized() const {
           return Size == FIntPoint(1024, 768) && Position == FIntPoint::ZeroValue;
       }
   };
   ```

2. **`UMossSaveData::WindowState` 필드 추가**:
   - Save/Load GDD §Data Schema에 필드 추가.
   - SaveSchemaVersion +1 (필드 추가 convention).
   - Fresh Start 시 default value 초기화.

3. **리사이즈 완료 시 저장**:
   ```cpp
   void UMossWindowSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
       Super::Initialize(Collection);
       // ...

       if (auto Window = FSlateApplication::Get().GetActiveTopLevelWindow()) {
           Window->GetOnWindowMoved().AddUObject(this, &UMossWindowSubsystem::OnWindowMovedOrResized);
           // Note: UE 5.6에서 OnWindowResized 콜백은 SWindow 직접 API 또는 
           // FSlateApplication::OnActiveTabChanged 경유 — 프로젝트 확인 후 정확 API 채택
       }
   }

   void UMossWindowSubsystem::OnWindowMovedOrResized(const TSharedRef<SWindow>& Window) {
       // Debounce: 200ms 이내 연속 이벤트는 마지막 1회만 저장
       GetWorld()->GetTimerManager().ClearTimer(SaveDebounceHandle);
       GetWorld()->GetTimerManager().SetTimer(SaveDebounceHandle, [this, Window]() {
           FWindowState NewState = CurrentWindowState();
           auto* SaveSub = GetGameInstance()->GetSubsystem<UMossSaveSubsystem>();
           if (SaveSub) {
               SaveSub->GetMutableSaveData()->WindowState = NewState;
               SaveSub->SaveAsync(ESaveReason::EWindowStateChanged);
           }
       }, 0.2f, false);
   }

   FWindowState UMossWindowSubsystem::CurrentWindowState() const {
       FWindowState State;
       auto Window = FSlateApplication::Get().GetActiveTopLevelWindow();
       if (Window.IsValid()) {
           const FVector2D Pos = Window->GetPositionInScreen();
           const FVector2D Size = Window->GetSizeInScreen();
           State.Position = FIntPoint((int32)Pos.X, (int32)Pos.Y);
           State.Size = FIntPoint((int32)Size.X, (int32)Size.Y);
       }
       State.bAlwaysOnTop = bAlwaysOnTop;  // story 004에서 멤버화
       State.WindowMode = CurrentWindowMode;
       const auto* Settings = UMossWindowDisplaySettings::Get();
       State.DPIScaleOverride = Settings->DPIScaleOverride;
       return State;
   }
   ```

4. **복원 — Initialize 시**:
   ```cpp
   void UMossWindowSubsystem::RestoreWindowState() {
       const auto* SaveSub = GetGameInstance()->GetSubsystem<UMossSaveSubsystem>();
       if (!SaveSub) return;
       const FWindowState& State = SaveSub->GetSaveData()->WindowState;

       if (State.IsDefaultInitialized()) {
           return;  // Fresh Start — story 001의 DefaultWindowWidth/Height 경로 사용
       }

       auto Window = FSlateApplication::Get().GetActiveTopLevelWindow();
       if (!Window.IsValid()) return;

       Window->MoveWindowTo(FVector2D(State.Position.X, State.Position.Y));
       Window->Resize(FVector2D(State.Size.X, State.Size.Y));
       // bAlwaysOnTop / WindowMode 적용은 story 004에서
   }
   ```

5. **`ESaveReason::EWindowStateChanged` enum 추가**:
   - Save/Load GDD `ESaveReason` enum에 새 값 추가 (또는 기존 `ESettingsChanged` 재사용 — Save/Load 설계 시 결정).

## Out of Scope

- Always-on-top 적용 (story 004)
- Focus 이벤트 (story 005)
- 화면 밖 복구 EC-07 (story 006)
- EC-09 모니터 DPI 혼합 복원 시 DPI 재계산 (story 002에서 `OnDisplayMetricsChanged`가 이미 처리)

## QA Test Cases

**Test Case 1 — FWindowState 구조 (AC-WD-FWS-01)**
- **Setup**: `Source/MadeByClaudeCode/Meta/Window/WindowState.h`.
- **Verify**:
  - `grep "USTRUCT" WindowState.h` + `FWindowState` 매치 1건.
  - `grep "UPROPERTY.*FIntPoint Position"` 1건.
  - `grep "UPROPERTY.*FIntPoint Size"` 1건.
  - `grep "UPROPERTY.*bool bAlwaysOnTop"` 1건.
  - `grep "UPROPERTY.*EWindowDisplayMode WindowMode"` 1건.
- **Pass**: 5 grep 충족.

**Test Case 2 — Save/Load 왕복 (AC-WD-03 INTEGRATION)**
- **Setup**: Mock SaveLoad fixture. Fresh Start → 기본 1024×768.
- **Steps**:
  - `Window->Resize(FVector2D(960, 720))`, `Window->MoveWindowTo(FVector2D(0, 0))`.
  - `OnWindowMovedOrResized` → 200ms debounce → SaveAsync commit.
  - App 종료 시뮬 → LoadAsync 재시작 시뮬.
  - `RestoreWindowState()` 호출.
- **Verify**:
  - `Window->GetSizeInScreen() ≈ (960, 720)` (±5px).
  - `Window->GetPositionInScreen() ≈ (0, 0)` (±5px).
- **Pass**: 두 조건 충족.

**Test Case 3 — Fresh Start (기본값)**
- **Setup**: SaveData 없음 (`WindowState.IsDefaultInitialized() == true`).
- **Verify**:
  - `RestoreWindowState()` 호출 후 창 크기 1024×768 유지.
  - `Resize` / `MoveWindowTo` 호출 0회 (변경 없음).
- **Pass**: 두 조건 충족.

**Test Case 4 — Debounce (성능 보호)**
- **Setup**: 100ms 이내 `OnWindowMovedOrResized` 5회 연속 발행.
- **Verify**:
  - `SaveAsync(EWindowStateChanged)` 호출 카운트 == 1 (마지막만 저장).
- **Pass**: 조건 충족.

## Test Evidence

- [ ] `tests/unit/meta/window_state_struct_test.cpp` — AC-WD-FWS-01.
- [ ] `tests/integration/meta/window_state_save_load_roundtrip_test.cpp` — AC-WD-03 (Test Case 2).
- [ ] `tests/integration/meta/window_state_fresh_start_test.cpp` — Test Case 3.
- [ ] `tests/unit/meta/window_state_debounce_test.cpp` — Test Case 4.

## Dependencies

- **Depends on**: story-001 (`UMossWindowDisplaySettings` + 창 생성), story-002 (DPI). Foundation Save/Load (`UMossSaveData.WindowState` 필드 + SaveAsync reason).
- **Unlocks**: story-004 (Always-on-top 적용 + FWindowState 필드), story-005 (Focus events), story-006 (EC-07 복구 시 FWindowState 사용).
