# Epic: Lumen Dusk Scene (+ PSO Precaching)

> **Layer**: Core
> **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md)
> **Architecture Module**: Core / Rendering — 유리병 씬 + Lumen HWRT + Post-process LUT + PSO Precaching
> **Status**: Ready
> **Stories**: 10 stories — see Stories table
> **Manifest Version**: 2026-04-19

## Overview

Lumen Dusk Scene은 책상 위 유리병 diorama의 환경 에셋·조명·카메라·DoF + PSO Precaching 소유를 담당한다. UE 5.6 Lumen HWRT로 6 GameState별 색온도(2,200K Farewell ~ 4,200K Dream)를 GI에 반영. DirectionalLight=Stationary + SkyLight=Movable. MPC는 머티리얼 read-only, GSM이 유일 writer. `PP_MossDusk` Post Process Volume이 Vignette(Farewell)/DoF(Dream)/Contrast를 소유하며 `FOnGameStateChanged` 자체 Lerp. PSO Precaching(ADR-0013 Async Bundle + Graceful Degradation)으로 6 상태 전환 first-load hitching 최소화 — splash screen 금지(Pillar 1).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0004](../../../docs/architecture/adr-0004-mpc-light-actor-sync.md) | Hybrid — MPC(셰이더 read) + GSM이 Light Actor 직접 구동 + Lumen Dusk가 Post-process LUT 소유 | **HIGH** (Lumen HWRT GPU 5ms budget — AC-LDS-04 [5.6-VERIFY]) |
| [ADR-0013](../../../docs/architecture/adr-0013-pso-precaching-strategy.md) | Async Bundle + Graceful Degradation. `PSO_MossDusk.bin` + `FShaderPipelineCache::SetBatchMode(Fast)` + Dawn Entry `IsPSOReady()` | **HIGH** (PSO 점진 컴파일 — AC-LDS-11 [5.6-VERIFY]) |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossLumenDuskSettings` — LumenGPUBudgetMs, PSOWarmupTimeoutSec. `ULumenDuskAsset`은 예외 (per-scene asset) | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-lumen-001 | HIGH risk — Lumen HWRT + PSO Precaching | ADR-0004 + ADR-0013 ✅ |
| TR-lumen-002 | `Farewell → Any` 전환 분기 금지 (AC-GSM-05 협력) | architecture §Cross-Layer ✅ |
| TR-lumen-003 | Rule 3 Lumen HWRT 설정 + Mobility (DirLight=Stationary, SkyLight=Movable) | ADR-0004 ✅ |
| TR-lumen-004 | Rule 5 Post-process `PP_MossDusk` (Vignette Farewell, DoF Dream) 소유 | ADR-0004 ✅ |
| TR-lumen-005 | AC-LDS-10 Farewell Vignette 점진 증가 (1.5-2.0s, 0.0→0.6) | ADR-0004 ✅ |
| TR-lumen-006 | AC-LDS-16 Dream DoF Formula 1 clamp [1.4, 4.0] | ADR-0004 ✅ |

## Key Interfaces

- **Publishes**: 없음 (Lumen Dusk는 viewer/listener)
- **Consumes**: `FOnGameStateChanged` (Vignette/DoF trigger), `Pipeline.GetStillnessBudgetAsset()` (Particle 슬롯 Background)
- **Owned types**: `ALumenDuskSceneActor` (Light Actor cache + PSO state), `ULumenDuskAsset`, `PP_MossDusk` Post Process Volume, `PSO_MossDusk.bin`
- **Settings**: `UMossLumenDuskSettings`
- **API**: `StartPSOPrecaching()`, `IsPSOReady()` (GSM Dawn Entry), `OnGameStateChangedHandler()` (self Lerp)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [씬 레이아웃 (정적 메시·고정 카메라·FOV 35°)](story-001-scene-layout-static-meshes-camera.md) | Config/Data | Ready | — (GDD contract) |
| 002 | [Lumen HWRT 설정 + `ULumenDuskAsset` + `UMossLumenDuskSettings`](story-002-lumen-hwrt-settings-ulumenduskasset.md) | Config/Data | Ready | ADR-0004, ADR-0011 |
| 003 | [`ALumenDuskSceneActor` + Light Actor 등록 (GSM `RegisterSceneLights` 연동)](story-003-lumen-dusk-scene-actor-register-lights.md) | Integration | Ready | ADR-0004 |
| 004 | [`PP_MossDusk` Post Process Volume + Vignette Farewell Lerp + MotionBlur Off](story-004-post-process-volume-vignette-farewell.md) | Integration | Ready | ADR-0004 |
| 005 | [Dream DoF Formula 1 (`ApertureFStop` Lerp) + Clamp \[1.4, 4.0\]](story-005-dream-dof-formula1-aperture-lerp.md) | Integration | Ready | ADR-0004 |
| 006 | [PSO Precaching Phase 1 (Build Time Bundle 생성)](story-006-pso-phase1-build-time-bundle.md) | Config/Data | Ready | ADR-0013 |
| 007 | [PSO Precaching Phase 2 (Runtime Async Bundle Load + `BatchMode::Fast`)](story-007-pso-phase2-runtime-async-bundle-load.md) | Integration | Ready | ADR-0013 |
| 008 | [PSO Precaching Phase 3 (Dawn Entry `IsPSOReady()` + 10s Timeout)](story-008-pso-phase3-dawn-entry-ispsoready.md) | Integration | Ready | ADR-0013 |
| 009 | [앰비언트 파티클 + Stillness Budget + Formula 3 (Spawn Density) + Dream Reduction](story-009-ambient-particle-stillness-budget-formula3.md) | Integration | Ready | ADR-0007 |
| 010 | [6 상태 시각 수동 Sign-off (AC-LDS-07)](story-010-six-state-manual-signoff-ac-lds-07.md) | Visual/Feel | Ready | ADR-0004 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증
- **AC-LDS-04 [5.6-VERIFY]** — 6 GameState 각각 Lumen GPU ≤ 5ms (실기 GPU 프로파일링 게이트)
- **AC-LDS-11 [5.6-VERIFY]** — PSO 첫 프레임 히칭 측정 + DegradedFallback 경로 검증
- AC-LDS-06 MPC `SetScalarParameterValue` 씬 코드 grep = 0건
- AC-LDS-07 6 상태 스크린샷 + QA 리드 sign-off (MANUAL)
- AC-LDS-09 MotionBlur.Amount = 0.0 확인
- AC-LDS-10 Farewell Vignette 1.5-2.0s 점진 증가 AUTOMATED
- AC-LDS-12 Dawn Entry `IsPSOReady()` 10초 timeout DegradedFallback 로그 검증
- DirectionalLight Mobility=Stationary / SkyLight Mobility=Movable 확인
- `PSO_MossDusk.bin` Cook 단계에서 생성 + Shipping 빌드 포함

## Next Step

Run `/create-stories core-lumen-dusk-scene` to break this epic into implementable stories.
