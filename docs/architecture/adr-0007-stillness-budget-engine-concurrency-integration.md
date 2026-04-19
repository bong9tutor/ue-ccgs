# ADR-0007: Stillness Budget ↔ UE Sound Concurrency / Niagara Scalability 통합 전략

## Status

**Accepted** (2026-04-19 — 결정 명확, VS Audio System 구현 시 본 ADR 채택. AC 검증은 VS sprint 진입 시)

## Date

2026-04-19

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | Audio / VFX / Concurrency |
| **Knowledge Risk** | LOW — `USoundConcurrency`, Niagara Scalability 모두 4.x+ 안정 |
| **References Consulted** | stillness-budget.md §Rule 6 + §OQ-1, lumen-dusk-scene.md §Rule 7 (Particle Background), `current-best-practices.md §3 Niagara` |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | VS Audio System 구현 시 Sound Concurrency Group 매핑 검증 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None |
| **Enables** | Audio System (#15, VS) 구현 — Stillness Budget Sound 채널이 엔진 Sound Concurrency와 협업하는 명확한 contract |
| **Blocks** | Audio System (VS) sprint — 본 ADR 없이는 Sound 채널이 게임 레이어와 엔진 레이어 이중 제약 충돌 가능 |
| **Ordering Note** | VS Audio System sprint 진입 전. MVP에서는 Sound는 placeholder 1 track이므로 본 ADR 결정이 즉시 영향 없음 — 사전 결정으로 VS sprint 단순화 |

## Context

### Problem Statement

Stillness Budget (#10)은 `EStillnessChannel { Motion, Particle, Sound }` 3 채널 슬롯 관리 시스템. **그러나 UE 자체에 이미 Sound Concurrency (USoundConcurrency)와 Niagara Scalability (FX 동시성) 시스템 존재**. 두 시스템이 협업해야 하나 충돌해야 하나? 

- 충돌 시: Stillness Budget이 Request Grant했는데 엔진 Sound Concurrency가 voice 차단 → 게임 레이어와 엔진 레이어 부정합
- 협업 시: 어느 시스템이 truth? Stillness Budget이 single source면 엔진 concurrency 중복; 엔진이 single source면 Stillness Budget의 Pillar 1 보호 정책 약화

### Constraints

- **Pillar 1 (조용한 존재)**: Stillness Budget의 핵심 약속 — "동시 이벤트 상한". 엔진 concurrency가 더 관대하면 Pillar 1 위반
- **Pillar 3 (말할 때만 말한다)**: Narrative slot 우선순위 보호 — 엔진 concurrency가 Background voice를 Narrative와 동등 취급하면 희소성 약화
- **MVP 음성 부재**: 현재 Sound 채널 사용은 ambient drone 1개 + SFX 5-10개 (VS Audio System으로 본격 시작)
- **엔진 표준 활용**: Sound Concurrency / Niagara Scalability를 우회하면 UE5 워크플로우 부정합 + 디버깅 도구 미사용
- **단일 플레이어 cozy 게임**: 엔진 concurrency의 voice limit 우려는 매우 낮음 (총 voice ~10개 이하)

### Requirements

- Stillness Budget이 게임 레이어 single source of truth (Pillar 1/3 보호)
- 엔진 시스템은 hardware-level fallback (예: 음원 ~32개 voice limit 등 cataclysm 보호)
- 두 시스템 간 무선의 충돌 없음 (Stillness Budget Grant했는데 voice 미재생, 또는 그 반대)

## Decision

**Hybrid 채택** — Stillness Budget이 게임 레이어 single source of truth, 엔진 시스템은 hardware-level safety net.

### Sound Channel 통합

```text
Game Layer:                Engine Layer:
─────────────────         ─────────────────
Stillness Budget          USoundConcurrency
EStillnessChannel::Sound  per-Concurrency-Group
SoundSlots = 3            MaxCount = 32 (UE default)
                          Resolution = StopOldest

Mapping:                  ┌────────────────────────┐
StillnessSound Channel ←→ │ SC_MossBaby_Stillness  │
                          │ MaxCount = 32 (over the │
                          │ game-layer limit)      │
                          └────────────────────────┘
```

- **단일 `USoundConcurrency` 인스턴스 `SC_MossBaby_Stillness` 생성** + 모든 게임 SFX/ambient에 적용
- 엔진 `MaxCount = 32` (게임 레이어 3 슬롯의 ~10×) — 엔진 layer에서 cataclysmic overflow만 방지, 일반 case는 게임 layer에서 control
- **엔진 Concurrency Resolution = `StopOldest`** — Stillness Budget Deny와 정합 (Stillness가 우선 차단, 엔진은 방어)

### Particle Channel (Niagara Scalability)

- Niagara `Effect Type` Asset 활용 — `Background`, `Standard`, `Narrative` 3 effect type 정의
- 각 Niagara System Asset이 `Effect Type` 속성 설정 — 엔진 Scalability가 type별 동시 instance 한계 적용
- Stillness Budget `EStillnessChannel::Particle` 슬롯이 game-layer 우선 control
- 엔진 Niagara Scalability는 hardware-level cap (예: low-spec GPU에서 200 particle 한계)

### Motion Channel

- UE에 motion-specific concurrency API 없음 — Stillness Budget이 단독 source
- 단, ReducedMotion ON 시 UE `r.MotionBlur`, `r.LensFlareQuality` 등 시스템 settings 동기화 권장 (별도 ADR or Implementation 결정)

### Conflict Resolution Order

```cpp
void RequestSoundEffect(USoundBase* Sound, EStillnessPriority Priority) {
    // 1. Game-layer first — Stillness Budget Request
    FStillnessHandle Handle = StillnessBudget->Request(EStillnessChannel::Sound, Priority);
    if (!Handle.bValid) return;  // Pillar 1 보호 — 엔진 layer 도달 안 함

    // 2. Engine-layer second — Sound Concurrency 자동 적용
    UAudioComponent* AC = UGameplayStatics::SpawnSound2D(Sound /* SC_MossBaby_Stillness 적용됨 */);
    if (!AC) {
        // 엔진 거부 (32 voice 초과 — 게임 layer 통과 후 hardware 한계)
        StillnessBudget->Release(Handle);
        UE_LOG(LogStillness, Warning, TEXT("Engine voice cap reached — game layer 3 슬롯 < engine 32 — 비정상 상황"));
    }

    // 3. Cleanup — Sound 종료 시 Handle Release (RAII)
    AC->OnAudioFinished.AddDynamic(this, &OnSoundFinished);  // → StillnessBudget->Release(Handle)
}
```

### Niagara Effect Type Mapping

| Niagara Effect Type | Stillness Priority | Effect Type Asset |
|---|---|---|
| `BackgroundParticle` | Background | `EFT_MossBaby_Background.uasset` |
| `StandardParticle` | Standard | `EFT_MossBaby_Standard.uasset` |
| `NarrativeParticle` | Narrative | `EFT_MossBaby_Narrative.uasset` |

각 Effect Type은 `Significance Settings`로 distance-based culling. 게임 layer Stillness Budget이 instantiation 단계 차단 우선.

## Alternatives Considered

### Alternative 1: Stillness Budget이 single source — 엔진 concurrency 미사용

- **Description**: SoundConcurrency / Niagara Scalability 자체를 비활성화. 모든 control은 Stillness Budget만
- **Pros**: 단일 시스템 — 동작 예측 쉬움
- **Cons**: 
  - UE 워크플로우 부정합 — 디자이너가 Niagara Effect Type 설정 의미 없어짐
  - Hardware-level cataclysm 방어 부재 — 버그로 100 voice 시도 시 엔진 crash 가능
  - `r.Niagara.MaxStatRecordedFrames` 같은 디버깅 도구 효과 감소
- **Rejection Reason**: 엔진 표준 회피의 비용 > 이점

### Alternative 2: 엔진 concurrency가 single source — Stillness Budget 폐기

- **Description**: USoundConcurrency MaxCount=3 + Niagara Scalability로 Stillness Budget 대체
- **Pros**: UE-native 접근
- **Cons**: 
  - **Pillar 1/3 보호 약화** — 엔진 concurrency는 priority 시스템 제한적, Narrative 슬롯 보호 어려움
  - `IsReducedMotion()` API 부재 — UI가 정적 표현 fallback 호출 경로 없음
  - GDD §Stillness Budget 전면 재설계 — 비용 거대
- **Rejection Reason**: Pillar 보호 약화 + 재설계 비용. Stillness Budget의 존재 이유 자체 부정

### Alternative 3: Hybrid (채택)

- 본 ADR §Decision

## Consequences

### Positive

- **Pillar 1/3 보호 유지**: Stillness Budget이 game-layer truth — 엔진은 hardware fallback
- **UE 워크플로우 정합**: 디자이너가 Niagara Effect Type, Sound Concurrency Asset 표준 사용
- **Cataclysm 방어**: Stillness Budget 버그 시 엔진 layer가 crash 방지
- **VS Audio System 단순 통합**: Audio System 구현 시 `SC_MossBaby_Stillness` 1개 자산만 모든 SFX에 적용

### Negative

- **이중 시스템 학습**: 디자이너가 두 layer (Stillness + Engine concurrency) 의미 파악 필요. **Mitigation**: 본 ADR §Decision 표를 onboarding 자료에 인용
- **VS Audio System sprint 의존**: 본 ADR의 verification은 VS 단계 — MVP에서는 dormant

### Risks

- **Engine voice limit 32가 게임 plus playtest 시 도달**: 단일 플레이어 cozy 게임에서 voice 32개 동시 = 비정상. **Mitigation**: VS playtest 시 `stat audio` 로 측정 — > 10 voice 시 디자인 검토
- **Niagara Effect Type Asset 누락**: 디자이너가 새 Niagara System 추가 시 Effect Type 미설정. **Mitigation**: `UEditorValidatorBase`로 모든 Niagara System에 Effect Type 강제 (VS 구현 시)

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `stillness-budget.md` | §OQ-1 "UE Sound Concurrency / Niagara Scalability와 Stillness Budget의 통합 전략" | **본 ADR이 OQ-1의 정식 결정 (Hybrid)** |
| `stillness-budget.md` | §Rule 6 ReducedMotion API + Sound 영향 없음 | 본 ADR이 Sound 채널을 game-layer source로 유지하여 RM 정책 강제 |
| `lumen-dusk-scene.md` | §Rule 7 Particle Background 슬롯 | Niagara Effect Type `BackgroundParticle` 매핑 |

## Performance Implications

- **CPU (Audio)**: USoundConcurrency 적용은 SFX 1회 spawn당 수 microsecond — 무시 가능
- **CPU (Niagara)**: Effect Type Significance Settings 적용 — Niagara 표준 cost
- **Memory**: `SC_MossBaby_Stillness` 자산 ~few KB + 3 Niagara Effect Type 자산 — 무시 가능

## Migration Plan

VS Audio System sprint 진입 시:
1. `Content/Audio/SC_MossBaby_Stillness.uasset` 생성 (USoundConcurrency, MaxCount=32, Resolution=StopOldest)
2. 모든 SFX/ambient 자산에 SC 적용
3. `Content/Niagara/EffectTypes/` 디렉토리 생성 + 3 Effect Type 자산 (`EFT_MossBaby_Background/Standard/Narrative`)
4. Lumen Dusk 앰비언트 파티클 자산에 `BackgroundParticle` Effect Type 적용
5. Card Hand UI 건넴 VFX에 `StandardParticle` 적용
6. MBC GrowthTransition / Dream reveal VFX에 `NarrativeParticle` 적용
7. AC 추가 — Stillness Budget Request Deny 시 SFX/Niagara spawn 0건 검증

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| Stillness Budget Sound Deny 시 spawn 차단 | Mock — Sound 채널 슬롯 가득, Request → Deny → SpawnSound2D 호출 0 | 음원 미재생 |
| 엔진 cataclysm fallback | Stillness Budget bypass 후 32 voice 시도 → 33번째 거부 | StopOldest 동작 확인 |
| Niagara Effect Type 매핑 | 모든 Niagara System 자산이 3 Effect Type 중 하나 보유 | UEditorValidator 통과 |
| ReducedMotion ON Sound 미영향 | `bReducedMotionEnabled = true` + Sound Request | Grant (Sound 영향 없음 — Rule 6) |

## Related Decisions

- **stillness-budget.md** §OQ-1 — 본 ADR 직접 출처
- **architecture.md** §Architecture Principles §Principle 4 (Layer Boundary) — game layer + engine layer 명확 분리
- **lumen-dusk-scene.md** §Rule 7 — Niagara Effect Type Background 매핑
