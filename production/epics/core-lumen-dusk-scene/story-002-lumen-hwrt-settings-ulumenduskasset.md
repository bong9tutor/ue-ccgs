# Story 002 — Lumen HWRT 설정 + `ULumenDuskAsset` + `UMossLumenDuskSettings`

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Config/Data
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **Engine Risk**: **HIGH** (Lumen HWRT — AC-LDS-04 [5.6-VERIFY] Implementation 단계 실기 GPU 프로파일링 대상)
- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 3 (Lumen HWRT 설정) + §Tuning Knobs
- **TR-ID**: TR-lumen-003 (Rule 3 Lumen HWRT 설정 + Mobility)
- **Governing ADR**: **ADR-0004** §Decision §1 — DirectionalLight Mobility = Stationary, SkyLight Mobility = Movable. ADR-0011 §Exceptions — `ULumenDuskAsset` per-scene asset.
- **Engine Notes**: **5.6-VERIFY AC**: AC-LDS-04 — `r.Lumen.HardwareRayTracing`, `r.Lumen.MaxTraceDistance`, `r.Lumen.SurfaceCacheResolution`. UE 5.6 Lumen HWRT CPU bottleneck 개선이 이 프로젝트 씬에서 실제로 충분한지 실측 (OQ-2). GPU 예산(5ms) 초과 시 SWRT 폴백 자동 전환 여부는 별도 session (Implementation milestone).
- **Control Manifest Rules**:
  - **Core Layer Rules §Required Patterns**: DirectionalLight Mobility = `Stationary`, SkyLight Mobility = `Movable`.
  - **Cross-Layer Rules §Tuning Knob**: `UMossLumenDuskSettings : UDeveloperSettings` (`UCLASS(Config=Game, DefaultConfig)`). `ULumenDuskAsset`은 ADR-0011 예외 (per-scene asset).
  - **Post-Cutoff API Verification Targets**: `r.Lumen.HardwareRayTracing` + `r.Lumen.SurfaceCacheResolution` AC-LDS-04.

## Acceptance Criteria

- **AC-LDS-04** [`MANUAL`/ADVISORY] [**5.6-VERIFY**] — 씬 로드 후 Stat GPU 활성화 시, Waiting 상태(기본 상태) 60초 관찰 시 Lumen GPU 시간 `LumenGPUBudgetMs` 이하 (기본 5.0ms). 초과 시 SWRT 폴백 또는 Lumen 설정 조정 필요. **전제조건**: OQ-1 (MPC-Light Actor 동기화) ADR 완료 후 실행.
- **AC-LDS-05** [`AUTOMATED`/BLOCKING] — Lumen 설정 파일에서 `r.Lumen.HardwareRayTracing`, `r.Lumen.MaxTraceDistance`, `r.Lumen.SurfaceCacheResolution` 값 grep 시 코드 내 하드코딩 없음. 모든 값이 `ULumenDuskAsset` DataAsset 또는 프로젝트 ini에서 참조.

## Implementation Notes

1. **`ULumenDuskAsset : UPrimaryDataAsset`** (`Source/MadeByClaudeCode/Core/LumenDusk/LumenDuskAsset.h`):
   - Feel knobs: `CameraFOV=35.0, BaseFStop=2.0, DreamFStopDelta=-0.5, VignetteIntensityFarewell=0.6, BloomIntensity=0.3, BaseSpawnRate=5.0, DreamParticleReductionRatio=0.8`.
   - Curve knobs: `LumenMaxTraceDistanceCm=300.0, LumenSurfaceCacheResolution=0.5, SceneGPUBudgetRatio=0.5`.
   - Gate knobs: `LumenGPUBudgetMs=5.0, PSOWarmupTimeoutSec=10.0`.
   - 모든 UPROPERTY(EditAnywhere) — GDD §Tuning Knobs 안전 범위를 `ClampMin/ClampMax` meta로 강제.
   - `GetPrimaryAssetId()` → `FPrimaryAssetId("LumenDusk", "Default")`.
2. **`UMossLumenDuskSettings : UDeveloperSettings`** (ADR-0011):
   - `UCLASS(Config=Game, DefaultConfig)`.
   - Runtime 변경 불필요한 `LumenGPUBudgetMs`, `PSOWarmupTimeoutSec`는 `meta=(ConfigRestartRequired=true)`.
   - 단순 proxy로 `ULumenDuskAsset` 접근 경로 제공.
