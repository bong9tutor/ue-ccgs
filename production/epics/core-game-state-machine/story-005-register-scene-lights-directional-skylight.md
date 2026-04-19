# Story 005 — `RegisterSceneLights` API + DirectionalLight/SkyLight 구동

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **Engine Risk**: **HIGH** (Lumen HWRT GPU budget 5ms 상속 — AC-LDS-04 [5.6-VERIFY] 실측 대상. Light Actor Temperature/Intensity 변경 시 Lumen Surface Cache partial 재계산.)
- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Rule 3 MPC와 Light Actor의 관계 note + [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 3 (Lumen HWRT 설정)
- **TR-ID**: TR-gsm-001 (OQ-6 MPC ↔ Light Actor 동기화 — BLOCKING)
- **Governing ADR**: **ADR-0004** §Decision §1 — GSM이 Light Actor 직접 구동. §Key Interfaces `RegisterSceneLights(ADirectionalLight*, ASkyLight*)` + `TickMPC` 블록 \[2\]. §Migration Plan §2-3, §6 (Mobility).
- **Engine Notes**: **5.6-VERIFY AC**: AC-LDS-04 (Lumen GPU 5ms). DirectionalLight Mobility = **Stationary** (Lumen GI 직접광 + 부분 Shadow baking). SkyLight Mobility = **Movable** (실시간 앰비언트). Lumen Dusk Scene Actor BeginPlay가 GSM Initialize 후 발생 (UE 표준 lifecycle) — 단, null check 필수 (subsystem 재초기화 / PIE restart 대응).
- **Control Manifest Rules**: Core Layer Rules §Required Patterns — `GSM TickMPC` 내 (1) MPC scalar → (2) Light Actor property 순차 갱신. `UMossGameStateSubsystem::RegisterSceneLights(ADirectionalLight*, ASkyLight*)` API 노출. Mobility: `Stationary`/`Movable`. Cross-Cutting — Raw pointer 대신 `TObjectPtr<>` 사용.

## Acceptance Criteria

- **AC-LDS-06** [`CODE_REVIEW`/BLOCKING] — Lumen Dusk Scene 내 모든 머티리얼 인스턴스에서 MPC 쓰기 코드 경로 grep (`SetVectorParameterValue`, `SetScalarParameterValue` on MPC) 시 씬 코드 0건. GSM만 MPC를 갱신.
- **AC-LDS-08** [`INTEGRATION`/BLOCKING] — Waiting 상태에서 Dream 트리거 시, MPC `ColorTemperatureK` Lerp 1.35초 진행 중 인접 프레임 간 변화량 ≤ 100K/frame @60fps. 단일 프레임 즉시 점프 없음.

## Implementation Notes

1. **`RegisterSceneLights` API** (ADR-0004 §Key Interfaces 그대로 follow):
   - Signature: `void UMossGameStateSubsystem::RegisterSceneLights(ADirectionalLight* Key, ASkyLight* Sky);`
   - 내부 저장: `TWeakObjectPtr<ADirectionalLight> KeyLightCache; TWeakObjectPtr<ASkyLight> SkyLightCache;` — `TWeakObjectPtr`로 sceneunload/PIE 재시작 대응.
2. **`TickMPC` 블록 \[2\] 추가** (story 003의 블록 \[1\]에 이어서, 동일 함수 내 순차):
   ```cpp
   if (auto* Key = KeyLightCache.Get()) {
       Key->GetLightComponent()->SetTemperature(Current.ColorTempK);
       Key->GetLightComponent()->SetIntensity(Current.KeyLightIntensity);
       FRotator NewRot = Key->GetActorRotation();
       NewRot.Pitch = -Current.LightAngleDeg;
       Key->SetActorRotation(NewRot);
   }
   if (auto* Sky = SkyLightCache.Get()) {
       Sky->GetLightComponent()->SetIntensity(Current.AmbientIntensity);
   }
   ```
3. **Mobility 설정 규약**: Lumen Dusk Scene Actor의 BeginPlay 또는 Editor 설정에서 `KeyLight->SetMobility(EComponentMobility::Stationary)` / `SkyLight->SetMobility(EComponentMobility::Movable)` 강제. CI grep으로 mobility 설정 검증 옵션.
4. **Frame delta guardrail (AC-LDS-08)**: TickMPC 내에서 `LastFrameColorTempK` 저장 + 현재값 차이 `>= 100.0f` 시 `checkf` 실패 (debug only) — Formula 2 D_blend가 충분한 값(1.35s 기본)이면 자동 준수. Lerp override 시 검증.
5. **Null check (ADR-0004 §Risks §Light Actor 누락)**: `TWeakObjectPtr::Get()` null 반환 시 skip — crash 없음. 경고 로그 1회 (첫 null 감지 시).

## Out of Scope

- Post-process Vignette/DoF (Lumen Dusk epic story 014/015)
- PSO Precaching (Lumen Dusk epic story 017-019)
- GPU profiling AC-LDS-04 [5.6-VERIFY] 실측 — 별도 session (Implementation milestone)

## QA Test Cases

**Test Case 1 — AC-LDS-06 MPC writer uniqueness (grep)**
- **Setup**: `grep -rnE "(SetScalarParameterValue|SetVectorParameterValue)\(.*MPC" Source/MadeByClaudeCode/ --include="*.cpp" --include="*.h"`
- **Verify**: 매치가 `Source/MadeByClaudeCode/Core/GameState/` 하위 파일에만 존재. Lumen Dusk, MBC 등 기타 경로 0건.
- **Pass**: grep 매치 범위 GSM 한정.

**Test Case 2 — AC-LDS-08 Waiting→Dream frame delta ≤ 100K**
- **Given**: `CurrentState = Waiting` (2,800K), trigger `RequestStateTransition(Dream, P2)`.
- **When**: TickMPC 매 프레임 `ColorTemperatureK` 값 로깅 (60fps, 1.35s 블렌드 = 81 frames).
- **Then**: max `|ColorTempK[n+1] - ColorTempK[n]|` ≤ 100K. 블렌드 시작/종료 값 ≈ 2800 / 4200 (±2K).

**Test Case 3 — Light Actor null safety**
- **Given**: `KeyLightCache` 미등록 상태 (RegisterSceneLights 호출 전).
- **When**: TickMPC 실행.
- **Then**: Crash 없음. MPC scalar는 정상 갱신 (블록 \[1\]). Light Actor 블록 \[2\]은 skip.

## Test Evidence

- [ ] `tests/integration/game-state/register_scene_lights_test.cpp` — Mock Light Actor 등록/해제
- [ ] `tests/integration/game-state/frame_delta_waiting_dream_test.cpp` — AC-LDS-08 결정론적 Lerp 기반
- [ ] `tests/unit/game-state/mpc_writer_grep.md` — AC-LDS-06 grep 명령 문서화
- [ ] `production/qa/evidence/lumen-gpu-budget-[date].md` — AC-LDS-04 [5.6-VERIFY] 실측 (MANUAL, 별도 session)

## Dependencies

- **Depends on**: story-003 (MPC tick), story-004 (delegate broadcast)
- **Unlocks**: Lumen Dusk stories (Light Actor placement in scene), story-007 (ReducedMotion E10 MossBabySSSBoost path)
