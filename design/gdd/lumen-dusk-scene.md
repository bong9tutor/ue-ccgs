# Lumen Dusk Scene (+ PSO Precaching)

> **Status**: Designed — 초안 완성, 리뷰 대기
> **Author**: bong9tutor + claude-code session
> **Created**: 2026-04-18
> **Last Updated**: 2026-04-18 (R1 초안)
> **Implements Pillar**: Pillar 1 (조용한 존재 — 씬은 결코 주의를 요구하지 않는다), Pillar 2 (선물의 시학 — 황혼 빛이 선물을 감싼다)
> **Priority**: MVP | **Layer**: Presentation (Core Env Assets) | **Category**: Core
> **Effort**: M (2–3 세션)
> **Depends On**: Game State Machine (#5) — PSO Precaching 소유, Stillness Budget (#10) 구독

---

## A. Overview

Lumen Dusk Scene은 Moss Baby의 **환경 컨테이너** — 유리병, 책상 표면, 배경 소품, 카메라, 대기(atmosphere)를 구성하는 씬 레이어다. 플레이어가 앱을 열면 가장 먼저 만나는 것이 이 씬이며, 21일 내내 이끼 아이가 살아가는 공간이다.

이 시스템의 책임은 다섯 가지로 나뉜다. **(1) 씬 구성** — 정적 메시(유리병, 책상, 배경 소품), 고정 카메라, 피사계 심도(DoF)로 손바닥 위 디오라마 느낌을 만든다. **(2) Lumen HWRT 조명** — 황혼 분위기의 GI를 HWRT Lumen으로 구현한다. 이것이 이 게임의 시각적 정체성이다. **(3) 상태 구동 대기(atmosphere)** — GSM 상태 전환 시 MPC(Material Parameter Collection)가 변경한 파라미터를 씬 자산이 자동으로 반영한다. 씬 자체는 MPC를 읽기만 한다. **(4) PSO Precaching** — 앱 첫 실행 또는 마운트 시 Pipeline State Object를 미리 컴파일해 첫 프레임 셰이더 컴파일 히치를 방지한다. **(5) 포스트 프로세싱** — 색 보정, 블룸, 비녜트를 GSM 상태에 따라 조정한다.

Lumen Dusk Scene은 GSM MPC를 *소비*하는 씬이지, MPC를 *구동*하는 주체가 아니다. 씬 내 모든 머티리얼은 GSM이 설정한 MPC 값을 참조하며, GSM의 상태 전환이 일어나는 순간 씬은 알아서 따라간다. 이 단방향 의존이 아키텍처의 핵심이다.

---

## B. Player Fantasy

황혼이 책상 위 유리병을 감쌀 때, 세계는 거기서 멈춘다. 밖은 여전히 바쁘지만, 이 작은 빛 덩어리 안에는 시간이 다른 속도로 흐른다.

플레이어는 이 게임의 씬이 "왜 이렇게 따뜻한지" 설명할 수 없다. 설명할 필요도 없다. 2,800K의 구리빛이 유리 곡면에 굴절되고, 이끼 아이의 표면에 스며들고, 책상의 나뭇결에 머무는 것 — 이 빛의 물리가 "여기는 안전하다"는 감각을 만든다. 광원 각도는 늘 낮고 비스듬하다. 정오의 빛은 이 세계에 없다.

그리고 어느 날 밤, 달빛이 유리병을 4,200K로 바꿔버린다. 그 순간 씬 전체가 "다른 무언가가 일어나고 있다"고 말한다. 대사 없이, 알림 없이, 조명만으로.

꿈 상태에서 배경이 더 흐려지는 것은 이끼 아이에게 시선이 집중되는 감각이다 — 세계가 좁아지고 이 존재만 선명해지는 순간. 이 DoF 강화는 `FocusDistance`(초점 거리) 변경이 아니라 `ApertureFStop`(조리개) 감소로 구현된다. 카메라는 자리를 바꾸지 않고, 세계가 스스로 물러나는 것이다.

**앵커 모먼트**: Waiting(구리빛) → Dream(차가운 달빛) 전환. 씬의 색온도가 뒤집히는 1.35초. 이 장면이 기억에 남는 이유는 씬이 만든 것이 아니라, 21일 동안 달라지지 않던 빛이 처음으로 달라졌기 때문이다.

**MDA 연결**:
- **Sensation (감각)** — 황혼 Lumen GI의 따뜻한 빛, DoF의 미니어처 보케. 1순위
- **Fantasy (환상)** — 작은 유리병 안 세계라는 설정이 씬 구성으로 강화됨. 2순위
- **Submission (침잠)** — 씬은 결코 플레이어에게 행동을 요구하지 않는다. 그냥 거기 있다

**Pillar 연결**:
- **Pillar 1 (조용한 존재)** — 씬은 조용하다. 카메라가 움직이지 않고, 조명이 갑자기 깜빡이지 않고, VFX가 화면을 가득 채우지 않는다. 앰비언트 파티클조차 Stillness Budget의 Background 슬롯 안에서만 허용된다
- **Pillar 2 (선물의 시학)** — 황혼의 낮은 빛은 이 세계의 시간이 선물을 건네기 좋은 때임을 암묵적으로 전달한다. 빛 자체가 의식(儀式)의 무대 설정이다

---

## C. Detailed Design

### Core Rules

#### Rule 1 — 씬 레이아웃 (Scene Composition)

씬은 다음 레이어로 구성된다. 모든 메시는 정적(Static)이며 Nanite 비활성화(소형 씬, 오버헤드 불요).

| 레이어 | 에셋 | 메모 |
|---|---|---|
| **Foreground** | `SM_GlassJar` — 유리병 본체 (이끼 아이 배치 기준점) | 씬의 중심축. 광학 굴절 머티리얼 필수 |
| **Surface** | `SM_Desk` — 책상 표면 타일 | 테이블 최소 텍스처. 나뭇결 권장 |
| **Background** | `SM_Background_Objects` — 책, 컵 등 소품 1–3개 | 깊이감 제공. DoF로 흐려짐. Full Vision에서 추가 |
| **Lighting** | `DirectionalLight` (주광원) + `SkyLight` (앰비언트) | GSM MPC 파라미터로 구동 |
| **Atmosphere** | `BP_AmbientParticle` — 포자·이슬 Niagara | Stillness Budget Background 슬롯 소비 |
| **Post Process Volume** | `PP_MossDusk` — 색 보정, 블룸, 비녜트 | 씬 전체 감쌈. Infinite Extent |

**씬 경계**: 카메라 FOV 밖 영역에는 메시 배치 없음. Lumen Surface Cache 커버리지 낭비 방지.

**Nanite 정책**: 모든 정적 메시에 `NaniteEnabled = false`. 소형 씬에서 Nanite 오버헤드가 순이익 없음 (성능 예산 §Formulas 참조).

#### Rule 2 — 고정 카메라 (Fixed Camera)

카메라는 **단 한 개**, 씬 생애주기 동안 이동하지 않는다.

| 파라미터 | 값 | 설명 |
|---|---|---|
| `CameraFOV` | 35° | 좁은 FOV로 망원 느낌. 클로즈업 미니어처 감 |
| `CameraPosition` | (0, −50, 15) — 단위 cm | 유리병 정면 약간 위에서 아래로 내려다봄 |
| `CameraRotation` | Pitch −10°, Yaw 0°, Roll 0° | 약간 아래를 향함 |
| `CameraDoFEnabled` | true | 원경 소품과 전경 유리병 사이 보케 |

**DoF 설정** — Formula 1 참조. 게임플레이 중 카메라 이동 없음. 컷신(시네마틱)도 별도 카메라로 처리하며 이 카메라를 조작하지 않는다.

**800×600 판독성 요건**: 해상도가 최소(800×600)로 낮아질 때 이끼 아이가 화면 높이의 최소 12% 이상을 차지해야 한다. 카메라 위치와 이끼 아이 메시 스케일로 보장.

#### Rule 3 — Lumen HWRT 설정 (Lighting)

이 게임의 조명은 HWRT(Hardware Ray Tracing) Lumen으로 구동된다. UE 5.6은 Lumen HWRT CPU bottleneck 개선을 포함하며, 소형 데스크톱 씬에서 품질과 성능의 균형을 허용한다.

**Lumen 설정**:

| 설정 | 값 | 근거 |
|---|---|---|
| `Lumen.HardwareRayTracing` | 1 (enabled) | SWRT 대비 GI 품질 우위. UE 5.6 CPU 병목 개선으로 허용 |
| `Lumen.MaxTraceDistance` | 300 (cm) | 씬 크기 ≈ 100cm. 3× 여유 |
| `Lumen.SceneCaptureCacheResolution` | 64 | 소형 씬 충분. 128은 오버킬 |
| `Lumen.SurfaceCacheResolution` | 0.5 | 절반 해상도. 메모리 절감 (Surface Cache 예산 §Formulas) |

**광원 Actor**:
- `DirectionalLight`: Mobility = Stationary (Lumen GI 기여 + 그림자). Temperature/Intensity는 GSM MPC `ColorTemperatureK` / `LightAngleDeg` 참조 (Rule 4)
- `SkyLight`: Mobility = Movable (실시간 Lumen 앰비언트 갱신). Intensity는 MPC `AmbientIntensity`로 구동

> **OQ-1 (열린 질문)**: GSM MPC scalar가 Light Actor의 Temperature/Intensity를 자동 반영하지 않는 UE 구조적 제약이 있다. GSM GDD OQ-6에서 이 아키텍처 결정 (`/architecture-decision`으로 ADR 필요)이 Lumen Dusk Scene 첫 구현 마일스톤 전에 이루어져야 한다. OQ-1 해소 전까지 주광원 색온도·각도 구동 방식이 미확정이다.

#### Rule 4 — MPC 구독 (State-Driven Atmosphere)

씬의 모든 머티리얼과 포스트 프로세싱은 GSM이 소유하는 `UMaterialParameterCollection`(이하 MPC)을 읽기 전용으로 참조한다. 씬은 MPC 값을 설정하지 않는다 — 오직 읽는다.

**씬이 소비하는 MPC 파라미터**:

| MPC 파라미터 | 소비하는 씬 에셋 | 반응 |
|---|---|---|
| `ColorTemperatureK` | DirectionalLight, SkyLight | 광원 색온도. GSM Blueprint 바인딩 또는 Post-process LUT (OQ-1 해소 후 확정) |
| `LightAngleDeg` | DirectionalLight | 주광원 수평 각도. Actor Rotation 구동 |
| `ContrastLevel` | `PP_MossDusk` Post Process Volume | Contrast 파라미터로 전달 |
| `AmbientR/G/B` | SkyLight, `PP_MossDusk` | 앰비언트 색상 |
| `AmbientIntensity` | SkyLight Intensity | 보조광 강도 |
| `MossBabySSSBoost` | Moss Baby 머티리얼 (MBC 소유) | 직접 소비하지 않음 — MBC가 읽음 |

**씬 자체 MPC 반응 타이밍**: `FOnGameStateChanged` 수신 후 MPC 값이 Lerp 중이면 씬은 Lerp 완료를 기다리지 않고 MPC 현재 값을 매 프레임 읽는다. 자동 따라가기.

#### Rule 5 — 포스트 프로세싱 (Post-Processing)

`PP_MossDusk` 포스트 프로세스 볼륨은 씬 전체를 감싸는 Infinite Extent 볼륨이다.

| PP 파라미터 | 연결 | 범위 |
|---|---|---|
| `Contrast` | MPC `ContrastLevel` 연동 | [0.1, 1.5] |
| `Bloom.Intensity` | 고정값 (상태 무관) | 0.3 — 은은한 블룸 |
| `Bloom.Threshold` | 고정값 | 1.0 |
| `Vignette.Intensity` | 상태별 오버라이드 가능 | [0.0, 0.6] — Farewell 시 점진 증가 |
| `ColorGrading.TemperatureType` | White Balance | `ColorTemperatureK` 연동 (OQ-1 해소 후) |
| `MotionBlur` | 강제 비활성화 | 0.0 — 조용한 씬에서 모션 블러 금지 |

**Farewell 비녜트**: Any→Farewell 전환 시 `Vignette.Intensity`가 0.0 → 0.6으로 서서히 증가한다. 지속 시간은 Farewell 블렌드 시간(1.5–2.0초)과 동기화. 이 비녜트 증가는 Lumen Dusk Scene이 소유한다 (GSM GDD §Visual 명시).

#### Rule 6 — PSO Precaching (Pipeline State Object 사전 컴파일)

Lumen Dusk Scene은 PSO Precaching의 소유자다. 목표: 앱 첫 로드 시(또는 첫 실행 시) 주요 셰이더가 이미 컴파일되어 첫 프레임 히치(hitching)가 없도록 한다.

**PSO 컴파일 타이밍**:
1. **초기 로드 단계** (`UGameInstanceSubsystem::Initialize()` — Data Pipeline과 동일 타이밍): PSO 번들 자산(`PSO_MossDusk.bin`) 비동기 로드 시작
2. **Data Pipeline Ready 이후, 씬 스폰 전**: PSO 번들 로드 완료 대기 (최대 `PSOWarmupTimeoutSec` — Tuning Knob)
3. **씬 스폰**: PSO 준비 완료 후 씬 액터 스폰. PSO 미완료 시 DegradedFallback 진행 (첫 프레임 히치 허용 — 기능 차단은 하지 않음)

**PSO 포함 범위**:
- 6개 GSM 상태의 MPC 극단값 조합
- Lumen Surface Cache 초기화 셰이더
- 포스트 프로세싱 조합 (Contrast 최소·최대)
- 앰비언트 파티클 Niagara 셰이더

> **구현 참고 (UE 5.6)**: `FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast)` 로 비동기 컴파일. `FShaderPipelineCache::NumPrecompilesRemaining()` 으로 완료 여부 폴링. PSO 번들 파일은 빌드 파이프라인에서 사전 생성 필요 (`r.ShaderPipelineCache.Enabled 1`).

#### Rule 7 — Stillness Budget 구독

씬 내 앰비언트 파티클과 조명 전환 애니메이션은 Stillness Budget의 Budget 슬롯을 소비한다.

| 효과 | 채널 | 우선순위 | 조건 |
|---|---|---|---|
| 앰비언트 파티클 (포자, 이슬) | `Particle` | `Background` | 씬 스폰 시 요청, 씬 소멸 시 Release |
| 조명 전환 애니메이션 | `Motion` | `Background` | GSM 상태 전환 시 요청, 전환 완료 시 Release |

- 앰비언트 파티클은 Background 우선순위 → Narrative 이벤트(꿈, 성장 변환) 시 선점 가능
- Deny 시: 파티클은 정지(스폰 중단), 조명은 MPC 값 즉시 적용(블렌드 없이)
- `IsReducedMotion()` 시: 파티클 스폰 전면 중단 + 조명 Lerp 즉시 적용

### States (Per-GSM-State Atmosphere)

GSM 6개 상태의 씬 외관은 GSM GDD §Detailed Design Rule 3의 MPC 목표값 테이블로 완전히 정의된다. 여기서는 씬 레이어 관점에서 각 상태의 **시각적 의도와 씬 고유 효과**만 기술한다.

| GSM State | 색온도 | 씬 분위기 | Lumen Dusk Scene 고유 효과 |
|---|---|---|---|
| **Menu** | 3,600K | 중성 라벤더 황혼. 게임 시작 전 중간 상태 | 파티클 슬로 (Background Request) |
| **Dawn** | 2,800K | 이른 아침 붉은 호박빛 — "새로운 날이 열린다" | 파티클 서서히 깨어남 |
| **Offering** | 3,200K | 선명한 황금빛 — "선물을 드릴 시간" | 파티클 약간 활성화 |
| **Waiting** | 2,800K | 구리빛 황혼 — 게임의 기본 상태 | 앰비언트 파티클 정상 루프 |
| **Dream** | 4,200K | **차가운 달빛** — 이 게임 유일의 냉색 조명 | 파티클 선점 해제(꿈 시퀀스가 Narrative 슬롯 사용). DoF 약간 강화 (Formula 1 `DreamFStopDelta` — 조리개 강화로 배경 더 흐림) |
| **Farewell** | 2,200K | 초 전 촛불빛 — 가장 따뜻하고 가장 마지막 빛 | Vignette 점진 증가 (Rule 5). 파티클 서서히 소멸 |

**Dream 상태 DoF 강화**: Dream 진입 시 `ApertureFStop`이 `BaseFStop + DreamFStopDelta × DreamBlend`로 감소한다 (Formula 1). 조리개가 열릴수록 피사계 심도가 얕아져 배경이 더 흐려지고, 이끼 아이에 시선이 집중된다. Dream 이탈 시 `DreamBlend`가 0으로 복귀하면서 `ApertureFStop`이 `BaseFStop` 기본값으로 돌아간다.

### Interactions with Other Systems

#### 1. Game State Machine (#5) — 인바운드 (Primary)

| 방향 | 데이터 | 시점 |
|---|---|---|
| GSM → Scene | `FOnGameStateChanged(EGameState OldState, EGameState NewState)` | 상태 전환 시 |
| GSM → Scene | MPC 파라미터 (읽기만 함) | 매 프레임 |

- Lumen Dusk Scene은 `FOnGameStateChanged`를 구독해 Farewell 비녜트 증가, Dream DoF 강화 등 씬 고유 효과를 처리한다
- MPC는 GSM이 소유·갱신. 씬 머티리얼은 참조만

#### 2. Stillness Budget (#10) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Scene → Budget | `Request(Particle, Background)` | 씬 스폰 시 |
| Scene → Budget | `Request(Motion, Background)` | GSM 상태 전환 시 |
| Budget → Scene | `FStillnessHandle` 또는 Invalid | 즉시 응답 |
| Scene → Budget | `Release(Handle)` | 씬 소멸 또는 전환 완료 시 |
| Scene ← Budget | `IsReducedMotion()` | 파티클 스폰 결정 시 |

#### 3. Moss Baby Character System (#6) — 간접

- 이끼 아이 Actor는 씬 안에 존재하지만 Lumen Dusk Scene이 소유하지 않는다
- MBC가 씬의 MPC를 읽기 전용으로 참조 (`AmbientMoodBlend` 파라미터)
- Lumen Surface Cache가 이끼 아이 메시 변경(GrowthTransition/FinalReveal) 시 1–2프레임 재빌드 발생 — 소형 씬에서 허용 범위

#### 4. Data Pipeline (#2) — 인바운드 (PSO 에셋 로드)

| 방향 | 데이터 | 시점 |
|---|---|---|
| Pipeline → Scene | PSO 번들 에셋 경로 조회 | 초기화 시 1회 |

#### 5. Window & Display Management (#14) — 인바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Window → Scene | 해상도·DPI 변경 이벤트 | 창 크기 변경 시 |

- 창 크기 변경 시 카메라 FOV 불변. 해상도만 변경
- 800×600 판독성 요건 확인 (Rule 2)

---

## D. Formulas

### Formula 1 — 피사계 심도 계산 (Scene.DoFParameters)

DoF는 유리병 중심에 초점을 맞추고 원경 소품을 흐린다.

```
FocusDistance = DistanceCameraToJar_cm   [고정값, 런타임 불변]
ApertureFStop = BaseFStop + DreamFStopDelta × DreamBlend
```

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `DistanceCameraToJar_cm` | D_f | float (cm) | [30, 80] | 카메라에서 유리병 중심까지 거리. 카메라 배치 결정 후 고정 |
| `BaseFStop` | F_base | float | [1.4, 4.0] | 기본 조리개 값. 낮을수록 보케 강함. 기본 2.0 |
| `DreamFStopDelta` | ΔF | float | [-1.0, 0.0] | Dream 상태 추가 개방량. 기본 -0.5 (더 흐림) |
| `DreamBlend` | B | float | [0.0, 1.0] | GSM Dream Lerp 진행률. MPC 블렌드와 동기 |

**Output Range**: ApertureFStop ∈ [1.4, 4.0] — clamp 적용

**Example**: BaseFStop=2.0, DreamBlend=0.5, ΔF=−0.5
- ApertureFStop = 2.0 + (−0.5) × 0.5 = **1.75** (Dream 중간 — 배경 약간 더 흐림)

**고정값 근거**: `FocusDistance`는 카메라 위치가 고정이므로 런타임 불변. Lerp 불필요.

### Formula 2 — 성능 예산 배분 (Scene.PerformanceBudget)

16.6ms 프레임 예산 내 Lumen Dusk Scene의 GPU 시간 상한.

```
T_scene = T_total × SceneGPUBudgetRatio
T_lumen = T_scene × LumenGPURatio
T_pp    = T_scene × PostProcessRatio
T_mesh  = T_scene − T_lumen − T_pp
```

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `T_total` | T | float (ms) | 16.6 | 프레임 전체 예산 (60fps, 성능 예산 §Technical) |
| `SceneGPUBudgetRatio` | R_scene | float | [0.3, 0.7] | 씬 총 GPU 비율. 기본 0.5 (8.3ms) |
| `LumenGPURatio` | R_lumen | float | [0.4, 0.7] | 씬 예산 중 Lumen 비율. 기본 0.55 (4.6ms) |
| `PostProcessRatio` | R_pp | float | [0.1, 0.2] | 포스트 프로세싱 비율. 기본 0.15 (1.2ms) |

**기본값 배분 예시** (SceneGPUBudgetRatio=0.5):
- 씬 총: 8.3ms
  - Lumen HWRT: 4.6ms
  - 포스트 프로세싱: 1.2ms
  - 메시 렌더: 2.5ms
- 나머지(UI, 로직, 기타): 8.3ms

**Lumen Surface Cache 메모리**:
- Surface Cache Resolution 0.5 × 기본 → 약 75MB (기본 150MB의 절반)
- HWRT BVH 구조체: 약 20–30MB (소형 씬 기준)
- 총 Lumen 메모리: 약 100–105MB
- 1.5GB ceiling 대비 약 7% — 허용 범위

### Formula 3 — 앰비언트 파티클 스폰 밀도 (Scene.ParticleDensity)

```
SpawnRate = BaseSpawnRate × BudgetMultiplier × (1 − DreamBlend × DreamParticleReductionRatio)
```

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `BaseSpawnRate` | R_base | float (particles/sec) | [2, 10] | 기본 스폰 속도. 기본 5.0/초 |
| `BudgetMultiplier` | M_budget | float | [0.0, 1.0] | Stillness Budget Particle 슬롯 가용 시 1.0, Deny 시 0.0 |
| `DreamBlend` | B | float | [0.0, 1.0] | Dream 상태 진행률. Dream 중 파티클 감소 |
| `DreamParticleReductionRatio` | R_dream | float | [0.5, 1.0] | Dream 최대 감소 비율. 기본 0.8 |

**Output Range**: [0, BaseSpawnRate]

**Example**: DreamBlend=0.5, BudgetMultiplier=1.0, R_dream=0.8
- SpawnRate = 5.0 × 1.0 × (1 − 0.5 × 0.8) = 5.0 × 0.6 = **3.0/초**

**근거**: Dream 상태는 "조용하고 차가운 달빛" — 포자나 이슬이 활발히 날리는 느낌보다 고요한 정지감이 Player Fantasy에 부합한다.

---

## E. Edge Cases

- **E1 — Lumen HWRT GPU 비용 초과 (성능 폴백)**: 실측 Lumen GPU 시간이 `LumenGPUBudgetMs` (기본 5.0ms, Tuning Knob) 초과 시 자동으로 SWRT(Software Ray Tracing)로 강등. 프레임 프로파일링 결과가 지속적으로 임계를 초과하면 GDD 개정과 `r.Lumen.HardwareRayTracing 0` 플래그로 설정 변경 필요. **SWRT로 전환 시 시각적 차이**: GI 품질이 낮아지고 반사가 부정확해진다. Moss Baby의 유리병 굴절 표현에 영향 가능. 첫 구현 마일스톤에 GPU 프로파일링 게이트 삽입 필수 (systems-index High-Risk Systems 참조).

- **E2 — 창 크기 변경 (Window Resize)**: 창 크기가 800×600 미만으로 조정 시 이끼 아이 가시성 확인 필요. Window & Display Management(#14)에서 최소 창 크기 800×600을 강제하므로 이 씬이 직접 처리하지 않아도 되나, 씬 자체에서도 최소 크기 기준 판독성 체크 (AC-LDS-09). 가로세로 비율 변경 시(와이드스크린↔정방형) FOV 보정 없음 — 고정 FOV 유지, 씬 양옆이 잘릴 수 있음. 이에 대한 대응은 카메라 위치가 고정이므로 가로 여백을 충분히 확보한 씬 레이아웃으로 처리.

- **E3 — Alt-Tab / 포커스 상실**: 앱이 포커스를 잃으면 렌더링을 최소화(최소 프레임레이트 모드). Lumen 업데이트 중단 → 포커스 복귀 시 1–2프레임 Lumen 재초기화 히치 가능. `r.OneFrameThreadLag 1` + 포커스 복귀 시 `r.Lumen.DiffuseIndirect.Allow 1` 재활성화로 대응. 이 히치는 허용 범위(사용자가 의도적 작업 전환 후 복귀한 시점).

- **E4 — 앱 첫 실행 (PSO 컴파일 미완료)**: PSO Precaching이 완료되기 전에 씬이 스폰되면 첫 몇 프레임에 셰이더 컴파일 히치(1–3프레임 stall) 가능. DegradedFallback 진행 (기능 차단 없음). 히치는 Day 1 Dawn 진입 중 발생 — Dawn 최소 체류 시간(DawnMinDwellSec) 내에 PSO 준비 완료를 목표로 하되, 미완료 시 허용. **첫 실행에서만 발생** — 이후 PSO 번들이 캐시되어 재발생 없음.

- **E5 — PSO 번들 파일 없음 또는 손상**: `PSO_MossDusk.bin` 파일을 찾을 수 없거나 손상된 경우 경고 로그 1회 후 PSO Precaching 없이 계속 진행. 기능에 영향 없음, 첫 프레임 히치만 발생. **빌드 파이프라인 문제**로 간주 — 배포 시 PSO 번들 포함 확인 CI 체크 추가 권장.

- **E6 — ReducedMotion ON 상태에서 씬 스폰**: 파티클 스폰 전면 없음. 조명 Lerp 즉시 적용. 씬의 정적 외관만 표시 — 기능에 영향 없음. Stillness Budget `IsReducedMotion()` 씬 스폰 시 확인.

- **E7 — Dream DoF Lerp 도중 Dream 이탈 (Budget Deny로 Dream 연기)**: DoF 중간값에서 Waiting 기본 FStop으로 역방향 Lerp 시작. MPC 블렌드와 동기화 필요. DoF Lerp 완료 체크와 MPC `DreamBlend` 값이 일치해야 함.

- **E8 — 6개 상태 MPC 극단값에서 씬 렌더 이상**: Dream(4,200K, Contrast 0.9) + Farewell(2,200K, Vignette 0.6) 극단값이 실기에서 예상 외 아티팩트(Lumen 노이즈, 클리핑)를 낼 수 있음. 첫 구현 마일스톤에서 모든 6개 상태 시각 확인 필수 (AC-LDS-07).

- **E9 — 배경 소품 메시 로드 실패**: 배경 소품은 Soft Reference로 참조 — 로드 실패 시 소품 없이 씬 계속 진행. 유리병과 책상 메시(Hard Reference)는 실패 시 크래시 → 로드 실패 처리 필수.

- **E10 — Farewell 상태에서 파티클 소멸 도중 앱 종료**: 파티클은 씬 소멸 시 강제 종료. 상태 저장 없음. 재시작 시 `LastPersistedState=Farewell` → 씬이 Farewell MPC값으로 즉시 스폰.

---

## F. Dependencies

| System | 방향 | 성격 | 인터페이스 | MVP/VS |
|---|---|---|---|---|
| **Game State Machine (#5)** | 인바운드 | **Hard** | `FOnGameStateChanged(EGameState, EGameState)` + MPC 읽기 전용 | MVP |
| **Stillness Budget (#10)** | 아웃바운드 | **Soft** | `Request(Particle/Motion, Background)`, `IsReducedMotion()` | MVP |
| **Moss Baby Character System (#6)** | 간접 | **None** (동거) | 동일 씬 내 Actor. MPC 공유 (읽기) | MVP |
| **Data Pipeline (#2)** | 인바운드 | **Soft** | PSO 번들 에셋 경로 조회 | MVP |
| **Window & Display Management (#14)** | 인바운드 | **Soft** | 해상도·DPI 변경 이벤트 | MVP |
| **Audio System (#15)** | 없음 | — | Lumen Dusk Scene은 오디오 직접 생성 없음. 오디오는 Audio System(VS) 담당 | VS |

**Hard dependency**: 이 시스템 없이 씬 동작 불가.
**Soft dependency**: 이 시스템 없어도 씬은 동작하나 기능 누락(파티클 없음, PSO 없음 등).
**None**: 데이터 의존 없음. 공간 공존.

**양방향 일관성 확인**:
- GSM GDD §Downstream 소비자 테이블에 "Lumen Dusk Scene (#11) — 상태별 환경 에셋·카메라·DoF 조정 — MVP" 명시 ✓
- Stillness Budget GDD §Downstream에 "Lumen Dusk Scene (#11) — Soft — `Request(Particle/Motion, Background)`" 명시 ✓
- Data Pipeline GDD §formulas에 "OQ-D-1: 텍스처 pool 크기는 Lumen Dusk Scene GDD #11에서 확정" 명시 — 이 GDD에서 Lumen Surface Cache 약 100MB로 확정. Data Pipeline GDD 업데이트 권장

---

## G. Tuning Knobs

모든 Knob은 `ULumenDuskAsset : UPrimaryDataAsset`에 `UPROPERTY(EditAnywhere)` 필드로 노출. 코드 내 리터럴 금지. 소유권 경계: GSM이 소유하는 `UGameStateMPCAsset`(MPC 목표값)과 별개 자산 — Lumen Dusk Scene은 카메라, DoF, 파티클, PSO 설정을 이 자산에서 단독 관리한다.

| Knob | 타입 | 기본값 | 안전 범위 | 카테고리 | 게임플레이 영향 |
|---|---|---|---|---|---|
| `CameraFOV` | float (도) | 35.0 | [28, 50] | Feel | 좁을수록 망원 미니어처 감. 넓으면 광각으로 씬 왜곡 |
| `BaseFStop` | float | 2.0 | [1.4, 4.0] | Feel | 낮을수록 보케 강함. 4.0 이상이면 DoF 무의미 |
| `DreamFStopDelta` | float | -0.5 | [-1.5, 0.0] | Feel | Dream DoF 강화량. 0이면 Dream 차이 없음 |
| `VignetteIntensityFarewell` | float | 0.6 | [0.3, 0.8] | Feel | Farewell 비녜트 최대 강도. 0.8 초과 시 과도한 어둠 |
| `BloomIntensity` | float | 0.3 | [0.1, 0.6] | Feel | 전체 블룸 강도. 0.6 초과 시 Pillar 2 위반 가능 |
| `BaseSpawnRate` | float | 5.0 | [2.0, 10.0] | Feel | 앰비언트 파티클 기본 스폰/초. 10 초과 시 Pillar 1 위반 |
| `DreamParticleReductionRatio` | float | 0.8 | [0.5, 1.0] | Feel | Dream 파티클 감소율. 1.0이면 Dream 중 파티클 완전 소멸 |
| `LumenMaxTraceDistanceCm` | float | 300.0 | [100, 600] | Curve | Lumen 추적 거리. 늘릴수록 GI 품질 향상, GPU 비용 증가 |
| `LumenSurfaceCacheResolution` | float | 0.5 | [0.25, 1.0] | Curve | Surface Cache 해상도. 1.0=풀해상도 150MB |
| `SceneGPUBudgetRatio` | float | 0.5 | [0.3, 0.7] | Curve | 씬 GPU 예산 비율 (F2). 0.7 초과 시 UI·로직 예산 부족 |
| `LumenGPUBudgetMs` | float (ms) | 5.0 | [3.0, 8.0] | Gate | SWRT 폴백 임계. 8.0 초과 시 60fps 유지 불가 영역 |
| `PSOWarmupTimeoutSec` | float (sec) | 10.0 | [5.0, 30.0] | Gate | PSO 컴파일 완료 대기 상한. 초과 시 DegradedFallback |

**Knob 상호작용 주의**:
- `LumenMaxTraceDistanceCm`과 `LumenSurfaceCacheResolution`을 동시에 높이면 GPU 예산 급증 → `LumenGPUBudgetMs` 초과 위험
- `BaseFStop`이 낮고(1.4) `CameraFOV`도 좁으면(28°) 전경 이끼 아이마저 흐려질 수 있음. `DistanceCameraToJar_cm`과 함께 조율 필요

---

## H. Acceptance Criteria

### Rule 1, 2 — 씬 구성·고정 카메라

**AC-LDS-01** | `CODE_REVIEW` | BLOCKING
- **GIVEN** Lumen Dusk Scene 레벨 파일 및 카메라 Actor 설정, **WHEN** CameraActor 이동 코드 경로 grep + 런타임 SetActorLocation/SetActorRotation 호출 확인, **THEN** 카메라 이동 코드 경로 0건. Blueprint 또는 Sequencer에 의한 카메라 이동도 없음 (Rule 2)

**AC-LDS-02** | `CODE_REVIEW` | BLOCKING
- **GIVEN** 씬 정적 메시 에셋 목록 (`SM_GlassJar`, `SM_Desk`, 배경 소품), **WHEN** NaniteEnabled 플래그 grep, **THEN** 모든 정적 메시에서 `bNaniteEnabled = false` (Rule 1)

**AC-LDS-03** | `MANUAL` | ADVISORY
- **GIVEN** 카메라 기본 배치(FOV=35°, Position=(0,−50,15)cm, Pitch=−10°), **WHEN** Viewport에서 씬 확인, **THEN** 이끼 아이가 화면 중앙에 위치하고 유리병이 명확히 보임. 800×600 해상도에서 이끼 아이가 화면 높이의 12% 이상 차지 (Rule 2 판독성 요건)

### Rule 3 — Lumen HWRT

**AC-LDS-04** | `MANUAL` | ADVISORY
- **GIVEN** 씬 로드 후 Stat GPU 활성화, **WHEN** Waiting 상태(기본 상태) 60초 관찰, **THEN** Lumen GPU 시간 `LumenGPUBudgetMs` 이하 (기본 5.0ms). 초과 시 SWRT 폴백 또는 Lumen 설정 조정 필요. **전제조건**: OQ-1 (MPC-Light Actor 동기화) ADR 완료 후 실행

**AC-LDS-05** | `AUTOMATED` | BLOCKING
- **GIVEN** Lumen 설정 파일, **WHEN** `r.Lumen.HardwareRayTracing`, `r.Lumen.MaxTraceDistance`, `r.Lumen.SurfaceCacheResolution` 값 grep, **THEN** 코드 내 하드코딩 없음. 모든 값이 `ULumenSceneAsset` DataAsset 또는 프로젝트 ini에서 참조

### Rule 4 — MPC 구독

**AC-LDS-06** | `CODE_REVIEW` | BLOCKING
- **GIVEN** Lumen Dusk Scene 내 모든 머티리얼 인스턴스 (`MI_GlassJar`, `MI_Desk`, `MI_Background`, `MI_PostProcess`), **WHEN** MPC 쓰기 코드 경로 grep (`SetVectorParameterValue`, `SetScalarParameterValue` on MPC object), **THEN** 씬 코드에서 MPC 파라미터 직접 쓰기 0건. GSM만 MPC를 갱신 (Rule 4 단방향 의존)

**AC-LDS-07** | `MANUAL` | BLOCKING
- **GIVEN** 6개 GSM 상태를 수동으로 순환, **WHEN** 각 상태 진입 후 씬 캡처, **THEN** (1) Menu: 중성 라벤더. (2) Dawn: 호박빛 낮은 광원. (3) Offering: 밝은 황금빛. (4) Waiting: 구리빛 황혼. (5) Dream: 차가운 달빛. (6) Farewell: 촛불빛 + 비녜트. 6개 상태 모두 시각적으로 구분 가능. QA 리드 sign-off 필요

**AC-LDS-08** | `INTEGRATION` | BLOCKING
- **GIVEN** Waiting 상태에서 Dream 트리거, **WHEN** MPC `ColorTemperatureK` Lerp 1.35초 진행 중 씬 관찰, **THEN** (1) 씬 색온도가 2,800K에서 4,200K 방향으로 부드럽게 전환. 인접 프레임 간 `ColorTemperatureK` 변화량 ≤ 100K/frame @60fps — 단일 프레임 즉시 점프 없음. (2) Stillness Budget Background 파티클 슬롯이 선점되어 파티클 스폰 감소 확인

### Rule 5 — 포스트 프로세싱

**AC-LDS-09** | `AUTOMATED` | BLOCKING
- **GIVEN** `PP_MossDusk` 포스트 프로세스 볼륨 설정, **WHEN** MotionBlur 파라미터 확인, **THEN** `r.MotionBlur.Amount = 0.0` 또는 MotionBlur 비활성화 (Rule 5 "MotionBlur 강제 비활성화")

**AC-LDS-10** | `INTEGRATION` | BLOCKING
- **GIVEN** Any→Farewell GSM 전환 트리거, **WHEN** Farewell MPC Lerp 진행 중 Vignette 강도 추적, **THEN** Vignette.Intensity가 0.0에서 `VignetteIntensityFarewell` (기본 0.6)까지 1.5–2.0초에 걸쳐 점진 증가. 즉시 점프 없음 (Rule 5 Farewell 비녜트)

### Rule 6 — PSO Precaching

**AC-LDS-11** | `MANUAL` | ADVISORY
- **GIVEN** 클린 빌드 (PSO 캐시 삭제) 후 앱 최초 실행, **WHEN** Day 1 Dawn 상태 진입 관찰 (화면 녹화), **THEN** 명시적 히치(1초 이상 프레임 정지)가 Dawn 체류 시간(DawnMinDwellSec=3.0초) 내에 발생하지 않음. 히치가 있다면 최대 1–2프레임(33–66ms) 이하. PSO 번들 포함 배포 확인 (Rule 6)

**AC-LDS-12** | `AUTOMATED` | BLOCKING
- **GIVEN** PSO 번들 파일(`PSO_MossDusk.bin`) 없는 상태로 앱 실행, **WHEN** PSO Precaching 초기화 진행, **THEN** 크래시 없이 DegradedFallback 진행 + `UE_LOG(LogLumenDusk, Warning)` 1회 기록 (E5)

### Rule 7 — Stillness Budget

**AC-LDS-13** | `INTEGRATION` | BLOCKING
- **GIVEN** 씬 스폰 + Budget Particle 슬롯 가용, **WHEN** 앰비언트 파티클 스폰 확인, **THEN** 유효한 `FStillnessHandle(Particle, Background)` 보유 + 파티클 스폰 활성 확인

**AC-LDS-14** | `INTEGRATION` | BLOCKING
- **GIVEN** Narrative 이벤트(Dream)로 Particle 슬롯 선점, **WHEN** 씬의 Background Particle Handle 상태 확인, **THEN** `OnPreempted` 수신 + 파티클 스폰 중단. Dream 종료 후 슬롯 복귀 시 파티클 재요청 (E3 Stillness Budget 규칙)

**AC-LDS-15** | `AUTOMATED` | BLOCKING
- **GIVEN** `IsReducedMotion() = true`, **WHEN** 씬 스폰 시 파티클 Request 시도, **THEN** Particle Request 호출 없음 (ReducedMotion 확인 후 조기 종료) (Rule 7)

### Formulas

**AC-LDS-16** | `AUTOMATED` | BLOCKING
- **GIVEN** Formula 1 DoF 계산 (BaseFStop=2.0, DreamFStopDelta=−0.5), **WHEN** DreamBlend=0.0, 0.5, 1.0 입력, **THEN** ApertureFStop = 2.0, 1.75, 1.5 (±0.01). clamp [1.4, 4.0] 범위 내 (F1)

**AC-LDS-17** | `AUTOMATED` | BLOCKING
- **GIVEN** Formula 3 파티클 스폰 밀도 (BaseSpawnRate=5.0, DreamParticleReductionRatio=0.8), **WHEN** DreamBlend=0.0, 0.5, 1.0 + BudgetMultiplier=1.0 입력, **THEN** SpawnRate = 5.0, 3.0, 1.0 (±0.01). BudgetMultiplier=0.0 입력 시 SpawnRate = 0.0 (F3)

### Cross-System

**AC-LDS-18** | `INTEGRATION` | ADVISORY
- **GIVEN** Moss Baby GrowthTransition 발생 (MBC GrowthTransition 상태), **WHEN** 씬 Lumen 업데이트 관찰 (2–3프레임), **THEN** Lumen Surface Cache 재빌드 발생 가능. 씬 렌더링 크래시 없음. 1–2프레임 GI 흐려짐 허용 범위 (Rule 1 메모)

---

| AC 유형 | 수량 | Gate |
|---|---|---|
| AUTOMATED | 6 | BLOCKING |
| CODE_REVIEW | 3 | BLOCKING |
| INTEGRATION | 6 | BLOCKING |
| MANUAL | 2 BLOCKING + 1 ADVISORY | 혼합 |
| **합계** | **18** | — |

---

## Open Questions

| # | Question | Owner | Target |
|---|---|---|---|
| ~~OQ-1~~ | ~~MPC-Light Actor 동기화 아키텍처~~ | — | **RESOLVED** (2026-04-19 by [ADR-0004](../../docs/architecture/adr-0004-mpc-light-actor-sync.md) — Hybrid: GSM이 Light Actor 직접 + Post-process LUT 블렌딩 + MPC 머티리얼 read-only). PSO Precaching 전략은 [ADR-0013](../../docs/architecture/adr-0013-pso-precaching-strategy.md) 별도 ADR로 architecture-level 격상 |
| OQ-2 | **Lumen HWRT vs SWRT 최종 선택** — UE 5.6 Lumen HWRT CPU 병목 개선이 이 프로젝트 씬에서 실제로 충분한지 첫 구현 마일스톤에서 실측 필요. GPU 예산(5ms) 초과 시 SWRT 폴백 자동 전환 여부 확인. | Lumen Dusk Scene 첫 구현 | MVP 첫 구현 마일스톤 |
| OQ-3 | **배경 소품 수와 GPU 비용** — SM_Background_Objects 소품 추가가 Lumen BVH 크기와 Surface Cache에 미치는 영향. MVP에서는 소품 0–1개로 시작 후 GPU 예산 내에서 추가 권장. | 첫 씬 구현 후 | MVP 구현 단계 |
| OQ-4 | **Data Pipeline 텍스처 pool 크기 확정** — entities.yaml의 `Pipeline.MemoryFootprintBytes` 노트에서 "텍스처 pool 크기는 Lumen Dusk Scene GDD #11에서 확정"으로 forward-declared. Lumen Surface Cache 100MB + 씬 텍스처(추정 50–100MB) = 총 150–200MB. Data Pipeline GDD OQ-D-1 해소. | Data Pipeline 담당자에게 전달 | MVP 구현 전 |
| OQ-5 | **Farewell 씬 전용 파티클 VFX** — MBC GDD OQ-5: "Farewell 상태의 골든 파티클(Art Bible §2 상태 5)은 Lumen Dusk Scene 소유". MVP에서는 Farewell 비녜트 증가만 구현. Vertical Slice에서 골든 파티클 Niagara 추가 검토. | Vertical Slice 설계 시 | VS |

---

## Implementation Notes

> 구현 담당 엔지니어에게 — 설계 의도와 주요 결정을 요약합니다.

1. **단방향 MPC 의존**: 씬 머티리얼이 MPC를 읽고, GSM이 MPC를 씁니다. 씬 코드에서 MPC 파라미터에 Set을 호출하지 마세요. `FOnGameStateChanged` 구독은 Farewell 비녜트 및 Dream DoF 같은 씬 고유 효과를 처리하는 데만 사용합니다.

2. **PSO Precaching 소유**: 이 시스템이 PSO Precaching을 소유합니다. `UGameInstanceSubsystem::Initialize()`에서 비동기 PSO 번들 로드를 시작하고, 씬 스폰 전 완료를 대기합니다(타임아웃 포함). 번들 파일은 빌드 파이프라인에서 사전 생성해야 합니다.

3. ~~GSM OQ-6 ADR 필수 선행~~: **RESOLVED 2026-04-19 by [ADR-0004](../../docs/architecture/adr-0004-mpc-light-actor-sync.md)** — Hybrid 채택. GSM이 `RegisterSceneLights(KeyLight, SkyLight)` 호출 받아 TickMPC에서 MPC + Light Actor 직접 구동. Lumen Dusk Scene Actor는 Light 핸들 등록 + Post-process LUT/Vignette/DoF 소유. 구현 가능.

4. **Lumen GPU 프로파일링 게이트**: 첫 씬 구현 완료 후 `stat GPU`, `stat Lumen`으로 모든 6개 GSM 상태에서 GPU 시간을 측정하세요. Lumen HWRT 5ms 초과 시 즉시 이 GDD와 설정을 함께 검토합니다.

5. **Stillness Budget RAII**: 씬 소멸자(`BeginDestroy`)에서 보유한 모든 `FStillnessHandle`을 Release하세요. 씬이 비정상 종료되어도 Budget 슬롯이 누수되지 않아야 합니다.
