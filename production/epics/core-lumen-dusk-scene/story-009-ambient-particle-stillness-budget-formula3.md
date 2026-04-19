# Story 009 — 앰비언트 파티클 + Stillness Budget + Formula 3 (Spawn Density) + Dream Reduction

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3h

## Context

- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 1 (Atmosphere layer) + §Rule 7 (Stillness Budget 구독) + §Formula 3 (파티클 스폰 밀도)
- **TR-ID**: N/A (GDD-derived Stillness integration)
- **Governing ADR**: ADR-0007 §Decision — Stillness Budget ↔ Engine concurrency Hybrid. Particle slots: `Background` 우선순위. Dream 선점 해제.
- **Engine Risk**: LOW
- **Engine Notes**: Niagara Effect Type `EFT_MossBaby_Background` 사용 (ADR-0007). 파티클 수 상한 200개 per 씬 (current-best-practices.md). Dream 진입 시 파티클 감소 — "조용하고 차가운 달빛" 정지감.
- **Control Manifest Rules**:
  - **Feature Layer Rules §Required Patterns**: Stillness Budget ↔ Engine concurrency Hybrid. Niagara Effect Type 3종.
  - **Feature Layer Rules §Forbidden**: "Never bypass Stillness Budget for game events".
  - **Feature Layer Rules §Performance Guardrails**: Niagara 파티클 수 상한 per 씬 — 200개.

## Acceptance Criteria

- **AC-LDS-13** [`INTEGRATION`/BLOCKING] — 씬 스폰 + Budget Particle 슬롯 가용 시 앰비언트 파티클 스폰 확인에서 유효한 `FStillnessHandle(Particle, Background)` 보유 + 파티클 스폰 활성 확인.
- **AC-LDS-14** [`INTEGRATION`/BLOCKING] — Narrative 이벤트(Dream)로 Particle 슬롯 선점 시 씬의 Background Particle Handle 상태 확인에서 `OnPreempted` 수신 + 파티클 스폰 중단. Dream 종료 후 슬롯 복귀 시 파티클 재요청.
- **AC-LDS-15** [`AUTOMATED`/BLOCKING] — `IsReducedMotion() = true` 상태에서 씬 스폰 시 파티클 Request 시도 시 Particle Request 호출 없음 (ReducedMotion 확인 후 조기 종료).
- **AC-LDS-17** [`AUTOMATED`/BLOCKING] — Formula 3 파티클 스폰 밀도 (BaseSpawnRate=5.0, DreamParticleReductionRatio=0.8)에서 DreamBlend=0.0, 0.5, 1.0 + BudgetMultiplier=1.0 입력 시 SpawnRate = 5.0, 3.0, 1.0 (±0.01). BudgetMultiplier=0.0 입력 시 SpawnRate = 0.0.

## Implementation Notes

1. **Formula 3 pure function**:
   ```cpp
   static float ParticleSpawnRate(float BaseSpawnRate, float BudgetMultiplier, 
                                   float DreamBlend, float DreamReductionRatio) {
       const float Value = BaseSpawnRate * BudgetMultiplier * 
                           (1.0f - DreamBlend * DreamReductionRatio);
       return FMath::Max(0.0f, Value);
   }
   ```
2. **`BP_AmbientParticle`** (Niagara System):
   - Effect Type = `EFT_MossBaby_Background` (ADR-0007).
   - Particle count cap = 50 per instance (200 씬 total budget 여유).
   - Spawn rate 동적 조정 — `SpawnRateScalar` user parameter 노출.
3. **`ALumenDuskSceneActor::BeginPlay` 확장** (story 003):
   ```cpp
   if (StillnessBudget->IsReducedMotion()) {
       return;  // AC-LDS-15 조기 종료
   }
   ParticleHandle = StillnessBudget->Request(
       EStillnessChannel::Particle, EStillnessPriority::Background);
   if (ParticleHandle.IsValid()) {
       SpawnAmbientParticles();
       SetParticleSpawnRate(Config->BaseSpawnRate);
   }
   ```
4. **`OnPreempted` handler** (AC-LDS-14):
   - `ParticleHandle->OnPreempted.AddDynamic(this, &ALumenDuskSceneActor::OnParticlePreempted);`
   - `OnParticlePreempted`: `StopAmbientParticles();` (Niagara deactivate).
5. **`OnBudgetRestored` handler** (AC-LDS-14):
   - `StillnessBudget->OnBudgetRestored(Particle).AddDynamic(this, &ALumenDuskSceneActor::OnParticleRestored);`
   - `OnParticleRestored`: 새 Handle 획득 + `SpawnAmbientParticles();` 재개.
6. **Dream DoF 동기 Spawn Rate 조정** (Formula 3):
   - GSM MPC의 `DreamBlend` 참조 (MPC scalar로 노출 가정) → `SpawnRate = Formula3(5.0, 1.0, DreamBlend, 0.8)` 매 tick 업데이트.
   - `SetParticleSpawnRate(SpawnRate)` — Niagara user parameter set.
7. **EndPlay cleanup**:
   - `ParticleHandle.Release();` — RAII (Lumen Dusk §Implementation Note 5).

## Out of Scope

- Stillness Budget 구현 자체 (Feature epic)
- GSM MPC `DreamBlend` 노출 (GSM epic story 003)
- Niagara system 에셋 제작 (Art workflow)

## QA Test Cases

**Test Case 1 — AC-LDS-13 Normal spawn**
- **Given**: `IsReducedMotion = false`, Mock Budget `Particle/Background` slot 가용.
- **When**: `BeginPlay` 완료.
- **Then**: `ParticleHandle.IsValid() == true`. Niagara component activated (`IsActive() == true`).

**Test Case 2 — AC-LDS-14 Preemption**
- **Given**: `ParticleHandle` valid, particle active.
- **When**: Mock Budget이 `Narrative` request (e.g., Dream) — `Particle/Background` 선점.
- **Then**: 
  - `OnParticlePreempted` 호출 1회.
  - Niagara `IsActive() == false`.
  - Dream 종료 시뮬 → `OnBudgetRestored` → 새 handle 획득 → Niagara 재활성화.

**Test Case 3 — AC-LDS-15 ReducedMotion skip**
- **Given**: `IsReducedMotion = true`.
- **When**: `BeginPlay`.
- **Then**: `StillnessBudget::Request` 호출 카운트 = 0. Niagara 미생성.

**Test Case 4 — AC-LDS-17 Formula 3 math**
- **Given**: `BaseSpawnRate=5.0, DreamReductionRatio=0.8`.
- **When**: `DreamBlend=0.0, 0.5, 1.0` with `BudgetMultiplier=1.0`.
- **Then**: `SpawnRate = 5.0, 3.0, 1.0` (±0.01). `BudgetMultiplier=0.0` → `SpawnRate = 0.0`.

## Test Evidence

- [ ] `tests/unit/lumen-dusk/formula3_particle_test.cpp` — AC-LDS-17
- [ ] `tests/integration/lumen-dusk/particle_stillness_test.cpp` — AC-LDS-13/14
- [ ] `tests/unit/lumen-dusk/reducedmotion_particle_skip_test.cpp` — AC-LDS-15

## Dependencies

- **Depends on**: story-003 (LumenDuskSceneActor), Feature Stillness Budget epic (Request/Release API)
- **Unlocks**: Cross-epic smoke test (Dream state particle reduction)
