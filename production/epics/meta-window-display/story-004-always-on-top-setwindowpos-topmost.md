# Story 004 — Always-on-top Windows API (`SetWindowPos HWND_TOPMOST`) + 포커스 강탈 금지

> **Epic**: [meta-window-display](EPIC.md)
> **Layer**: Meta
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/window-display-management.md](../../../design/gdd/window-display-management.md) §C Rule 7 Always-on-top + §E EC-04 다른 앱 전체화면 + §Implementation Notes §3 + §H AC-WD-09 기본값 + AC-WD-10 토글 적용
- **TR-ID**: TR-window-005 (Rule 7 / AC-WD-10 포커스 강탈 금지)
- **Governing ADR**: [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) — `bAlwaysOnTopDefault` knob
- **Engine Risk**: LOW — Windows `SetWindowPos` Win32 API 안정. HWND 획득은 `GetOSWindowHandle` UE 5.6 stable.
- **Engine Notes**: `FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle()`로 HWND 획득. `#include <windows.h>` 필요 (Win64 platform guard). `SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE` 플래그로 위치·크기·포커스 변경 없이 z-order만 변경 (포커스 강탈 금지 AC-WD-10 핵심).
- **Control Manifest Rules**:
  - Meta Layer Rules §Forbidden Approaches — "Never steal focus on always-on-top activation"
  - Cross-Layer Rules §Cross-Cutting — 모달·알림 금지 (Pillar 1)

## Acceptance Criteria

- **AC-WD-09** [`MANUAL`/ADVISORY] — 세이브 데이터 없이 첫 실행 (`bAlwaysOnTopDefault = false`) → 창 `bAlwaysOnTop == false`. Windows `GetWindowLong(GWL_EXSTYLE) & WS_EX_TOPMOST == 0`.
- **AC-WD-10** / **TR-window-005** [`MANUAL`/ADVISORY] — Always-on-top 토글 활성화 → 창이 즉시 다른 앱 위에 표시 (메모장 위에 Moss Baby 창 나타남). Windows `GetWindowLong(GWL_EXSTYLE) & WS_EX_TOPMOST != 0`. 포커스 이동 없음.
- **OQ-IMP-5** / **TR-window-001** [`MANUAL`/ADVISORY] — Steam Deck 데스크톱 모드 Proton에서 `SetWindowPos(HWND_TOPMOST)` 동작 실기 확인.

## Implementation Notes

1. **Always-on-top 적용 함수** (GDD §Implementation Notes §3 인용):
   ```cpp
   #if PLATFORM_WINDOWS
   #include "Windows/AllowWindowsPlatformTypes.h"
   #include <windows.h>
   #include "Windows/HideWindowsPlatformTypes.h"
   #endif

   void UMossWindowSubsystem::SetAlwaysOnTop(bool bEnable) {
       bAlwaysOnTop = bEnable;

   #if PLATFORM_WINDOWS
       auto Window = FSlateApplication::Get().GetActiveTopLevelWindow();
       if (!Window.IsValid()) return;
       HWND Hwnd = (HWND)Window->GetNativeWindow()->GetOSWindowHandle();
       if (Hwnd == nullptr) return;

       HWND InsertAfter = bEnable ? HWND_TOPMOST : HWND_NOTOPMOST;
       // SWP_NOACTIVATE: 포커스 강탈 금지 (AC-WD-10 핵심)
       ::SetWindowPos(Hwnd, InsertAfter, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
   #endif

       // FWindowState에 반영 (story 003 로직 재사용 — SaveAsync debounce)
       if (auto* SaveSub = GetGameInstance()->GetSubsystem<UMossSaveSubsystem>()) {
           SaveSub->GetMutableSaveData()->WindowState.bAlwaysOnTop = bEnable;
           SaveSub->SaveAsync(ESaveReason::EWindowStateChanged);
       }
   }

   bool UMossWindowSubsystem::IsAlwaysOnTop() const {
       return bAlwaysOnTop;
   }
   ```

