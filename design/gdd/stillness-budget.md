# Stillness Budget

> **Status**: Designed (pending review)
> **Author**: bong9tutor + claude-code session
> **Last Updated**: 2026-04-17
> **Implements Pillar**: Pillar 1 (조용한 존재 — 동시 이벤트 상한으로 "조용함" 강제)
> **Priority**: MVP | **Layer**: Feature (조기 명시) | **Category**: Meta

## Overview

Stillness Budget은 게임 내 동시 시각·청각 이벤트(애니메이션, 파티클, 사운드 큐)의 **상한을 정의하고 강제하는 단일 출처(single source of truth)**다. 세 가지 채널(Motion, Particle, Sound)별로 독립된 동시 활성 슬롯 수를 관리하며, downstream 시스템은 효과를 발생시키기 전에 슬롯 가용 여부를 조회한다. 상한 초과 시 가장 낮은 우선순위의 효과가 억제되거나 큐잉된다.

이 시스템이 없으면 Card Hand UI의 카드 건넴 VFX, Moss Baby의 성장 반응 애니메이션, Lumen Dusk Scene의 앰비언트 파티클, 꿈 알림의 사운드 큐가 동시에 발생해 "cozy 과부하"가 일어난다 — Pillar 1(조용한 존재: "재촉도, 죄책감도 없다")이 기술적으로 무너지는 것이다. Stillness Budget은 이 침묵의 톤을 **디자이너가 데이터로 튜닝 가능한 게임 레이어 예산**으로 보호한다.

**Reduced Motion 토글**은 이 시스템의 공개 API다. 활성화 시 모든 예산을 최소로 강제하여 파티클·흔들림·전환 애니메이션을 전면 억제한다. 이 토글은 MVP에서 바로 사용 가능하며, Accessibility Layer(VS)의 Text Scale/Colorblind와 결합될 때까지 이 시스템이 접근성의 첫 번째 라인이다.

### 책임 경계 (이 시스템이 *아닌* 것)
- **개별 효과의 시각적 구현 아님** — 파티클 외형·애니메이션 커브는 각 소비 시스템이 소유. Stillness Budget은 "지금 이 효과를 켜도 되는가"만 판정
- **엔진 레벨 concurrency 설정 아님** — UE Sound Concurrency Group, Niagara Scalability는 독립 작동. 이 시스템은 *게임 레벨* 예산으로 엔진 설정 위에 놓이는 추가 레이어

### 구현 결정 → ADR 후보
- **ADR 후보**: UE Sound Concurrency / Niagara Scalability와의 통합 전략 — 게임 레이어 예산과 엔진 내장 concurrency의 협업 방식. 아키텍처 단계에서 결정.

## Player Fantasy

플레이어는 이 게임이 "이상하게 조용하다"는 것을 느끼지만, 그 이유를 설명할 수 없다.

카드를 건네면 파티클이 폭발하는 대신 빛이 조금 따뜻해진다. 꿈이 오지 않은 날은 이끼 아이가 그냥 거기 있다. 그것만으로도 충분하다.

이것은 절제가 아니라 **이 세계의 규모**다. 손바닥 위 유리병 안에서 일어나는 일은 손바닥 위 규모여야 한다. Stillness Budget은 그 규모를 지킨다 — 동시에 세 가지가 경쟁하는 대신, 하나가 조용히 일어나고, 사라지고, 다음 하나가 온다.

**Pillar 연결**:
- **Pillar 1 (조용한 존재)** — 시각/청각 과부하가 사라지면 재촉의 감각도 사라진다. "이 게임은 나를 서두르게 만들지 않는다."
- **Pillar 3 (말할 때만 말한다)** — 이끼 아이의 침묵이 텍스트에만 적용되는 것이 아니다. 애니메이션, 파티클, 사운드도 "필요할 때만 일어난다." 희소성이 감정을 만든다.

**플레이어 순간 (닻)**: 카드를 건넨 후 창을 닫지 않고 황혼 빛을 그냥 바라보는 순간. Stillness Budget이 없으면 이 순간은 존재하지 않는다 — 세 가지가 동시에 움직이는 화면을 사람은 바라보지 않고, *처리한다*.

## Detailed Design

### Core Rules

