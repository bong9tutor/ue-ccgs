# ADR-0004: MPC ↔ Light Actor 동기화 아키텍처 — Hybrid (Direct Light + Post-process LUT)

## Status

**Accepted** (2026-04-19 — `/architecture-review` 통과, C3 architecture Rule 5 wording 완화로 GSM Light Actor 직접 호출 명시적 허용. Lumen Dusk 첫 milestone GPU 프로파일링 결과 (AC-LDS-04 [5.6-VERIFY])는 Implementation 단계 검증 — 결정 자체는 안정)

## Date

2026-04-19

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 (hotfix 5.6.1 권장) |
| **Domain** | Core / Rendering / Lighting |
| **Knowledge Risk** | **HIGH** — Lumen HWRT GPU 비용 (AC-LDS-04 [5.6-VERIFY]), PSO Precaching 점진 컴파일 (AC-LDS-11). Light Actor Mobility (Stationary/Movable) + Lumen GI 재계산 비용 검증 필수 |
| **References Consulted** | `current-best-practices.md §1 Rendering`, `breaking-changes.md §Rendering Pipeline / PSO 컴파일`, game-state-machine.md §OQ-6 + §Rule 3 (MPC 매핑), lumen-dusk-scene.md §OQ-1 + §Rule 3 (Lumen 설정), moss-baby-character-system.md §Material Params |
| **Post-Cutoff APIs Used** | UE 5.6 Lumen HWRT settings (`r.Lumen.HardwareRayTracing`, `r.Lumen.SurfaceCacheResolution`) — 5.6에서 CPU bottleneck 개선 |
| **Verification Required** | AC-LDS-04 GPU 프로파일링 게이트 (5ms budget, 모든 6 GameState 상태에서 실측) + 6 상태 시각 검증 AC-LDS-07 (QA 리드 sign-off) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None (Foundation/Core 다른 ADR과 독립) |
| **Enables** | GSM MPC orchestration (§Rule 3/4) 실구현, Lumen Dusk Scene Light Actor 구동, 6 GameState 시각 전환 (Dream/Farewell anchor moments) |
| **Blocks** | **Lumen Dusk Scene 첫 구현 마일스톤** — 본 ADR Accepted 전 Light Actor 구동 코드 작성 금지 (Lumen Dusk Implementation Notes §3) |
| **Ordering Note** | Lumen Dusk 첫 milestone 직전 필수. MVP 구현에서 가장 리스크 높은 HIGH risk 도메인 — 실기 검증 결과 따라 본 ADR 재검토 가능 (superseded) |

## Context

### Problem Statement

