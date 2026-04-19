# Story 006 — 화면 밖 위치 복구 EC-07 + 모달 금지 grep + 씬 비율 레터박스

> **Epic**: [meta-window-display](EPIC.md)
> **Layer**: Meta
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/window-display-management.md](../../../design/gdd/window-display-management.md) §C Rule 10 모달 금지 + §D Formula 2 씬 비율 레터박스 + §E EC-07 저장된 위치 화면 밖 + §H AC-WD-11/13/14
- **TR-ID**: TR-window-003 (AC-WD-13 모달 금지) + TR-window-007 (EC-07 / AC-WD-14 화면 밖 복구)
- **Governing ADR**: *(직접 없음 — GDD contract)*
- **Engine Risk**: LOW — `FDisplayMetrics` + `FPlatformApplicationMisc::GetWorkArea` + grep CI hook 모두 UE 5.6 stable
- **Engine Notes**: `FDisplayMetrics::RebuildDisplayMetrics()` 또는 `FSlateApplication::Get().GetInitialDisplayMetrics()` — UE 5.6에서 `GetCachedDisplayMetrics` 사용 가능. Primary display size는 `FDisplayMetrics::PrimaryDisplayWidth/Height`. 50% 가시성 계산: 창 bounds와 display bounds 교집합 면적 ≥ 창 면적의 50%.
- **Control Manifest Rules**:
  - Meta Layer Rules §Required Patterns — "Window 위치가 화면 밖이면 기본 모니터 중앙 이동 + 50% 가시성 보장 — EC-07"
  - Meta Layer Rules §Forbidden Approaches — "Never create modal dialog — Rule 10, AC-WD-13, Pillar 1 위반"
  - Cross-Layer Rules §Cross-Cutting — 모달·팝업·모달 다이얼로그 금지 (Pillar 1)

## Acceptance Criteria

- **AC-WD-11** [`AUTOMATED`/BLOCKING] — `SceneAspectRatio = 4:3`, 창 1280×720 (16:9). 씬 렌더 영역 960×720, 좌우 레터박스 160px. 씬 내 이끼 아이 찌그러지지 않음.
- **AC-WD-13** / **TR-window-003** [`MANUAL`/ADVISORY, with CODE_REVIEW supplement] — `Windowed` → `BorderlessWindowed` 변경 시 확인 팝업·모달 없이 즉시 전환. grep: `SNew(SModal*)` 또는 `FMessageDialog::Open` 호출 = 0건 in Window 코드.
- **AC-WD-14** / **TR-window-007** [`AUTOMATED`/BLOCKING] — `FWindowState.Position = (-5000, -5000)` 세이브 데이터 주입. 게임 시작 → 창 기본 모니터 중앙 이동. 창의 최소 50%가 화면 내.

## Implementation Notes

1. **화면 밖 복구 함수 (EC-07)**:
   ```cpp
   void UMossWindowSubsystem::EnsureOnScreen() {
       auto Window = FSlateApplication::Get().GetActiveTopLevelWindow();
       if (!Window.IsValid()) return;

       FDisplayMetrics Metrics;
       FDisplayMetrics::RebuildDisplayMetrics(Metrics);

       const FVector2D Pos = Window->GetPositionInScreen();
       const FVector2D Size = Window->GetSizeInScreen();
       const float WindowArea = Size.X * Size.Y;

       // Primary display bounds
       const FIntRect PrimaryRect(
           0, 0,
           Metrics.PrimaryDisplayWidth,
           Metrics.PrimaryDisplayHeight);

       // 창 bounds
       const FIntRect WindowRect(
           (int32)Pos.X, (int32)Pos.Y,
           (int32)(Pos.X + Size.X), (int32)(Pos.Y + Size.Y));

       // 교집합 면적
       const int32 ClipX = FMath::Max(WindowRect.Min.X, PrimaryRect.Min.X);
       const int32 ClipY = FMath::Max(WindowRect.Min.Y, PrimaryRect.Min.Y);
       const int32 ClipXMax = FMath::Min(WindowRect.Max.X, PrimaryRect.Max.X);
       const int32 ClipYMax = FMath::Min(WindowRect.Max.Y, PrimaryRect.Max.Y);
       const int32 IntersectW = FMath::Max(0, ClipXMax - ClipX);
       const int32 IntersectH = FMath::Max(0, ClipYMax - ClipY);
       const float VisibleArea = IntersectW * IntersectH;

       // 50% 미만 → 기본 모니터 중앙 이동 (EC-07)
       if (VisibleArea < WindowArea * 0.5f) {
           const FVector2D CenterPos(
               (Metrics.PrimaryDisplayWidth - Size.X) * 0.5f,
               (Metrics.PrimaryDisplayHeight - Size.Y) * 0.5f);
           Window->MoveWindowTo(CenterPos);
           UE_LOG(LogMossWindow, Log,
                  TEXT("EC-07: window moved to center (%f, %f)"),
                  CenterPos.X, CenterPos.Y);
           // Pillar 1: 알림 없음
       }
   }
   ```

