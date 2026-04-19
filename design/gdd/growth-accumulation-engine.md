# Growth Accumulation Engine

> **Status**: Approved (R4 lean review — 2026-04-18. R1-R3 총 20 blocker 해소, RECOMMENDED 8건 구현 이관)
> **Author**: bong9tutor + claude-code session
> **Last Updated**: 2026-04-18 (R3)
> **Implements Pillar**: Pillar 2 (선물의 시학), Pillar 4 (끝이 있다)
> **Priority**: MVP | **Layer**: Feature | **Category**: Gameplay
> **Effort**: L (4+ 세션)

---

## Overview

Growth Accumulation Engine은 매일 건네진 카드의 태그(`TArray<FName>`)를 **누적 벡터**(`TMap<FName, float>`)로 집적하고, 이 벡터의 분포에 따라 **성장 단계 전환**(Sprout → Growing → Mature)과 **최종 형태 결정**(Day 21)을 수행하는 순수 데이터 처리 시스템이다.

이 시스템은 플레이어에게 직접 보이지 않는다. 플레이어는 카드를 건넬 뿐이고, 성장 단계의 시각 변화는 Moss Baby Character System(#6)이, 카드 제시는 Card System(#8)이 담당한다. Growth Accumulation Engine은 그 사이에 있는 **투명한 변환기** — 카드 태그라는 입력을 받아 성장 이벤트라는 출력을 내보낸다. 결정자이지 표현자가 아니다.

세 가지 책임으로 나뉜다. **(1) 태그 벡터 누적** — 카드가 건네질 때마다 해당 카드의 태그(계절·원소·감정)를 누적 벡터에 가산. 벡터는 `UMossSaveData` 안의 `FGrowthState` sub-struct로 영속화된다. **(2) 성장 단계 평가** — 누적 일수(DayIndex)와 벡터 크기를 기준으로 EGrowthStage 전환을 결정하고 `FOnGrowthStageChanged`를 발행한다. **(3) 최종 형태 매핑** — 21일차(MVP 7일차)에 누적 벡터의 지배적 태그 조합을 `FFinalFormRow` 테이블과 대조하여 최종 형태를 선택하고 `FOnFinalFormReached`를 발행한다.

어떤 숫자도 플레이어에게 노출되지 않는다 (Pillar 2). 이 시스템의 모든 출력은 다른 시스템이 시각·서사적 경험으로 번역하는 원재료다.

MVP에서는 7일 축약 아크, 카드 10장, 최종 형태 1종으로 누적 → 성장 단계 파이프라인을 검증한다. Full 스코프에서 21일, 카드 40장, 최종 형태 8–12종으로 확장 시 태그 벡터 → 형태 매핑의 변별력이 핵심 과제가 된다.

## Player Fantasy

이끼 아이는 카드를 소비하지 않는다 — 흡수한다. 봄의 카드는 토양에 봄을 남기고, 어둠의 카드는 토양에 깊이를 판다. 21일 뒤 피어나는 것은 토양이 기억한 계절의 합산이다. 플레이어는 이 토양의 문법을 완전히 이해할 수 없다 — 패턴을 감지하되 공식을 알 수 없는 **반투명한 인과**가 해석의 즐거움을 만든다.

이 시스템이 전달해야 할 감정의 핵심은 **되돌아보는 순간의 경이감**(retrospective wonder)이다. 성장 엔진은 플레이어에게 직접 보이지 않지만, 결과는 매일 이끼 아이의 윤곽으로 돌아온다. 보이지 않는 누적이 보이는 변화로 맺힐 때, 플레이어는 인과를 '발견'하는 것이지 '계산'하는 것이 아니다.

**앵커 모먼트**:
- **첫 번째 성장 전환 (Sprout → Growing)**: "어, 달라졌다." 플레이어가 처음으로 누적의 존재를 감각하는 순간. 설명 없이, 숫자 없이, 어제와 다른 윤곽만으로
- **Day 21 최종 형태 결정**: 최종 형태를 보고 플레이어가 과거 카드 선택을 역추적하는 내면의 순간. 시스템이 설명하지 않아도 플레이어가 스스로 인과를 구성한다

**Pillar 연결**:
- **Pillar 2 (선물의 시학)** — 이 시스템의 수학은 플레이어 경험에서 시학이어야 한다. 최적해를 찾는 유혹이 아니라 의미를 읽는 충동. 디자인 테스트: "이 조합이면 이 형태가 나온다"를 확신하면 실패, "아마 이런 느낌이지 않을까"이면 성공
- **Pillar 4 (끝이 있다)** — 최종 형태는 21일 누적의 되돌릴 수 없는 결과물. 이 일회성이 매일의 카드 선택에 무게를 부여한다

**설계 원칙 — 반투명한 인과**: 플레이어는 패턴을 감지한다 — "따뜻한 카드를 많이 줬더니..." 하지만 정확한 공식은 알 수 없다. 완전히 이해하면 퍼즐이 되고, 전혀 이해 못하면 랜덤이 된다. 그 사이의 반투명함이 이 게임의 감정 설계다. 이 원칙은 Formula 섹션의 튜닝 기준이 된다.

**피드백 경로와 성장 단계의 역할 분리**: 성장 단계 전환(CR-4)은 DayIndex 임계값에 의해 결정되며 카드 내용과 무관하다 — 이것은 의도적이다. 중간 성장 단계(Sprout→Growing→Mature)는 시간의 경과를 표지하는 마일스톤이다. 카드가 만드는 변화는 두 경로로 돌아온다: **(1)** Season 카테고리 카드를 건넸을 때 CR-6 머티리얼 힌트가 갱신되어 색조·SSS 오프셋이 미세하게 변화한다 — 이것이 일일 피드백의 실체다. MVP에서 Element·Emotion 카테고리는 머티리얼 오프셋 0이므로 시각 피드백이 발생하지 않는다 (OQ-GAE-3에서 Full 확장 예정. Full에서 미도입 시 Player Fantasy 달성 불가). **(2)** Day 21 최종 형태(CR-5)가 누적 벡터 **전체**(모든 카테고리 포함)를 반영한다. 반투명한 인과는 CR-6의 미세한 머티리얼 시프트(일일, Season 한정)와 Day 21의 최종 형태 결정(종합, 전 카테고리)의 이중 구조에서 형성된다. 일일 힌트(F4)는 현재 지배적 방향의 미리보기이며, 최종 형태(F3)는 전체 누적의 내적 결산이다. 두 로직이 다른 결과를 낼 수 있다 — 이것이 반투명한 인과의 일부다.

## Detailed Design

### Core Rules

#### CR-1 — 태그 벡터 누적

카드 건넴(`FOnCardOffered(FName CardId)`) 수신 시:

1. Data Pipeline에서 `GetCardRow(CardId)` → `FGiftCardRow` pull
2. `FGiftCardRow.Tags` 순회, 각 태그에 가산:
   ```
   FGrowthState.TagVector[tag] += 1.0f
   ```
3. `FGrowthState.LastCardDay = CurrentDayIndex`
4. `FGrowthState.TotalCardsOffered += 1`
5. 머티리얼 힌트 재계산 (CR-6)
6. `SaveAsync(ESaveReason::ECardOffered)` — **2–5와 동일 game-thread 함수 호출** (Save/Load atomicity contract)
7. **`OnCardProcessedByGrowth.Broadcast({CardId, bIsDay21Complete, FinalFormId})`** — Stage 2 delegate 발행. ADR-0005 (2026-04-19) 도입. 마지막 statement — GSM + Dream Trigger가 Stage 2 구독자로서 이 broadcast 후에 실행됨. `bIsDay21Complete`는 CR-5 EvaluateFinalForm 결과 (Day 21 + TotalCardsOffered>0이면 true). **Day 21 순서 보장의 핵심 — C++ 호출 스택 순서로 Growth → GSM/Dream 전개**

모든 태그는 균등 가중 `+1.0f`. 카드별 차등 가중치 없음 (C1 schema guard + MVP 단순화). Full에서 카테고리 가중치 Tuning Knob 검토.

#### CR-2 — 태그 카테고리 네임스페이스

태그 FName은 점(.) 접두사로 카테고리를 구분:

| 카테고리 | 접두사 | 예시 태그 | MVP 수 |
|---|---|---|---|
| **계절** | `Season.` | `Season.Spring`, `Season.Winter` | 4 |
| **원소** | `Element.` | `Element.Water`, `Element.Earth` | 4 |
| **감정** | `Emotion.` | `Emotion.Joy`, `Emotion.Calm` | 4 |

카테고리 목록은 `UGrowthConfigAsset`의 `TagCategories: TArray<FName>` 필드로 외부화. 코드 내 카테고리 하드코딩 금지.

#### CR-3 — 빈 Tags Schema Guard

`UEditorValidatorBase` 서브클래스에서 `FGiftCardRow` 행 저장 시 `Tags.Num() == 0` 검사. 빈 Tags = 에러. Data Pipeline OQ-E-2 해소: **Schema 차단**.

#### CR-4 — 성장 단계 평가

`FOnDayAdvanced(int32 NewDayIndex)` 수신 직후, 동기 평가:

| 전환 | Full 조건 (21일) | MVP 조건 (7일) |
|---|---|---|
| Sprout → Growing | DayIndex ≥ `GrowingDayThreshold` (기본 6) | DayIndex ≥ 3 |
| Growing → Mature | DayIndex ≥ `MatureDayThreshold` (기본 15) | DayIndex ≥ 5 |
| Mature → FinalForm | DayIndex = `GameDurationDays` (21) | DayIndex = 7 |

임계값은 `UGrowthConfigAsset`의 Tuning Knob. 평가 결과 단계 변경 시:
- `FGrowthState.CurrentStage` 갱신
- `FOnGrowthStageChanged(EGrowthStage NewStage)` 발행
- 변경 없으면 이벤트 미발행

**다단계 건너뛰기**: 부재로 DayIndex가 급증한 경우(예: Sprout에서 Day 16으로 점프) 중간 단계를 순회하지 않고 **최종 해당 단계로 직행**. `FOnGrowthStageChanged`는 최종 단계만 1회 발행. MBC E1과 일관.

#### CR-5 — 최종 형태 결정

`DayIndex = GameDurationDays` 도달 시:

1. 누적 벡터 정규화:
   ```
   TotalWeight = sum(TagVector[tag] for all tags)
   V_norm[tag] = TagVector[tag] / TotalWeight
   ```
   `TotalWeight = 0` (카드 미건넴) → Default Form

2. 각 `FFinalFormRow`의 `RequiredTagWeights`와 내적:
   ```
   Score(form) = sum(V_norm[tag] × form.RequiredTagWeights[tag])
   ```

3. `FinalFormId = argmax(Score)`. 동점 시 FormId 사전순 첫 번째.

4. `FGrowthState.FinalFormId` 기록 → `FOnFinalFormReached(FName FinalFormId)` 발행

**MVP**: FFinalFormRow 1행. argmax는 항상 해당 행 반환. 코드 경로 Full과 동일.

#### CR-6 — 머티리얼 변주 힌트 (MVP)

카드 건넴마다 누적 벡터에서 **지배적 카테고리 비율**을 계산하여 `FGrowthMaterialHints` 갱신:

```
CategoryWeight[cat] = sum(TagVector[tag] for tag in category cat)
DominantCategory = argmax(CategoryWeight)
DominanceRatio = CategoryWeight[DominantCategory] / TotalWeight
```

| 카테고리 | HueShift 오프셋 | SSS 오프셋 |
|---|---|---|
| Season.Spring | +0.02 × DominanceRatio | +0.05 × DominanceRatio |
| Season.Summer | +0.01 × DominanceRatio | +0.03 × DominanceRatio |
| Season.Autumn | -0.01 × DominanceRatio | +0.02 × DominanceRatio |
| Season.Winter | -0.02 × DominanceRatio | -0.03 × DominanceRatio |

오프셋 테이블은 `UGrowthConfigAsset`에 외부화.

**`GetMaterialHints()` API 계약**: Growth는 `FGrowthMaterialHints`를 반환한다. 이 struct는 **raw 오프셋**(HueShiftOffset = offset[DominantTag] × DominanceRatio, SSSOffset = offset[DominantTag] × DominanceRatio)과 보조 정보(DominantCategory, DominanceRatio)를 담는다. Growth는 MBC 프리셋 값을 알지 못하며 clamp를 수행하지 않는다. **MBC가 프리셋에 오프셋을 가산하고 최종 clamp를 수행하는 책임을 진다** (`HueShift_final = clamp(preset + HueShiftOffset, -0.1, 0.1)`). F4 공식은 end-to-end 파이프라인을 문서화하지만, Growth↔MBC API 경계는 raw 오프셋이다.

**TotalWeight = 0이면**: 모든 오프셋 0 (변주 없음).
**반투명한 인과**: 오프셋이 작아서(±0.02) 플레이어는 "약간 따뜻하다" 정도만 감지. 정확한 수치는 알 수 없음.

#### CR-7 — FGrowthState 구조체

| 필드 | 타입 | 설명 |
|---|---|---|
| `TagVector` | `TMap<FName, float>` | 누적 태그 벡터. 키=태그 FName, 값=누적 가중치 |
| `CurrentStage` | `EGrowthStage` | 현재 성장 단계. 기본 Sprout |
| `LastCardDay` | `int32` | 마지막 카드 건넴 일차. 0=미건넴. MBC DryingFactor 입력 |
| `TotalCardsOffered` | `int32` | 누적 건넨 카드 수 |
| `FinalFormId` | `FName` | 최종 형태 ID. 비어있으면 미결정 |

`UMossSaveData` 안에 typed sub-struct로 거주. Save/Load가 직렬화 책임, Growth가 의미적 소유. MBC OQ-4 해소: `LastCardDay`는 `FGrowthState` 소유.

#### CR-8 — FFinalFormRow 필드 정의 (Growth 소유)

| 필드 | 타입 | 설명 |
|---|---|---|
| `FormId` | `FName` | Primary key. Data Pipeline stub에서 이미 정의 |
| `RequiredTagWeights` | `TMap<FName, float>` | 이 형태의 "이상적" 태그 분포. Score 내적의 대상 |
| `DisplayName` | `FText` | 형태 이름 (String Table 경유) |
| `MeshPath` | `FSoftObjectPath` | 최종 형태 Static Mesh 경로 |
| `MaterialPresetPath` | `FSoftObjectPath` | 최종 형태 머티리얼 프리셋 경로 |

Data Pipeline에서 forward-declared stub(FormId만)이었던 것을 이 GDD가 확장. `RequiredTagWeights`가 형태 변별의 핵심.

**DataTable 편집 제한 (RESOLVED 2026-04-19 by [ADR-0010](../../docs/architecture/adr-0010-ffinalformrow-storage-format.md))**: UE DataTable 에디터는 USTRUCT 내 `TMap` 필드의 인라인 셀 편집을 지원하지 않는다. CSV 임포트도 중첩 맵 구조를 처리하지 못한다. **ADR-0010이 Option (B) 채택** — `FFinalFormRow` 대신 `UMossFinalFormAsset : UPrimaryDataAsset` per-form 자산으로 격상. 각 형태를 별도 DataAsset으로 관리, TMap 인라인 편집 가능. ADR-0002 §Decision 표의 "FinalForm = DataTable" 결정도 ADR-0010이 amend함. 이 GDD의 인터페이스 계약(RequiredTagWeights의 의미적 구조)은 저장 형식과 독립적이다.

### States and Transitions

| 상태 | 설명 | 진입 조건 | 이탈 조건 |
|---|---|---|---|
| **Uninitialized** | 서브시스템 생성 직후 | 앱 시작 | `FOnLoadComplete` 수신 → FGrowthState 복원 |
| **Accumulating** | 태그 가산 + 단계 평가 루프 | FGrowthState 복원 완료 | DayIndex = GameDurationDays |
| **FormDetermination** | 최종 형태 계산 (1회, 동기) | Day = GameDurationDays | 계산 완료 |
| **Resolved** | 종단 상태. 모든 이벤트 무시 | FormDetermination 완료 | — (앱 종료까지) |

**Accumulating 내부 루프**:
- `FOnCardOffered` → CR-1 (태그 가산 → 저장)
- `FOnDayAdvanced` → CR-4 (단계 평가)
- 두 이벤트는 독립 — 같은 프레임에 둘 다 올 수 있음 (Card → Day 순서)

**로드 복구**: `FGrowthState.FinalFormId`가 비어있지 않으면 → Resolved로 직행 (이미 형태 결정 완료). `FGrowthState.CurrentStage`가 Sprout이 아니면 → 해당 단계에서 Accumulating 재개.

### Interactions with Other Systems

#### 1. Data Pipeline (#2) — 아웃바운드 (pull)

| API | 시점 | 용도 |
|---|---|---|
| `GetCardRow(FName CardId)` | `FOnCardOffered` 수신 시 | 카드 태그 조회 |
| `GetGrowthFormRow(FName FormId)` | Day = GameDurationDays | 최종 형태 메시·머티리얼 경로 |
| `GetAllGrowthFormRows()` | Day = GameDurationDays | 전체 형태 테이블 순회 (Score 계산) |

#### 2. Save/Load (#3) — 양방향

| 방향 | 데이터 | 시점 |
|---|---|---|
| Save → Growth | `FGrowthState` 복원 | Initialize() 1회 |
| Growth → Save | 갱신된 `FGrowthState` | 카드 건넴 후 `SaveAsync(ECardOffered)` |

Atomicity: CR-1 step 2–5와 step 6은 동일 함수 내.

#### 3. Moss Baby Character (#6) — 아웃바운드

| 이벤트/API | 데이터 | MBC 반응 |
|---|---|---|
| `FOnGrowthStageChanged` | `EGrowthStage NewStage` | GrowthTransition: 메시 교체 + 머티리얼 Lerp |
| `FOnFinalFormReached` | `FName FinalFormId` | FinalReveal: 최종 메시 + 느린 Lerp |
| `GetMaterialHints()` | `FGrowthMaterialHints` (HueShift·SSS 오프셋) | 프리셋 + 오프셋 적용 |

MBC OQ-1 해소: 인터페이스 확정.
MBC OQ-4 해소: `FGrowthState.LastCardDay` — MBC가 DryingFactor 계산에 읽기 전용 사용.

#### 4. Card System (#8) — 인바운드 (Stage 1 Subscriber)

| 이벤트 | 데이터 | Growth 반응 |
|---|---|---|
| `FOnCardOffered` | `FName CardId` | CR-1 (태그 가산 → SaveAsync → **Stage 2 broadcast**) |

Card System은 Stage 1 이벤트만 발행. Growth가 Pipeline에서 카드 데이터를 직접 pull.

#### 4b. Game State Machine (#5) + Dream Trigger (#9) — 아웃바운드 (Stage 2 Publisher, ADR-0005 2026-04-19)

| 이벤트 | 데이터 | 시점 |
|---|---|---|
| **`FOnCardProcessedByGrowth`** | `const FGrowthProcessResult&` (CardId + bIsDay21Complete + FinalFormId) | Growth CR-1 step 7 — 태그 가산 + CR-5 FinalForm 결정 + SaveAsync 완료 **후 마지막 statement** |

- Subscribers: **GSM** (Offering→Waiting 또는 Day 21 Farewell P0 전환), **Dream Trigger** (CR-1 평가 — 최신 TagVector 보장)
- **책임**: Growth는 Stage 2 forwarder — Card의 Stage 1 이벤트를 받아 처리 완료 후 Stage 2로 전파. UE Multicast Delegate 등록 순서 비공개 계약 회피
- Day 21 Final Form 정확성의 중심: Growth의 `FinalFormId` 확정 + `FOnFinalFormReached` 발행 + Stage 2 broadcast가 모두 같은 call stack 내 수행 → GSM이 Farewell P0 진입 시점에 `FSessionRecord.FinalFormId`가 이미 저장 완료 상태

#### 5. Time & Session (#1) — 인바운드

| 이벤트 | 데이터 | Growth 반응 |
|---|---|---|
| `FOnDayAdvanced` | `int32 NewDayIndex` | CR-4 (단계 평가) |

#### 6. Dream Trigger (#9) — 아웃바운드 (pull API)

| API | 데이터 | 시점 |
|---|---|---|
| `GetTagVector()` | `TMap<FName, float>` (읽기 전용 사본) | Dream이 트리거 조건 평가 시 |
| `GetCurrentStage()` | `EGrowthStage` | Dream이 단계별 꿈 필터링 시 |

Growth는 Dream에 push하지 않음. Dream이 `FOnDayAdvanced` 수신 후 자체 평가 시 Growth pull.

## Formulas

### F1 — Tag Accumulation (GAE.TagAccumulation)

카드 건넴 시 태그별 누적 가산.

`V[tag] += W_card × W_cat[cat(tag)]`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| V[tag] | V | float | [0, unbounded] | 태그의 누적 가중치. `FGrowthState.TagVector[tag]` |
| W_card | Wc | float | {1.0f} | 카드 1장당 기여 가중치. MVP 고정 1.0f |
| W_cat[cat(tag)] | Wk | float | [0.0, 2.0] | 카테고리 승수. MVP 기본 1.0f. 0.0 = 해당 카테고리 완전 무시 (에디터 경고 발생). `UGrowthConfigAsset.CategoryWeightMap` |
| cat(tag) | — | FName | {Season, Element, Emotion} | 태그의 카테고리 네임스페이스 (CR-2 접두사 파싱) |

**Output Range:** V[tag] ≥ 0, 상한 없음. MVP에서 Wc=1.0f, Wk=1.0f이면 단순 `+1.0f` 가산.

**Example:** 카드 "봄비" (Tags: `[Season.Spring, Emotion.Calm]`), MVP 설정:
```
전: TagVector = { Season.Spring: 2.0, Emotion.Calm: 1.0 }
후: TagVector = { Season.Spring: 3.0, Emotion.Calm: 2.0 }
```

### F2 — Vector Normalization (GAE.VectorNorm)

최종 형태 결정을 위한 누적 벡터 정규화.

`V_norm[tag] = V[tag] / TotalWeight`
`TotalWeight = Σ V[tag]` (모든 태그 합산)

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| V[tag] | V | float | [0, unbounded] | F1 누적 후의 태그별 가중치 |
| TotalWeight | T | float | [0, unbounded] | 전체 태그 가중치 합. 0이면 Default Form |
| V_norm[tag] | Vn | float | [0.0, 1.0] | 정규화된 태그 비율. 모�� 태그 합 = 1.0 |

**Output Range:** V_norm ∈ [0.0, 1.0]. 합계 = 1.0 (부동소수점 오차 ≤ 1e-5).

**TotalWeight = 0 가드:** `if (TotalWeight < KINDA_SMALL_NUMBER)` → 정규화 건너뜀, Default Form 반환 (CR-5).

**Example:**
```
TagVector = { Season.Spring: 3.0, Emotion.Calm: 2.0, Element.Water: 1.0 }
TotalWeight = 6.0
V_norm = { Season.Spring: 0.500, Emotion.Calm: 0.333, Element.Water: 0.167 }
```

### F3 — Form Score (GAE.FormScore)

최종 형태 선택을 위한 내적 점수.

`Score(form) = Σ_tag ( V_norm[tag] × RequiredTagWeights[form][tag] )`
`FinalFormId = argmax(Score)`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| V_norm[tag] | Vn | float | [0.0, 1.0] | F2 정규화 벡터 |
| RequiredTagWeights[form][tag] | R | float | [0.0, 1.0] | FFinalFormRow의 이상적 태그 분포. 합계 = 1.0 권고 |
| Score(form) | S | float | [0.0, unbounded] | V_norm과 RequiredTagWeights의 내적. RequiredTagWeights 합계=1.0이면 Score∈[0.0,1.0], 합계>1.0이면 Score>1.0 가능 (EC-13). argmax 상대 비교이므로 절대 Score 무의미 |
| FinalFormId | — | FName | FFinalFormRow key | Score 최대 형태의 ID |

**Output Range:** RequiredTagWeights 합계=1.0 준수 시 Score ∈ [0.0, 1.0]. 합계>1.0이면 Score>1.0 가능 (EC-13 참조). 12개 태그 균등 분포 시 완벽 일치 Score ≈ 0.083. **argmax 상대 비교이므로 절대 Score 값에는 의미가 없다** — 형태 간 순서만 유효. 동점 시 FormId 사전순 첫 번째.

**디자인 권고:** `RequiredTagWeights` 합계 = 1.0으로 정규화 설계. `UEditorValidatorBase`에서 행 저장 시 합계 검증 추가.

**Example — 태그 3개, 형태 2개:**
```
V_norm = { Season.Spring: 0.50, Emotion.Calm: 0.33, Element.Water: 0.17 }

FormA "봄 이끼" RequiredTagWeights:
  { Season.Spring: 0.70, Element.Water: 0.20, Emotion.Calm: 0.10 }

FormB "심연 이끼" RequiredTagWeights:
  { Element.Water: 0.60, Emotion.Calm: 0.30, Season.Winter: 0.10 }

Score(FormA) = (0.50 × 0.70) + (0.33 × 0.10) + (0.17 × 0.20) = 0.417
Score(FormB) = (0.50 × 0.00) + (0.33 × 0.30) + (0.17 × 0.60) = 0.201

FinalFormId = "FormA" (0.417 > 0.201)
```

### F4 — Material Variation Hints (GAE.MaterialHints)

MVP 머티리얼 변주를 위한 지배 카테고리 기반 오프셋.

```
CategoryWeight[cat] = Σ_{tag ∈ cat} V[tag]
DominantCategory = argmax(CategoryWeight)
DominantTag = argmax(V[tag] for tag ∈ DominantCategory)
DominanceRatio = CategoryWeight[DominantCategory] / TotalWeight

HueShift_final = clamp(HueShift_preset + HueShift_offset[DominantTag] × DominanceRatio, -0.1, 0.1)
SSS_final = clamp(SSS_preset + SSS_offset[DominantTag] × DominanceRatio, 0.0, 1.0)
```

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| CategoryWeight[cat] | Cw | float | [0, unbounded] | 카테고리 내 전체 태그 누적 합 |
| DominantCategory | — | FName | {Season, Element, Emotion} | 최대 CategoryWeight 카테고리. 동점 시 사전순 |
| DominantTag | — | FName | tag names | 지배 카테고리 내 최대 태그. 오프셋 선택 키 |
| DominanceRatio | Dr | float | [0.0, 1.0] | 지배 카테고리의 전체 비율 |
| HueShift_preset | Hp | float | [-0.1, 0.1] | MBC 성장 단계 프리셋 기본 HueShift |
| SSS_preset | Sp | float | [0.0, 1.0] | MBC 성장 단계 프리셋 기본 SSS |
| HueShift_offset | Ho | float | [-0.02, +0.02] | 태그별 최대 HueShift 편차. UGrowthConfigAsset |
| SSS_offset | So | float | [-0.05, +0.05] | 태그별 최대 SSS 편차 |

**오프셋 테이블** (UGrowthConfigAsset 외부화):

| DominantTag | HueShift_offset | SSS_offset |
|---|---|---|
| Season.Spring | +0.02 | +0.05 |
| Season.Summer | +0.01 | +0.03 |
| Season.Autumn | -0.01 | +0.02 |
| Season.Winter | -0.02 | -0.03 |
| Element.* | 0.00 | 0.00 |
| Emotion.* | 0.00 | 0.00 |

비계절 카테고리는 오프셋 0. Full에서 Element·Emotion 오프셋 확장 가능.

**Output Range:** HueShift_final ∈ [-0.1, 0.1] (clamp), SSS_final ∈ [0.0, 1.0] (clamp). TotalWeight=0 → DominanceRatio=0 → 오프셋 0.

**Example:**
```
TagVector = { Season.Spring: 6.0, Season.Summer: 2.0, Element.Water: 1.0, Emotion.Calm: 1.0 }
TotalWeight = 10.0
CategoryWeight[Season] = 8.0, [Element] = 1.0, [Emotion] = 1.0
DominantCategory = Season, DominantTag = Season.Spring (6.0 > 2.0)
DominanceRatio = 8.0 / 10.0 = 0.80

HueShift_final = clamp(0.00 + 0.02 × 0.80, -0.1, 0.1) = 0.016
SSS_final = clamp(0.55 + 0.05 × 0.80, 0.0, 1.0) = 0.590
```

### F5 — MVP Stage Day Scaling (GAE.DayScaling)

Full 21일 임계를 MVP 축약 아크로 변환하는 참조 공식.

`DayThreshold_MVP(stage) = max(1, round(DayThreshold_Full(stage) × (GameDurationDays_MVP / GameDurationDays_Full)))`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| DayThreshold_Full | Df | int32 | [1, 21] | Full 아크 단계 전환 임계일 |
| GameDurationDays_Full | Gf | int32 | {21} | 레지스트리 상수 |
| GameDurationDays_MVP | Gm | int32 | {7} | MVP 축약 총 일수 |
| DayThreshold_MVP | Dm | int32 | [1, Gm-1] | 스케일된 MVP 임계일 |

**Output Range:** DayThreshold_MVP ∈ [1, GameDurationDays_MVP - 1]. FinalForm 임계는 항상 GameDurationDays.

**변환표 (7/21 = 1/3):**

| 단계 전환 | Full 임계 | 공식 결과 | CR-4 지정값 | 차이 사유 |
|---|---|---|---|---|
| Sprout → Growing | 6 | 2 | **3** | 첫 전환 앵커 모먼트 1일 여유 |
| Growing → Mature | 15 | 5 | **5** | 일치 |
| Mature → FinalForm | 21 | 7 | **7** | = GameDurationDays |

이 공식은 초기값 산출 근거. **최종 임계값은 `UGrowthConfigAsset` Tuning Knob에서 독립 설정** — MVP↔Full 동시 지원의 핵심 구조.

## Edge Cases

### Zero / Extreme Input

- **EC-1 — If DayIndex = GameDurationDays에 도달했을 때 TotalCardsOffered = 0이면**: F2 TotalWeight < KINDA_SMALL_NUMBER 가드 → 정규화 스킵 → `FinalFormId = "Default"` → `FOnFinalFormReached("Default")` 발행. MaterialHints 오프셋 전부 0. 별도 에러 처리 없음 (의도된 경로).

- **EC-2 — If 단일 카드만 건넸고 태그가 1개뿐이면**: TagVector = `{TagX: 1.0}`, V_norm = `{TagX: 1.0}`. TagX를 가장 많이 요구하는 Form 선택. DominanceRatio = 1.0 (EC-15와 겹침). 정상 경로.

- **EC-3 — If 모든 카드가 동일한 태그 1개만 가지면**: TagVector = `{TagX: N}`, V_norm = `{TagX: 1.0}`. Score 최대 Form 선택. 극단적이지만 공식 내 처리됨.

- **EC-4 — If RequiredTagWeights가 플레이어 TagVector에 없는 태그만 보유하면**: `V_norm[missing_tag]` = 0.0 → Score = 0.0. 모든 Form의 Score = 0.0이면 동점 → FormId 사전순 첫 번째. 구현 시 `TMap::FindRef` (기본값 0.0f) 패턴 사용.

### Timing

- **EC-5 — If `FOnCardOffered`와 `FOnDayAdvanced`가 동일 프레임에 도착하면**: CR-1(태그 가산)이 먼저, CR-4(단계 평가)가 나중. 같은 날 건넨 카드가 벡터에 포함된 상태에서 단계 평가.

- **EC-6 — If `FOnCardOffered`가 Resolved 상태에서 수신되면**: 종단 상태 — 모든 이벤트 무시. TagVector, LastCardDay, TotalCardsOffered 불변. SaveAsync 미호출. 상태 가드: `if (CurrentState != Accumulating) return;`

- **EC-7 — If `FOnDayAdvanced`가 Resolved 상태에서 수신되면**: EC-6과 동일. CR-4 미실행. 중복 `FOnFinalFormReached` 발행 없음.

### Data Failure

- **EC-8 — If `GetCardRow(CardId)`가 nullptr를 반환하면**: CR-1 전체 스킵 — 태그 가산 없음, LastCardDay 미갱신, SaveAsync 미호출. `UE_LOG(LogGrowth, Warning)` 1회. 플레이어에게 에러 표시 없음 (Pillar 1).

- **EC-9 — If `GetAllGrowthFormRows()`가 빈 배열을 반환하면**: argmax 대상 없음 → Default Form 폴백. EC-1과 동일 경로. `UEditorValidatorBase`에서 0행 상태 저장 시 경고.

### Day Gaps

- **EC-10 — If 부재로 DayIndex가 Sprout에서 Day 16으로 점프하면**: CR-4 다단계 건너뛰기 — `FOnGrowthStageChanged(Mature)` **1회만** 발행. 중간 Growing 미경유. MBC E1과 일관.

- **EC-11 — If 시스템 시계 후방 이동으로 LastCardDay > 현재 DayIndex가 발생하면**: Time System이 BACKWARD_GAP으로 분류 → `FOnDayAdvanced` 미발행 → Growth 상태 변경 없음. 단, 저장된 `LastCardDay`가 미래값을 유지. **MBC cross-system contract**: DryingFactor 계산 시 `DayGap = max(0, DayIndex - LastCardDay)` 가드 필수. MBC dependency 섹션에 명시 필요.

### Form Selection

- **EC-12 — If 모든 FFinalFormRow의 Score가 완전 동점이면**: FormId FName 사전순 첫 번째 선택. 부동소수점 완전 동점은 극히 드물지만, 동일 RequiredTagWeights ��계 시 발생 가능. `UEditorValidatorBase`에서 동일 RequiredTagWeights 경고 권고.

- **EC-13 — If RequiredTagWeights 합이 1.0이 아닌 경우**: 합 > 1.0이면 Score > 1.0 가능하나 argmax 대소 관계 보존. 합 < 1.0도 마찬가지. 에디터 경고(비블로킹). 런타임 fallback 불필요.

### Material Hints

- **EC-14 — If DominantCategory 동점이면 (F4)**: FName 사전순 첫 번째 ("Element" < "Emotion" < "Season"). 결정론적. 플레이어에게 보이지 않는 내부 타이브레이커 (반투명한 인과 원칙과 일관). **카테고리 내 DominantTag 동점** (예: `Season.Spring == Season.Summer`)에도 동일 규칙 적용 — FName 사전순 첫 번째. 모든 argmax 동점은 FName 사전순으로 통일.

- **EC-15 — If DominanceRatio = 1.0이면 (모든 카드가 단일 카테고리)**: 오프셋 최대 적용. Season.Spring일 때 SSS_final = clamp(Sp + 0.05 × 1.0). SSS_preset 최대값이 0.95 이하면 clamp 도달 없음. **MBC Tuning Knob 권고**: SSS_preset 상한을 0.80으로 제한하여 오프셋 여유 확보.

### Persistence

- **EC-16 — If SaveAsync 완료 전 앱 강제 종료되면**: 직전 저장 시점의 FGrowthState로 복원 — 마지막 카드 1장 누적 누락. 하루 1장 구조에서 영향 미미. 별도 이중 쓰기 불필요.

- **EC-17 — If FGrowthState.FinalFormId가 비어있지 않은데 CurrentStage ≠ FinalForm인 채로 로드되면**: FinalFormId 비어있지 않음 우선 → Resolved로 직행. `FOnFinalFormReached(FinalFormId)` **1회 재발행** — MBC가 최종 형태 메시를 화면에 표시하려면 이벤트 필요.

## Dependencies

| System | 방향 | 성격 | 인터페이스 | MVP/VS |
|---|---|---|---|---|
| **Data Pipeline (#2)** | 아웃바운드 (pull) | **Hard** | `GetCardRow(CardId)`, `GetAllGrowthFormRows()`, `GetGrowthFormRow(FormId)` | MVP |
| **Save/Load (#3)** | 양방향 | **Hard** | `FGrowthState` typed slice — Init에서 read, 카드 건넴마다 `SaveAsync(ECardOffered)` | MVP |
| **Time & Session (#1)** | 인바운드 | **Hard** | `FOnDayAdvanced(int32 NewDayIndex)` — 단계 평가 트리거 | MVP |
| **Card System (#8)** | 인바운드 (Stage 1) | **Hard** | `FOnCardOffered(FName CardId)` — 태그 가산 트리거 (Growth가 Stage 1 구독, ADR-0005) | MVP |
| **Game State Machine (#5)** | 아웃바운드 (Stage 2) | **Hard** | **`FOnCardProcessedByGrowth(const FGrowthProcessResult&)`** — GSM이 Offering→Waiting 또는 Day 21 Farewell P0 전환 트리거로 구독 (ADR-0005 2026-04-19) | MVP |
| **Dream Trigger (#9)** | 아웃바운드 (Stage 2) | **Hard** | **`FOnCardProcessedByGrowth`** — Dream Trigger CR-1 평가 트리거로 구독 (ADR-0005) — 최신 TagVector 보장 | MVP |
| **Moss Baby Character (#6)** | 아웃바운드 | **Hard** | `FOnGrowthStageChanged(EGrowthStage)`, `FOnFinalFormReached(FName)`, `GetMaterialHints()`, `GetLastCardDay()` | MVP |
| **Dream Trigger (#9)** | 아웃바운드 (pull) | **Soft** | `GetTagVector()`, `GetCurrentStage()` — Dream이 트리거 평가 시 pull | MVP |
| **Farewell Archive (#17)** | 아웃바운드 (pull) | **Soft** | `GetFinalFormId()`, `GetTagVector()` — 작별 화면에서 최종 형태 표시 | VS |

### Bidirectional Consistency Notes

- **MBC (#6)**: Interactions §3에서 인터페이스 확정. MBC OQ-1 (FOnGrowthStageChanged·FOnFinalFormReached 확정) 및 OQ-4 (LastCardDay → FGrowthState 소유) 해소
- **Save/Load (#3)**: atomicity contract §5 준수 — CR-1 step 2–6 동일 함수 내. AC에서 검증
- **Dream Trigger (#9)**: GDD 미작성 — pull API (`GetTagVector`, `GetCurrentStage`)는 provisional. Dream GDD 작성 시 확인 필요
- **Card System (#8)**: GDD 미작성 — `FOnCardOffered` 인터페이스는 MBC에서 이미 사용 중이므로 안정적
- **Farewell Archive (#17)**: VS 스코프. pull API는 provisional

### Cross-System Contract (EC-11)

MBC는 DryingFactor 계산 시 `DayGap = max(0, DayIndex - FGrowthState.LastCardDay)` 가드 필수. 시스템 시계 후방 이동 시 `LastCardDay > DayIndex`가 가능하므로 음수 방어 필요. MBC dependency 섹션 갱신 대상.

## Tuning Knobs

| Knob | 타입 | 기본값 | 안전 범위 | 영향 |
|---|---|---|---|---|
| `GrowingDayThreshold` | int32 | 6 (MVP: 3) | [3, GameDurationDays-3] | Sprout→Growing 전환 일차. 너무 빠르면 첫 전환 앵커 약화 |
| `MatureDayThreshold` | int32 | 15 (MVP: 5) | [GrowingDay+2, GameDurationDays-1] | Growing→Mature 전환 일차 |
| `GameDurationDaysOverride` | int32 | 0 (=레지스트리 값) | [5, 30] | 0이면 GameDurationDays(21) 사용. MVP 7일 테스트용 오버라이드 |
| `DefaultFormId` | FName | "Default" | — | TotalWeight=0 또는 테이블 빈 경우 폴백 형태 ID |
| `CategoryWeightMap` | TMap\<FName, float\> | 전부 1.0f | [0.0, 2.0] per entry | 카테고리별 태그 가산 승수 (F1). 0.0 = 해당 카테고리 무시 (에디터 경고). Full에서 차등 적용 |
| `TagCategories` | TArray\<FName\> | {Season, Element, Emotion} | — | 허용 카테고리 목록. 코드 내 하드코딩 금지 |
| `HueShiftOffsetMap` | TMap\<FName, float\> | CR-6 테이블 참조 | [-0.05, +0.05] | 태그별 HueShift 최대 편차 (F4) |
| `SSSOffsetMap` | TMap\<FName, float\> | CR-6 테이블 참조 | [-0.10, +0.10] | 태그별 SSS 최대 편차 (F4) |

**외부화**: 모든 Knob은 `UGrowthConfigAsset : UPrimaryDataAsset`에 `UPROPERTY(EditAnywhere)` 필드로 노출. Data Pipeline 경유 로드.

**Knob 상호작용 경고**:
- `GrowingDayThreshold` ≥ `MatureDayThreshold`이면 Growing 단계 소멸 → `UEditorValidatorBase`에서 `Growing < Mature < GameDuration` 순서 검증
- `CategoryWeightMap`에 0.0f 입력 시 해당 카테고리 완전 무시 — 의도적 설계 가능하나 에디터 경고 출력 권고

## Visual/Audio Requirements

이 시스템은 순수 데이터 처리 시스템이다. 시각·오디오 출력을 직접 생성하지 않음. 머티리얼 변주 힌트(CR-6)는 MBC(#6)가 소비하여 시각화하고, 성장 이벤트는 MBC와 Lumen Dusk Scene(#11)이 시각화한다.

## UI Requirements

이 시스템은 UI 위젯을 생성하지 않는다. 성장 관련 UI가 필요할 경우(진행 상태 등) Pillar 2에 의해 숫자·바·퍼센트 표시는 금지이며, 시각적 변화만 허용된다.

## Acceptance Criteria

### Tag Accumulation & Persistence

**AC-GAE-01** | `AUTOMATED` | BLOCKING
- **GIVEN** Accumulating 상태, `TagVector = {}` 초기, 카드 `Tags = [Season.Spring, Emotion.Calm]`, **WHEN** `FOnCardOffered(CardId)` 발행, **THEN** `TagVector["Season.Spring"] == 1.0f`, `TagVector["Emotion.Calm"] == 1.0f`, `TotalCardsOffered == 1`, `LastCardDay == CurrentDayIndex`

**AC-GAE-02** | `CODE_REVIEW` | BLOCKING
- **GIVEN** CR-1 구현 소스, **WHEN** 소스 검사, **THEN** Step 2–5(태그 가산·LastCardDay·TotalCardsOffered·MaterialHints 갱신) + step 6 `SaveAsync(ECardOffered)` + **step 7 `OnCardProcessedByGrowth.Broadcast` (ADR-0005 Stage 2)** 호출 사이에 `co_await`, `AsyncTask`, `Tick` 분기 없음 — 동일 call stack 내 (in-memory 일관성 계약 + Day 21 순서 보장)

**AC-GAE-03** | `AUTOMATED` | BLOCKING
- **GIVEN** `FGiftCardRow` 검증 로직 (에디터: `UEditorValidatorBase` 서브클래스, 런타임: Initialize 시 DataTable 순회), **WHEN** `Tags.Num() == 0`인 행 검증, **THEN** 에디터: `EDataValidationResult::Invalid` 반환 (저장 차단). 런타임: `UE_LOG(Error)` + 해당 행 카탈로그 등록 스킵. 참고: `UEditorValidatorBase`는 에디터 전용(`WITH_EDITOR`) — 패키지 빌드 스키마 가드를 위해 런타임 검증 병행 필수

### Growth Stage Transitions

**AC-GAE-04** | `AUTOMATED` | BLOCKING
- **GIVEN** Accumulating / Sprout, `GrowingDayThreshold = 3`, **WHEN** `FOnDayAdvanced(3)` 수신, **THEN** `CurrentStage == Growing`, `FOnGrowthStageChanged(Growing)` 정확히 1회 발행

**AC-GAE-05** | `AUTOMATED` | BLOCKING
- **GIVEN** Accumulating / Sprout, `GrowingDayThreshold = 3`, `MatureDayThreshold = 5`, 이전 `FOnDayAdvanced` 미수신 (DayIndex 기준 = 0), **WHEN** `FOnDayAdvanced(16)` 수신 (다단계 건너뛰기), **THEN** `CurrentStage == Mature`, `FOnGrowthStageChanged` **Mature 1회만** 발행 (Growing 미발행)

### Final Form Determination

**AC-GAE-06** | `AUTOMATED` | BLOCKING
- **GIVEN** `TagVector = {Season.Spring: 3.0, Emotion.Calm: 2.0, Element.Water: 1.0}`, FormA RequiredTagWeights = `{Season.Spring: 0.70, Element.Water: 0.20, Emotion.Calm: 0.10}`, FormB RequiredTagWeights = `{Element.Water: 0.60, Emotion.Calm: 0.30, Season.Winter: 0.10}`, **WHEN** `FOnDayAdvanced(GameDurationDays)` 수신, **THEN** Score(FormA) ≈ 0.417 (±1e-4), Score(FormB) ≈ 0.201 (±1e-4), `FinalFormId == "FormA"`, `FOnFinalFormReached("FormA")` 정확히 1회 발행

**AC-GAE-07** | `AUTOMATED` | BLOCKING
- **GIVEN** `TotalCardsOffered == 0` (TotalWeight=0), `DefaultFormId = "Default"`, **WHEN** `FOnDayAdvanced(GameDurationDays)` 수신, **THEN** `FinalFormId == "Default"`, `FOnFinalFormReached("Default")` 발행 (EC-1)

### Material Variation Hints

**AC-GAE-08** | `AUTOMATED` | BLOCKING
- **GIVEN** `TagVector = {Season.Spring: 6.0, Season.Summer: 2.0, Element.Water: 1.0, Emotion.Calm: 1.0}`, HueShift_offset[Season.Spring]=+0.02, SSS_offset[Season.Spring]=+0.05, **WHEN** `GetMaterialHints()` 호출, **THEN** DominantCategory == Season, DominantTag == Season.Spring, DominanceRatio ≈ 0.80 (±1e-5), HueShiftOffset ≈ 0.016 (±1e-5), SSSOffset ≈ 0.040 (±1e-5). 반환값은 raw 오프셋 — MBC 프리셋·clamp 미포함

**AC-GAE-09** | `AUTOMATED` | BLOCKING
- **GIVEN** `TotalWeight == 0`, **WHEN** `GetMaterialHints()` 호출, **THEN** HueShiftOffset == 0.0, SSSOffset == 0.0, DominanceRatio == 0.0 (raw 오프셋 전부 0)

### FGrowthState Persistence

**AC-GAE-10** | `INTEGRATION` | BLOCKING
- **GIVEN** `TagVector = {Season.Spring: 3.0}`, `CurrentStage = Growing`, `TotalCardsOffered = 3`, `LastCardDay = 2`를 `SaveAsync`로 저장, **WHEN** 앱 재시작 후 Load 완료, **THEN** 모든 필드가 저장 전 값과 동일, Growth가 Growing Accumulating 상태로 복원

### FFinalFormRow Validation

**AC-GAE-11a** | `AUTOMATED` | BLOCKING
- **GIVEN** `FFinalFormRow` 검증 로직 (에디터: `UEditorValidatorBase` 서브클래스, 런타임: Initialize 시 DataTable 순회), **WHEN** `RequiredTagWeights` 비어있거나 합계 = 0인 행 검증, **THEN** 에디터: `EDataValidationResult::Invalid` 반환 (저장 차단). 런타임: `UE_LOG(Error)` + 해당 행 Score 계산 스킵 (Default Form 폴백 경로로 합류). AC-GAE-03과 동일 이중 가드 패턴

**AC-GAE-11b** | `AUTOMATED` | ADVISORY
- **GIVEN** `FFinalFormRow` 검증 로직, **WHEN** `RequiredTagWeights` 합계 > 0이나 합계 ≠ 1.0 (±1e-3)인 행 검증, **THEN** `EDataValidationResult::Valid` + Warning 메시지 1개 (비블로킹 경고)

### Formula Verification

**AC-GAE-12** | `AUTOMATED` | BLOCKING
- **GIVEN** `TagVector = {Season.Spring: 2.0, Emotion.Calm: 1.0}`, `CategoryWeightMap` 전부 1.0f, **WHEN** `Tags = [Season.Spring, Emotion.Calm]` 카드 건넴, **THEN** `TagVector["Season.Spring"] == 3.0f`, `TagVector["Emotion.Calm"] == 2.0f` (F1)

**AC-GAE-13** | `AUTOMATED` | BLOCKING
- **GIVEN** `TagVector = {Season.Spring: 3.0, Emotion.Calm: 2.0, Element.Water: 1.0}`, **WHEN** F2 정규화 실행, **THEN** V_norm ≈ {0.500, 0.333, 0.167} (±1e-5), 합계 = 1.0 (±1e-5)

**AC-GAE-14** | `AUTOMATED` | BLOCKING
- **GIVEN** TagVector = {Season.Spring: 3.0, Emotion.Calm: 2.0, Element.Water: 1.0} (F2 정규화를 내부에서 수행), FormA RequiredTagWeights = {Season.Spring: 0.70, Element.Water: 0.20, Emotion.Calm: 0.10}, FormB = {Element.Water: 0.60, Emotion.Calm: 0.30, Season.Winter: 0.10}, **WHEN** F2 정규화 + F3 Score 계산 파이프라인 실행, **THEN** Score(FormA) ≈ 0.417 (±1e-3), Score(FormB) ≈ 0.201 (±1e-3), FinalFormId == "FormA"

**AC-GAE-15** | `AUTOMATED` | BLOCKING
- **GIVEN** DominanceRatio = 1.0, HueShift_offset[DominantTag] = +0.02, SSS_offset[DominantTag] = +0.05, **WHEN** `GetMaterialHints()` 호출, **THEN** HueShiftOffset = 0.02 × 1.0 = **0.02**, SSSOffset = 0.05 × 1.0 = **0.05**. Growth는 raw 오프셋만 반환 — 최종 clamp는 MBC 책임 (preset + offset 합산 후 HueShift [-0.1, 0.1], SSS [0.0, 1.0] 범위로 clamp)

### Edge Case Verification

**AC-GAE-16** | `AUTOMATED` | BLOCKING
- **GIVEN** Accumulating / Sprout, `GrowingDayThreshold = 3`, `TagVector = {}`, 카드 Tags = `[Season.Spring]`, **WHEN** 동일 프레임에 `FOnCardOffered(CardId)` → `FOnDayAdvanced(3)` 순서 처리, **THEN** `TagVector["Season.Spring"] == 1.0f` (단계 평가 시점에 카드 태그 포함), `CurrentStage == Growing`, `FOnGrowthStageChanged(Growing)` 정확히 1회 발행

**AC-GAE-17** | `AUTOMATED` | BLOCKING
- **GIVEN** Resolved 상태 (FinalFormId 확정), `TagVector = {Season.Spring: 3.0}`, **WHEN** `FOnCardOffered`와 `FOnDayAdvanced` 각각 발행, **THEN** TagVector·TotalCardsOffered·CurrentStage·FinalFormId 불변, SaveAsync 미호출, 추가 이벤트 미발행

**AC-GAE-18** | `AUTOMATED` | BLOCKING
- **GIVEN** `GetCardRow(CardId)` → nullptr (존재하지 않는 CardId), `TagVector = {Season.Spring: 1.0}`, `TotalCardsOffered = 1`, **WHEN** `FOnCardOffered` 발행, **THEN** TagVector 불변, LastCardDay 미갱신, SaveAsync 미호출, `LogGrowth Warning` 1회 출력

**AC-GAE-19** | `INTEGRATION` | BLOCKING
- **GIVEN** 저장 데이터 `FinalFormId = "FormA"` (비어있지 않음), `CurrentStage = Mature` (≠ FinalForm), **WHEN** 앱 재시작 후 Load 완료, **THEN** Resolved 상태 진입, `FOnFinalFormReached("FormA")` 1회 재발행, `FOnGrowthStageChanged` 추가 미발행 (EC-17)

**AC-GAE-20** | `MANUAL` | ADVISORY
- **GIVEN** `TagVector = {Season.Spring: 2.0}`, `TotalCardsOffered = 2` 상태에서 카드 1장 건넴 직후, **WHEN** 테스터가 Task Manager로 앱 프로세스 강제 종료 → 앱 재시작 후 Load 완료, **THEN** `TotalCardsOffered ≥ 0`, `TagVector` 모든 값 ≥ 0.0f, `FinalFormId == ""` 또는 유효 FormId, `CurrentStage ∈ {Sprout, Growing, Mature}`, `TotalCardsOffered` 차이 ≤ 1 (직전 완료 저장 대비). 수동 검증: 세이브 파일 손상 없이 이전 완료 저장으로 복원됨을 확인 (EC-16)

### EC/Formula 추가 커버리지

**AC-GAE-21** | `AUTOMATED` | BLOCKING
- **GIVEN** `TagVector = {Emotion.Joy: 3.0, Emotion.Calm: 2.0}` (Season/Element 태그 없음), FormX RequiredTagWeights = `{Season.Spring: 0.70, Element.Water: 0.30}`, FormY RequiredTagWeights = `{Season.Winter: 0.50, Element.Earth: 0.50}`, **WHEN** `FOnDayAdvanced(GameDurationDays)` 수신, **THEN** Score(FormX) == 0.0, Score(FormY) == 0.0, `FinalFormId == "FormX"` ("FormX" < "FormY" 사전순) (EC-4 검증)

**AC-GAE-22** | `AUTOMATED` | BLOCKING
- **GIVEN** FormA RequiredTagWeights = `{Season.Spring: 0.50, Emotion.Calm: 0.50}`, FormB RequiredTagWeights = `{Season.Spring: 0.50, Emotion.Calm: 0.50}` (완전 동일), `TagVector = {Season.Spring: 1.0, Emotion.Calm: 1.0}`, **WHEN** `FOnDayAdvanced(GameDurationDays)` 수신, **THEN** Score(FormA) == Score(FormB), `FinalFormId == "FormA"` ("FormA" < "FormB" 사전순) (EC-12 검증)

**AC-GAE-23** | `AUTOMATED` | BLOCKING
- **GIVEN** `TagVector = {}` 초기, `CategoryWeightMap = {Season: 2.0f, Emotion: 0.5f, Element: 1.0f}`, 카드 Tags = `[Season.Spring, Emotion.Calm]`, **WHEN** `FOnCardOffered(CardId)` 발행, **THEN** `TagVector["Season.Spring"] == 2.0f` (W_card × W_cat = 1.0 × 2.0), `TagVector["Emotion.Calm"] == 0.5f` (W_card × W_cat = 1.0 × 0.5) (F1 W_cat≠1.0 경로 검증)

---

| AC 타입 | 수량 | Gate |
|---|---|---|
| AUTOMATED | 19 | BLOCKING (18) / ADVISORY (1: AC-GAE-11b) |
| INTEGRATION | 2 | BLOCKING |
| CODE_REVIEW | 1 | BLOCKING |
| MANUAL | 1 | ADVISORY (AC-GAE-20) |
| **합계** | **23** | — |

## Open Questions

| # | Question | Owner | Target | Status |
|---|---|---|---|---|
| OQ-GAE-1 | Full에서 DayIndex + 벡터 크기 복합 조건 Stage 전환 도입 여부 — Pillar 1 위반 리스크 vs 반투명 인과 강화 | Game Designer | Full 스코프 설계 | Open |
| OQ-GAE-2 | Full에서 카테고리별 `CategoryWeightMap` 차등 가중치 도입 여부 — 현재 전부 1.0f | Systems Designer | Full 스코프 설계 | Open |
| OQ-GAE-3 | Element·Emotion 카테고리의 머티리얼 오프셋 확장 — 현재 Season만 오프셋, 나머지 0. **Full 미도입 시 Player Fantasy 달성 불가** (일일 피드백이 Season 편향). 최소 오프셋(±0.005) 또는 대안 피드백 경로 검토 필수 | Art Director + Game Designer | Full 스코프 설계 | Open |
| OQ-GAE-4 | MBC `SSS_preset` 상한 제한 (EC-15 권고) — SSS ≤ 0.80 또는 0.95? 오프셋 여유 확보 | Art Director + MBC | MBC GDD 갱신 시 | Open |
| OQ-GAE-5 | Full 8–12종 형태 간 RequiredTagWeights 직교성 검증 방법 — 에디터 자동 검증? 디자이너 수동? | Systems Designer | Full 스코프 설계 | Open |

### Resolved (이 GDD가 해소한 외부 OQ)

| 원래 OQ | 해소 내용 |
|---|---|
| **MBC OQ-1** | `FOnGrowthStageChanged(EGrowthStage)`, `FOnFinalFormReached(FName)` 인터페이스 확정 (Interactions §3) |
| **MBC OQ-4** | `LastCardDay` → `FGrowthState` 소유 (CR-7). MBC가 DryingFactor 계산에 읽기 전용 사용 |
| **DP OQ-E-2** | 빈 Tags 카드 → Schema 차단 (CR-3). UEditorValidatorBase에서 에러 |