**Rule 1 — 시스템 정의**: Stillness Budget은 상태 머신이 아닌 **순수 쿼리 서비스**다. 내부 상태(Running, Paused 등)를 갖지 않는다. 매 요청마다 현재 활성 슬롯 수를 집계해 허용 여부를 반환한다.

**Rule 2 — 채널 정의**: 세 채널은 독립적이며 채널 간 슬롯 공유 없음.

| 채널 | ID | 대상 효과 | `UStillnessBudgetAsset` 필드 |
|---|---|---|---|
| Motion | `EStillnessChannel::Motion` | 스켈레탈/머티리얼 애니메이션, UI 트랜지션, 카드 wobble | `MotionSlots` |
| Particle | `EStillnessChannel::Particle` | Niagara 이펙트 (포자, 이슬, 빛 먼지) | `ParticleSlots` |
| Sound | `EStillnessChannel::Sound` | 시각 단서와 쌍으로 오는 음향 큐 (BGM 제외) | `SoundSlots` |

코드 내 리터럴 기본값 금지. 슬롯 수는 `UStillnessBudgetAsset`의 `UPROPERTY(EditAnywhere)` 필드로만 존재 (Data Pipeline AC-DP-03).

**Rule 3 — 우선순위**: 3단계.

| Priority | 값 | 설명 | 선점 대상 |
|---|---|---|---|
| `Background` | 0 | 환경 루프, 장식 파티클, 앰비언트 사운드 | — |
| `Standard` | 1 | 카드 wobble, 일반 UI 트랜지션, 건넴 SFX | Background |
| `Narrative` | 2 | 꿈 시퀀스, 성장 변환, 작별 연출 | Background, Standard |

동일 우선순위 간 선점 없음 (FIFO — 먼저 점유한 슬롯 유지).

**Rule 4 — 슬롯 생명주기**: 명시적 호출 기반 (프레임 기반 아님).

```
Request(Channel, Priority) → [Grant: FStillnessHandle | Deny: Invalid Handle]
                               ↓ (Grant)
                            Active (슬롯 점유)
                               ↓
                            Release(Handle) — 명시 호출 또는 핸들 소멸자 자동 호출 (RAII)
```

- **Grant**: 빈 슬롯 존재 또는 선점 가능 시 → 유효한 `FStillnessHandle` 반환
- **Deny**: 슬롯 없음 + 선점 불가 → Invalid 핸들 반환. **소비자가 효과를 건너뜀**. Budget이 소비자에게 직접 지시하지 않음
- 슬롯에 타임아웃 없음. Release 누락은 버그로 간주

**Rule 5 — 선점(Preemption)**: 고우선 요청이 저우선 활성 슬롯을 강제 해제.

- `Narrative` 요청 시 슬롯 부족 → `Background` 슬롯 탐색 → 강제 Release + Grant
- `Standard` 요청 → `Background` 선점 가능, `Narrative` 선점 불가
- 선점된 소유자에게 `FStillnessHandle::OnPreempted` delegate 발행 → 소비자가 효과 조기 종료

**Rule 6 — Reduced Motion**: `bReducedMotionEnabled` (`UStillnessBudgetAsset` 필드) = `true` 시:

| 채널 | Reduced Motion 동작 |
|---|---|
| Motion | 슬롯 상한 0 취급. 모든 요청 즉시 Deny |
| Particle | 슬롯 상한 0 취급. 모든 요청 즉시 Deny |
| Sound | **영향 없음**. 시각 채널과 독립 — 청각 접근성은 별도 관심사 |

필드 값 자체를 변경하지 않음 (계산 시점에 0으로 읽힘).

**Rule 7 — DegradedFallback 처리**: `Pipeline.GetStillnessBudgetAsset()` == `nullptr` 시:

- 모든 채널의 `Request()` → Invalid 핸들 반환 (효과 건너뜀)
- 하드코딩 기본값 사용 금지 (AC-DP-03)
- `UE_LOG(LogStillness, Warning, ...)` 1회 기록, 반복 로그 금지
- "비활성화" ≠ "모든 효과 허용". 소비자는 핸들 유효성을 항상 확인

**Rule 8 — 공개 API 계약**: 소비자는 Grant/Deny 결과에 따라 독자적 분기. Budget이 소비자 로직을 직접 호출하지 않음 (단방향 의존).