3. **Lumen 설정 적용** (AC-LDS-05):
   - `DefaultEngine.ini`의 `[SystemSettings]` 섹션에서 Lumen 설정:
     ```
     r.Lumen.HardwareRayTracing=1
     r.Lumen.MaxTraceDistance=300.0
     r.Lumen.SceneCaptureCacheResolution=64
     r.Lumen.SurfaceCacheResolution=0.5
     ```
   - 단, GDD §Rule 3 표의 `LumenMaxTraceDistanceCm`/`LumenSurfaceCacheResolution`은 `ULumenDuskAsset` 경유로 재정의 허용 — 런타임 조정용.
   - **코드 리터럴 금지 (AC-LDS-05)**: C++ 코드에서 `IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.HardwareRayTracing"))->Set(1)` 식으로 설정 시 값은 Asset에서 조회.
4. **Light Actors 배치** (ADR-0004 §Decision §Mobility):
   - `ADirectionalLight`: Mobility = **Stationary** (Lumen 직접광 + 부분 Shadow baking). Editor 설정.
   - `ASkyLight`: Mobility = **Movable** (실시간 Lumen 앰비언트 갱신). Editor 설정.
   - 레벨 Actor properties panel에서 설정 — CI grep 검증 옵션.
5. **GPU Profiling setup** (AC-LDS-04 [5.6-VERIFY]):
   - Implementation milestone에서 `stat GPU` + `stat Lumen` 명령 실행 → 6 GameState 각각 측정.
   - 별도 session (Implementation 단계) — GPU profiling은 본 story 범위 밖.

## Out of Scope

- ADR-0004 Light Actor 등록 (`RegisterSceneLights`) — GSM story 005에서 구현, 본 story는 Actor 배치만.
- Post-process Volume (story 004)
- PSO precaching (story 006-008)
- GPU profiling AC-LDS-04 [5.6-VERIFY] 실측은 별도 session (Implementation milestone)

## QA Test Cases

**Test Case 1 — AC-LDS-05 Lumen setting grep**
- **Setup**: `grep -rnE "r\.Lumen\.(HardwareRayTracing|MaxTraceDistance|SurfaceCacheResolution).*[0-9]" Source/MadeByClaudeCode/Core/LumenDusk/ --include="*.cpp" --include="*.h"`
- **Verify**: 매치는 `ULumenDuskAsset` 필드 접근 경유 또는 Asset getter 호출 — 직접 숫자 0건.
- **Pass**: grep의 모든 매치가 Asset 경유 확인.

**Test Case 2 — ULumenDuskAsset schema**
- **Setup**: Editor에서 `ULumenDuskAsset` 인스턴스 생성 → 모든 knob 편집.
- **Verify**: 12 UPROPERTY 모두 편집 가능. ClampMin/ClampMax 범위 준수 (GDD §Tuning Knobs 안전 범위).
- **Pass**: Editor UI 확인.

**Test Case 3 — Light Actor Mobility (Editor config)**
- **Setup**: Level editor → Outliner → `DirectionalLight` / `SkyLight` → Properties.
- **Verify**: DirectionalLight Mobility = `Stationary`, SkyLight Mobility = `Movable`.
- **Pass**: 스크린샷 + QA 리드 확인.

**Test Case 4 — AC-LDS-04 [5.6-VERIFY] 실측 (MANUAL, 별도 session)**
- **Setup**: Shipping 또는 Development 빌드 + `stat GPU` + `stat Lumen` 활성화.
- **Verify**: Waiting 상태 60s 관찰 — Lumen GPU time ≤ 5.0ms.
- **Pass**: 수동 녹화 + QA 리드 sign-off. **Implementation milestone 별도 session에서 실행**.

## Test Evidence

- [ ] `tests/unit/lumen-dusk/lumen_settings_grep.md` — AC-LDS-05
- [ ] `tests/unit/lumen-dusk/ulumenduskasset_schema_test.cpp` — Asset UPROPERTY validation
- [ ] `production/qa/evidence/lumen-mobility-[YYYY-MM-DD].md` — Light Actor Mobility 스크린샷
- [ ] `production/qa/evidence/lumen-gpu-budget-6-states-[YYYY-MM-DD].md` — AC-LDS-04 [5.6-VERIFY] 별도 session

## Dependencies

- **Depends on**: story-001 (scene layout)
- **Unlocks**: story-003 (Light Actor integration via GSM), story-004 (Post-process Volume), GSM epic story 005 (`RegisterSceneLights`)
