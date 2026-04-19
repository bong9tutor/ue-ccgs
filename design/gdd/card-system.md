# Card System

> **Status**: In Design
> **Author**: bong9tutor + claude-code session
> **Last Updated**: 2026-04-18
> **Implements Pillar**: Pillar 2 (선물의 시학 — 카드는 숫자가 아니라 이미지·계절·감정), Pillar 1 (조용한 존재 — 놓친 날에 대한 재촉·페널티 없음)
> **Priority**: MVP | **Layer**: Feature | **Category**: Gameplay
> **Effort**: M (2–3 세션)

## Overview

Card System은 매일 3장의 선물 카드를 제시하고, 플레이어가 1장을 골라 이끼 아이에게 건네는 **일일 카드 의식(card ritual)의 데이터·로직 계층**이다. 책임은 세 가지로 나뉜다 — **(1) 일일 카드 풀 구성** — `FOnDayAdvanced` 수신 시 시즌 가중치 기반 랜덤으로 전체 카드 카탈로그에서 3장을 선택, **(2) GSM Offering 상태 연동** — Dawn 완료 시 GSM의 Prepare 신호에 Ready로 응답하여 Offering 진입을 허가, **(3) 카드 건넴 이벤트 발행** — 플레이어의 선택이 확정되면 `FOnCardOffered(CardId)`를 브로드캐스트하여 Growth Accumulation(#7, 태그 가산), GSM(#5, 상태 전환), Moss Baby Character(#6, 시각 반응)가 소비.

플레이어는 이 시스템을 **능동적으로** 경험한다 — 매일 하루 1회, 3장 중 1장을 고르는 것은 게임의 유일한 의사결정 순간이다. 그러나 이 의사결정의 *물리적 행위*(드래그·터치·클릭)는 Card Hand UI(#12)가 소유한다. Card System은 "어떤 3장이 제시되는가"와 "선택이 확정되었다"만 관장하는 순수 데이터 레이어다.

### 책임 경계 (이 시스템이 *아닌* 것)
- **카드 시각 표현 아님** — 카드 일러스트, 드래그 인터랙션, 선택 피드백은 Card Hand UI(#12) 소유
- **태그 해석 아님** — 태그(계절·원소·감정)의 누적 및 성장 단계 결정은 Growth Accumulation Engine(#7) 소유
- **카드 데이터 정의 아님** — `FGiftCardRow` 구조체 스키마와 로딩 정책은 Data Pipeline(#2) 소유. 이 GDD는 필드의 *의미*와 *카드 풀 운용 규칙*을 정의
- **카드 SFX 아님** — 카드 넘김·건넴 사운드는 Audio System(#15, VS) 소유

### 구현 결정 → ADR 후보
- **ADR 후보**: 카드 풀 재생성 시점 — Day 시작 즉시(eager) vs Offering 진입 시(lazy). MVP 10장은 차이 없지만 Full 40장 + 복잡한 시즌 가중치 시 Offering 진입 지연에 영향
- **Data Pipeline ADR-001/002** (컨테이너 선택, 로딩 전략): Card System의 `GetCardRow()`·`GetAllCardIds()` 호출 패턴에 직접 영향. 해당 ADR 확정 후 Card System 구현 세부가 결정됨

## Player Fantasy

카드를 고르는 것은 계산이 아니다 — **건넴**이다. 산책길에서 들꽃 세 송이를 발견하고, 하나를 꺾어 유리병 옆에 놓는 것과 같다. 어떤 꽃이 "맞는지"는 모른다. 그저 오늘은 이 색이, 이 계절이, 이 기분이 어울린다고 느꼈을 뿐이다. 선택은 분석이 아니라 **직감과 애정의 표현**이다.

이 시스템의 감정적 핵심은 **순수한 증여**다. 가르치는 것도, 명령하는 것도, 최적화하는 것도 아니다. 줄 수 있다는 것, 그것이 유일한 권능이다. 결과를 통제할 수 없다는 무력감조차 돌봄의 일부가 된다 — Princess Maker에서 딸에게 선물을 사주는 행위, Tamagotchi에서 밥을 주는 행위에서 수치적 보상을 빼고 감정적 의미만 남긴 형태.

Card System의 보이지 않는 절반 — 시즌 가중치, 덱 구성 규칙, 중복 방지 — 은 플레이어에게 "세계가 오늘 이 3장을 보냈다"는 감각을 만든다. 매일의 3장은 작은 운명처럼 느껴지고, 그 운명 안에서의 선택은 제한적이지만 진짜다. 이 이중 구조(시스템의 제안 + 플레이어의 응답)가 "나는 이 존재를 이해하고 있다"는 환상을 지탱한다.

**앵커 모먼트**: 카드 3장이 펼쳐지고, 플레이어의 손이 하나를 향해 움직이는 그 순간. 2–3초의 망설임. 이 망설임은 고민이 아니라 **의식(ritual)** — 매일 반복되는 작고 조용한 건넴의 시학. Card Hand UI(#12)가 이 물리적 순간을 구현하지만, Card System이 그 순간에 의미를 부여하는 것은 "어떤 3장이 왔는가"의 맥락이다.

**Pillar 연결**:
- **Pillar 2 (선물의 시학)** — 이 시스템은 Pillar 2의 직접 구현이다. 카드 위에 숫자가 없으므로 플레이어는 계산하지 않는다. "어떤 계절의 바람"이 "+3 생명력"보다 나은 이유를 묻지 않아도, 건넴의 순간이 그 답이다
- **Pillar 1 (조용한 존재)** — 증여는 본질적으로 비강제적. 잘못된 선택은 없고, 최적 선택도 없다. 어느 카드를 건네도 이끼 아이는 받아들인다
- **Pillar 4 (끝이 있다)** — 21번의 선물. 매일 건네는 행위에 무게를 부여하는 것은 이 횟수가 유한하다는 사실이다. 마지막 카드는 마지막 선물이다

**설계 원칙 — 선택의 순간에 계산하지 않아야 결과를 보았을 때 발견이 강해진다**: Growth Accumulation Engine(#7)은 "반투명한 인과"와 "되돌아보는 순간의 경이감"을 약속한다. Card System은 그 상류(upstream)로서, 선택 시 인지 부하를 최소화한다. 플레이어가 직감으로 건넸기 때문에, 며칠 뒤 성장 변화를 보고 "혹시 그때 건넨 것 때문에?"라는 역추적의 즐거움이 발생한다. 카드 선택에서 해석과 발견을 뺀 것이 아니라, 다른 시스템(꿈 텍스트 → Dream Trigger #9, 성장 변화 → Growth #7)으로 **시간차 배달**한 것이다.

## Detailed Design

### Core Rules

#### CR-CS-1 — 일일 카드 풀 구성

`FOnDayAdvanced(int32 NewDayIndex)` 수신 시 3장의 카드를 선택한다.

**단계 1: 계절 결정**

DayIndex에서 현재 계절을 도출한다.

```
SegmentLength = GameDurationDays / 4
SeasonIndex = clamp(floor((DayIndex - 1) / SegmentLength), 0, 3)
  0 → Season.Spring, 1 → Season.Summer, 2 → Season.Autumn, 3 → Season.Winter
```

Full 21일: Day 1–6 Spring, 7–11 Summer, 12–16 Autumn, 17–21 Winter (6+5+5+5일).
MVP 7일: Day 1–2 Spring, 3–4 Summer, 5–6 Autumn, 7 Winter.

계절 결정은 `UCardSystemConfigAsset`의 `SeasonDayRanges` 필드로 오버라이드 가능 (Tuning Knob §7).

**단계 2: 가중치 계산**

카탈로그 내 각 카드 C에 대해:

```
BaseWeight = C.Tags에 "Season.[현재계절]" 포함 ? InSeasonWeight : OffSeasonWeight
                                                  (기본 3.0)       (기본 1.0)

// 연속 일차 소프트 억제
PreviousDayPenalty = C ∈ PreviousDayHand ? ConsecutiveDaySuppression : 1.0
                                            (기본 0.5)

FinalWeight(C) = BaseWeight × PreviousDayPenalty
```

**단계 3: 결정론적 가중 랜덤 샘플링 (중복 없이 3장)**

```
// 결정론적 시드: 같은 날 + 같은 플레이스루 → 항상 같은 난수열
Seed = HashCombine(FSessionRecord.PlaythroughSeed, DayIndex)
Stream = FRandomStream(Seed)

Pool = Pipeline.GetAllCardIds()  // 전체 카탈로그
for i in [0, 1, 2]:
  TotalWeight = Σ FinalWeight(C) for all C in Pool
  r = Stream.FRandRange(0.0f, TotalWeight)
  누적 순회 → r이 속하는 카드 선택
  if r ≥ TotalWeight: 마지막 카드 선택 (float 경계 조건 fallback)
  Pool에서 선택된 카드 제거
```

- `PlaythroughSeed`는 `FSessionRecord`에 영속화 (첫 실행 시 `FMath::Rand()` 생성, 이후 불변)
- `FRandomStream` 사용 → 전역 RNG 비오염 + 테스트 시드 주입 가능 (AC-CS-15)
- 동일 DayIndex 재시작 시 동일 난수열 → 기저 가중치(Base Weight)가 동일하면 동일 3장 보장
- 앱 재시작 시 PreviousDayHand 소실로 소프트 억제 가중치가 달라질 수 있으나, 억제의 확률 차이(~8%p)가 미미하여 대부분의 경우 동일 3장 유지. CR-CS-5 설계 근거 참조

**결과**: `DailyHand: TArray<FName>` (3장, 순서 무의미). Card Hand UI(#12)의 배치 순서는 이 배열과 독립적.

#### CR-CS-2 — 덱 재생성 조건

- 같은 DayIndex로 `FOnDayAdvanced` 중복 수신 시: 재생성 스킵 (`if NewDayIndex <= CurrentDayIndex: return`)
- `bHandOffered == true` 상태에서 새 DayIndex 수신 시: 정상 재생성 (이전 핸드 덮어쓰기)
- **Eager 생성**: `FOnDayAdvanced` 수신 즉시 풀 구성. GSM Prepare 신호는 Ready 확인 게이트만 담당

#### CR-CS-3 — GSM Prepare/Ready 프로토콜

**Prepare 수신 시:**
1. `DailyHand.Num() >= 1` → 즉시 `CardSystemReady(bDegraded = DailyHand.Num() < 3)` 응답
2. 현재 Preparing 상태 → 샘플링 완료 후 자동 응답
3. DailyHand 미구성 (`FOnDayAdvanced` 미수신) → On-demand 생성 시도 후 응답

**5초 타임아웃**: GSM의 `CardSystemReadyTimeoutSec`(기본 5.0초) 초과 시 Degraded 플래그와 함께 강제 Offering 진입 (GSM E9 계약).

**Degraded Ready**: 카드 0–2장. 빈 슬롯 처리는 Card Hand UI(#12) GDD에 위임.

#### CR-CS-4 — FOnCardOffered 발행

Card Hand UI(#12)의 드래그 완료 시 `ConfirmOffer(FName CardId)` 호출.

**검증 게이트:**
```
ConfirmOffer(FName CardId) → bool:
  if CurrentState != Ready: return false
  if !DailyHand.Contains(CardId): return false
  if bHandOffered: return false
  // 통과 — 확정
  bHandOffered = true
  OfferedCardId = CardId
  FOnCardOffered.Broadcast(CardId)
  return true
```

- `FOnCardOffered`는 하루 최대 1회 발행
- 취소 불가 — Broadcast 이후 되돌릴 수 없음 (Pillar 1: 잘못된 선택은 없다)
- 반환값 `false`는 Card Hand UI가 드래그 롤백에 사용

**Card System은 카드의 태그를 직접 읽지 않는다.** 태그 조회는 Growth(#7)가 `FOnCardOffered` 수신 후 `Pipeline.GetCardRow(CardId)`로 자체 수행.

#### CR-CS-5 — 비영속화 설계 (결정론적 시드)

Card System은 DailyHand를 직접 세이브에 기록하지 않는다. 단, `PlaythroughSeed`는 `FSessionRecord`에 영속화하여 결정론적 재생성을 보장한다.

| 상태 | 복구 방법 |
|---|---|
| DailyHand | `FOnDayAdvanced` 재수신 시 결정론적 재생성 (`PlaythroughSeed + DayIndex` → 동일 시드 → 동일 난수열) |
| bHandOffered | `FGrowthState.LastCardDay == CurrentDayIndex`로 추론 |
| CurrentDayIndex | `FSessionRecord.DayIndex`에서 획득 |
| PlaythroughSeed | `FSessionRecord.PlaythroughSeed` — 첫 실행 시 `FMath::Rand()` 생성, 이후 불변 |
| PreviousDayHand | 재생성 불가 — 앱 재시작 시 소프트 억제 비활성화 (연속 일차 정보 소실). 의도적 수용: 억제는 UX 편의, 게임 기계에 영향 없음 |

**설계 근거**: "세계가 오늘 이 3장을 보냈다"는 Player Fantasy를 보호하기 위해, 같은 날 같은 플레이스루에서는 동일 난수열로 핸드를 구성한다. 앱 재시작 시 PreviousDayHand 소실로 소프트 억제 가중치만 달라지나, 억제의 확률 차이(~8%p)가 미미하여 대부분의 경우 동일 3장이 유지된다. 직접 영속화(TArray\<FName\>) 대신 시드 1개(uint32)만 저장하여 비영속 아키텍처의 단순성을 보존한다.

#### CR-CS-6 — 카드 필드 의미 정의

`FGiftCardRow`의 스키마는 Data Pipeline(#2)이 소유하지만, 필드의 **의미**는 Card System이 정의한다.

| 필드 | 타입 | Card System에서의 의미 |
|---|---|---|
| `CardId` | FName | 카드의 유일 식별자. Pipeline 조회 키이자 FOnCardOffered 인자. **네이밍 컨벤션**: `Card_[Season]_[Name]` PascalCase (예: `Card_Spring_Breeze`). FName은 대소문자 비구분이므로 DataTable 로드 시 중복 CardId 검증 필수 |
| `Tags` | TArray\<FName\> | 계절·원소·감정 태그. 시즌 가중치 계산(CR-CS-1)과 Growth 태그 가산(Growth CR-1)의 입력. 최소 1개, 최대 3개 권장 |
| `DisplayName` | FText | Card Hand UI(#12)의 카드 이름 표시용. Card System은 미참조 |
| `Description` | FText | Card Hand UI(#12)의 카드 설명 표시용. Card System은 미참조 |
| `IconPath` | FSoftObjectPath | Card Hand UI(#12)의 카드 일러스트 텍스처 경로. Card System은 미참조 |

**C1 schema guard 재확인**: 수치 효과 필드(int32/float/double 류) 추가 금지. Pillar 2 보호 — Data Pipeline GDD §C1 계약.

#### CR-CS-7 — 카탈로그 크기별 동작

| 상황 | 동작 |
|---|---|
| 카탈로그 ≥ 3장 (정상) | 가중 샘플링 3장 |
| 카탈로그 = 2장 | 2장 모두 선택, Degraded Ready |
| 카탈로그 = 1장 | 1장 선택, Degraded Ready |
| 카탈로그 = 0장 (Pipeline DegradedFallback) | 0장, Degraded Ready, `UE_LOG(Error)` |

MVP 10장 / Full 40장 환경에서는 "카탈로그 < 3장"은 에디터 설정 오류.

### States and Transitions

Card System의 내부 상태는 4개이며, 모두 비영속화 대상이다.

| 상태 | 설명 |
|---|---|
| `Uninitialized` | 서브시스템 생성 직후. Pipeline Ready 전 |
| `Preparing` | `FOnDayAdvanced` 처리 중 — 가중 샘플링 실행 |
| `Ready` | 핸드 구성 완료, Offering 대기 |
| `Offered` | 오늘의 카드 건넴 완료. 다음 `FOnDayAdvanced`까지 대기 |

**전환 표:**

| From | To | 트리거 | Guard | 부작용 |
|---|---|---|---|---|
| Uninitialized | Preparing | `FOnDayAdvanced(N)` | Pipeline Ready | 샘플링 시작 |
| Uninitialized | Preparing | `OnPrepareRequested()` (GSM on-demand) | Pipeline Ready | EC-CS-4: `FOnDayAdvanced` 미수신 시 on-demand 샘플링. `FSessionRecord.DayIndex` 직접 읽어 계절 결정 |
| Preparing | Ready | 샘플링 완료 | `DailyHand.Num() >= 1` | GSM Prepare 대기 중이면 즉시 Ready 응답. 이후 `bHandOffered` 복원 체크 (아래 참조) |
| Ready | Offered | `ConfirmOffer(CardId)` 성공 | CR-CS-4 검증 통과 | `FOnCardOffered` 브로드캐스트 |
| Ready | Offered | `bHandOffered` 복원 | `GrowthEngine.GetLastCardDay() == CurrentDayIndex` | 앱 재시작 시 당일 건넴 완료 복원. `FOnCardOffered` 미발행 |
| Offered | Preparing | `FOnDayAdvanced(N+1)` | `NewDayIndex > CurrentDayIndex` | 새 핸드 생성 |
| Ready | Preparing | `FOnDayAdvanced(N+1)` | `NewDayIndex > CurrentDayIndex` | 미건넴 → 핸드 재생성 |

**Day 1 시퀀스**: `FOnDayAdvanced(1)` → Uninitialized→Preparing→Ready → GSM Prepare → Ready 응답 → Dawn→Offering 전환.

**앱 재시작 (당일 미건넴)**: 모든 상태가 비영속이므로 `Uninitialized`에서 시작. `CurrentDayIndex` 초기값 = -1 (EC-CS-19). `FOnDayAdvanced(N)` 수신 → `N > -1` guard 통과 → Uninitialized→Preparing→Ready → GSM Prepare → Ready 응답 → Offering.

**앱 재시작 (당일 건넴 완료)**: `Uninitialized`에서 시작 (Offered 아님 — 비영속). `FOnDayAdvanced(N)` → Uninitialized→Preparing→Ready. Ready 진입 직후 `bHandOffered = (GrowthEngine.GetLastCardDay() == N)` 체크 → `true` → Ready→Offered 전환 (`FOnCardOffered` 미발행). GSM은 `LastPersistedState == Waiting`으로 복원하여 Prepare 미발행. Card System은 Offered 상태로 유휴.

### Interactions with Other Systems

#### 1. Data Pipeline (#2) — 아웃바운드 (pull)

| API | 시점 | 용도 |
|---|---|---|
| `Pipeline.GetAllCardIds()` → `TArray<FName>` | CR-CS-1 샘플링 시 | 전체 카탈로그 키 |
| `Pipeline.GetCardRow(FName)` → `TOptional<FGiftCardRow>` | Ready 검증 시 (선택적) | 카드 유효성 확인 |

Card System은 `void Initialize(FSubsystemCollectionBase& Collection)` override 내에서 `Collection.InitializeDependency<UDataPipelineSubsystem>()` 호출 필수. 이 API는 Initialize() override 내부에서만 접근 가능 — 생성자나 다른 멤버 함수에서 호출 불가.

#### 2. Time & Session System (#1) — 인바운드

| 이벤트 | Card System 반응 |
|---|---|
| `FOnDayAdvanced(int32 NewDayIndex)` | CR-CS-1 샘플링 → Preparing → Ready |

직접 구독 (GSM 경유 불필요 — GSM GDD §Interaction 4 계약).

#### 3. Game State Machine (#5) — 양방향

| 방향 | 신호 | 시점 |
|---|---|---|
| GSM → Card | `OnPrepareRequested()` (직접 호출) | Dawn Entry 시 |
| Card → GSM | `OnCardSystemReady(bool bDegraded)` (직접 호출) | 핸드 준비 완료 |
| Card → GSM | `FOnCardOffered(FName CardId)` (브로드캐스트) | 건넴 확정 |

GSM은 Card System의 내부 로직(덱 구성, 가중치)에 관여하지 않음. Card System은 GSM의 상태 전환 로직에 관여하지 않음.

#### 4. Growth Accumulation Engine (#7) — 아웃바운드 (이벤트)

| 이벤트 | Growth 반응 |
|---|---|
| `FOnCardOffered(FName CardId)` | CR-1: Pipeline에서 태그 pull → 벡터 가산 → 저장 |

Card System은 이벤트만 발행. 카드 내용(태그) 해석 책임 없음.

#### 5. Moss Baby Character System (#6) — 아웃바운드 (이벤트)

| 이벤트 | MBC 반응 |
|---|---|
| `FOnCardOffered(FName CardId)` | Reacting 상태 진입 (0.3s 시각 반응) |

#### 6. Card Hand UI (#12) — 인바운드 (플레이어 행위)

| API | 방향 | 시점 |
|---|---|---|
| `GetDailyHand()` → `TArray<FName>` | UI → Card | Offering 진입 시 카드 표시용 |
| `ConfirmOffer(FName CardId)` → `bool` | UI → Card | 드래그 완료 시 |

**FGuid vs FName 타입 해소**: `FOnCardOffered`의 정식 시그니처는 `FOnCardOffered(FName CardId)`. **RESOLVED 2026-04-18** — Growth #7, GSM #5, MBC #6 총 6곳 FGuid 참조 수정 완료 (OQ-CS-1 참조).

## Formulas

### F-CS-1 — 계절 결정 (SeasonForDay)

The SeasonForDay formula is defined as:

`SeasonForDay = SeasonNames[ clamp( floor((DayIndex - 1) / SegmentLength), 0, 3 ) ]`

where: `SegmentLength = GameDurationDays / 4`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|----------|--------|------|-------|-------------|
| 현재 일차 | DayIndex | int | [1, GameDurationDays] | FSessionRecord.DayIndex |
| 플레이스루 일수 | GameDurationDays | int | {7, 21} | 레지스트리 상수. MVP=7, Full=21 |
| 구간 길이 | SegmentLength | float | (0, GameDurationDays] | GameDurationDays / 4 |
| 계절 인덱스 | SeasonIndex | int | {0, 1, 2, 3} | 0=Spring, 1=Summer, 2=Autumn, 3=Winter |

**Output Range:** 항상 {Spring, Summer, Autumn, Winter} 중 하나. clamp(0,3)이 경계 초과 방지.

**Example (Full 21일, SegmentLength=5.25):**

| DayIndex | (DayIndex-1)/5.25 | floor | 계절 |
|---|---|---|---|
| 1 | 0.00 | 0 | Spring |
| 6 | 0.95 | 0 | Spring |
| 7 | 1.14 | 1 | Summer |
| 12 | 2.10 | 2 | Autumn |
| 17 | 3.05 | 3 | Winter |
| 21 | 3.81 | 3 | Winter |

**Example (MVP 7일, SegmentLength=1.75):**
Day 1–2 Spring, 3–4 Summer, 5–6 Autumn, 7 Winter.

### F-CS-2 — 카드 가중치 (CardWeight)

The CardWeight formula is defined as:

`CardWeight(C) = BaseWeight(C) × SuppressMultiplier(C)`

where:
- `BaseWeight(C) = (Season.CurrentSeason ∈ C.Tags) ? InSeasonWeight : OffSeasonWeight`
- `SuppressMultiplier(C) = (C.CardId ∈ PreviousDayHand) ? ConsecutiveDaySuppression : 1.0`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|----------|--------|------|-------|-------------|
| 제철 가중치 | InSeasonWeight | float | (0, ∞) | 기본 3.0. Tuning Knob |
| 비제철 가중치 | OffSeasonWeight | float | (0, ∞) | 기본 1.0. Tuning Knob |
| 연속 억제 계수 | ConsecutiveDaySuppression | float | (0, 1] | 기본 0.5. 1.0이면 억제 없음 |
| 전일 핸드 | PreviousDayHand | TSet\<FName\> | 원소 수 [0, 3] | 앱 재시작 시 공집합 |

**Output Range:** 최솟값 `OffSeasonWeight × ConsecutiveDaySuppression` (기본 0.5), 최댓값 `InSeasonWeight` (기본 3.0). 0 초과 보장 — 모든 카드는 선택 가능성 유지.

**Example (Spring, 10장 카탈로그):**

| 카드 | Tags 포함 계절 | 전일 핸드 | BaseWeight | Suppress | CardWeight |
|---|---|---|---|---|---|
| Card_S1 | Spring | 예 | 3.0 | 0.5 | **1.5** |
| Card_S2 | Spring | 예 | 3.0 | 0.5 | **1.5** |
| Card_S3 | Spring | 아니오 | 3.0 | 1.0 | **3.0** |
| Card_Su1 | Summer | 아니오 | 1.0 | 1.0 | **1.0** |
| Card_W1 | Winter | 아니오 | 1.0 | 1.0 | **1.0** |

총 가중치 W₁ = 1.5+1.5+3.0+(1.0×7) = **13.0**

### F-CS-3 — 선택 확률 (SelectionProbability)

The SelectionProbability formula is defined as:

가중 랜덤 without-replacement 3회 추출에서 카드 k의 DailyHand 포함 확률:

`P(k ∈ DailyHand) = 1 - ∏_{r=1}^{3} (1 - w_k / W_r)`

여기서 `W_r`은 라운드 r 시작 시 Pool에 남은 카드들의 가중치 합. 정확한 `W_r`은 이전 라운드의 선택 결과에 의존하므로 기대값으로 근사.

**Variables:**

| Variable | Symbol | Type | Range | Description |
|----------|--------|------|-------|-------------|
| 카드 가중치 | w_k | float | (0, 3.0] | F-CS-2 출력 |
| 라운드 r 총 가중치 | W_r | float | (0, ∞) | Pool_r의 가중치 합 |
| 카탈로그 크기 | N | int | [3, 200] | 전체 카드 수 |

**Output Range:** (0, 1]. 카탈로그에 3장만 있으면 모든 카드 P=1.0.

**Example (10장 카탈로그, Spring Day 2):**

| 카드 그룹 | CardWeight | 근사 P(∈ DailyHand) | 기준선(0.30) 대비 |
|---|---|---|---|
| 억제 없는 Spring (w=3.0) | 3.0 | ~0.54 | **+80%** |
| 억제된 Spring (w=1.5) | 1.5 | ~0.30 | 기준선 수준 |
| 비제철 카드 (w=1.0) | 1.0 | ~0.22 | -27% |

**설계 검증**: 소프트 억제(0.5×)가 제철 부스트(3.0×)를 절반으로 낮춰 w=1.5. 억제된 Spring 카드의 선택 확률이 기준선(0.30)과 유사 — "완전 배제가 아닌 부드러운 후순위". 비제철 카드도 ~22% 확률로 등장 — Pillar 2(반투명한 인과) 유지.

## Edge Cases

### 카탈로그 크기

- **EC-CS-1 — If 카탈로그 = 0장 (Pipeline DegradedFallback)**: DailyHand를 빈 배열로 확정, `UE_LOG(Error)` 출력, `CardSystemReady(bDegraded = true)` 즉시 응답. `FOnCardOffered` 발행 불가. GSM E9 경로로 Offering 진입 후 Waiting 자동 전환. Growth 태그 가산 없음.
- **EC-CS-2 — If 카탈로그 = 1–2장**: 가용 카드 전부를 DailyHand에 포함. `CardSystemReady(bDegraded = true)`. `ConfirmOffer`는 DailyHand 내 CardId만 수락. MVP 10장 환경에서는 에디터 설정 오류.
- **EC-CS-3 — If 모든 카드의 태그가 동일 계절 (동질적 카탈로그)**: 모든 카드의 `BaseWeight`가 동일해져 가중치 효과 소멸. 런타임 크래시 없음. 에디터 단계에서 계절 태그 분산 경고 발행 권장 (비 BLOCKING).

### 타이밍

- **EC-CS-4 — If `FOnDayAdvanced` 수신 전에 GSM Prepare 도달**: CR-CS-3 3번 분기 — On-demand 샘플링 시작. `FSessionRecord.DayIndex`를 직접 읽어 계절 결정. 정상 경로에서는 FOnDayAdvanced가 항상 선행하지만, Day 1 최초 실행 시 극단 타이밍으로 발생 가능.
- **EC-CS-5 — If Preparing 상태에서 GSM Prepare 도달**: 샘플링 완료 후 자동 Ready 응답. 정상 샘플링 시간(≪ 1ms)이 5초 타임아웃 이내.
- **EC-CS-6 — If `FOnDayAdvanced`와 `ConfirmOffer`가 동일 프레임 처리**: UE Multicast Delegate는 동기 순차. Input→Game 처리 순서에 따라 `ConfirmOffer`가 먼저 처리되면 현재 DailyHand로 정상 건넴 → 이후 `FOnDayAdvanced`로 재생성. 역순이면 새 DailyHand 생성 → `ConfirmOffer`는 이전 CardId로 게이트 2번 실패.

### ConfirmOffer 가드

- **EC-CS-7 — If ConfirmOffer가 Ready가 아닌 상태에서 호출**: 게이트 1번 실패 → `return false`. Card Hand UI 드래그 롤백.
- **EC-CS-8 — If ConfirmOffer의 CardId가 DailyHand에 없음**: 게이트 2번 실패 → `return false`. `UE_LOG(Warning)`. Card Hand UI 버그 방어.
- **EC-CS-9 — If ConfirmOffer 이중 호출 (bHandOffered = true)**: 게이트 3번 실패 → `return false`. `FOnCardOffered`는 하루 최대 1회 보장. Growth CR-1의 "하루 1회 태그 가산" 계약의 상류 방어선.

### 멀티 데이 어드밴스

- **EC-CS-10 — If FOnDayAdvanced가 복수 일차 점프 (예: Day 3→8)**: 최종 DayIndex 기준 단 1회 샘플링. 중간 날짜 별도 처리 없음. GSM E8(Dawn 1회만)과 일관.
- **EC-CS-11 — If FOnDayAdvanced가 동일 DayIndex로 중복 수신**: CR-CS-2 가드 — `NewDayIndex <= CurrentDayIndex` → 재생성 스킵. 현재 DailyHand 보존.

### DayIndex 경계

- **EC-CS-19 — If DayIndex = 0 (초기화 전 버그)**: F-CS-1은 `clamp(floor(-1/SegmentLength), 0, 3) = 0` → Spring 반환 (크래시 없음, 그러나 잘못된 계절). CR-CS-2의 `CurrentDayIndex` 초기값이 0이면 guard `NewDayIndex <= CurrentDayIndex`를 통과하여 DayIndex=0이 처리됨. **방어**: `CurrentDayIndex` 초기값을 -1로 설정하거나, `NewDayIndex < 1` 시 `UE_LOG(Warning)` + 스킵.
- **EC-CS-20 — If GameDurationDays < 4 (config 오버라이드 오류)**: F-CS-1의 `SegmentLength < 1.0` → 일부 계절이 할당 일수 0. 기능적 크래시 없으나 계절 분배 구조 붕괴. **방어**: `UCardSystemConfigAsset` PostLoad에서 `GameDurationDays ∈ {7, 21}` 검증 (config 오버라이드 경로 포함). 범위 외 값 → `UE_LOG(Error)` + 기본값(21) 폴백.

### 앱 재시작

- **EC-CS-12 — If 앱 재시작, 당일 미건넴**: `FGrowthState.LastCardDay ≠ CurrentDayIndex` → `bHandOffered = false` 추론. `FOnDayAdvanced` 재수신 → DailyHand 결정론적 재생성 (`PlaythroughSeed + DayIndex` 시드). PreviousDayHand는 공집합 (소프트 억제 비활성화) — 억제 가중치 차이로 0~1장이 달라질 수 있으나, 시드에 의한 난수열은 동일하므로 대부분의 경우 이전 세션과 같은 3장 유지.
- **EC-CS-13 — If 앱 재시작, 당일 건넴 완료**: `FGrowthState.LastCardDay == CurrentDayIndex` → `bHandOffered = true` 복원. `FOnDayAdvanced` 핸들러에서 `bHandOffered = (GrowthEngine.GetLastCardDay() == NewDayIndex)` 설정. GSM은 Waiting으로 복원하여 Prepare 미발행. 중복 건넴 방지.
- **EC-CS-14 — If FOnCardOffered 발행 직후 SaveAsync 커밋 전 앱 종료**: Growth CR-1 태그 가산은 동기 완료되었으나 `SaveAsync` 비동기 커밋 미완. 재시작 시 EC-CS-12 경로 — "미건넴"으로 추론, 다시 카드 건넬 수 있음. Save/Load per-trigger atomicity 계약상 허용된 1-commit loss.

### Day 21 (최종 일차)

- **EC-CS-15 — If Day 21 정상 카드 건넴**: F-CS-1 → Winter. 정상 3장 → `ConfirmOffer` → `FOnCardOffered` → Growth CR-4에서 `DayIndex == GameDurationDays` → `FOnFinalFormReached`. GSM E13 — Offering→Waiting 즉시 Farewell P0 전환. `FOnDayAdvanced`는 Farewell 후 미발행.
- **EC-CS-16 — If Day 21 Offering 중 LONG_GAP_SILENT 수신**: GSM E7 — Card Hand UI 강제 Hide → 즉시 Farewell P0. `FOnCardOffered` 미발행 → 21일차 태그 없이 최종 형태 결정.
- **EC-CS-17 — If Day 21 FOnCardOffered와 Farewell 전환 레이스**: `FOnCardOffered` 브로드캐스트는 동기이지만, UE Multicast Delegate의 핸들러 실행 순서는 **공개 계약이 아니다** (내부 등록 순서에 의존). Growth CR-1 태그 가산이 GSM Farewell P0 전환보다 먼저 완료되어야 최종 카드가 벡터에 반영된다. **Blocking ADR 필수** (OQ-CS-3): Multicast 등록 순서에 의존하지 않는 **명시적 호출 체인** 설계가 확정되어야 함 (예: Growth 완료 후 별도 delegate 발행 → GSM 구독, 또는 Card System이 Growth 직접 호출 후 GSM 통지).

### 크로스 시스템

- **EC-CS-18 — If Growth Engine의 GetCardRow가 nullptr 반환**: Card System은 `ConfirmOffer` 성공 처리 완료 후이므로 `bHandOffered = true`. Growth 측 "빈 건넴" — 태그 가산 없이 `LastCardDay`만 갱신. Pipeline `GetAllCardIds`/`GetCardRow` 일관성은 Data Pipeline R3 fail-close 계약이 보장.

### 구현 ADR 필요 사항

| # | 케이스 | 결정 내용 |
|---|---|---|
| 1 | EC-CS-13 | `bHandOffered` 복원을 위한 Growth `GetLastCardDay()` pull API 인터페이스 합의 |
| 2 | EC-CS-17 | ~~Blocking ADR~~ **RESOLVED by ADR-0005** (2026-04-19) — 2단계 delegate 패턴: Card 발행 `FOnCardOffered` → Growth 처리 완료 후 `FOnCardProcessedByGrowth` → GSM/Dream Trigger |
| 3 | EC-CS-18 | `GetAllCardIds`/`GetCardRow` 일관성 책임 분배 (Pipeline vs Card System null check) |

## Dependencies

| 시스템 | 방향 | 강도 | 인터페이스 | 티어 |
|---|---|---|---|---|
| **Data Pipeline (#2)** | 아웃바운드 (pull) | **Hard** | `GetAllCardIds()`, `GetCardRow(FName)` | MVP |
| **Time & Session (#1)** | 인바운드 | **Hard** | `FOnDayAdvanced(int32)` — 덱 재생성 트리거 | MVP |
| **Game State Machine (#5)** | 양방향 | **Hard** | GSM→Card: `OnPrepareRequested()`. Card→GSM: `OnCardSystemReady(bool)`. **GSM은 Stage 2 `FOnCardProcessedByGrowth` 구독 (ADR-0005 2026-04-19)** — Card의 Stage 1 `FOnCardOffered`를 직접 구독하지 않음 | MVP |
| **Growth Accumulation (#7)** | 아웃바운드 (이벤트) | **Hard** | Stage 1 `FOnCardOffered(FName)` — Growth가 구독 + CR-1 처리 후 Stage 2 `FOnCardProcessedByGrowth` 발행 (ADR-0005). Day 21 순서 보장의 중심 | MVP |
| **Moss Baby Character (#6)** | 아웃바운드 (이벤트) | **Soft** | `FOnCardOffered(FName)` — MBC가 구독해 Reacting 상태 진입 | MVP |
| **Card Hand UI (#12)** | 인바운드 | **Hard** | `GetDailyHand()`, `ConfirmOffer(FName)` — 플레이어 인터랙션 수신 | MVP |
| **Audio System (#15)** | 아웃바운드 (이벤트) | **Soft** | `FOnCardOffered(FName)` — Audio가 구독해 카드 건넴 SFX 재생 | VS |

**양방향 일관성 확인:**
- Time GDD §Interaction: "`FOnDayAdvanced`를 Card System이 직접 구독" ✓
- GSM GDD §Interaction 4: "Card System은 `FOnDayAdvanced`를 직접 구독 — GSM 경유 불필요" ✓
- Growth GDD §Interaction 4: "`FOnCardOffered` → CR-1 (Stage 1, ADR-0005)" ✓
- Data Pipeline GDD §Interaction: "`Pipeline.GetCardRow`, `GetAllCardIds`" ✓
- MBC GDD: "`FOnCardOffered` 구독해 Reacting (Stage 1)" ✓

## Tuning Knobs

| Knob | 타입 | 기본값 | 안전 범위 | 영향 | 극단 시 |
|---|---|---|---|---|---|
| `InSeasonWeight` | float | 3.0 | [1.0, 10.0] | 제철 카드 출현 확률 | > 5.0: 계절 카드만 등장, 반투명한 인과 약화. < 1.5: 계절감 소멸 |
| `OffSeasonWeight` | float | 1.0 | [0.1, 3.0] | 비제철 카드 출현 확률 | > InSeasonWeight: 비제철이 우세, 계절 의미 역전. < 0.5: 비제철 극희귀 |
| `ConsecutiveDaySuppression` | float | 0.5 | [0.05, 1.0] | 전일 핸드 카드 억제 강도 | < 0.2: 사실상 차단. = 1.0: 억제 없음. = 0: 완전 배제 → TotalWeight=0 가능, F-CS-3 퇴행 |
| `HandSize` | int | 3 | [1, 5] | 일일 제시 카드 수 | = 1: 선택권 없음 (증여만 존재). > 4: 선택 과부하, Pillar 2 위반 위험 |
| `SeasonDayRanges` | TArray\<FIntPoint\> | F-CS-1 auto | [1, GameDurationDays] | 계절별 DayIndex 구간 | 구간 겹침: 카드가 복수 계절 부스트. 빈 구간: 해당 날 OffSeasonWeight만 적용 |
| `GameDurationDays` | int | 21 (MVP: 7) | {7, 21} | F-CS-1 SegmentLength 영향 | 레지스트리 상수. 변경 시 성장 단계 임계값과 동기화 필요 |

**상호작용 주의:**
- `InSeasonWeight`와 `OffSeasonWeight`는 비율만 의미 있음. (6.0, 2.0)과 (3.0, 1.0)은 동등
- `HandSize` 변경 시 `ConfirmOffer` 검증 로직 변경 불필요 (DailyHand.Contains로 동적 대응)
- `GameDurationDays`는 이 시스템의 Tuning Knob이 아닌 레지스트리 상수 — Time GDD(#1)가 소유. 여기서는 참조만

**PostLoad 검증 (`UCardSystemConfigAsset::PostLoad`):**
- `InSeasonWeight ∈ [1.0, 10.0]` — 범위 밖 → `UE_LOG(Warning)` + clamp
- `OffSeasonWeight ∈ [0.1, 3.0]` — 범위 밖 → `UE_LOG(Warning)` + clamp
- `ConsecutiveDaySuppression ∈ [0.05, 1.0]` — 범위 밖 → `UE_LOG(Warning)` + clamp. = 0이면 전일 핸드 카드의 `FinalWeight=0` → Pool에서 사실상 제외되어 "소프트 억제" 설계 의도 위반
- `InSeasonWeight >= OffSeasonWeight` — 역전 시 `UE_LOG(Warning)` + 자동 swap
- `HandSize ∈ [1, 5]` — 범위 밖 → `UE_LOG(Warning)` + clamp(3)
- `GameDurationDays ∈ {7, 21}` — EC-CS-20 방어 (Time GDD 소유 상수이나, Config 오버라이드 경로 존재 시 이중 검증)

**ConfigAsset 위치**: `UCardSystemConfigAsset` (`UPrimaryDataAsset` 상속). Data Pipeline을 통해 로드. 단일 인스턴스.

## Acceptance Criteria

### CR-CS-1: 일일 카드 풀 구성

**AC-CS-01** | `AUTOMATED` | BLOCKING
- **GIVEN** 카탈로그 10장(Spring 3, Summer 2, Autumn 2, Winter 3) 로드 완료, **WHEN** `FOnDayAdvanced(1)` 수신 후 `GetDailyHand()` 호출, **THEN** 반환 배열 원소 수 = 3, 모두 카탈로그 내 유효 FName, 서로 중복 없음

**AC-CS-02** | `AUTOMATED` | BLOCKING
- **GIVEN** `GameDurationDays = 21`, **WHEN** DayIndex=7에서 F-CS-1 실행, **THEN** `SeasonForDay(7, 21) = Season.Summer`. 해당 계절 카드의 `BaseWeight = InSeasonWeight(기본 3.0)`, 비해당 카드의 `BaseWeight = OffSeasonWeight(기본 1.0)`. SegmentLength는 GameDurationDays에서 동적 도출 (하드코딩 금지)

### CR-CS-2: 덱 재생성 조건

**AC-CS-03** | `AUTOMATED` | BLOCKING
- **GIVEN** `FOnDayAdvanced(5)` 수신 후 DailyHand 구성 완료, **WHEN** 동일 `FOnDayAdvanced(5)` 재수신, **THEN** DailyHand 불변, 샘플링 미재실행

### CR-CS-3: GSM Prepare/Ready

**AC-CS-04** | `INTEGRATION` | BLOCKING
- **GIVEN** `FOnDayAdvanced(3)` 후 Ready 상태, **WHEN** GSM이 `OnPrepareRequested()` 호출, **THEN** `OnCardSystemReady(bDegraded = false)` 즉시 응답, 경과 시간 < 5.0초

**AC-CS-05** | `INTEGRATION` | BLOCKING
- **GIVEN** Uninitialized Card System (`FOnDayAdvanced` 미수신), **WHEN** GSM이 `OnPrepareRequested()` 호출, **THEN** On-demand 샘플링 시작 후 완료 시 `OnCardSystemReady()` 응답

### CR-CS-4: ConfirmOffer 검증 게이트

**AC-CS-06** | `AUTOMATED` | BLOCKING
- **GIVEN** Ready 상태, `DailyHand = ["Card_A", "Card_B", "Card_C"]`, **WHEN** `ConfirmOffer("Card_A")` 호출, **THEN** `return true`, `bHandOffered = true`, `FOnCardOffered` 정확히 1회 브로드캐스트

**AC-CS-07** | `AUTOMATED` | BLOCKING
- **GIVEN** Ready 상태, `DailyHand = ["Card_A", "Card_B", "Card_C"]`, **WHEN** (1) `ConfirmOffer("Card_D")` (2) 성공 후 즉시 재호출 (3) Preparing 상태에서 호출, **THEN** 세 경우 모두 `return false`, `FOnCardOffered` 미발행

### CR-CS-5: 비영속화 복원

**AC-CS-08** | `AUTOMATED` | BLOCKING
- **GIVEN** 앱 재시작 시뮬, `FGrowthState.LastCardDay = 4`, `DayIndex = 4`, **WHEN** `FOnDayAdvanced(4)` 수신, **THEN** `bHandOffered = true` 복원, `ConfirmOffer()` → `false` (중복 건넴 방지)

**AC-CS-09** | `AUTOMATED` | BLOCKING
- **GIVEN** 앱 재시작 시뮬, `FGrowthState.LastCardDay = 3`, `DayIndex = 4`, **WHEN** `FOnDayAdvanced(4)` 후 `ConfirmOffer("Card_A")` (DailyHand 포함), **THEN** `bHandOffered = false`, `ConfirmOffer` 성공, `FOnCardOffered` 정상 발행. `PreviousDayHand = ∅`

### CR-CS-6: C1 Schema Guard + FName 계약

**AC-CS-10** | `CODE_REVIEW` | BLOCKING
- **GIVEN** `FGiftCardRow` 구조체 정의 파일, **WHEN** `int32`/`float`/`double` 수치 필드 및 `FOnCardOffered` 인자의 `FGuid` 사용을 grep, **THEN** `FGiftCardRow`에 수치 효과 필드 0건, `FOnCardOffered` 시그니처가 `FName` 단일 인자

### CR-CS-7: 카탈로그 크기 엣지

**AC-CS-11** | `AUTOMATED` | BLOCKING
- **GIVEN** Pipeline DegradedFallback, 카탈로그 0장, **WHEN** `FOnDayAdvanced(1)` + `OnPrepareRequested()`, **THEN** `DailyHand.Num() = 0`, `UE_LOG(Error)` 1회, `OnCardSystemReady(bDegraded = true)`, `ConfirmOffer()` → `false`

**AC-CS-12** | `AUTOMATED` | BLOCKING
- **GIVEN** 카탈로그 2장만 반환, **WHEN** `FOnDayAdvanced(1)`, **THEN** `DailyHand.Num() = 2`, `OnCardSystemReady(bDegraded = true)`, DailyHand 내 CardId로 `ConfirmOffer` → `true`

### F-CS-1: 계절 경계 전수 검증

**AC-CS-13** | `AUTOMATED` | BLOCKING
- **GIVEN** `GameDurationDays = 21`, **WHEN** DayIndex = {1, 6, 7, 12, 17, 21} 각각에 F-CS-1 실행, **THEN** 출력이 순서대로 {Spring, Spring, Summer, Autumn, Winter, Winter}

**AC-CS-13b** | `AUTOMATED` | BLOCKING
- **GIVEN** `GameDurationDays = 21`, `CurrentDayIndex` 초기값 = -1, **WHEN** `FOnDayAdvanced(0)` 수신, **THEN** 샘플링 스킵 (`NewDayIndex < 1` guard), `UE_LOG(Warning)` 1회, DailyHand 미변경. 크래시 없음

### EC-CS-20: GameDurationDays 경계 검증

**AC-CS-20a** | `AUTOMATED` | BLOCKING
- **GIVEN** `UCardSystemConfigAsset`에 `GameDurationDays = 3` (안전 범위 {7, 21} 밖), **WHEN** PostLoad 검증 실행, **THEN** `UE_LOG(Error)` 1회, `GameDurationDays` 기본값(21)으로 폴백. F-CS-1의 `SegmentLength` 정상 산출

### F-CS-2: 소프트 억제 가중치

**AC-CS-14** | `AUTOMATED` | BLOCKING
- **GIVEN** Spring, `InSeasonWeight = 3.0`, `OffSeasonWeight = 1.0`, `ConsecutiveDaySuppression = 0.5`, `PreviousDayHand = {"Card_S1", "Card_S2"}`, **WHEN** Card_S1(Spring, 억제), Card_S3(Spring, 미억제), Card_Su1(Summer, 미억제) 가중치 계산, **THEN** 결과 = {1.5, 3.0, 1.0}. 모든 카드 Weight > 0

### F-CS-3: 선택 확률 통계 검증

**AC-CS-15a** | `AUTOMATED` | BLOCKING
- **GIVEN** 10장 카탈로그 (Spring 3, Summer 2, Autumn 2, Winter 3), Season=Spring, `InSeasonWeight=3.0`, `OffSeasonWeight=1.0`, `ConsecutiveDaySuppression=0.5`, PreviousDayHand에 Spring 2장 포함, **WHEN** F-CS-2 공식으로 각 카드 가중치 계산 후 F-CS-3 기대 선택 확률 산출, **THEN** 억제 없는 Spring(w=3.0) 기대 P ∈ [0.45, 0.63], 억제 Spring(w=1.5) 기대 P ∈ [0.22, 0.38], 비제철(w=1.0) 기대 P ∈ [0.16, 0.28]. 공식 결정론 검증 — 입력이 동일하면 출력이 동일

**AC-CS-15b** | `AUTOMATED` | BLOCKING
- **GIVEN** 동일 카탈로그·가중치 설정, 고정 시드(`PlaythroughSeed = 12345`), **WHEN** 10,000회 반복 샘플링 후 각 카드의 DailyHand 포함 빈도 집계, **THEN** 관측 빈도가 F-CS-3 기대 확률의 ±5%p 이내 수렴. 고정 시드로 결정론적 재현 가능

### Day 21 크로스 시스템

**AC-CS-16** | `INTEGRATION` | BLOCKING
- **GIVEN** DayIndex=21, Offering 상태, Growth가 `FOnCardOffered` 구독(Stage 1), GSM은 `FOnCardProcessedByGrowth` 구독(Stage 2) (ADR-0005), **WHEN** `ConfirmOffer(CardId)` → Stage 1 `FOnCardOffered` 브로드캐스트, **THEN** 처리 순서 (C++ 호출 스택으로 강제): (1) Growth CR-1 태그 가산 + CR-5 EvaluateFinalForm → (2) `FOnFinalFormReached(FormId)` 발행 → (3) `SaveAsync(ECardOffered)` → (4) Stage 2 `FOnCardProcessedByGrowth.Broadcast(Result)` → (5) GSM Farewell P0 전환. `FOnDayAdvanced` Farewell 후 미발행. *ADR-0005 BLOCKER 해소, unblocked 2026-04-19*

### FGuid 미사용 전역 계약

**AC-CS-17** | `CODE_REVIEW` | BLOCKING
- **GIVEN** Card System, Growth, GSM 구현 파일 (`Source/` 하위 `.h`, `.cpp`), **WHEN** CardId 컨텍스트에서 `FGuid` grep, **THEN** 해당 컨텍스트 `FGuid` 사용 0건, `FName`만 사용

### CR-CS-3: GSM 타임아웃 degraded 경로

**AC-CS-18** | `INTEGRATION` | BLOCKING
- **GIVEN** Card System Uninitialized, Pipeline 응답 5초 초과 지연 시뮬, **WHEN** GSM이 `OnPrepareRequested()` 호출 후 `CardSystemReadyTimeoutSec`(기본 5.0초) 경과, **THEN** GSM이 `bDegraded = true` 플래그와 함께 Offering 강제 진입 (GSM E9 계약). Card System의 `OnCardSystemReady` 미도달. Card Hand UI는 빈 슬롯 또는 부분 슬롯 표시

### AC 요약

| 유형 | 수량 | Gate | 비고 |
|---|---|---|---|
| AUTOMATED | 15 | BLOCKING | AC-CS-15 → 15a/15b 분리, AC-CS-13b·AC-CS-20a 추가 |
| INTEGRATION | 4 | BLOCKING | AC-CS-05 AUTOMATED→INTEGRATION 재분류. AC-CS-16 unblocked 2026-04-19 by ADR-0005 |
| CODE_REVIEW | 2 | BLOCKING | — |
| **합계** | **21** | — | R3 리뷰 반영 |

## Implementation Notes

다음 사항은 GDD 규칙이 아닌 구현 단계 권장 사항이다.

- **Tags 타입**: `TArray<FName>` 대신 `FGameplayTagContainer` 사용 권장. `Season.Spring` 등의 계층적 태그 구조가 FGameplayTag의 자연스러운 사용 사례. GAS 없이 독립 사용 가능. `/architecture-decision`에서 정식 결정
- **FRandomStream 주입**: CR-CS-1 Step 3의 `FRandomStream`을 외부 주입 가능하게 설계 — 테스트에서 시드 제어, 전역 RNG 비오염
- **DYNAMIC vs non-dynamic delegate**: `FOnCardOffered`는 현재 `DECLARE_DYNAMIC_MULTICAST_DELEGATE`이나, Blueprint 노출이 불필요하면 `DECLARE_MULTICAST_DELEGATE_OneParam`(non-dynamic)으로 경량화 가능. Blueprint 바인딩 의도 확정 후 결정
- **타이머 메커니즘**: CR-CS-3의 5초 타임아웃은 `FTSTicker` 또는 `FTimerManager` 중 선택 필요. `UGameInstanceSubsystem`에서 `GetWorld()`가 null인 초기화 시점이 있으므로 `FTSTicker` 권장. `/architecture-decision`에서 확정

## Open Questions

| # | 질문 | 소유자 | 해결 대상 | 상태 |
|---|---|---|---|---|
| OQ-CS-1 | ~~`FOnCardOffered(FGuid)` → `FOnCardOffered(FName)` stale 참조 수정~~ | Card System GDD | `/consistency-check` | **RESOLVED** (2026-04-18 — Growth #7, GSM #5, MBC #6 총 6곳 수정 완료) |
| ~~OQ-CS-2~~ | ~~EC-CS-13: `GetLastCardDay()` pull API~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0012](../../docs/architecture/adr-0012-growth-getlastcardday-api.md) — `int32 GetLastCardDay() const`, sentinel 0 = 미건넴) |
| OQ-CS-3 | ~~EC-CS-17: `FOnCardOffered` 구독 순서~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0005](../../docs/architecture/adr-0005-foncardoffered-processing-order.md) — **2단계 Delegate 패턴 채택**. Stage 1 `FOnCardOffered` (Card → Growth, MBC) + Stage 2 `FOnCardProcessedByGrowth` (Growth → GSM, Dream Trigger). Day 21 순서 C++ 호출 스택으로 보장) |
| OQ-CS-4 | EC-CS-18: `GetAllCardIds`/`GetCardRow` 일관성 책임 분배 — Pipeline R3 fail-close 계약으로 충분한지 vs Card System 내 null check 추가 | Data Pipeline GDD(#2) | 구현 단계 검토 | **Open** |
| ~~OQ-CS-5~~ | ~~카드 풀 재생성 시점 — Eager vs Lazy~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0006](../../docs/architecture/adr-0006-card-pool-regeneration-timing.md) — Eager 채택 + Full 40장 < 1ms 검증 AC) |
| OQ-CS-6 | `UCardSystemConfigAsset` 레지스트리 등록 — Data Pipeline에 단일 인스턴스 자산으로 등록 필요. `UStillnessBudgetAsset` 패턴 참조. Asset Manager `PrimaryAssetTypesToScan` 등록 방식 포함 | Data Pipeline GDD(#2) | Card System 구현 시 | **Open** |
| OQ-CS-7 | PlaythroughSeed 영속화 — `FSessionRecord`에 `PlaythroughSeed: uint32` 필드 추가 필요. Save/Load GDD(#3)와 entity registry `FSessionRecord` struct 갱신 대상 | Save/Load GDD(#3) | Card System 구현 시 | **Open** |
