# Story 002 — Per-Monitor DPI Awareness V2 + `WM_DPICHANGED` 재계산 + DPIScaleOverride

> **Epic**: [meta-window-display](EPIC.md)
> **Layer**: Meta
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/window-display-management.md](../../../design/gdd/window-display-management.md) §C Rule 5 DPI 인식 + §D Formula 1 DPI 스케일 계산 + §E EC-02 세션 중 DPI 변경 + §E EC-09 다중 모니터 DPI 혼합 + §Implementation Notes §2
- **TR-ID**: TR-window-006 (EC-02 / AC-WD-04 Per-Monitor DPI Awareness V2)
- **Governing ADR**: [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) §DPIScaleOverride knob
- **Engine Risk**: LOW — Per-Monitor DPI V2는 UE 5.6 기본 지원 (GDD §Implementation Notes §2). 단 `app.manifest`에 `<dpiAwareness>PerMonitorV2</dpiAwareness>` 선언 확인 필요.
- **Engine Notes**: UE 5.6 기본적으로 Per-Monitor DPI V2 처리 — Windows 디스플레이 설정 변경 시 `WM_DPICHANGED` 수신 자동. `UUserInterfaceSettings::GetDPIScaleBasedOnSize()` 또는 `FSlateApplication::Get().GetApplicationScale()`로 현재 DPI 획득. 다중 모니터 전환 시 `FDisplayMetrics` 재조회. 커스텀 스케일 적용은 `SWindow::SetDPIScaleFactor` 또는 `UUserInterfaceSettings`의 DPI Curve 조정.
- **Control Manifest Rules**:
  - Meta Layer Rules §Required Patterns — "Per-Monitor DPI Awareness V2 — 모니터 이동 시 `WM_DPICHANGED` 자동 재계산"
  - Cross-Layer Rules §Cross-Cutting — 재시작 불필요, 알림 없음 (Pillar 1)

## Acceptance Criteria

- **AC-WD-04** / **TR-window-006** [`AUTOMATED`/BLOCKING] — `DPIScaleOverride = 0.0` (자동), 시스템 DPI = 144 (150% 스케일). 초기화 완료 시 `CurrentDPIScale == 1.5` (±0.01).
- **AC-WD-05** [`AUTOMATED`/BLOCKING] — `DPIScaleOverride = 1.25`, 시스템 DPI = 144. 초기화 완료 시 `CurrentDPIScale == 1.25` (오버라이드 우선, Formula 1 무시).
- **AC-WD-DPI-EC02** [`AUTOMATED`/BLOCKING] — 세션 중 `WM_DPICHANGED` 수신 시뮬 (192 DPI → 96 DPI) → `CurrentDPIScale` 재계산, UMG 스케일 즉시 갱신.

## Implementation Notes

1. **`app.manifest` DPI 선언 확인** — `Config/Windows/DefaultGame.ini` 또는 프로젝트 build setup에서:
   - Windows 매니페스트에 `<dpiAwareness>PerMonitorV2</dpiAwareness>` 선언 존재 확인. UE 5.6 기본 템플릿은 이미 포함 — 누락 시 `Build.cs`에서 확인.

2. **DPI 스케일 계산 함수** (GDD Formula 1):
   ```cpp
   float UMossWindowSubsystem::CalculateDPIScale() const {
       const auto* Settings = UMossWindowDisplaySettings::Get();

       // DPIScaleOverride > 0 → 자동 계산 무시 (Formula 1 분기)
       if (Settings->DPIScaleOverride > 0.0f) {
           return Settings->DPIScaleOverride;
       }

       // 자동 계산: SystemDPI / ReferenceDPI (96)
       TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
       if (!Window.IsValid()) return 1.0f;

       const float SystemDPI = Window->GetNativeWindow()->GetDPIScaleFactor() * 96.0f;
       float Scale = SystemDPI / 96.0f;

       // 클램프 (GDD Formula 1 주석)
       Scale = FMath::Clamp(Scale, 0.5f, 3.0f);
       return Scale;
   }
   ```

