# Story 004 — `PP_MossDusk` Post Process Volume + Vignette Farewell Lerp + MotionBlur Off

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3h

## Context

- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 5 (포스트 프로세싱) + §States Farewell
- **TR-ID**: TR-lumen-004 (Rule 5 Post-process `PP_MossDusk` 소유) + TR-lumen-005 (AC-LDS-10 Farewell Vignette 점진 증가 1.5-2.0s, 0.0→0.6)
- **Governing ADR**: **ADR-0004** §Decision §2 — `PP_MossDusk` Post Process Volume이 `Contrast`, `Bloom.Intensity`, `Vignette.Intensity`, `ColorGrading.TemperatureType` 소유. Farewell Vignette 자체 Lerp (`OnGameStateChangedHandler`).
- **Engine Risk**: MEDIUM (Post-process GPU budget 1.2ms — Scene GPU budget F2 의존)
- **Engine Notes**: `Bloom.Intensity = 0.3` 고정 (상태 무관). `Bloom.Threshold = 1.0` 고정. `MotionBlur = 0.0` 강제 비활성화 (조용한 씬). `Contrast`는 MPC `ContrastLevel` 참조. Farewell 블렌드 시간 1.5-2.0초 동기화.
- **Control Manifest Rules**: Core Layer Rules §Required Patterns — "`PP_MossDusk` Post Process Volume이 Vignette/DoF/Contrast 소유 — `FOnGameStateChanged` 구독 + 자체 Lerp. 씬 머티리얼은 MPC scalar read-only 참조".

## Acceptance Criteria

- **AC-LDS-09** [`AUTOMATED`/BLOCKING] — `PP_MossDusk` 포스트 프로세스 볼륨 설정에서 MotionBlur 파라미터 확인 시 `r.MotionBlur.Amount = 0.0` 또는 MotionBlur 비활성화.
- **AC-LDS-10** [`INTEGRATION`/BLOCKING] — Any→Farewell GSM 전환 트리거 시 Farewell MPC Lerp 진행 중 Vignette 강도 추적 시 Vignette.Intensity가 0.0에서 `VignetteIntensityFarewell` (기본 0.6)까지 1.5-2.0초에 걸쳐 점진 증가. 즉시 점프 없음.

## Implementation Notes

1. **`APostProcessVolume PP_MossDusk`** 배치 (`MossBabyMain.umap`):
   - Infinite Extent = true (씬 전체 감쌈).
   - Priority = 0.
   - Settings:
     - `Contrast` — MPC `ContrastLevel` 바인딩 (material parameter collection scalar).
     - `Bloom.Intensity = 0.3` 고정.
     - `Bloom.Threshold = 1.0`.
     - `Vignette.Intensity = 0.0` 초기값 — Farewell 진입 시 Lerp.
     - `MotionBlur.Amount = 0.0` (AC-LDS-09).
     - `ColorGrading.TemperatureType = WhiteBalance` — MPC `ColorTemperatureK` 참조.
2. **`ALumenDuskSceneActor::OnGameStateChangedHandler(EGameState OldState, EGameState NewState)`** (ADR-0004 §Key Interfaces):
   - GSM의 `OnGameStateChanged` delegate 구독 (story 003의 BeginPlay에서 `AddDynamic`).
   - `if (NewState == EGameState::Farewell) { StartVignetteLerp(0.0f, Config->VignetteIntensityFarewell, 1.75f); }` — 중간값 1.75s = (1.5+2.0)/2.
3. **`StartVignetteLerp(float From, float To, float Duration)`**:
   - `FTimerHandle` 또는 `FTSTicker` 기반 60Hz tick. SmoothStep ease 적용 (`ease(x) = x²(3−2x)`) — GSM Formula 1과 동일.
   - 각 tick: `CurrentVignette = From + (To - From) * SmoothStep(T/Duration);`
   - `PostProcessVolume->Settings.VignetteIntensity = CurrentVignette;`
4. **DoF Lerp (story 005에서 구현) 연동**:
   - `if (NewState == EGameState::Dream) { StartDoFLerp(Config->BaseFStop, Config->BaseFStop + Config->DreamFStopDelta, 1.35f); }` — story 005 에서 구현. 본 story 범위 외.
5. **MotionBlur 비활성화 grep** (AC-LDS-09):
   - Console variable: `r.MotionBlur.Amount=0` ini 설정. 또는 `PP_MossDusk` Volume에서 MotionBlur.Amount = 0 명시.
   - CI grep: `grep -rn "MotionBlurAmount.*[^0]" Source/MadeByClaudeCode/` = 0건.

## Out of Scope

- Dream DoF Formula 1 (story 005)
- Contrast MPC binding 자동 참조는 MPC setup (GSM epic story 003)
- MPC writer ban grep (GSM epic story 005 — AC-LDS-06)

## QA Test Cases

**Test Case 1 — AC-LDS-09 MotionBlur off**
- **Setup**: `PP_MossDusk` Volume properties 확인 + Console `r.MotionBlur.Amount` 쿼리.
- **Verify**: `MotionBlur.Amount = 0.0`. Motion-blurred 객체 없음 (screenshot).
- **Pass**: 두 조건 모두 충족.

**Test Case 2 — AC-LDS-10 Farewell Vignette Lerp**
- **Given**: `CurrentState = Waiting`. `PP_MossDusk->VignetteIntensity = 0.0`.
- **When**: GSM broadcasts `OnGameStateChanged(Waiting, Farewell)`. 1.75s 경과.
- **Then**: 
  - 전환 시작 직후 (t=0.1s): Vignette ≈ 0.01 (smoothstep 초기 저감).
  - t=0.875s (50%): Vignette ≈ 0.3 (smoothstep 중간).
  - t=1.75s (100%): Vignette = 0.6 (±0.01).
  - 즉시 점프 없음 — 인접 프레임 간 변화량 ≤ 0.015 per frame @60fps.

**Test Case 3 — 다른 상태 Vignette 불변**
- **Given**: `CurrentState = Waiting`, Vignette = 0.0.
- **When**: `OnGameStateChanged(Waiting, Dream)` 수신.
- **Then**: `VignetteIntensity` 여전히 0.0 (Farewell만 조정).

## Test Evidence

- [ ] `tests/unit/lumen-dusk/post_process_settings_test.cpp` — `PP_MossDusk` 필드 검증
- [ ] `tests/integration/lumen-dusk/farewell_vignette_lerp_test.cpp` — AC-LDS-10 frame-by-frame
- [ ] `tests/unit/lumen-dusk/motion_blur_off_grep.md` — AC-LDS-09 CI grep

## Dependencies

- **Depends on**: story-001 (scene layout), story-003 (LumenDuskSceneActor + GSM subscription)
- **Unlocks**: story-005 (Dream DoF)