2. **`RestoreWindowState` 확장** — 복원 후 EnsureOnScreen 호출:
   ```cpp
   void UMossWindowSubsystem::RestoreWindowState() {
       // story 003 로직: Position/Size 적용
       // ...
       EnsureOnScreen();  // EC-07 방어
   }
   ```

3. **씬 비율 레터박스** (Formula 2):
   ```cpp
   FIntRect UMossWindowSubsystem::CalculateSceneLetterbox(const FVector2D& ViewportSize) const {
       const auto* Settings = UMossWindowDisplaySettings::Get();
       const float TargetAR = (float)Settings->SceneAspectRatioNumerator /
                              (float)Settings->SceneAspectRatioDenominator;
       const float ViewportAR = ViewportSize.X / ViewportSize.Y;

       int32 RenderW, RenderH, OffsetX, OffsetY;
       if (ViewportAR > TargetAR) {
           RenderH = (int32)ViewportSize.Y;
           RenderW = (int32)(RenderH * TargetAR);
           OffsetX = ((int32)ViewportSize.X - RenderW) / 2;
           OffsetY = 0;
       } else if (ViewportAR < TargetAR) {
           RenderW = (int32)ViewportSize.X;
           RenderH = (int32)(RenderW / TargetAR);
           OffsetX = 0;
           OffsetY = ((int32)ViewportSize.Y - RenderH) / 2;
       } else {
           RenderW = (int32)ViewportSize.X;
           RenderH = (int32)ViewportSize.Y;
           OffsetX = OffsetY = 0;
       }
       return FIntRect(OffsetX, OffsetY, OffsetX + RenderW, OffsetY + RenderH);
   }
   ```
   - 적용 방식은 GDD §Implementation Notes §6 — UMG `SConstraintCanvas` 또는 커스텀 `UGameViewportClient` 서브클래스. 본 story는 계산 API만 제공, UMG 통합은 Art/UI 단계.

4. **CI grep hook — 모달 금지 (AC-WD-13 CODE_REVIEW)**:
   ```bash
   # .claude/hooks/validate-window-modal-ban.sh
   FORBIDDEN=(
       'SNew.*SModal'
       'FMessageDialog::Open'
       'FMessageDialog::Show'
       'GEngine->ShowModal'
   )
   FOUND=0
   for P in "${FORBIDDEN[@]}"; do
       if grep -rE "$P" Source/MadeByClaudeCode/Meta/Window/ 2>/dev/null; then
           FOUND=1
           echo "ERROR: Forbidden modal API '$P' in Window code (AC-WD-13)"
       fi
   done
   [ $FOUND -eq 0 ]
   ```

5. **AC-WD-12 800×600 UI 판독 — MANUAL QA** (`production/qa/evidence/ac-wd-12-800x600-ui.md`):
   - Step 1: 창 크기 800×600.
   - Step 2: 카드 3장 + 이끼 아이 씬 + 일기 버튼 모두 표시.
   - Step 3: 텍스트·버튼 겹침·잘림 없음.
   - Step 4: 스크린샷 캡처.
   - Pass: 모든 UI 요소 판독 가능.