- `Request(Channel, Priority) → FStillnessHandle` — 슬롯 요청
- `Release(Handle)` — 명시적 반납
- `IsReducedMotion() → bool` — UI가 대체 표현 선택에 사용

### States and Transitions

Stillness Budget은 상태 머신이 아니므로 자체 상태 전환이 없다. 자산 가용성에 따른 동작 모드만 존재:

| 모드 | 조건 | 동작 |
|---|---|---|
| **Normal** | `UStillnessBudgetAsset` 정상 로드 | 채널별 슬롯 상한 적용, Request/Release 정상 작동 |
| **ReducedMotion** | Normal + `bReducedMotionEnabled == true` | Motion/Particle → Deny all, Sound → Normal |
| **Unavailable** | `UStillnessBudgetAsset == nullptr` (DegradedFallback) | 모든 Request → Deny, 효과 전면 건너뜀 |

모드 전환은 자동 (자산 로드 상태와 Reduced Motion 플래그에 의해 결정). 외부 명시 전환 없음.

### Interactions with Other Systems

#### 1. Data Pipeline (#2) — 인바운드 (업스트림)

| 방향 | 데이터 | 시점 |
|---|---|---|
| Pipeline → Stillness | `UStillnessBudgetAsset*` (전체 튜닝 데이터) | Initialize 시 1회 pull + 캐싱 |

- DegradedFallback 시 `nullptr` → Unavailable 모드 진입

#### 2. Card Hand UI (#12) — 아웃바운드 (다운스트림)

| 방향 | 데이터 | 시점 |
|---|---|---|
| Card Hand UI → Stillness | `Request(Motion, Standard)` — 카드 wobble | 카드 hover/선택 시 |
| Card Hand UI → Stillness | `Request(Particle, Standard)` — 건넴 VFX | IA_OfferCard 트리거 시 |
| Stillness → Card Hand UI | `FStillnessHandle` 또는 Invalid | 즉시 응답 |
| Card Hand UI → Stillness | `Release(Handle)` | 애니메이션/VFX 종료 시 |
| Card Hand UI ← Stillness | `IsReducedMotion()` | UI가 정적 하이라이트로 대체 |

#### 3. Lumen Dusk Scene (#11) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Scene → Stillness | `Request(Particle, Background)` — 앰비언트 파티클 (포자, 이슬) | 씬 로드 + 주기적 재요청 |
| Scene → Stillness | `Request(Motion, Background)` — 빛 보간 애니메이션 | 상태 전환 시 |

- 앰비언트 파티클은 Background 우선순위 → Narrative 이벤트 시 선점 가능

#### 4. Moss Baby Character (#6) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Character → Stillness | `Request(Motion, Standard)` — 반응 애니메이션 | 카드 건넴 후 |
| Character → Stillness | `Request(Motion, Narrative)` — 성장 변환 | 성장 단계 전환 시 |
| Character → Stillness | `Request(Particle, Narrative)` — 성장 VFX | 성장 단계 전환 시 |

- 성장 변환은 Narrative 최고 우선순위 → 다른 Motion/Particle 선점

#### 5. Dream Journal UI (#13) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Journal UI → Stillness | `Request(Motion, Narrative)` — 꿈 reveal 트랜지션 | 꿈 일기 열림 시 |
| Journal UI → Stillness | `Request(Sound, Narrative)` — 꿈 차임 SFX | 꿈 일기 열림 시 |

#### 6. Audio System (#15, VS) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Audio → Stillness | `Request(Sound, Background)` — 앰비언트 레이어 | 씬 로드 시 |
| Audio → Stillness | `Request(Sound, Standard)` — 카드 SFX | 카드 건넴 시 |

- BGM은 Stillness Budget 관할 **밖** (별도 볼륨 제어). Budget은 "이벤트성 사운드"만 관리

#### 7. Game State Machine (#5) — 인바운드 (간접)

| 방향 | 데이터 | 시점 |
|---|---|---|
| GSM 상태 변경 | 소비자들이 상태 전환 시 Request/Release 호출 | 상태 전환 시 |

- Stillness Budget은 GSM을 직접 구독하지 않음. 각 소비자가 GSM 상태에 반응하여 Budget을 호출

