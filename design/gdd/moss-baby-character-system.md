# Moss Baby Character System

> **Status**: R2 수정 완료 — `/design-review` 재실행 대기
> **Author**: bong9tutor + claude-code session
> **Last Updated**: 2026-04-17 (R2 — design-review 4 BLOCKER + 6 RECOMMENDED 적용)
> **Implements Pillar**: Pillar 2 (선물의 시학 — 숫자 없는 시각적 성장), Pillar 1 (조용한 존재 — 절제된 반응), Pillar 4 (끝이 있다 — 최종 형태의 완성)
> **Priority**: MVP | **Layer**: Core | **Category**: Gameplay
> **Effort**: M (2–3 세션)

---

## Overview

Moss Baby Character System은 유리병 속 이끼 영혼 — 이 게임의 유일한 캐릭터 — 의 **시각적 존재와 반응**을 책임진다. 플레이어가 매일 앱을 열면 가장 먼저 보는 것이 이끼 아이이고, 21일 동안 건넨 카드의 흔적이 이 존재의 몸에 새겨지는 것을 지켜보는 것이 게임의 핵심 경험이다.

이 시스템의 책임은 세 가지로 나뉜다. **(1) 성장 단계별 시각 표현** — 3개 성장 단계(Sprout, Growing, Mature)와 최종 형태(MVP 1종, Full 8–12종)를 정적 메시 교체 + 머티리얼 파라미터로 구현한다. **(2) 실시간 반응** — 카드를 건넬 때의 짧은 빛·색 변화, 아이들 상태의 느린 브리딩(4–6초 주기), 꿈 도착 시의 미세한 발광 반응. **(3) 상태 기반 외관** — "고요한 대기"(플레이어 장기 부재 시 이끼 아이가 조용히 쉬는 상태 → 첫 카드로 깨어남)와 성장에 따른 점진적 실루엣 변화.

MVP에서 이끼 아이는 **정적 메시 + Material Instance Dynamic**으로 구현된다 — 블렌드 셰이프와 스켈레탈 리깅은 명시적으로 MVP 범위 밖이다. 모든 시각 변화는 머티리얼 파라미터(Subsurface 강도, 색조, 발광, 러프니스)와 메시 교체로 달성한다. 이 선택은 "이끼는 근육이 아니라 표면으로 변한다"는 Art Bible의 유기물 철학과 일치한다.