3. **UMG에 스케일 적용**:
   ```cpp
   void UMossWindowSubsystem::ApplyDPIScale() {
       const float Scale = CalculateDPIScale();
       CurrentDPIScale = Scale;

       TSharedPtr<SWindow> Window = FSlateApplication::Get().GetActiveTopLevelWindow();
       if (Window.IsValid()) {
           Window->SetDPIScaleFactor(Scale);
       }

       // UMG ViewportClient 반영 — UUserInterfaceSettings의 ApplicationScale도 조정 가능
       // (UE 5.6 SlateApplication::SetApplicationScale은 multiplicative)
   }
   ```

4. **`WM_DPICHANGED` 콜백 통합** (EC-02):
   - `FSlateApplication::Get().OnDisplayMetricsChanged().AddLambda([this]() { ApplyDPIScale(); });`
   - 또는 UE 5.6 `FDisplayMetrics::GetDisplayMetrics()` polling in Tick.
   - 재계산 + UMG 즉시 갱신 — 재시작 없음 (GDD EC-02).
   - 알림 없음 (Pillar 1).

5. **EC-09 다중 모니터 DPI 혼합**:
   - 창이 두 모니터에 걸친 경우 UE 5.6 Per-Monitor V2가 창 중심 위치 모니터의 DPI를 기준으로 자동 결정 — 본 story에서 추가 처리 불필요. `OnDisplayMetricsChanged` 콜백만으로 충분.

## Out of Scope

- FWindowState 저장 (story 003)
- Always-on-top (story 004)
- 화면 밖 복구 (story 006)
- UMG 위젯별 스케일 커스터마이징 (외부 — Title & Settings UI VS)

## QA Test Cases

**Test Case 1 — 자동 DPI 계산 144 (AC-WD-04)**
- **Setup**: Mock `Window->GetNativeWindow()->GetDPIScaleFactor() = 1.5` (144 DPI 시 반환값). `DPIScaleOverride = 0.0`.
- **Verify**:
  - `CalculateDPIScale()` 반환값 == 1.5 (±0.01).
  - `ApplyDPIScale()` 호출 후 `CurrentDPIScale == 1.5`.
- **Pass**: 두 조건 충족.

**Test Case 2 — Override 적용 (AC-WD-05)**
- **Setup**: Mock DPI factor 1.5, `DPIScaleOverride = 1.25`.
- **Verify**:
  - `CalculateDPIScale()` 반환값 == 1.25 (Formula 1 자동 계산 무시).
- **Pass**: 조건 충족.

**Test Case 3 — 클램프 동작**
- **Setup**: Mock 극단 DPI factor 4.0 (Formula 1 Scale = 4.0).
- **Verify**:
  - `CalculateDPIScale()` 반환값 == 3.0 (클램프 상한).
- **Pass**: 조건 충족.

**Test Case 4 — DPI 변경 이벤트 (AC-WD-DPI-EC02)**
- **Setup**: Mock `FSlateApplication::OnDisplayMetricsChanged` broadcast — DPI 192 → 96 시뮬.
- **Verify**:
  - `ApplyDPIScale()` 호출 1회.
  - `CurrentDPIScale` 갱신 (1.0).
  - UMG notification·모달 생성 0건 (Pillar 1).
- **Pass**: 3 조건 충족.

**Test Case 5 — `app.manifest` DPI V2 선언**
- **Setup**: 빌드 출력 매니페스트 확인.
- **Verify**: `grep "PerMonitorV2" Binaries/Win64/*.manifest` 매치 1건.
- **Pass**: 조건 충족.

## Test Evidence

- [ ] `tests/unit/meta/window_dpi_calculation_test.cpp` — Test Cases 1, 2, 3.
- [ ] `tests/integration/meta/window_dpi_event_test.cpp` — Test Case 4 (AC-WD-DPI-EC02).
- [ ] `production/qa/evidence/ac-wd-04-dpi-manifest.md` — Test Case 5 + MANUAL 100%/150%/200% 전환 스크린샷.

## Dependencies

- **Depends on**: story-001 (`UMossWindowDisplaySettings` DPIScaleOverride knob).
- **Unlocks**: story-003 (FWindowState 저장 시 DPI 반영 제외 — DPI는 OS 실시간), story-004~006.