#### 8. Accessibility Layer (#18, VS) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Accessibility → Stillness | `IsReducedMotion()` 조회 | 설정 UI에서 토글 표시 |
| Accessibility → Stillness Asset | `bReducedMotionEnabled` 변경 | 설정 UI에서 토글 조작 |

- Reduced Motion 토글의 UX는 Accessibility Layer/Title Settings UI가 소유. Stillness Budget은 플래그 저장·조회만 담당

### Data Ownership

| 데이터/타입 | 소유 시스템 | 읽기 허용 | 쓰기 허용 |
|---|---|---|---|
| `UStillnessBudgetAsset` 필드 전체 | Stillness Budget (정의), Data Pipeline (로딩) | 모든 소비자 (`IsReducedMotion()` 경유) | 에디터 디자이너 + Accessibility Layer (Reduced Motion 토글) |
| `EStillnessPriority` enum | Stillness Budget (정의) | 모든 소비자 | — |
| `EStillnessChannel` enum | Stillness Budget (정의) | 모든 소비자 | — |
| `FStillnessHandle` | Stillness Budget (발행) | 발행받은 소비자 | Stillness Budget (Release 시 무효화) |
| 활성 슬롯 집계 (런타임) | Stillness Budget (유일) | 디버깅 시 조회 허용 | Stillness Budget (유일) |

## Formulas

### Formula 1 — Slot Availability Check (`Budget.CanGrant`)

슬롯 요청 시 허용 여부를 결정한다.

`CanGrant = (Active_ch < EffLimit_ch) OR (FreeByPreempt_ch ≥ 1)`

여기서 `FreeByPreempt_ch = count of Active slots in channel ch with priority < P_req`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| 요청 채널 | `ch` | enum | `{Motion, Particle, Sound}` | 슬롯을 요청하는 채널 |
| 요청 우선순위 | `P_req` | int | `{0, 1, 2}` | Background=0, Standard=1, Narrative=2 |
| 유효 슬롯 상한 | `EffLimit_ch` | int | `[0, MaxSlots_ch]` | Formula 2 출력값 |
| 현재 활성 슬롯 수 | `Active_ch` | int | `[0, EffLimit_ch]` | 채널 ch의 현재 Grant 상태 슬롯 수 |
| 선점 가능 슬롯 수 | `FreeByPreempt_ch` | int | `[0, Active_ch]` | 우선순위 < P_req인 활성 슬롯 수 |

**Output**: `bool`. `true` → Grant (선점 필요 시 가장 낮은 우선순위 슬롯 1개 해제 후 Grant). `false` → Deny (Invalid 핸들 반환).

**Example** (Motion 채널, EffLimit=2, 활성 슬롯 2개 — P=0 1개 + P=1 1개):
- 요청 P=2(Narrative): FreeByPreempt = 2(P<2인 슬롯 2개) → `true`. P=0 슬롯 선점
- 요청 P=0(Background): FreeByPreempt = 0, Active(2) ≥ EffLimit(2) → `false`. Deny

### Formula 2 — Effective Slot Limit (`Budget.EffLimit`)

Reduced Motion 상태에 따른 실효 슬롯 상한 계산.

```
EffLimit_ch = MaxSlots_ch    if NOT bReducedMotion
            = 0              if bReducedMotion AND ch ∈ {Motion, Particle}
            = MaxSlots_ch    if bReducedMotion AND ch = Sound
```

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| 기본 슬롯 상한 | `MaxSlots_ch` | int | `[1, 16]` | `UStillnessBudgetAsset`의 채널별 슬롯 필드값 |
| 채널 | `ch` | enum | `{Motion, Particle, Sound}` | 계산 대상 채널 |
| Reduced Motion | `bReducedMotion` | bool | `{true, false}` | 접근성 설정 |

**Output**: `int [0, MaxSlots_ch]`. `EffLimit=0` → 해당 채널의 모든 요청 즉시 Deny.

**Example**:

| bReducedMotion | ch | MaxSlots | EffLimit |
|---|---|---|---|
| false | Motion | 2 | 2 |
| false | Sound | 3 | 3 |
| true | Motion | 2 | **0** |
| true | Particle | 2 | **0** |
| true | Sound | 3 | 3 |

