# Story 003 — `ALumenDuskSceneActor` + Light Actor 등록 (GSM `RegisterSceneLights` 연동)

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **Engine Risk**: **HIGH** (GSM의 TickMPC가 Light Actor 직접 구동 — Lumen HWRT GPU 5ms budget 상속)
- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 3 (광원 Actor) + §Interactions 1 (GSM 인바운드)
- **TR-ID**: TR-lumen-001 (HIGH risk Lumen HWRT) + TR-lumen-003 (Rule 3 Mobility)
- **Governing ADR**: **ADR-0004** §Decision §1 + §Key Interfaces `RegisterSceneLights` + §Migration Plan §4 "ALumenDuskSceneActor::BeginPlay에서 RegisterSceneLights 호출".
- **Engine Notes**: **5.6-VERIFY AC**: AC-LDS-04 (Lumen GPU 5ms). GSM Subsystem Initialize 후 Actor BeginPlay — UE 표준 lifecycle. `TWeakObjectPtr<>` 로 GSM cache — sceneunload/PIE 재시작 대응.
- **Control Manifest Rules**:
  - **Core Layer Rules §Required Patterns**: `UMossGameStateSubsystem::RegisterSceneLights(ADirectionalLight*, ASkyLight*)` API — Lumen Dusk Scene Actor BeginPlay에서 호출. DirectionalLight Mobility = `Stationary`, SkyLight Mobility = `Movable`.

## Acceptance Criteria

- **RegisterSceneLights 호출 검증** [`INTEGRATION`/BLOCKING] (본 story 신규 AC, ADR-0004 §Validation Criteria §"Light Actor 캐시 등록") — `ALumenDuskSceneActor::BeginPlay` 시 GSM `RegisterSceneLights(KeyLight, SkyLight)` 1회 호출 확인. GSM `KeyLightCache.IsValid() == true`, `SkyLightCache.IsValid() == true` 검증.

## Implementation Notes

1. **`ALumenDuskSceneActor : AActor`** (`Source/MadeByClaudeCode/Core/LumenDusk/LumenDuskSceneActor.h`):
   - `UPROPERTY(EditAnywhere) TObjectPtr<ADirectionalLight> KeyLight;`
   - `UPROPERTY(EditAnywhere) TObjectPtr<ASkyLight> SkyLight;`
   - `UPROPERTY(EditAnywhere) TObjectPtr<ULumenDuskAsset> Config;`
   - `BeginPlay()` override — GSM 등록 + Post-process subscription (story 004) + Stillness Budget request (story 009).
2. **`BeginPlay()` 구현** (ADR-0004 §Key Interfaces):
   ```cpp
   void ALumenDuskSceneActor::BeginPlay() {
       Super::BeginPlay();
       auto* GSM = GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>();
       if (ensure(GSM && KeyLight && SkyLight)) {
           GSM->RegisterSceneLights(KeyLight, SkyLight);
       }
       // Post-process subscription — story 004
       // PSO precaching start — story 006
       // Stillness Budget particle request — story 009
   }
   ```
3. **에디터 level placement**:
   - `MossBabyMain.umap`에 `ALumenDuskSceneActor` 1개 배치. 
   - Details panel에서 `KeyLight`, `SkyLight`, `Config` references 설정.
   - Light Actor mobility는 Actor properties panel에서 이미 설정 (story 002).
4. **EndPlay cleanup**:
   - `EndPlay(EEndPlayReason)`에서 GSM의 `UnregisterSceneLights()` 호출 (ADR-0004 확장 — 구현 선택) 또는 `TWeakObjectPtr` 자동 invalidation 신뢰.
5. **Lazy-init 패턴 (GDD §구현 참고)**:
   - GSM Subsystem이 초기화 미완 시 (edge case — PIE restart)에는 next-tick re-acquire. `FTimerHandle`로 retry 시도.

## Out of Scope

- GSM `RegisterSceneLights` 구현 자체 (GSM epic story 005)
- Post-process Vignette/DoF 구독 (story 004)
- PSO precaching (story 006-008)
- Stillness Budget particle (story 009)

## QA Test Cases

**Test Case 1 — BeginPlay registration**
- **Setup**: PIE 실행 → `ALumenDuskSceneActor` spawn (level placement).
- **Verify**: 
  - `ALumenDuskSceneActor::BeginPlay` 완료 후 GSM의 `KeyLightCache.Get() == KeyLight` (포인터 equality).
  - `GSM->SkyLightCache.Get() == SkyLight`.
  - 로그: `UE_LOG(LogLumenDusk, Log, TEXT("RegisterSceneLights: Key=%s Sky=%s"), ...)` 1회.
- **Pass**: 두 `TWeakObjectPtr` valid + 로그 1회.

**Test Case 2 — GSM 미등록 edge case**
- **Setup**: PIE 시작 시 GSM이 null (시뮬 — `ensure` 실패).
- **Verify**: Crash 없음. Warning 로그 1회. 후속 GSM TickMPC는 null guard (story 005)로 skip.
- **Pass**: crash 없음.

**Test Case 3 — Mobility confirmation (Editor)**
- **Setup**: `MossBabyMain.umap` 열기 → Outliner → `KeyLight` / `SkyLight` 선택 → Details panel.
- **Verify**: `KeyLight.Mobility == Stationary`, `SkyLight.Mobility == Movable`.
- **Pass**: 두 조건 모두 충족.

## Test Evidence

- [ ] `tests/integration/lumen-dusk/register_scene_lights_test.cpp` — BeginPlay registration
- [ ] `tests/unit/lumen-dusk/gsm_null_guard_test.cpp` — edge case crash 방지
- [ ] `production/qa/evidence/lumen-dusk-mobility-[YYYY-MM-DD].png` — Editor 스크린샷

## Dependencies

- **Depends on**: story-001 (scene layout), story-002 (Lumen settings + Light Actor placement)
- **Unlocks**: GSM epic story 005 (TickMPC Light Actor 구동), story-004 (Post-process subscription)
