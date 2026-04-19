# Story 007: ADR-0007 Hybrid Integration — VS Deferred Stub

> **Epic**: feature-stillness-budget
> **Layer**: Feature
> **Type**: Integration
> **Status**: Deferred to VS
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: N/A (VS sprint)

## Context

- **GDD Reference**: `design/gdd/stillness-budget.md` §OQ-1 RESOLVED + `production/epics/feature-stillness-budget/EPIC.md` VS 유의점
- **TR-ID**: TR-stillness-001 (OQ-1 Sound Concurrency + Niagara Scalability 통합)
- **Governing ADR**: [ADR-0007](../../../docs/architecture/adr-0007-stillness-budget-engine-concurrency-integration.md) — Hybrid 채택 (VS sprint에서 실제 자산 생성)
- **Engine Risk**: LOW (VS sprint 시 재평가)
- **Engine Notes**: `SC_MossBaby_Stillness` USoundConcurrency (MaxCount=32, Resolution=StopOldest) + `EFT_MossBaby_Background/Standard/Narrative` Niagara Effect Type 3종 — **MVP에서는 미구현**, Audio System VS sprint 진입 시 본 story 활성화.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — "Stillness Budget ↔ Engine concurrency Hybrid" (VS 완전 통합)
  - Feature Layer §Forbidden Approaches — "Never bypass Stillness Budget for game events" (VS 시 SC_MossBaby_Stillness 통합 후에도 유효)

## Acceptance Criteria (VS Sprint — 현재 Deferred)

1. **VS** — `Content/Audio/SC_MossBaby_Stillness.uasset` 생성 (USoundConcurrency, MaxCount=32, Resolution=StopOldest) — ADR-0007 §Migration Plan
2. **VS** — 모든 SFX/ambient 자산에 `SC_MossBaby_Stillness` 적용 (`UEditorValidatorBase`로 누락 검증 권장)
3. **VS** — `Content/Niagara/EffectTypes/` 디렉토리 + 3 Effect Type 자산 (`EFT_MossBaby_Background`, `EFT_MossBaby_Standard`, `EFT_MossBaby_Narrative`)
4. **VS** — Lumen Dusk 앰비언트 파티클에 `BackgroundParticle` Effect Type 적용
5. **VS** — Card Hand UI 건넴 VFX에 `StandardParticle` 적용
6. **VS** — MBC GrowthTransition / Dream reveal VFX에 `NarrativeParticle` 적용
7. **VS** — Sound request 순서 검증: (1) Stillness Budget `Request` → (2) Engine spawn (SoundConcurrency 자동) → (3) RAII Release
8. **VS** — AC 추가: Stillness Budget Request Deny 시 SFX/Niagara spawn = 0건 검증
9. **VS** — `stat audio` 실측 — VS playtest 시 voice count > 10이면 디자인 검토

## MVP Status (본 Epic 종료 조건)

- **Motion/Particle/Sound 채널 game-layer slot counting만 구현** (Story 001-006)
- `USoundConcurrency` 자산 + Niagara Effect Type 자산은 **VS sprint에서 추가**
- 본 story는 MVP에서 **Deferred to VS** 상태 유지 — `/story-done` 불필요

## Implementation Notes (VS 참조)

- **Sound Channel Hybrid 통합** (ADR-0007 §Decision):
  ```cpp
  void RequestSoundEffect(USoundBase* Sound, EStillnessPriority Priority) {
      FStillnessHandle Handle = StillnessBudget->Request(EStillnessChannel::Sound, Priority);
      if (!Handle.bValid) return;  // Pillar 1 보호
      UAudioComponent* AC = UGameplayStatics::SpawnSound2D(Sound);  // SC_MossBaby_Stillness 자동 적용
      if (!AC) {
          StillnessBudget->Release(Handle);
          UE_LOG(LogStillness, Warning, TEXT("Engine voice cap reached"));
      }
      AC->OnAudioFinished.AddDynamic(this, &OnSoundFinished);  // RAII Release
  }
  ```
- **Niagara Effect Type Mapping** (ADR-0007):
  | Priority | Effect Type Asset |
  |---|---|
  | Background | `EFT_MossBaby_Background` |
  | Standard | `EFT_MossBaby_Standard` |
  | Narrative | `EFT_MossBaby_Narrative` |
- **UEditorValidator** (VS): 모든 Niagara System 자산에 Effect Type 강제

## Out of Scope (MVP)

- USoundConcurrency 자산 생성 (VS)
- Niagara Effect Type 자산 생성 (VS)
- Audio System subsystem 구현 (VS Audio System sprint)
- `stat audio` 측정 (VS playtest)

## QA Test Cases (VS Sprint)

**Given** (VS) Stillness Budget Sound 슬롯 가득 + `Request(Sound, Standard)` → Deny, **When** `SpawnSound2D` 호출 경로, **Then** spawn 0건, 음원 미재생 (ADR-0007 §Validation).

**Given** (VS) Stillness Budget bypass + 32 voice 시도, **When** 33번째 spawn, **Then** `StopOldest` resolution 동작 (엔진 cataclysm fallback).

**Given** (VS) 모든 Niagara System 자산, **When** UEditorValidator 실행, **Then** Effect Type 누락 자산 0건.

**Given** (VS) `bReducedMotionEnabled=true` + `Request(Sound, Standard)`, **When** 호출, **Then** Grant (Rule 6 — Sound 영향 없음).

## Test Evidence (VS Sprint)

- **VS Integration test**: `tests/integration/Stillness/test_adr0007_hybrid.cpp`
- VS playtest evidence: `production/qa/evidence/vs-audio-system-stat-audio.md`

## Dependencies

- Story 001-006 (Stillness Budget MVP — game-layer slot counting)
- **VS Audio System sprint** (`Content/Audio/` 자산 생성)
- **VS Lumen Dusk integration** (Niagara Effect Type 적용)
- **VS Card Hand UI integration** (StandardParticle 적용)
- **VS MBC integration** (NarrativeParticle 적용)