> **EffLimit 갱신 시 기존 Active 슬롯**: 강제 해제하지 않음. 자연 Release까지 유지 (과거 Grant 소급 취소 없음).
>
> **DegradedFallback**: Formula 외부 조건. EffLimit 계산 전에 서비스가 nullptr 확인 → 무조건 Deny 반환 (Formula 우회).

## Edge Cases

- **E1 — MaxSlots = 0 설정**: `UStillnessBudgetAsset`에서 특정 채널의 MaxSlots를 0으로 설정하면 ReducedMotion 상태와 무관하게 해당 채널의 모든 요청이 Deny. 의도적 설정(해당 채널 완전 비활성화)을 허용하되, Initialize 시점에 `UE_LOG(LogStillness, Warning)` 1회 기록

- **E2 — ReducedMotion 토글 ON — 활성 슬롯 처리**: 세션 중 ReducedMotion이 ON으로 전환되면 Motion/Particle EffLimit이 즉시 0이 된다. **기존 활성 핸들은 강제 해제하지 않음** — 자연 Release까지 유지 (Formula 2 주석과 일관: "과거 Grant 소급 취소 없음"). 새 요청만 Deny

- **E3 — ReducedMotion 토글 OFF 복귀**: ReducedMotion이 OFF로 복귀하면 EffLimit이 MaxSlots로 회복. 이전에 Deny되었던 소비자들은 자동 재시도하지 않음. **`OnBudgetRestored(EStillnessChannel)` delegate 발행** — 관심 있는 소비자가 구독하여 재요청. 미구독 소비자는 다음 자연 이벤트(카드 건넴, 상태 전환 등)에서 정상 Request

- **E4 — 동일 프레임 다중 요청**: 같은 프레임에 동일 채널·동일 우선순위 요청이 복수 도착하면, 실행 순서(코드 호출 순서)에 따라 선착순 처리. 비결정적 순서(BeginPlay 등)에서는 결과가 달라질 수 있으나, 슬롯 수가 2-3이고 동시 요청은 드물므로 실질적 문제 없음. 명시적으로 FIFO (호출 순서)

- **E5 — 선점 체인 방지**: OnPreempted를 받은 소비자가 핸들러 내에서 즉시 동일 채널에 재요청하면 재귀적 선점 가능. **금지 규칙**: OnPreempted 콜백 내에서의 Request() 호출을 금지 (다음 프레임 이후 재요청 허용). 구현 시 재진입 가드 `bProcessingPreemption` 플래그로 assert

- **E6 — Narrative vs Narrative 선점 시도**: Narrative 슬롯이 모두 찼을 때 새 Narrative 요청 도착 → 선점 대상 없음 (동일 우선순위 간 선점 금지, Rule 3) → Deny. 설계 의도: 꿈 시퀀스와 성장 변환이 동시에 발생할 확률은 극히 낮음 (꿈은 세션 시작 시, 성장은 카드 건넴 후). 만약 발생 시 먼저 요청한 효과가 우선

- **E7 — 이중 Release (Double Release)**: RAII 핸들의 Release가 두 번 호출되면 (소멸자 중복, 복사 후 양쪽 소멸) ActiveCount가 음수로 오염. **해소**: Release는 idempotent — 핸들 내부 `bValid` 플래그를 확인하고, 이미 false면 no-op. `FStillnessHandle`은 복사 금지 (move-only semantics)

- **E8 — 선점 후 Release**: 소비자가 OnPreempted를 받았으나 핸들 참조를 유지한 채 나중에 Release 호출. **해소**: 선점 시점에 서버 측에서 핸들을 즉시 무효화 (`bValid = false`). 이후 Release는 E7과 같이 no-op

- **E9 — DegradedFallback 시 요청**: `UStillnessBudgetAsset == nullptr` → MaxSlots를 알 수 없음. 모든 채널 전면 Deny (fail-close). DegradedFallback은 Initialize 시점에 결정되므로, 이 상태에서 활성 슬롯이 존재할 수 없음 (슬롯 Grant는 Initialize 이후에만 가능)

- **E10 — 단일 채널 MaxSlots = 1 선점 루프**: MaxSlots=1에서 Background 점유 → Standard 선점 → Background 재요청 Deny → 다음 이벤트에서 재시도. **E5의 재진입 금지**로 프레임 내 루프는 방지됨. 프레임 간 폴링은 소비자 설계 책임 — 무한 폴링 금지 (이벤트 기반 재요청만 허용)