2. **기본값 적용 — Initialize 시**:
   ```cpp
   void UMossWindowSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
       Super::Initialize(Collection);
       // ... Settings + 창 생성 + FWindowState 복원 (story 001-003)

       const auto* Settings = UMossWindowDisplaySettings::Get();
       const auto* SaveData = GetGameInstance()->GetSubsystem<UMossSaveSubsystem>()->GetSaveData();

       bool bInitialTopmost = false;
       if (!SaveData->WindowState.IsDefaultInitialized()) {
           bInitialTopmost = SaveData->WindowState.bAlwaysOnTop;
       } else {
           bInitialTopmost = Settings->bAlwaysOnTopDefault;
       }

       SetAlwaysOnTop(bInitialTopmost);
   }
   ```

3. **우클릭 컨텍스트 메뉴 또는 Title & Settings UI 토글**:
   - MVP: 우클릭 컨텍스트 메뉴 또는 단축키 (예: F10)로 토글.
   - VS: Title & Settings UI에서 토글.
   - OQ-3 (GDD Open Questions) — UX 결정 필요, 본 story는 API 제공만.

4. **EC-04 대응 (다른 앱 전체화면)**:
   - OS가 독점 전체화면 앱의 TOPMOST보다 우선 — UE 측 추가 처리 불필요.
   - 다른 앱이 BorderlessWindowed라면 Moss Baby TOPMOST 유지 — 알림 없음 (Pillar 1).

## Out of Scope

- 우클릭 컨텍스트 메뉴 UX (OQ-3)
- Title & Settings UI 통합 (VS)
- Focus 이벤트 (story 005)

## QA Test Cases

**Test Case 1 — AC-WD-09 기본값 (MANUAL)**
- Evidence: `production/qa/evidence/ac-wd-09-default-topmost.md`.
- Steps:
  - SaveData 없이 첫 실행 (`bAlwaysOnTopDefault = false`).
  - Windows 실행 시 `GetWindowLong(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST` 확인 (Spy++ 또는 코드 assert).
- Pass: `WS_EX_TOPMOST == 0`.

**Test Case 2 — AC-WD-10 토글 + 포커스 미이동 (MANUAL)**
- Evidence: `production/qa/evidence/ac-wd-10-topmost-toggle.md`.
- Steps:
  - 게임 실행, 메모장 열고 메모장 포커스.
  - 게임 창에서 Always-on-top 토글 활성 (F10 또는 UX 경로).
  - Visual: 게임 창이 메모장 위로 나옴.
  - 포커스: 메모장이 활성 창 유지 (포커스 강탈 없음).
  - `GetWindowLong(gameHWnd, GWL_EXSTYLE) & WS_EX_TOPMOST != 0`.
- Pass: 4 조건 모두 충족.

**Test Case 3 — OQ-IMP-5 Steam Deck Proton (MANUAL)**
- Evidence: `production/qa/evidence/oq-imp-5-steam-deck-topmost.md`.
- Steps:
  - Steam Deck 데스크톱 모드 진입 (Proton 환경).
  - Moss Baby 실행, 다른 데스크톱 앱 (예: Firefox) 열기.
  - Always-on-top 토글 활성.
  - Visual 확인: Moss Baby가 Firefox 위에 유지되는가?
  - Proton logs 확인: `SetWindowPos` 호출 성공 여부.
- Pass: TOPMOST 동작 확인 (또는 Proton 제한 사항 문서화).

**Test Case 4 — Save/Load 왕복**
- **Setup**: `SetAlwaysOnTop(true)` 호출 후 앱 재시작.
- **Verify**:
  - 재시작 후 `IsAlwaysOnTop() == true`.
  - `GetWindowLong` WS_EX_TOPMOST flag set.
- **Pass**: 두 조건 충족.

## Test Evidence

- [ ] `tests/integration/meta/window_always_on_top_test.cpp` — Test Case 4 (Save/Load 왕복).
- [ ] `production/qa/evidence/ac-wd-09-default-topmost.md` — MANUAL.
- [ ] `production/qa/evidence/ac-wd-10-topmost-toggle.md` — MANUAL.
- [ ] `production/qa/evidence/oq-imp-5-steam-deck-topmost.md` — Steam Deck Proton MANUAL (OQ-IMP-5).

## Dependencies

- **Depends on**: story-001 (`bAlwaysOnTopDefault` knob), story-003 (`FWindowState.bAlwaysOnTop` 저장 경로). Platform Windows API (Win32).
- **Unlocks**: story-005 (Focus 이벤트 통합), story-006 (EC-07 복구).