Game State Machine (#5)이 소유하는 `UMaterialParameterCollection` (MPC) scalar 파라미터는 머티리얼 셰이더에서만 자동 참조된다. **DirectionalLight Actor의 Temperature/Intensity 및 SkyLight Intensity는 MPC 값 변경을 자동 반영하지 않는다** — 이는 UE 5.6의 구조적 한계다.

Art Bible §2 + GSM Rule 3은 6 GameState별 색온도 (2,200K Farewell ~ 4,200K Dream)와 주광원 각도 (12° ~ 40°)를 Lumen GI에 직접 반영해야 함을 요구한다. MPC만으로는 Light Actor 프로퍼티가 변하지 않아 Lumen GI가 정적 상태를 유지 — Player Fantasy 앵커 모먼트 (Waiting 구리빛 → Dream 달빛 전환)가 실현 불가능.

### Constraints

- **MPC는 머티리얼에서만 read**: 셰이더 pixel sample. Light Actor 프로퍼티 자동 바인딩 없음
- **Lumen HWRT GPU budget**: 5ms (LumenGPUBudgetMs, Lumen Dusk AC-LDS-04). Light Actor Temperature 변경 시 Lumen Surface Cache 재계산 비용 발생 가능
- **Pillar 1 (조용한 존재)**: 상태 전환은 빛과 색으로만 — 알림·모달 없음. 시각 변화가 GI·머티리얼·포스트프로세스 일관
- **AC 자동화 요구**: AC-GSM-08 (mid-transition restart), AC-LDS-08 (Waiting→Dream 전환 시 프레임당 `ColorTemperatureK` 변화량 ≤ 100K) — 결정적 구동 필요 (Blueprint tick 순서 예측 불가)
- **Light Actor Mobility 제약**: Stationary Light는 Lumen 직접광 + Shadow map baking 허용. Movable Light는 Lumen 실시간 재계산 (GPU 비용 ↑). SkyLight는 Movable (실시간 Lumen 앰비언트 갱신)

### Requirements

- 6 GameState 전환 시 색온도·광원 각도·대비가 Lumen GI에 반영
- MBC 머티리얼 (이끼 아이 SSS 등)과 Lumen Dusk 머티리얼 (유리병 굴절 등)은 MPC 자동 참조 유지
- Post-process 효과 (Contrast, Ambient tint, Farewell Vignette)는 `PP_MossDusk` Post Process Volume이 소유
- AC 검증 가능한 결정성 (Blueprint tick 순서 예측 불가 회피)
- Lumen GPU budget 5ms 이내 유지
- 구현 복잡도 최소화 (3 메커니즘 중 하나만 선택)

## Decision

**Hybrid 채택** — 두 메커니즘의 명확한 역할 분담:

1. **GSM이 Light Actor 직접 구동 (Lumen GI 핵심 입력)**
   - `DirectionalLight.Temperature` + `DirectionalLight.Intensity` + `DirectionalLight.GetActorRotation()` (YawPitch) — GSM Rule 3의 MPC `ColorTemperatureK`, `LightAngleDeg`, `AmbientIntensity`와 함께 **동일 TickMPC 함수 내 순차 갱신**
   - `SkyLight.Intensity` — GSM MPC `AmbientIntensity` 연동
   - `UMossGameStateSubsystem`이 Light Actor의 `TWeakObjectPtr<>`을 BeginPlay에 캐싱, TickMPC에서 Lerp 갱신

2. **Lumen Dusk Scene이 Post-process LUT + Volume 파라미터 소유 (Final Color Grading)**
   - `PP_MossDusk` Post Process Volume의 `Contrast`, `Bloom.Intensity`, `Vignette.Intensity`, `ColorGrading.TemperatureType`는 MPC scalar를 직접 참조 (기존 설계 유지)
   - Farewell Vignette 점진 증가 (AC-LDS-10 0.0→0.6 in 1.5-2.0s) — `PP_MossDusk`가 GSM `FOnGameStateChanged` 구독 + 자체 Lerp
   - Dream DoF 강화 (AC-LDS-16 Formula 1) — Lumen Dusk가 카메라 DoF 직접 제어

3. **MPC는 머티리얼에서만 read (기존 설계 유지)**
   - MBC 머티리얼: `AmbientMoodBlend` 파라미터로 GSM MPC 앰비언트 자동 반영
   - Lumen Dusk 메시 머티리얼 (`MI_GlassJar`, `MI_Desk` 등): MPC scalar 직접 참조
   - **씬 어디에서도 MPC `SetScalarParameterValue` 호출 금지** (AC-LDS-06 grep 0건 — GSM 유일 writer)

### Architecture Diagram

```text
                    [GSM TickMPC — Every Frame]
                              │
            ┌─────────────────┼─────────────────┐
            │                 │                 │
            ▼                 ▼                 ▼
    [MPC SetScalar]   [DirectionalLight]   [SkyLight]
      (8 params)       Temperature         Intensity
            │          Intensity
            │          Rotation
            │               │                 │
            │               └──────┬──────────┘
            ▼                      ▼
    [Material Shader]        [Lumen HWRT GI]
      (MBC, Lumen Dusk)            │
      auto-reference               │
            │                      │
            └──────────┬───────────┘
                       ▼
              [Render Thread]
                       │
                       ▼
    [PP_MossDusk Post Process Volume]
     - Contrast (MPC read)
     - Vignette (자체 Lerp on FOnGameStateChanged)
     - DoF (Lumen Dusk 소유, AC-LDS-16 Formula 1)
                       ▼
                [Final Frame]
```

### Key Interfaces

```cpp
// GSM 내 Light Actor 캐시 + Tick
UCLASS()
class UMossGameStateSubsystem : public UGameInstanceSubsystem {
    // ... (existing)
private:
    // Lumen Dusk Scene Actor에 배치된 Light Actor 캐시 (level BeginPlay에서 등록)
    UPROPERTY() TWeakObjectPtr<ADirectionalLight> KeyLightCache;
    UPROPERTY() TWeakObjectPtr<ASkyLight>          SkyLightCache;

    // Lumen Dusk Scene이 호출 (BeginPlay)
public:
    void RegisterSceneLights(ADirectionalLight* Key, ASkyLight* Sky);

private:
    bool TickMPC(float DeltaTime) {
        // Formula 1 SmoothStep Lerp — 현재 MPC 값 갱신
        const float BlendT = ...;  // 전환 진행률 (Formula 2 StateBlendDuration)
        const FGameStateMPCTarget Current = InterpolateMPCTarget(SourceState, TargetState, BlendT);

        // [1] MPC scalar (머티리얼 read)
        MPCInstance->SetScalarParameterValue(TEXT("ColorTemperatureK"),  Current.ColorTempK);
        MPCInstance->SetScalarParameterValue(TEXT("LightAngleDeg"),       Current.LightAngleDeg);
        MPCInstance->SetScalarParameterValue(TEXT("ContrastLevel"),       Current.Contrast);
        MPCInstance->SetVectorParameterValue(TEXT("AmbientRGB"),           Current.AmbientRGB);
        MPCInstance->SetScalarParameterValue(TEXT("AmbientIntensity"),     Current.AmbientIntensity);
        MPCInstance->SetScalarParameterValue(TEXT("MossBabySSSBoost"),     Current.SSSBoost);

        // [2] Light Actor 직접 구동 (Lumen GI 입력)
        if (auto* Key = KeyLightCache.Get()) {
            Key->GetLightComponent()->SetTemperature(Current.ColorTempK);
            Key->GetLightComponent()->SetIntensity(Current.KeyLightIntensity);
            FRotator NewRot = Key->GetActorRotation();
            NewRot.Pitch = -Current.LightAngleDeg;  // 각도 convention
            Key->SetActorRotation(NewRot);
        }
        if (auto* Sky = SkyLightCache.Get()) {
            Sky->GetLightComponent()->SetIntensity(Current.AmbientIntensity);
        }

        // [3] Post-process: Lumen Dusk Scene이 별도 Tick — 본 함수는 관여 안 함
        return true;
    }
};

// Lumen Dusk Scene Actor — BeginPlay에서 Light 등록
void ALumenDuskSceneActor::BeginPlay() {
    Super::BeginPlay();
    auto* GSM = GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>();
    GSM->RegisterSceneLights(KeyLight, SkyLight);
}

// PP_MossDusk Post Process Volume — FOnGameStateChanged 구독 + 자체 Lerp
void ALumenDuskSceneActor::OnGameStateChangedHandler(EGameState OldState, EGameState NewState) {
    if (NewState == EGameState::Farewell) {
        StartVignetteLerp(0.0f, 0.6f, 1.75f);  // AC-LDS-10
    }
    if (NewState == EGameState::Dream) {
        StartDoFLerp(Config->BaseFStop, Config->BaseFStop + Config->DreamFStopDelta, 1.35f);  // AC-LDS-16
    }
}
```

## Alternatives Considered

### Alternative 1: GSM이 Light Actor 직접 구동 단독 (Post-process LUT 없음)

- **Description**: MPC + Light Actor 직접 제어만. PP_MossDusk는 고정 파라미터 (Vignette·DoF 변화 없음)
- **Pros**: 단순. 구현 복잡도 최소
- **Cons**: **Art Bible §Terrarium Dusk 부드러운 컬러 그레이딩 표현 부족** — Farewell Vignette 점진 증가 (AC-LDS-10 필수 AC)와 Dream DoF 강화 (AC-LDS-16 필수 AC) 불가. GDD 요구사항 미충족
- **Rejection Reason**: 필수 AC 2건 불만족 → 부분적 Pillar 4 앵커 모먼트 손실

### Alternative 2: Post-process LUT 단독 (Light Actor 구동 없음)

- **Description**: Light Actor Temperature/Intensity 고정. 색온도 변화는 Post-process LUT 블렌딩으로만 표현
- **Pros**: Lumen Surface Cache 재계산 비용 0 (Light 변경 없음) → Lumen GPU budget 매우 안정
- **Cons**: **Lumen GI가 실제 색온도 변화 반영 안 함** — Moss Baby 이끼 아이 표면의 GI 반사광이 항상 동일 색. Player Fantasy의 "Waiting 구리빛 → Dream 달빛" 전환이 screen-space 보정처럼 느껴짐 (표면 하위 SSS 색상 변화는 MPC로 보정 가능하나, 유리병 굴절·공간 전체 GI는 LUT로 커버 불가). **렌더링 정확도 손실**
- **Rejection Reason**: 안커 모먼트의 핵심 가치 (GI 기반 색온도 변화)가 screen 보정으로 대체됨 → "달빛이 씬을 감싸는" 감각 약화

### Alternative 3: Blueprint 바인딩으로 MPC → Light Actor 복사

- **Description**: Light Actor의 Construction Script 또는 Tick Blueprint에서 MPC scalar 읽어 Temperature/Intensity set
- **Pros**: 디자이너가 Blueprint에서 바인딩 로직 직접 편집
- **Cons**:
  1. **Blueprint tick 순서 비결정**: UE Actor Tick Priority 설정에도 tick 순서가 frame 간 변동 가능. AC-GSM-08 (mid-transition restart) 검증 시 결정성 부재
  2. **Debug 어려움**: Blueprint 그래프에 값 검사 breakpoint 한계. C++ 코드 리뷰·정적 분석 불가
  3. **Cycle 리스크**: Blueprint 의존 관계가 Subsystem ↔ Actor ↔ Material로 확장되면 init 순서 race 가능
- **Rejection Reason**: 결정성·debug 용이성·cycle 회피가 architecture 우선. 디자이너 튜닝은 MPC Asset (`UGameStateMPCAsset`) 편집으로 충분

### Alternative 4: Hybrid (채택) — (a) + (b) 역할 분담

- 본 ADR의 §Decision

## Consequences

### Positive

- **Lumen GI 정확도**: Light Actor Temperature/Intensity 실제 변경으로 6 GameState 색온도가 GI 반사광에 반영 — "달빛이 씬을 감싸는" 감각 완전 실현
- **Post-process 책임 분담 명확**: Vignette·DoF 등 screen-space 효과는 Lumen Dusk Scene, Light·MPC는 GSM — Layer Boundary 유지
- **AC 자동화 가능**: C++ 결정적 구동 → AC-GSM-08 (mid-transition) + AC-LDS-08 (Waiting→Dream frame delta) 자동화
- **필수 AC 모두 충족**: AC-LDS-07 (6 상태 시각 구분), AC-LDS-10 (Farewell Vignette), AC-LDS-16 (Dream DoF), AC-LDS-08 (Waiting→Dream 부드러운 전환)
- **MPC 읽기 전용 유지**: Lumen Dusk 씬 머티리얼에서 `SetScalarParameterValue` 호출 금지 (AC-LDS-06 grep) — GSM이 유일 writer 원칙

### Negative

- **Light Actor Mobility 결정 의무**: DirectionalLight = Stationary (Lumen GI 기여 + 부분 Shadow baking), SkyLight = Movable (실시간 앰비언트) — GSM이 Temperature 변경 시 Lumen Surface Cache 재계산 발생 가능
- **GSM과 Lumen Dusk Scene 간 초기화 순서 의존**: Lumen Dusk Actor BeginPlay → `RegisterSceneLights` 호출이 GSM Subsystem Initialize 후에 발생해야 함. **Mitigation**: Actor BeginPlay는 Subsystem Initialize 이후 — UE 표준 lifecycle, 문제 없음
- **Lumen GPU 비용 변동**: 6 상태 전환 시 Temperature 변경 → Lumen Surface Cache partial 재계산. GPU budget 5ms 초과 리스크. **Mitigation**: AC-LDS-04 [5.6-VERIFY] 모든 6 상태에서 GPU 시간 측정 + 초과 시 SWRT 자동 폴백 분기 (Lumen Dusk §E1)

### Risks

- **Lumen HWRT GPU 5ms 초과**: Temperature 급격 변경 (Waiting 2,800K → Dream 4,200K, 1.35s) 중 Surface Cache 재계산 피크. **Mitigation**: 
  1. AC-LDS-04 [5.6-VERIFY] 실측 후 `LumenSurfaceCacheResolution` 0.5 → 0.25 조정 가능
  2. Blend 지속 시간 (`D_blend`) 연장 (Waiting→Dream 1.35s → 2.0s) — 변화율 감소
  3. 최후의 수단: SWRT 폴백 (`r.Lumen.HardwareRayTracing 0`) — Lumen Dusk E1 허용
- **Post-process LUT와 MPC Contrast 중복 적용**: `PP_MossDusk`의 `Contrast`가 MPC `ContrastLevel` 참조 + Post Process Volume 자체 Contrast 파라미터 이중 적용 가능. **Mitigation**: PP_MossDusk 설정 시 MPC 참조 Contrast만 활성화, Volume의 native Contrast는 0.5 (no-op) 고정
- **Light Actor 누락 시 nullptr crash**: `TWeakObjectPtr.Get()` null 체크 누락 시 TickMPC crash. **Mitigation**: GSM `TickMPC`에 null check 필수 (상기 Key Interfaces 의사코드)

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `game-state-machine.md` | §OQ-6 BLOCKING ADR — "MPC-Light Actor 동기화 아키텍처: (a) GSM이 Light Actor 직접 제어 / (b) Post-process LUT / (c) Blueprint 바인딩" | **본 ADR이 OQ-6의 정식 결정** (hybrid (a)+(b) 채택) |
| `game-state-machine.md` | §Rule 3 MPC 목표값 8 파라미터 | GSM TickMPC가 MPC + Light Actor 동시 갱신 |
| `game-state-machine.md` | §Rule 4 MPC Lerp Formula 1 (SmoothStep) | TickMPC 내 순차 적용 — 결정적 |
| `game-state-machine.md` | AC-GSM-08 mid-transition restart | C++ 결정적 구동으로 자동화 |
| `lumen-dusk-scene.md` | §OQ-1 BLOCKING — "MPC-Light Actor 동기화 아키텍처" (GSM OQ-6과 동일) | **본 ADR이 동일 결정으로 해소** |
| `lumen-dusk-scene.md` | §Rule 3 Lumen HWRT 설정 + DirectionalLight Stationary + SkyLight Movable | 본 ADR이 Mobility 채택 + Temperature 구동 |
| `lumen-dusk-scene.md` | §Rule 4 MPC 구독 (씬 머티리얼 read-only) | 본 ADR이 MPC 읽기 전용 유지 확정 |
| `lumen-dusk-scene.md` | §Rule 5 Post-process `PP_MossDusk` (Vignette Farewell) | 본 ADR이 Lumen Dusk 소유 확정 |
| `lumen-dusk-scene.md` | AC-LDS-04 [5.6-VERIFY] Lumen GPU 5ms budget | 본 ADR Consequences Risks §Mitigation |
| `lumen-dusk-scene.md` | AC-LDS-07 6 상태 시각 구분 (QA sign-off) | 본 ADR이 6 상태 Temperature·Intensity·Rotation 각각 적용 |
| `lumen-dusk-scene.md` | AC-LDS-06 MPC `SetScalarParameterValue` 씬 코드 0건 grep | 본 ADR이 GSM 유일 writer 원칙 확정 |
| `lumen-dusk-scene.md` | AC-LDS-10 Farewell Vignette 점진 증가 | Lumen Dusk 자체 Lerp (OnGameStateChangedHandler) |
| `lumen-dusk-scene.md` | AC-LDS-16 Dream DoF Formula 1 | Lumen Dusk 소유 |
| `moss-baby-character-system.md` | §Material Params §AmbientMoodBlend — GSM MPC 앰비언트 자동 반영 | MPC는 머티리얼 read-only 유지로 자동 반영 |
| `moss-baby-character-system.md` | AC-MBC-03 머티리얼 파라미터 리터럴 금지 (UMossBabyPresetAsset만) | MBC 자체 파라미터는 Preset 경유, AmbientMoodBlend는 MPC read |

## Performance Implications

- **CPU (GSM TickMPC)**: per-frame 8 MPC SetScalar + 3-4 Light Actor property set + 1 SkyLight set ≈ 50-100μs/frame (무시 가능, 16.6ms budget의 0.5%)
- **GPU (Lumen HWRT)**: 5 GameState 전환 시 Light Actor Temperature 변경 → Lumen Surface Cache partial 재계산. 피크 +0.5-1.5ms (Waiting→Dream 1.35s 블렌드 중). 5ms budget 내 유지 가능하나 AC-LDS-04 실측 의무
- **GPU (Post-process)**: `PP_MossDusk` Vignette Lerp + DoF Lerp ≈ 1.2ms (Lumen Dusk D2 Formula 기준)
- **Memory**: Light Actor 2개 TWeakObjectPtr 캐시 ≈ 16 bytes, MPC Instance 1개 ≈ 256 bytes — 무시 가능
- **Load Time**: 영향 없음 (초기화 시점에 Light Actor 스폰 + MPC 로드는 Lumen Dusk Scene 단일 레벨 로드에 포함)

## Migration Plan

Foundation sprint 이후, Lumen Dusk Scene 첫 구현 마일스톤 직전:

1. **UGameStateMPCAsset 정의 확정** (GSM §Rule 3 8 파라미터) — 기존 GDD 명세
2. **UMossGameStateSubsystem::RegisterSceneLights() API 추가** — 상기 Key Interfaces
3. **UMossGameStateSubsystem::TickMPC() 구현** — MPC + Light Actor 순차 갱신
4. **ALumenDuskSceneActor::BeginPlay에서 RegisterSceneLights 호출**
5. **ALumenDuskSceneActor::OnGameStateChangedHandler() 구현** — Vignette·DoF Lerp 자체 소유
6. **Light Actor Mobility 설정**: DirectionalLight = Stationary, SkyLight = Movable (Lumen Dusk §Rule 3)
7. **PP_MossDusk Post Process Volume 구성**: Contrast는 MPC 참조, Vignette·DoF는 자체 Lerp
8. **AC-LDS-04 실기 GPU 프로파일링 게이트** — 6 GameState 각각에서 Lumen GPU 시간 측정
9. **AC-LDS-07 수동 시각 검증** — 6 상태 스크린샷 + QA 리드 sign-off
10. **PSO Precaching** — Lumen Dusk §R6 + AC-LDS-11 병행

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| GSM 유일 MPC writer | AC-LDS-06 grep `SetScalarParameterValue\|SetVectorParameterValue` in Lumen Dusk code | 0건 |
| Light Actor 캐시 등록 | `RegisterSceneLights()` 호출 여부 로그 | BeginPlay 시 1회 호출 |
| Temperature 동기화 | AC-LDS-08 — Waiting→Dream 전환 중 frame delta ≤ 100K/frame @60fps | 단일 frame 점프 없음 |
| Lumen GPU budget | AC-LDS-04 [5.6-VERIFY] — 6 상태 각각 `stat GPU` + `stat Lumen` | ≤ 5ms `LumenGPUBudgetMs` |
| 6 상태 시각 구분 | AC-LDS-07 수동 — Menu / Dawn / Offering / Waiting / Dream / Farewell 스크린샷 | QA 리드 sign-off |
| Farewell Vignette | AC-LDS-10 — 1.5-2.0s 점진 증가 (0.0→0.6) | 즉시 점프 없음 |
| Dream DoF | AC-LDS-16 — Formula 1 clamp [1.4, 4.0] | 정확 |
| MotionBlur 비활성 | AC-LDS-09 — `r.MotionBlur.Amount = 0.0` | 확인 |
| mid-transition restart | AC-GSM-08 — P0 인터럽트 시 new V_start = 인터럽트 시점 중간값 | C++ 결정적 구동 검증 |
| GSM ↔ Light Actor 초기화 순서 | Subsystem Initialize 후 Actor BeginPlay 확인 | null check 미발화 |

## Related Decisions

- **game-state-machine.md** §OQ-6 — 본 ADR의 직접 출처 (BLOCKING)
- **lumen-dusk-scene.md** §OQ-1 — 동일 ADR (GSM OQ-6과 중복 — 본 ADR이 두 OQ 동시 해소)
- **ADR-0011** (`UDeveloperSettings`) — GSM/Lumen Dusk Tuning Knob은 `UMossGameStateSettings` / `UMossLumenDuskSettings`. 단, `UGameStateMPCAsset`·`ULumenDuskAsset`은 ADR-0011 예외 (per-content 데이터)
- **architecture.md** §Data Flow §3.6 MPC Update Path — 본 ADR의 §Architecture Diagram이 해당 섹션 구체화
- **Lumen Dusk AC-LDS-04 / AC-LDS-11 [5.6-VERIFY]** — Implementation 단계 검증 의무