## Dependencies

### Upstream Dependencies (이 시스템이 의존하는 대상)

| 의존 대상 | 의존 강도 | 인터페이스 | 비고 |
|---|---|---|---|
| Data Pipeline (#2) | Hard | `Pipeline.GetStillnessBudgetAsset() → UStillnessBudgetAsset*` | Initialize 시 1회 pull + 캐싱. nullptr → Unavailable 모드 (전면 Deny) |

Foundation Layer 시스템(Time, Save/Load, Input Abstraction)과는 의존 관계 없음.

### Downstream Dependents (이 시스템에 의존하는 시스템)

| 시스템 | 의존 강도 | 인터페이스 | 비고 |
|---|---|---|---|
| **Card Hand UI (#12)** | Hard | `Request(Motion/Particle, Standard)`, `IsReducedMotion()` | 카드 wobble + 건넴 VFX. Deny 시 정적 표현으로 대체 |
| **Lumen Dusk Scene (#11)** | Soft | `Request(Particle/Motion, Background)` | 앰비언트 파티클. Deny 시 정적 씬 유지 — 기능 핵심 아님 |
| **Moss Baby Character (#6)** | Hard | `Request(Motion/Particle, Standard/Narrative)` | 반응 애니메이션 + 성장 VFX |
| **Dream Journal UI (#13)** | Soft | `Request(Motion/Sound, Standard)` | 일기 열기·페이지 넘김·아이콘 펄스 트랜지션. Narrative 우선순위는 GSM Dream state 진입 자체가 소유 (Dream Journal UI는 그 파생 UI 피드백) |
| **Audio System (#15, VS)** | Soft | `Request(Sound, Background/Standard)` | 이벤트 SFX. BGM은 관할 밖 |
| **Accessibility Layer (#18, VS)** | Soft | `IsReducedMotion()`, `bReducedMotionEnabled` 변경 | Reduced Motion 토글 UX 소유 |

### Bidirectional Consistency Notes

모든 연결은 **단방향 (Consumer → Stillness Budget)**. 역방향 데이터 흐름은 `OnPreempted` delegate와 `OnBudgetRestored` delegate뿐 (이벤트 통지, 데이터 반환 아님).

각 downstream GDD 작성 시 다음 참조 명시 필요:
- "Stillness Budget (#10)의 `Request(Channel, Priority)` 호출 필수. Invalid Handle 시 효과 건너뜀."
- "Reduced Motion 활성 시 시각 효과 대체 경로 필요 (`IsReducedMotion()` 조회)."

### Data Ownership

§Detailed Design의 Data Ownership 테이블 참조.

## Tuning Knobs

| 노브 | 기본값 (후보) | 안전 범위 | 너무 높으면 | 너무 낮으면 | 참조 |
|---|---|---|---|---|---|
| `MotionSlots` | 2 | `[1, 6]` | 동시 애니메이션 과다 → cozy 과부하, Pillar 1 위반 | 카드 건넴 + 이끼 반응이 동시에 안 됨 — 핵심 루프 체감 훼손 | Formula 2, Rule 2 |
| `ParticleSlots` | 2 | `[1, 6]` | 파티클 경쟁으로 시각 소음 | 앰비언트 파티클 + 건넴 VFX 동시 불가 — 선점만 발생 | Formula 2, Rule 2 |
| `SoundSlots` | 3 | `[1, 8]` | 동시 SFX 겹침 → 오디오 혼탁 | 앰비언트 + 카드 SFX + 꿈 차임 동시 불가 | Formula 2, Rule 2 |
| `bReducedMotionEnabled` | `false` | `{true, false}` | — | — | Formula 2, Rule 6 |

### 노브 상호작용

- `MotionSlots`와 `ParticleSlots`는 독립적 — 채널 간 슬롯 공유 없음
- `SoundSlots`는 시각 채널과 독립 — Reduced Motion의 영향을 받지 않음
- `bReducedMotionEnabled`는 `MotionSlots`와 `ParticleSlots`의 실효값을 0으로 오버라이드하지만, 저장된 기본값은 변경하지 않음 (토글 OFF 시 즉시 복원)

### Playtest Tuning Priorities

- **1순위**: `MotionSlots` — MVP에서 카드 건넴 + 이끼 반응이 동시에 보이는지가 핵심. 1 vs 2 비교 테스트
- **2순위**: `ParticleSlots` — 앰비언트 파티클이 건넴 VFX에 의해 선점되는 빈도. 2로 시작, 체감 조절
- **3순위**: `SoundSlots` — VS Audio System 구현 후 조정. MVP는 placeholder audio이므로 후순위

## Visual/Audio Requirements

이 시스템은 인프라 레이어로 직접적인 Visual/Audio 출력이 없다. 개별 효과의 시각/청각 구현은 각 소비 시스템(Card Hand UI, Moss Baby Character, Audio System 등)이 소유한다.

## UI Requirements

이 시스템은 UI를 소유하지 않는다. Reduced Motion 토글의 사용자 인터페이스는 Title & Settings UI (#16, VS) 또는 Accessibility Layer (#18, VS)가 소유한다.

## Acceptance Criteria

> Given-When-Then 형식. QA 테스터가 GDD 없이 독립 검증 가능.

### AC-SB-01 SLOT_GRANT_BASIC
- **GIVEN** Motion 채널 EffLimit=2, Active=1
- **WHEN** `Request(Motion, Standard)` 호출
- **THEN** 유효한 `FStillnessHandle` 반환, Active=2
- **Type**: AUTOMATED — **Source**: Rule 4, F1

### AC-SB-02 SLOT_DENY_FULL
- **GIVEN** Motion 채널 EffLimit=2, Active=2, 선점 가능 슬롯 없음
- **WHEN** `Request(Motion, Background)` 호출
- **THEN** Invalid 핸들 반환, Active 변화 없음
- **Type**: AUTOMATED — **Source**: Rule 4, F1

### AC-SB-03 CHANNEL_INDEPENDENCE
- **GIVEN** Motion 채널 EffLimit=2, Active=2 / Particle 채널 Active=0
- **WHEN** `Request(Particle, Standard)` 호출
- **THEN** 유효한 핸들 반환 (Motion 포화가 Particle에 영향 없음)
- **Type**: AUTOMATED — **Source**: Rule 2

### AC-SB-04 PRIORITY_PREEMPT_LOWER
- **GIVEN** Motion 채널 EffLimit=2, Active=[P=Background, P=Standard]
- **WHEN** `Request(Motion, Narrative)` 호출
- **THEN** P=Background 슬롯 선점 해제, 신규 핸들 Grant, `OnPreempted` delegate 발행
- **Type**: AUTOMATED — **Source**: Rule 3, Rule 5, F1

### AC-SB-05 PREEMPT_SAME_PRIORITY_DENIED
- **GIVEN** Motion 채널 EffLimit=2, Active=[P=Standard, P=Standard]
- **WHEN** `Request(Motion, Standard)` 호출
- **THEN** Invalid 핸들 반환, 기존 슬롯 선점 없음
- **Type**: AUTOMATED — **Source**: Rule 3

### AC-SB-06 REDUCED_MOTION_DENY_VISUAL
- **GIVEN** `bReducedMotionEnabled=true`, Motion/Particle MaxSlots=2
- **WHEN** `Request(Motion, Narrative)` 및 `Request(Particle, Narrative)` 각각 호출
- **THEN** 두 요청 모두 Invalid 핸들 반환
- **Type**: AUTOMATED — **Source**: Rule 6, F2

### AC-SB-07 REDUCED_MOTION_SOUND_UNAFFECTED
- **GIVEN** `bReducedMotionEnabled=true`, Sound MaxSlots=3, Active=0
- **WHEN** `Request(Sound, Standard)` 호출
- **THEN** 유효한 핸들 반환 (Sound EffLimit = MaxSlots 유지)
- **Type**: AUTOMATED — **Source**: Rule 6

### AC-SB-08 DEGRADED_FALLBACK_DENY_ALL
- **GIVEN** `UStillnessBudgetAsset == nullptr` (DegradedFallback 모드)
- **WHEN** 세 채널 각각 `Request()` 호출
- **THEN** 모든 요청 Invalid 핸들 반환, 하드코딩 기본값 사용 없음
- **Type**: AUTOMATED — **Source**: Rule 7, E9

### AC-SB-09 DEGRADED_FALLBACK_LOG_ONCE
- **GIVEN** DegradedFallback 모드
- **WHEN** `Request()` 5회 연속 호출
- **THEN** `UE_LOG(LogStillness, Warning)` 정확히 1회 기록
- **Type**: AUTOMATED — **Source**: Rule 7

### AC-SB-10 NO_HARDCODED_SLOTS
- **GIVEN** StillnessBudget 구현 소스 전체 (`Source/MadeByClaudeCode/` — Budget 서비스 포함)
- **WHEN** 슬롯 수 리터럴 검색 (`grep -rn "= 2\|= 3" ... StillnessBudget`)
- **THEN** `UStillnessBudgetAsset` UPROPERTY 참조만 존재, 인라인 매직 넘버 없음
- **Type**: CODE_REVIEW — **Source**: Rule 2

### AC-SB-11 DOUBLE_RELEASE_IDEMPOTENT
- **GIVEN** 유효한 `FStillnessHandle` (Active 증가 상태)
- **WHEN** `Release()` 2회 연속 호출
- **THEN** Active 감소는 1회만 발생, 두 번째 호출 no-op (크래시 없음)
- **Type**: AUTOMATED — **Source**: E7

### AC-SB-12 NO_REENTRANT_REQUEST_IN_PREEMPTED
- **GIVEN** `OnPreempted` 핸들러 내에서 동일 채널 `Request()` 재호출 시도
- **WHEN** 선점 이벤트 발생
- **THEN** 재진입 가드 assert 발동 또는 핸들러 내 Request 무시
- **Type**: AUTOMATED — **Source**: E5

### AC-SB-13 BUDGET_RESTORED_DELEGATE
- **GIVEN** ReducedMotion=ON 중 Deny된 소비자가 `OnBudgetRestored` 구독 중
- **WHEN** `bReducedMotionEnabled=false`로 토글 복원
- **THEN** `OnBudgetRestored(Motion)` / `OnBudgetRestored(Particle)` delegate 각각 발행
- **Type**: INTEGRATION — **Source**: E3

---

### Coverage 요약

| 분류 | Criterion 수 |
|---|---|
| 슬롯 Grant/Deny 기본 | 2 (SB-01, SB-02) |
| 채널 독립성 | 1 (SB-03) |
| 우선순위 + 선점 | 2 (SB-04, SB-05) |
| Reduced Motion | 2 (SB-06, SB-07) |
| DegradedFallback | 2 (SB-08, SB-09) |
| 코드 품질 | 1 (SB-10) |
| 핸들 생명주기 | 2 (SB-11, SB-12) |
| 토글 전환 통합 | 1 (SB-13) |
| **Total** | **13** |
| AUTOMATED | 11 (84.6%) |
| CODE_REVIEW | 1 (7.7%) |
| INTEGRATION | 1 (7.7%) |
| MANUAL | 0 (0%) |

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|---|---|---|
| ~~OQ-1~~ | ~~UE Sound Concurrency / Niagara Scalability 통합 전략~~ | — | **RESOLVED** (2026-04-19 by [ADR-0007](../../docs/architecture/adr-0007-stillness-budget-engine-concurrency-integration.md) — Hybrid: Stillness Budget이 game-layer source + 엔진 concurrency는 hardware-level safety net. SC_MossBaby_Stillness MaxCount=32 + Niagara Effect Type 3종) |
| OQ-2 | `bReducedMotionEnabled` 토글의 영속화 — Save/Load에 포함? 별도 설정 파일? 세션 간 유지 방식 | `systems-designer` | Title & Settings UI GDD (#16, VS) 작성 시 |
| OQ-3 | 채널별 최적 슬롯 수 (MotionSlots=2, ParticleSlots=2, SoundSlots=3 후보) — 플레이테스트로 확정 필요 | `game-designer` | MVP 구현 후 플레이테스트 |
| OQ-4 | OnPreempted를 받은 소비자의 graceful termination 패턴 — 즉시 이펙트 중단 vs fade-out 허용 여부. 소비자별 다를 수 있음 | `gameplay-programmer` | Card Hand UI / Moss Baby Character GDD 작성 시 |
