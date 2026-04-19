# Story 005 — Focus 이벤트 (50ms debounce) + `bDisableWorldRendering` 최소화 절약 + No-Pause/No-Mute

> **Epic**: [meta-window-display](EPIC.md)
> **Layer**: Meta
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/window-display-management.md](../../../design/gdd/window-display-management.md) §C Rule 6 포커스 관리 + §States (Active/Background/Minimized 전환) + §E EC-03 Alt-Tab 빠른 전환 + §Implementation Notes §4-5 + §H AC-WD-06/07/08
- **TR-ID**: TR-window-004 (Rule 6 / AC-WD-06 focus loss 일시정지·음소거 금지)
- **Governing ADR**: *(직접 없음 — GDD contract)*
- **Engine Risk**: LOW — `FSlateApplication::OnWindowActivatedDelegate` / `OnApplicationActivationStateChanged` + `UGameViewportClient::bDisableWorldRendering` UE 5.6 stable
- **Engine Notes**: UE 5.6 `UGameViewportClient::Activated` / `Deactivated` 이벤트가 최소화/복원 감지에 더 신뢰성 있음 (GDD §Implementation Notes §5). `FTimerHandle FocusDebounceTimer` with `FocusEventDebounceMsec` (기본 50ms) — Alt-Tab 빠른 전환 (EC-03) 병합. `bDisableWorldRendering = true`는 최소화 상태에서만 — Background(포커스만 잃음, visible)에서는 렌더 유지.
- **Control Manifest Rules**:
  - Meta Layer Rules §Required Patterns — "Window 포커스 상실 시 `bDisableWorldRendering = true` — CPU/GPU 절약 (최소화 시)"
  - Meta Layer Rules §Forbidden Approaches — "Never auto-pause game on focus loss", "Never auto-mute audio on focus loss"

## Acceptance Criteria

- **AC-WD-06** / **TR-window-004** [`MANUAL`/ADVISORY] — 게임 Active 상태, 카드 3장 표시. Alt-Tab으로 다른 앱 포커스 → 알림·팝업·모달·태스크바 깜빡임 없음. 게임 일시정지 없음. 이끼 아이 씬 계속 렌더.
- **AC-WD-07** [`AUTOMATED`/BLOCKING] — Time & Session System 초기화됨, 게임 Active. Alt-Tab → `OnWindowFocusLost()` 콜백 1회 발행 (50ms 디바운스 후).
- **AC-WD-08** [`AUTOMATED`/BLOCKING] — `EInputMode::Mouse`, 창 Background 상태. 창 포커스 획득 → 커서 표시 (`APlayerController::bShowMouseCursor == true`).
- **AC-WD-Minimize-01** / (EC-06 관련) [`AUTOMATED`/BLOCKING] — 최소화 진입 시 `UGameViewportClient::bDisableWorldRendering = true` (`bDisableRenderWhenMinimized = true`일 때). 복원 시 `false`.

## Implementation Notes

1. **Focus 이벤트 구독** (GDD §Implementation Notes §4-5):
   ```cpp
   void UMossWindowSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
       Super::Initialize(Collection);
       // ... story 001-004

       // Active/Deactivated → UGameViewportClient 경유
       if (auto* ViewportClient = GetGameInstance()->GetGameViewportClient()) {
           // UE 5.6 공식 콜백 API 확인 후 (Activated/Deactivated 구독)
       }

       // 또는 FSlateApplication
       FSlateApplication::Get().OnApplicationActivationStateChanged()
           .AddUObject(this, &UMossWindowSubsystem::OnApplicationActivationChanged);
   }

   void UMossWindowSubsystem::OnApplicationActivationChanged(const bool bIsActive) {
       const auto* Settings = UMossWindowDisplaySettings::Get();

       // Debounce (AC-WD-07, EC-03)
       GetWorld()->GetTimerManager().ClearTimer(FocusDebounceTimer);
       GetWorld()->GetTimerManager().SetTimer(FocusDebounceTimer, [this, bIsActive]() {
           if (bIsActive) {
               OnWindowFocusGained.Broadcast();
               RestoreCursorVisibility();
               // bDisableWorldRendering = false (최소화 복원 시)
               if (auto* VC = GetGameInstance()->GetGameViewportClient()) {
                   VC->bDisableWorldRendering = false;
               }
           } else {
               OnWindowFocusLost.Broadcast();
               // AC-WD-06: 일시정지 없음, 음소거 없음, 모달 없음
               // bDisableWorldRendering = true 는 최소화 진입 시만 (Background는 렌더 유지)
           }
       }, Settings->FocusEventDebounceMsec / 1000.0f, false);
   }
   ```