Growth Accumulation Engine(#7)이 태그 벡터를 기반으로 "어떤 성장 단계인지, 어떤 최종 형태인지"를 결정하면, 이 시스템은 그 결정을 **플레이어가 볼 수 있는 시각적 변화로 번역**한다. 번역기이지 결정자가 아니다.

## Player Fantasy

이끼 아이는 당신을 필요로 하지 않는다. 재촉하지 않고, 보상하지 않고, 떠나도 탓하지 않는다. 다만 돌아오면 거기 있다 — 어제보다 조금 다른 윤곽으로, 같은 느린 숨결로. 이 존재의 존재감은 당신에게 기대는 것이 아니라, 당신이 기댈 수 있는 고요를 만드는 것이다.

그리고 그 고요 안에서, 당신이 건넨 것들이 이 존재의 표면에 스며든다. 봄의 카드를 건네면 부드러운 곡면이 되고, 어둠의 카드를 건네면 깊어지는 음각이 된다. 이끼 아이는 당신의 선택을 기억하지 않는다 — 선택이 이끼 아이가 **된다**. 어떤 최종 형태도 정답이 아니다. 다만 21일 뒤, 거기 남은 실루엣은 당신이 그 시간 동안 무엇을 주었는지의 유일한 증거다.

**앵커 모먼트**: Day 21 — 최종 형태가 드러나는 순간. 플레이어는 "그때 그 카드를 골랐으니까"라고 혼자 중얼거린다. 정답도 아니고 오답도 아닌, 돌이킬 수 없는 결과. 그것이 오직 내 이끼인 이유다.

**Pillar 연결**:
- **Pillar 1 (조용한 존재)** — 이끼 아이의 반응은 언제나 절제된다. 카드를 건네면 빛이 한 번 따뜻해지고, 그것으로 끝이다. 과잉 반응·감사 대사·하트 이펙트 모두 금지
- **Pillar 2 (선물의 시학)** — 성장 단계와 최종 형태에 숫자가 없다. 스프라우트에서 매추어로의 전환은 "+5 성장"이 아니라 "윤곽이 솟았다". 플레이어는 계산하지 않고 지켜본다
- **Pillar 4 (끝이 있다)** — 최종 형태는 한 번뿐이다. 21일이 끝나면 이끼 아이는 완성되고, 그 형태는 바꿀 수 없다. 이 일회성이 형태에 무게를 준다

**MVP 범위 주석**: MVP에서 최종 형태는 1종이므로, "선택이 형태를 결정한다"는 앵커 모먼트는 완전 실현되지 않는다. 대신 MVP에서는 **머티리얼 파라미터 변주**(카드 선택 이력에 따른 HueShift·SSS 미세 편차)로 "같은 메시라도 내 이끼는 다르다"는 감각을 부분 달성한다. 앵커 모먼트의 완전한 실현은 Full 스코프의 8–12종 최종 형태에서 달성된다. Growth Accumulation Engine(#7) GDD에서 MVP 머티리얼 변주의 구체적 매핑을 정의한다.

## Detailed Design

### Core Rules

#### CR-1 — 성장 단계 시각 매핑

| 단계 | 기간 | 메시 에셋 | 트리거 |
|---|---|---|---|
| **Sprout** | Day 1–5 | `SM_MossBaby_Sprout` | 초기 상태 (Day 1) |
| **Growing** | Day 6–14 | `SM_MossBaby_Growing` | Growth Engine `FOnGrowthStageChanged(Growing)` |
| **Mature** | Day 15–20 | `SM_MossBaby_Mature` | Growth Engine `FOnGrowthStageChanged(Mature)` |
| **Final Form** | Day 21 | `SM_MossBaby_Final_[ID]` | Growth Engine `FOnFinalFormReached(FinalFormID)` |

메시 교체는 **즉시 요청** (CPU 측 `SetStaticMesh()` 즉시 반환, 시각 반영은 다음 렌더 프레임. Lumen Surface Cache 재빌드로 1–2프레임 GI 흐려짐 가능 — 소형 씬에서 허용 범위). 교체 직후 머티리얼 파라미터가 신규 프리셋 목표값으로 **1–2초 Lerp** 이행. MVP 최종 형태 1종, Full 8–12종.

#### CR-2 — 머티리얼 파라미터 세트

이끼 아이의 모든 시각 변화는 `UMaterialInstanceDynamic`의 6개 파라미터로 구동:

| 파라미터 | 타입 | 범위 | 역할 |
|---|---|---|---|
| `SSS_Intensity` | float | [0.0, 1.0] | 서브서피스 산란 강도. 성장할수록 증가, 고요한 대기 시 감소 |
| `HueShift` | float | [-0.1, 0.1] | 색조 오프셋. Sprout 황록 → Mature 심록 |
| `EmissionStrength` | float | [0.0, 0.3] | 자체 발광. 카드 반응·꿈 알림·브리딩에 사용 |
| `Roughness` | float | [0.3, 0.9] | 표면 거칠기. 고요한 대기 시 0.75(미세 서리) ↔ Mature 0.35(촉촉) |
| `DryingFactor` | float | [0.0, 1.0] | 고요한 대기 정도. 0=활성, 1=깊은 쉼. Formula 5로 다른 파라미터에 선형 보간 |
| `AmbientMoodBlend` | float | [0.0, 1.0] | GSM MPC 앰비언트를 머티리얼로 반영하는 블렌드 계수 |

**단계별 프리셋 기본값** (DataAsset `UMossBabyPresetAsset` 외부화):

| 파라미터 | Sprout | Growing | Mature | Final | Drying=1 |
|---|---|---|---|---|---|
| SSS_Intensity | 0.35 | 0.55 | 0.75 | 0.80 | 0.25 (흐려짐) |
| HueShift | +0.04 | 0.00 | -0.04 | 형태별 | -0.02 (차가운 톤) |
| EmissionStrength | 0.02 | 0.03 | 0.04 | 0.05 | 0.005 (빛 감소) |
| Roughness | 0.65 | 0.50 | 0.35 | 0.30 | 0.75 (미세 서리) |
| AmbientMoodBlend | 0.3 | 0.5 | 0.7 | 0.8 | 0.1 |

`DryingFactor`는 독립 레이어 — 어느 성장 단계든 "고요한 대기"가 덮어씌워진다. 보간 방식은 Formula 5 참조. 시각적으로 이끼 아이가 "잠들어 쉬는 중"이며 탓할 대상 없음 (Pillar 1: 죄책감 유발 금지). 서리 낀 듯한 차가운 톤·빛 감소로 표현하되, 황변·탈색·건조화와 같은 훼손 표현은 금지.

#### CR-3 — 카드 반응 (Reacting)

카드 건넴(`FOnCardOffered`) 시 단 한 번, 절제된 반응:
- `EmissionStrength` +0.15 → 지수 감쇠로 기본값 복귀 (τ=0.1s, Formula 4 참조)
- `SSS_Intensity` +0.08 → 지수 감쇠로 기본값 복귀 (τ=0.33s, Formula 4 참조)
- Stillness Budget `Motion` 채널 `Standard` 우선순위 Request. Deny 시 반응 건너뜀
- 반응 중 추가 카드 반응 불가 (핸들 유지 중 중복 Request Deny — 자연 쿨다운)
- 카드 종류와 무관하게 동일 반응 (MVP). 카드별 차등 반응은 VS 검토

#### CR-4 — 브리딩 (Idle 호흡)

`EmissionStrength`를 4–6초 주기 사인파로 ±0.02 진동. **Idle 상태에서만** 활성. Reacting·GrowthTransition 상태에서 일시 중단. ReducedMotion ON 시 즉시 중단.

#### CR-5 — 고요한 대기 상태 (Quiet Rest)

플레이어가 며칠 오지 않으면 이끼 아이는 **조용히 쉬는 상태**로 전환된다. 이것은 방치의 결과가 아니라 이끼 아이의 자연스러운 리듬이다 — 돌보는 이가 없는 날에도 이끼 아이는 탓하지 않고, 그저 고요 속에 머문다 (Pillar 1: "떠나도 탓하지 않는다"). DayIndex가 마지막 카드 건넴 이후 2일 이상 전진하면 `DryingFactor`가 0에서 비례 증가 (Formula 2 참조). 시각적으로는 서리 낀 듯 차가운 톤·빛 감소로 표현되며, 황변·탈색·건조화와 같은 훼손 표현은 금지. 첫 카드 건넴(`FOnCardOffered`) 수신 시 `DryingFactor`를 **즉시 0으로** 복귀 — 이끼 아이가 깨어난다.

**Pillar 1 설계 근거**: 이 상태의 시각 표현은 "내가 방치해서 이렇게 됐다"가 아닌 "잠들어 있었구나"를 유도해야 한다. 디자인 테스트: 복귀한 플레이어의 첫 반응이 "미안"이면 실패, "오래 잤네"이면 성공.

### States and Transitions

| 상태 | 설명 | 진입 트리거 | 이탈 트리거 |
|---|---|---|---|
| **Idle** | 느린 브리딩. 기본 상태 | 기본 / 다른 상태 완료 | 아래 각 트리거 |
| **Reacting** | 카드 반응 지수 감쇠 (≈1.65s, Formula 4) | `FOnCardOffered` | 5τ_sss(≈1.65s) 경과 → Idle |
| **GrowthTransition** | 메시 교체 + 머티리얼 Lerp (1–2s) | `FOnGrowthStageChanged` | Lerp 완료 → Idle |
| **QuietRest** | DryingFactor 상승. 서리·빛 감소·차가운 톤 (고요한 쉼) | DayGap ≥ 2 (앱 시작 시 감지) | `FOnCardOffered` → 즉시 Idle |
| **FinalReveal** | 최종 형태 메시 교체 + 느린 Lerp (2–3s) | `FOnFinalFormReached(ID)` | Lerp 완료 → 영구 Idle |

**우선순위**: FinalReveal > GrowthTransition > Reacting > QuietRest/Idle.
Reacting 중 GrowthTransition 도착 시 Reacting 즉시 중단 → GrowthTransition 진입.
**감정 설계 근거**: 이 우선순위는 "더 큰 변화가 더 작은 변화를 흡수한다"는 원칙을 따른다. 최종 형태 공개(FinalReveal)는 21일의 절정이므로 모든 것에 우선하고, 성장 전환(GrowthTransition)은 수일에 걸친 누적의 결과이므로 순간적 카드 반응(Reacting)보다 앞선다. 단, 마지막 카드 건넴 직후 성장 전환이 연속 발생하는 경우(E3) Reacting이 0.3s 미만으로 잘릴 수 있다 — 이 순간의 감정 손실은 허용하되, 카드 수락 확인은 Card Hand UI가 별도 제공한다.

### Interactions with Other Systems

#### 1. Growth Accumulation Engine (#7) — 인바운드

| 이벤트 | 데이터 | Moss Baby 반응 |
|---|---|---|
| `FOnGrowthStageChanged` | `EGrowthStage NewStage` | GrowthTransition: 메시 교체 + 머티리얼 Lerp |
| `FOnFinalFormReached` | `FName FinalFormID` | FinalReveal: 최종 메시 교체 + 느린 Lerp |

Growth Engine이 결정, Moss Baby가 시각화. 역방향 의존 없음.

#### 2. Card System (#8) — 인바운드

| 이벤트 | 데이터 | 반응 |
|---|---|---|
| `FOnCardOffered` | `FName CardId` | Reacting (Emission 지수 감쇠 + SSS 지수 감쇠, Formula 4) + QuietRest 즉시 복구 |

#### 3. Game State Machine (#5) — 인바운드

| GSM 상태 | Moss Baby 반응 |
|---|---|
| `Waiting` | 브리딩 재개 |
| `Offering` | 브리딩 일시 중단 (반응 대기) |
| `Dream` | EmissionStrength +0.05 미세 증가 (꿈 알림 발광) |
| `Farewell` | EmissionStrength 서서히 0 → "마지막 숨" |

MPC는 읽기 전용 — GSM이 소유, Moss Baby 머티리얼이 참조.

#### 4. Stillness Budget (#10) — 아웃바운드

| 상황 | Channel | Priority |
|---|---|---|
| Reacting (카드 반응) | Motion | Standard |
| GrowthTransition | Motion | Narrative |
| FinalReveal | Motion | Narrative |
| QuietRest 복구 (깨어남) | Motion | Standard |

## Formulas

### Formula 1 — 머티리얼 파라미터 Lerp (MBC.MaterialLerp)

성장 단계 전환·Drying 복구 시 머티리얼 파라미터 보간.

`V(t) = V_current + (V_target − V_current) × ease(t / D_transition)`

`ease(x) = x²(3 − 2x)` [SmoothStep — GSM Formula 1과 동일]

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| V_current | V₀ | float | 파라미터 의존 | 전환 시작 시점의 현재 값 |
| V_target | V₁ | float | 파라미터 의존 | 목표 프리셋 값 (CR-2 테이블) |
| D_transition | D | float (sec) | [0.0, 3.0] | 전환 지속 시간. GrowthTransition=1.5s, FinalReveal=2.5s |
| t | t | float (sec) | [0, D] | 경과 시간 |

**D=0 예외 (ReducedMotion)**: D_transition=0이면 Formula를 호출하지 않고 V_target을 즉시 적용한다. `ease(t/0)` 나눗셈을 회피.

**t 클램프 (오버런 방어)**: 호출 시 `t = min(t, D_transition)` 적용 필수. SmoothStep `ease(x)`는 x ∈ [0, 1] 밖에서 단조 보장 없음 — x > 1.5이면 ease 음수, x = 2이면 ease = −4로 파라미터가 정상 범위 밖으로 발산. 프레임 스파이크로 t가 D를 초과할 수 있으므로 방어 필수.

**Output Range**: [min(V₀, V₁), max(V₀, V₁)]
**Example**: Sprout→Growing, SSS_Intensity: V₀=0.35, V₁=0.55, D=1.5s, t=0.75s → ease(0.5)=0.5 → V = 0.35 + 0.20 × 0.5 = **0.45**

### Formula 2 — DryingFactor 계산 (MBC.DryingFactor)

`DryingFactor = clamp((float(DayGap) − float(DryingThresholdDays)) / float(DryingFullDays), 0.0, 1.0)`

⚠️ **구현 주의**: 모든 피연산자를 **반드시 float로 캐스팅** 후 나눗셈. C++ 정수 나눗셈 시 DayGap 3–6 구간이 전부 0을 반환하여 점진적 변화가 사라짐.

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| DayGap | G | int | [0, 21] | 마지막 카드 건넴 이후 경과 일수 |
| DryingThresholdDays | T | int | [1, 5] | 고요한 대기 시작 임계. 기본 2 |
| DryingFullDays | F | int | [3, 10] | 깊은 쉼까지 추가 일수. 기본 5 |

**Output Range**: [0.0, 1.0]. 0=활성, 1=깊은 쉼
**Example**: T=2, F=5, DayGap=4 → DryingFactor = clamp(float(4−2)/float(5), 0, 1) = **0.4**
**Boundary verification**: G=0→0.0, G=2(=T)→0.0, G=3→0.2, G=4→0.4, G=6→0.8, G=7(=T+F)→1.0, G=100→clamp→1.0

### Formula 3 — 브리딩 사인파 (MBC.BreathingCycle)

`E_breath(t) = E_base + A_breath × sin(2π × t / P_breath)`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| E_base | E₀ | float | [0.02, 0.05] | 현재 성장 단계의 기본 EmissionStrength |
| A_breath | A | float | [0.01, 0.02] | 호흡 진폭. 기본 0.02. **반드시 A ≤ E₀** 유지 (Sprout E₀=0.02이므로 상한 0.02). DataAsset `ClampMax` 적용 권장 |
| P_breath | P | float (sec) | [4.0, 6.0] | 호흡 주기. 기본 5.0 |

**Output Range**: [max(0, E₀ − A), E₀ + A] — 출력은 항상 ≥ 0으로 clamp
**Example**: E₀=0.03, A=0.02, P=5.0, t=1.25s → sin(π/2)=1 → E = **0.05**

### Formula 4 — 카드 반응 지수 감쇠 (MBC.CardReactionDecay)

카드 건넴 시 Emission·SSS 순간 부스트 후 기본값으로 자연 감쇠.

`V(t) = V_base + Delta × e^(−t / τ)`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| V_base | V₀ | float | 파라미터 의존 | 현재 성장 단계의 기본값 |
| Delta | Δ | float | Emission: 0.15, SSS: 0.08 | 순간 부스트 크기 (Tuning Knob) |
| τ | τ | float (sec) | Emission: 0.1, SSS: 0.33 | 시간 상수. 3τ 시점에 부스트의 95% 감쇠 |
| t | t | float (sec) | [0, ∞) | 반응 시작 이후 경과 시간 |

**Output Range**: [V₀, V₀ + Δ]
**Practical cutoff**: t ≥ 5τ 시점에 V(t) ≈ V₀ (오차 < 0.7%). Emission: 0.5s, SSS: 1.65s에서 실질 복귀. Reacting 상태 지속 시간은 max(5τ_emission, 5τ_sss) ≈ 1.65s를 기준으로 종료 판정 (기존 0.3s에서 조정).
**Example**: Emission — V₀=0.03, Δ=0.15, τ=0.1, t=0.3s → V = 0.03 + 0.15 × e^(−3) ≈ 0.03 + 0.0075 ≈ **0.0375** (95% 감쇠)

**QuietRest→Reacting 전환 순서**: QuietRest 상태에서 카드 건넴 시 (1) DryingFactor 즉시 0 리셋 (CR-5) → (2) V_base = **DryingFactor 리셋 후의 성장 단계 프리셋 값** (CR-2 테이블) → (3) Formula 4 적용. DryingOverlay 적용값(Formula 5)이 아닌 순수 단계 프리셋이 기준.

### Formula 5 — DryingFactor 오버레이 (MBC.DryingOverlay)

DryingFactor가 0 < DF < 1일 때 각 머티리얼 파라미터의 유효값을 결정.

`V_effective = V_stage + (V_quiet − V_stage) × DryingFactor`

선형 보간: DryingFactor=0이면 성장 단계 프리셋, DryingFactor=1이면 QuietRest 컬럼(CR-2).

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| V_stage | Vs | float | CR-2 테이블 | 현재 성장 단계의 프리셋 값 |
| V_quiet | Vq | float | CR-2 Drying=1 컬럼 | 고요한 대기 목표값 |
| DryingFactor | DF | float | [0, 1] | Formula 2 출력 |

**Output Range**: [min(Vs, Vq), max(Vs, Vq)]
**Example**: Mature SSS, DF=0.5 → V = 0.75 + (0.25 − 0.75) × 0.5 = 0.75 − 0.25 = **0.50**
**HueShift 안전성**: Mature(−0.04) + QuietRest(−0.02) → DF=0.5 시 V = −0.04 + (−0.02 − (−0.04)) × 0.5 = −0.04 + 0.01 = **−0.03**. 심록에서 약간 차가운 방향으로 이동 — 역전 없이 자연스러운 변화.

## Edge Cases

- **E1 — 성장 단계 다중 건너뛰기**: 3일 부재로 Sprout→Mature 직행 시 중간 Growing 재생 안 함. 최종 단계 메시 즉시 교체 + 머티리얼 Lerp 1회. Pillar 1: "놓친 날을 강제 관람시키지 않는다."

- **E2 — GrowthTransition 중 FinalReveal 도착**: FinalReveal 우선. GrowthTransition 즉시 중단 → FinalReveal 진입. 머티리얼은 중간값에서 Final 목표로 리타겟.

- **E3 — Reacting 중 GrowthTransition 도착**: Reacting 즉시 중단, Budget Release → GrowthTransition 진입.

- **E4 — QuietRest 상태에서 카드 건넴 + 동시 성장 전환**: `FOnCardOffered`로 QuietRest 즉시 해제 → Reacting → Reacting 완료 후 GrowthTransition 처리.

- **E5 — DayGap > 21 (DryingFactor 계산)**: clamp로 1.0 상한. LONG_GAP_SILENT로 Farewell이 선��하므로 QuietRest 시각 불필요 — Farewell이 우선.

- **E6 — ReducedMotion ON 상태에서 GrowthTransition**: Formula 1의 D=0 예외 규칙 적용 — V_target 즉시 설정, Lerp 호출 없음. 메시 교체는 영향 없음.

- **E7 — Stillness Budget 전체 Deny에서 Reacting**: 카드 반응 건너뜀. Growth 누적은 정상 (Growth Engine 책임). Card Hand UI에서 수락 확인 별도 제공.

- **E8 — FinalReveal 메시 로드 실패**: Mature 메시 유지 + 진단 로그. 에러 표시 금지 (Pillar 1). 머티리얼은 Mature 기반으로 Final 프리셋 적용.

- **E9 — 브리딩 + Reacting EmissionStrength 충돌**: Reacting이 브리딩을 일시 중단. 완료 후 현재 사인 위상에서 재개. 충돌 없음.

## Dependencies

| System | 방향 | 성격 | 인터페이스 | MVP/VS |
|---|---|---|---|---|
| **Data Pipeline (#2)** | 인바운드 | **Hard** | `FFinalFormRow` (메시·머티리얼 경로 조회) | MVP |
| **Growth Accumulation (#7)** | 인바운드 | **Hard** | `FOnGrowthStageChanged`, `FOnFinalFormReached`, `GetMaterialHints()`, `GetLastCardDay()` | MVP |
| **Card System (#8)** | 인바운드 | **Hard** | `FOnCardOffered(FName CardId)` | MVP |
| **Game State Machine (#5)** | 인바운드 | **Soft** | `FOnGameStateChanged(EGameState OldState, EGameState NewState)` + MPC 읽기 | MVP |
| **Stillness Budget (#10)** | 아웃바운드 | **Soft** | `Request(Motion, Standard/Narrative)` | MVP |
| **Save/Load (#3)** | 인바운드 | **Soft** | 앱 시작 시 DayGap 계산용 DayIndex 읽기 (LastCardDay는 Growth #7 경유) | MVP |

**양방향 확인**: Growth Engine (#7) GDD 작성 완료 (2026-04-17) — `FOnGrowthStageChanged`, `FOnFinalFormReached`, `GetMaterialHints()`, `GetLastCardDay()` 인터페이스 확정. MBC OQ-1·OQ-4 해소됨.

## Tuning Knobs

| Knob | 타입 | 기본값 | 안전 범위 | 영향 |
|---|---|---|---|---|
| `GrowthTransitionDurationSec` | float | 1.5 | [1.0, 3.0] | 성장 단계 머티리얼 Lerp 시간 |
| `FinalRevealDurationSec` | float | 2.5 | [2.0, 4.0] | 최종 형태 Lerp. 너무 길면 Pillar 1 위반 |
| `CardReactionEmissionBoost` | float | 0.15 | [0.05, 0.25] | 카드 반응 발광 증분 |
| `CardReactionTauEmission` | float | 0.1 | [0.05, 0.2] | Emission 지수 감쇠 시간 상수 τ (Formula 4) |
| `CardReactionTauSSS` | float | 0.33 | [0.15, 0.5] | SSS 지수 감쇠 시간 상수 τ (Formula 4) |
| `BreathingAmplitude` | float | 0.02 | [0.01, 0.02] | 호흡 진폭. A ≤ E_base(Sprout 0.02) 유지 필수. 안전 범위 상한 = Sprout E_base (0.02) |
| `BreathingPeriodSec` | float | 5.0 | [4.0, 6.0] | 호흡 주기 |
| `DryingThresholdDays` | int | 2 | [2, 5] | 고요한 대기 시작 임계 일수. T=1이면 하루만 비워도 발동 — Pillar 1 위반 위험이므로 최소 2 |
| `DryingFullDays` | int | 5 | [3, 10] | 깊은 쉼까지 추가 일수 |
| 프리셋 파라미터 (CR-2) | float × 6 × 5 | 표 참조 | 표 참조 | 성장 단계별 머티리얼 목표값 |

**외부화**: 모든 Knob은 `UMossBabyPresetAsset : UPrimaryDataAsset`에 `UPROPERTY(EditAnywhere)` 필드로 노출.

## Visual/Audio Requirements

### Visual (REQUIRED — Character System)

**Art Bible §3 준수 사항**:
- **실루엣**: 둥근 상부, 뭉툭한 하부, 비대칭 유기적 균형. 성장 단계별 실루엣 변화 (Day 1–5 원형 → Day 6–14 위로 뻗음 → Day 15–20 기울어짐·특징 강화)
- **최소 크기**: 화면 높이의 12% 이상 유지. 32×32px 아이콘에서도 읽혀야 함
- **표면**: Subsurface Scattering 필수. 이끼·유기물 표면만. 금속·플라스틱 금지
- **색상**: 이끼 녹 코어 팔레트 ���. 고요한 대기(QuietRest) 시 서리 낀 듯 차가운 톤(HueShift −0.02), 빛 감소. 황변·탈색·건조화 표현 금지 (Pillar 1)

**성장 단계별 VFX**:
- **GrowthTransition**: 머티리얼 Lerp만 (1.5s). Niagara 파티클 없음 (MVP). VS에서 미세 포자 이펙트 검토
- **FinalReveal**: 머티리얼 Lerp (2.5s). Farewell 상태의 골든 파티클(Art Bible §2 상태 5)은 Lumen Dusk Scene(#11) 소유
- **카드 반응**: Emission 플래시 0.3s. 하트·별·파티클 폭발 모두 금지 (Pillar 1)

**머티리얼 구성 (MVP)**:
- Static Mesh + Material Instance Dynamic
- 마스터 머티리얼 1개: SSS, HueShift, Emission, Roughness, DryingFactor, AmbientMoodBlend 6개 파라미터 노출
- GSM MPC를 머티리얼 내부에서 참조 (앰비언트 무드 자동 반영)

### Audio

이 시스템은 오디오를 직접 생성하지 않음. Audio System(#15, VS)이 상태별 SFX를 담당. MVP에서 Moss Baby 관련 사운드 없음.

> **📌 Asset Spec** — Visual requirements가 정의되었습니다. Art Bible 승인 후 `/asset-spec system:moss-baby-character-system`을 실행해 성장 단계별 메시·머티리얼 스펙을 생성할 수 있습니다.

## UI Requirements

Moss Baby Character System은 UI 위젯을 생성하지 않는다. 이끼 아이는 3D 씬 내 Actor이며, UI는 Card Hand UI(#12), Dream Journal UI(#13) 등 별도 시스템이 담당한다.

## Acceptance Criteria

### Growth Stage & Mesh

**AC-MBC-01** | `AUTOMATED` | BLOCKING
- **GIVEN** Sprout 상태에서 GrowthTransition 이벤트 발생, **WHEN** 메시 교체 수행, **THEN** StaticMesh 레퍼런스가 Growing 메시로 교체되고 이전 메시 참조 해제 (CR-1)

**AC-MBC-02** | `INTEGRATION` | BLOCKING
- **GIVEN** Sprout에서 Mature로 다단계 스킵, **WHEN** 성장 단계 2 이상 건너뛰기, **THEN** StaticMesh 레퍼런스가 Growing을 중간값으로 경유하지 않고 Mature로 직행 + MaterialLerp 호출 횟수 = 1 (상태 쿼리 검증, 렌더링 불필요) (E1)

### Material Parameters

**AC-MBC-03** | `CODE_REVIEW` | BLOCKING
- **GIVEN** 6개 머티리얼 파라미터 정의, **WHEN** Source/ 하위 *.h, *.cpp grep으로 (1) SSS_Intensity, HueShift, EmissionStrength, Roughness, DryingFactor, AmbientMoodBlend 리터럴 float 초기화(= 0.xx 등) 검색, (2) UMossBabyPresetAsset 외부에서 SetScalarParameterValue 직접 호출 검색, **THEN** 위 패턴 0건 (CR-2)

### Card Reaction

**AC-MBC-04** | `AUTOMATED` | BLOCKING
- **GIVEN** Reacting 상태 진입 (Delta_E=0.15, τ_E=0.1), **WHEN** Formula 4 순수 함수에 t=0, 0.3, 0.5, 1.65 입력, **THEN** Emission 출력이 각각 V₀+0.15, V₀+0.0075(±0.001), V₀+0.0011(±0.001), V₀(±0.001). 렌더링 불필요 — 내부 float 값 검증 (F4)

**AC-MBC-05** | `INTEGRATION` | BLOCKING
- **GIVEN** Budget Deny 상태, **WHEN** 카드 반응 트리거 시도, **THEN** Emission·SSS 불변, 반응 타이머 미시작 (E7)

**AC-MBC-17** | `AUTOMATED` | BLOCKING
- **GIVEN** Reacting 상태 활성 중 (t=0.5s, Emission 감쇠 진행 중), **WHEN** 두 번째 `FOnCardOffered` 수신, **THEN** EmissionStrength 재부스트 없음 (V₀+Δ로 리셋되지 않음) + 첫 감쇠 곡선 유지. Stillness Budget 핸들 중복 Request Deny 확인 (CR-3)

### Breathing

**AC-MBC-06** | `AUTOMATED` | BLOCKING
- **GIVEN** E_base=0.03, A=0.02, P=5.0 (고정 입력), **WHEN** Formula 3 순수 함수에 t=0.0, 1.25, 2.5, 3.75, 5.0 입력, **THEN** 출력값이 각각 0.03, 0.05, 0.03, 0.01, 0.03 (오차 ±0.001). 실시간 경과 불필요 — 결정론적 수학 검증 (F3)

**AC-MBC-07** | `CODE_REVIEW` | BLOCKING
- **GIVEN** 호흡 사인파 적용 코드 (EmissionStrength 사인파 계산 함수 호출처) grep, **WHEN** 상태 분기 패턴 확인, **THEN** EMossCharacterState::Idle 분기 내부에서만 호출. Reacting/GrowthTransition/FinalReveal/QuietRest 분기 내 호출 0건 (CR-4)

### Drying

**AC-MBC-08** | `AUTOMATED` | BLOCKING
- **GIVEN** Formula 2 (DryingFactor, T=2, F=5), **WHEN** DayGap=0, 2, 3, 4, 7, 100 입력, **THEN** 출력 0.0, 0.0, 0.2, 0.4, 1.0, 1.0 (F2, 정수 나눗셈 방지 float 캐스팅 검증 포함)

**AC-MBC-09** | `INTEGRATION` | BLOCKING
- **GIVEN** DryingFactor > 0, **WHEN** 첫 카드 건넴(`FOnCardOffered` 이벤트 수신 — Card System 연동), **THEN** DryingFactor 즉시 0.0 리셋 확인 (CR-5)

**AC-MBC-16** | `AUTOMATED` | BLOCKING
- **GIVEN** Formula 5 (DryingOverlay), Mature 단계 프리셋 SSS_Intensity=0.75, QuietRest SSS=0.25, **WHEN** DryingFactor=0.0, 0.5, 1.0 입력, **THEN** V_effective = 0.75, 0.50, 0.25 (±0.001). HueShift 검증: Mature(−0.04), QuietRest(−0.02), DF=0.5 → V_effective = −0.03 (±0.001) (F5)

### State Priority

**AC-MBC-10** | `INTEGRATION` | BLOCKING
- **GIVEN** GrowthTransition 중 FinalReveal 도착, **WHEN** 상태 머신 처리, **THEN** GrowthTransition 대기 없이 즉시 FinalReveal 진입, 중간 메시 미렌더링 (E2)

### ReducedMotion

**AC-MBC-11** | `AUTOMATED` | BLOCKING
- **GIVEN** ReducedMotion ON + 성장 전환 트리거, **WHEN** MaterialLerp 호출, **THEN** duration=0, 목표값 즉시 도달 (E6)

### State Transition Interrupts

**AC-MBC-13** | `INTEGRATION` | BLOCKING
- **GIVEN** Reacting 상태 활성 중 GrowthTransition 이벤트 도착, **WHEN** 상태 머신 처리, **THEN** Reacting 즉시 중단 + Stillness Budget Release 확인 + GrowthTransition 진입. Reacting의 잔여 Emission/SSS 감쇠는 중단됨 (E3)

**AC-MBC-14** | `INTEGRATION` | BLOCKING
- **GIVEN** QuietRest 상태(DryingFactor > 0) + FOnCardOffered + 동시 FOnGrowthStageChanged, **WHEN** 순서대로 처리, **THEN** DryingFactor 즉시 0 리셋 → Reacting 진입 → Reacting 완료 후 GrowthTransition 진입. QuietRest 시각이 잔존하지 않음 (E4)

**AC-MBC-15** | `AUTOMATED` | BLOCKING
- **GIVEN** Idle 브리딩 활성 중 (t_interrupted 시점 기록) FOnCardOffered 트리거, **WHEN** Reacting 진입, **THEN** (1) 브리딩 사인파 즉시 중단 — EmissionStrength가 사인파 계산 없이 고정. (2) Reacting 완료 판정을 타이머 주입으로 강제 트리거 후 Idle 복귀 시, EmissionStrength = E_base + A × sin(2π × t_interrupted / P) (±0.001) — 중단 시점 위상에서 연속 재개 확인. 실시간 경과 의존 없음, 결정론적 검증 (E9)

### Fallback

**AC-MBC-12** | `MANUAL` | ADVISORY
- **GIVEN** Final 메시 로드 실패 시뮬레이션, **WHEN** FinalReveal 진입 시도, **THEN** 크래시 없이 Mature 메시 폴백 + 경고 로그 (E8)

---

| AC 타입 | 수량 | Gate |
|---|---|---|
| AUTOMATED | 8 | BLOCKING |
| CODE_REVIEW | 2 | BLOCKING |
| INTEGRATION | 6 | BLOCKING |
| MANUAL | 1 | ADVISORY |
| **합계** | **17** | — |

## Open Questions

| # | Question | Owner | Target |
|---|---|---|---|
| OQ-1 | Growth Engine(#7)의 `FOnGrowthStageChanged`·`FOnFinalFormReached` 인터페이스 확정 — 현재 provisional | Growth Accumulation GDD (#7) | Growth GDD 작성 시 |
| OQ-2 | 카드별 차등 반응 (카드 태그에 따라 Emission 색상·SSS 변화량 다름) — MVP 단일 반응, VS 검토 | Card System GDD (#8) | Vertical Slice |
| OQ-3 | Blend Shape / Morph Target 도입 시점 — MVP 이후 언제, 어떤 성장 전환에 적용? | 아키텍처 단계 | VS 또는 Full |
| ~~OQ-4~~ | ~~`LastCardDay` 필드 위치~~ | **RESOLVED** (Growth GDD, 2026-04-17): `FGrowthState.LastCardDay` — Growth 소유. MBC는 `GetLastCardDay()` pull API로 읽기 전용 사용 | — |
| OQ-5 | FinalReveal VFX (포자 파티클 등) — MVP에서 머티리얼 Lerp만, VS에서 Niagara 추가 여부 | Lumen Dusk Scene GDD (#11) | Vertical Slice |