## Out of Scope

- `BorderlessWindowed` / `Fullscreen` 모드 전환 구현 (Title & Settings UI VS)
- Steam Deck 게임 모드 자동 Fullscreen (Rule 9) — VS
- OQ-2 최소화 중 머티리얼 애니메이션 영향 (MBC 협의)

## QA Test Cases

**Test Case 1 — EC-07 화면 밖 복구 (AC-WD-14)**
- **Setup**: Mock `FWindowState.Position = FIntPoint(-5000, -5000)`, `Size = (1024, 768)`. Primary display 1920×1080.
- **Verify**:
  - `RestoreWindowState()` 호출 → `EnsureOnScreen()` 실행.
  - `Window->GetPositionInScreen() ≈ ((1920-1024)/2, (1080-768)/2)` = (448, 156).
  - 창의 50% 이상 화면 내 (1024×768 모두 안).
- **Pass**: 3 조건 충족.

**Test Case 2 — 씬 레터박스 16:9 뷰포트 (AC-WD-11)**
- **Setup**: `SceneAspectRatio = 4:3`. `ViewportSize = (1280, 720)`.
- **Verify**:
  - `CalculateSceneLetterbox` 반환 `FIntRect(160, 0, 1120, 720)`.
  - RenderWidth = 960, RenderHeight = 720.
  - OffsetX = 160 (좌우 레터박스).
- **Pass**: 3 조건 충족.

**Test Case 3 — 씬 레터박스 4:3 뷰포트 (정확 일치)**
- **Setup**: `ViewportSize = (800, 600)` (AR 4:3).
- **Verify**:
  - `CalculateSceneLetterbox` 반환 `FIntRect(0, 0, 800, 600)` (레터박스 없음).
- **Pass**: 조건 충족.

**Test Case 4 — 모달 금지 grep (AC-WD-13)**
- **Setup**: `Source/MadeByClaudeCode/Meta/Window/` 전 파일.
- **Verify**:
  - `grep "SNew.*SModal"` 매치 0건.
  - `grep "FMessageDialog::Open"` 매치 0건.
  - `grep "FMessageDialog::Show"` 매치 0건.
  - `grep "GEngine->ShowModal"` 매치 0건.
- **Pass**: 4 grep 모두 0건.

**Test Case 5 — AC-WD-13 모드 전환 (MANUAL)**
- Evidence: `production/qa/evidence/ac-wd-13-modal-ban.md`.
- Steps:
  - Windowed 모드 실행.
  - Settings에서 BorderlessWindowed로 변경.
  - 관찰: 확인 팝업·모달 없음.
  - 모드 즉시 전환.
- Pass: 모달 미생성 + 즉시 전환.

**Test Case 6 — AC-WD-12 800×600 UI (MANUAL)**
- Evidence: `production/qa/evidence/ac-wd-12-800x600-ui.md` — 스크린샷 + 리드 서명.

## Test Evidence

- [ ] `tests/integration/meta/window_off_screen_recovery_test.cpp` — Test Case 1 (AC-WD-14).
- [ ] `tests/unit/meta/scene_letterbox_formula_test.cpp` — Test Cases 2, 3 (AC-WD-11).
- [ ] CI hook `.claude/hooks/validate-window-modal-ban.sh` — Test Case 4 (AC-WD-13 CODE_REVIEW supplement).
- [ ] `production/qa/evidence/ac-wd-13-modal-ban.md` — Test Case 5 MANUAL.
- [ ] `production/qa/evidence/ac-wd-12-800x600-ui.md` — Test Case 6 MANUAL.

## Dependencies

- **Depends on**: story-001 (SceneAspectRatio knob, 창 생성), story-003 (`FWindowState` 복원 경로 — `EnsureOnScreen` 통합 지점).
- **Unlocks**: Epic Done.