2. **최소화 감지 — `UGameViewportClient::Deactivated`** (GDD §Implementation Notes §5):
   ```cpp
   void UMossWindowSubsystem::OnViewportDeactivated() {
       const auto* Settings = UMossWindowDisplaySettings::Get();
       if (Settings->bDisableRenderWhenMinimized) {
           if (auto* VC = GetGameInstance()->GetGameViewportClient()) {
               VC->bDisableWorldRendering = true;
           }
       }
   }

   void UMossWindowSubsystem::OnViewportActivated() {
       if (auto* VC = GetGameInstance()->GetGameViewportClient()) {
           VC->bDisableWorldRendering = false;
       }
   }
   ```
   - 최소화 감지 API는 UE 5.6 `FGenericWindow::IsMinimized()` polling 또는 `UGameViewportClient::Activated` 이벤트 — 구현 시 정확한 API 확인.

3. **커서 가시성 복원** (AC-WD-08):
   ```cpp
   void UMossWindowSubsystem::RestoreCursorVisibility() {
       auto* InputAbstraction = GetGameInstance()->GetSubsystem<UMossInputAbstractionSubsystem>();
       if (!InputAbstraction) return;
       const EInputMode CurrentMode = InputAbstraction->GetCurrentInputMode();

       if (auto* PC = GetGameInstance()->GetFirstLocalPlayerController()) {
           // Mouse 모드 → 커서 표시, Gamepad → 숨김 (AC-WD-08)
           PC->bShowMouseCursor = (CurrentMode == EInputMode::Mouse);
       }
   }
   ```

4. **Time & Session System 연동**:
   - `OnWindowFocusLost` / `OnWindowFocusGained` delegate — Time & Session이 구독 (GDD §Interactions 2).
   - 포커스 상실 ≠ 세션 일시정지 (Time & Session 자체 규칙 — 본 story는 delegate 발행만 책임).

5. **디바운스 정확도** (AC-WD-07):
   - `FTimerHandle.ClearTimer` + `SetTimer` 패턴으로 재발행 시 이전 타이머 취소 → 연속 이벤트의 마지막만 콜백 발행.
   - 50ms 이내 Alt-Tab 3회 시 콜백 1회.

## Out of Scope

- Time & Session System 자체 suspend/resume 로직 (Foundation Time Session epic)
- Audio 음소거 UX (플레이어 선택 — Audio System VS)
- 화면 밖 복구 EC-07 (story 006)

## QA Test Cases

**Test Case 1 — Focus Lost 콜백 (AC-WD-07)**
- **Setup**: Mock `FSlateApplication::OnApplicationActivationStateChanged(false)` broadcast.
- **Verify**:
  - 50ms 후 `OnWindowFocusLost` broadcast 1회.
  - `UGameplayStatics::IsGamePaused(GetWorld()) == false` (일시정지 없음).
  - Audio mix state 변경 없음.
- **Pass**: 3 조건 충족.

**Test Case 2 — Debounce 병합 (EC-03)**
- **Setup**: 30ms 간격으로 activation flip 5회 (t=0, 30, 60, 90, 120ms).
- **Verify**:
  - 마지막 state에 대응하는 콜백 1회만 발행.
- **Pass**: 조건 충족.

**Test Case 3 — Focus Gained 커서 복원 (AC-WD-08)**
- **Setup**: Background 상태, Mock InputMode = `Mouse`, `bShowMouseCursor = false`.
- **Verify**:
  - Activation true broadcast → debounce 후 `OnWindowFocusGained` 발행.
  - `bShowMouseCursor == true`.
- **Pass**: 두 조건 충족.

**Test Case 4 — 최소화 렌더 중단 (AC-WD-Minimize-01)**
- **Setup**: `bDisableRenderWhenMinimized = true`. Mock 최소화 이벤트.
- **Verify**:
  - `UGameViewportClient::bDisableWorldRendering == true`.
  - 복원 이벤트 후 `bDisableWorldRendering == false`.
- **Pass**: 두 조건 충족.

**Test Case 5 — AC-WD-06 MANUAL**
- Evidence: `production/qa/evidence/ac-wd-06-no-pause-no-mute.md`.
- Steps:
  - Offering 상태, 카드 3장 표시 중.
  - Alt-Tab 다른 앱.
  - 관찰: 알림/모달 생성 없음, 태스크바 깜빡임 없음.
  - 게임 씬 계속 렌더 (이끼 아이 살아있음).
  - Audio 음소거 없음.
- Pass: 모든 관찰 조건 충족.

## Test Evidence

- [ ] `tests/integration/meta/window_focus_debounce_test.cpp` — Test Cases 1, 2 (AC-WD-07).
- [ ] `tests/integration/meta/window_focus_cursor_test.cpp` — Test Case 3 (AC-WD-08).
- [ ] `tests/integration/meta/window_minimize_render_test.cpp` — Test Case 4.
- [ ] `production/qa/evidence/ac-wd-06-no-pause-no-mute.md` — AC-WD-06 MANUAL.

## Dependencies

- **Depends on**: story-001 (`FocusEventDebounceMsec`, `bDisableRenderWhenMinimized` knob). Foundation Input Abstraction (`EInputMode`), Foundation Time & Session (delegate 구독).
- **Unlocks**: story-006 (EC-07 복구).
