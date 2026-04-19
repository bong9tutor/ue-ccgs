# Epic: Window & Display Management

> **Layer**: Meta
> **GDD**: [design/gdd/window-display-management.md](../../../design/gdd/window-display-management.md)
> **Architecture Module**: Meta / Window — 800×600 min + Per-Monitor DPI V2 + focus handling + FWindowState save
> **Status**: Ready
> **Stories**: 6 created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Window & Display Management는 데스크톱 cozy companion의 창 생태계를 정의한다. 800×600 최소 크기(EC-08) + Per-Monitor DPI Awareness V2(WM_DPICHANGED 자동 재계산, EC-02) + 저장된 창 위치의 50% 가시성 보장(EC-07) + 포커스 상실 시 `bDisableWorldRendering=true`(최소화 절약). Always-on-top 지원(Rule 7) — 다른 앱 위 표시하되 포커스 강탈 금지. 모달 다이얼로그 절대 금지(Rule 10, Pillar 1). 게임 일시정지·음소거·알림 금지(Rule 6, Pillar 1). `FWindowState`(위치/크기/AlwaysOnTop)를 Save/Load에 위임. Input Abstraction focus events 구독. Steam Deck Proton에서 `SetWindowPos(HWND_TOPMOST)` 동작은 OQ-IMP-5 실기 검증.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossWindowDisplaySettings` — MinWindowWidth/Height, DefaultAlwaysOnTop 등 knob | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-window-001 | OQ-4 Steam Deck Proton `SetWindowPos(HWND_TOPMOST)` 동작 | OQ-IMP-5 Implementation task ✅ |
| TR-window-002 | `UDeveloperSettings` knob exposure | ADR-0011 ✅ |
| TR-window-003 | Rule 10 / AC-WD-13 모달 다이얼로그 금지 (Pillar 1) | GDD contract ✅ |
| TR-window-004 | Rule 6 / AC-WD-06 focus loss 시 일시정지·음소거 금지 | GDD contract ✅ |
| TR-window-005 | Rule 7 / AC-WD-10 Always-on-top 포커스 강탈 금지 | GDD contract ✅ |
| TR-window-006 | EC-02 / AC-WD-04 Per-Monitor DPI Awareness V2 | GDD contract ✅ |
| TR-window-007 | EC-07 / AC-WD-14 저장된 위치 화면 밖 → 기본 모니터 중앙 + 50% 가시성 | GDD contract ✅ |
| TR-window-008 | EC-08 800×600 최소 크기 OS 강제 | GDD contract ✅ |

## Key Interfaces

- **Publishes**: focus events (Window → Time 전파 — OnWindowFocusLost/Gained)
- **Consumes**: Input Abstraction focus events, Save/Load `FWindowState` 영속화
- **Owned types**: `FWindowState` struct (위치/크기/AlwaysOnTop), Windows native API 래핑 (`SetWindowPos(HWND_TOPMOST)`)
- **Settings**: `UMossWindowDisplaySettings`
- **Platform integration**: C++ Windows API — Plugin 또는 Game Instance Subsystem에서 GameViewportClient의 HWND 핸들 취득

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [`UMossWindowDisplaySettings` + 최소/기본 창 크기 OS 강제](story-001-min-window-size-default-size-developer-settings.md) | UI | Ready | ADR-0011 |
| 002 | [Per-Monitor DPI Awareness V2 + `WM_DPICHANGED` 재계산 + DPIScaleOverride](story-002-per-monitor-dpi-awareness-v2.md) | UI | Ready | ADR-0011 |
| 003 | [`FWindowState` 구조체 + Save/Load 왕복 + 리사이즈 완료 시 영속화](story-003-fwindow-state-save-load-persistence.md) | Integration | Ready | — |
| 004 | [Always-on-top Windows API (`SetWindowPos HWND_TOPMOST`) + 포커스 강탈 금지](story-004-always-on-top-setwindowpos-topmost.md) | UI | Ready | ADR-0011 |
| 005 | [Focus 이벤트 (50ms debounce) + `bDisableWorldRendering` 최소화 절약 + No-Pause/No-Mute](story-005-focus-events-no-pause-no-mute.md) | Integration | Ready | — |
| 006 | [화면 밖 위치 복구 EC-07 + 모달 금지 grep + 씬 비율 레터박스](story-006-off-screen-recovery-modal-ban-scene-letterbox.md) | UI | Ready | — |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-WD-01~14)
- AC-WD-04 DPI 전환 (100% → 150% → 200%) 수동 검증
- AC-WD-06 focus loss 시 게임 진행 유지 (일시정지 없음)
- AC-WD-10 Always-on-top 활성 시 포커스 강탈 없음
- AC-WD-13 모달 다이얼로그 grep — `SNew(SModal*)` 또는 `FMessageDialog::Open` 등 모달 API 호출 = 0건
- AC-WD-14 화면 밖 위치 복구 — 다중 모니터 시뮬 테스트
- OQ-IMP-5 — Steam Deck 데스크톱 모드 Proton에서 Always-on-top 동작 실기 확인 (manual QA)
- `FWindowState` Save/Load 왕복 테스트

## Next Step

Run `/create-stories meta-window-display` to break this epic into implementable stories.
