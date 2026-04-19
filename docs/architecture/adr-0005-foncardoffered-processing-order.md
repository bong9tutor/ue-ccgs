# ADR-0005: `FOnCardOffered` 처리 순서 보장 — 2단계 Delegate 패턴

## Status

**Accepted** (2026-04-19 — `/architecture-review` 통과, B1 BLOCKER 해결 Migration Plan §1-9 실행 완료: Card/Dream Trigger/GSM/Growth GDD + entities.yaml 모두 Stage 2 패턴 반영)

## Date

2026-04-19

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | Core / Event System / Cross-System Communication |
| **Knowledge Risk** | LOW — UE Multicast Delegate (`DECLARE_DYNAMIC_MULTICAST_DELEGATE`) 4.x+ 안정. 단, **등록 순서는 비공개 계약** (UE doc 명시적 "no guaranteed ordering") — 본 ADR이 이 비공개성을 의존하지 않는 패턴 채택 |
| **References Consulted** | card-system.md §CR-CS-4 + §EC-CS-17 + OQ-CS-3, growth-accumulation-engine.md §CR-1, game-state-machine.md §E13 + §Interaction 4, architecture.md §Data Flow §3.3 + race condition #1 |
| **Post-Cutoff APIs Used** | None — 표준 UE Multicast Delegate |
| **Verification Required** | AC-CS-16 — Day 21 통합 테스트 (Growth 태그 가산 → FOnFinalFormReached 발행 → GSM Farewell P0 순서 검증). 본 ADR Accepted 2026-04-19로 `[BLOCKED by OQ-CS-3]` 마킹 해소됨 |

## ADR Dependencies

| Field | Value |
|---|---|
| **Depends On** | None (Foundation/Core 다른 ADR과 독립) |
| **Enables** | Card System `FOnCardOffered` 브로드캐스트 정상 구현. Day 21 최종 형태 결정 정확성. Growth → GSM → MBC → Dream Trigger 처리 순서 결정성 |
| **Blocks** | ~~Card System sprint Day 21 통합 테스트~~ — **해소됨** (2026-04-19 Accepted). AC-CS-16이 Stage 2 패턴으로 설계되어 `[BLOCKED by OQ-CS-3]` 마킹 제거. Card System sprint 진입 시 즉시 실행 가능 |
| **Ordering Note** | Card System 구현 시 필수. MVP 7일에서 Day 7 Final Form 결정 테스트에도 동일 적용 — 사실상 MVP 구현 초기부터 영향 |

## Context

### Problem Statement

Card System (#8)이 `FOnCardOffered(FName CardId)` 브로드캐스트 시 4개 downstream 시스템이 구독 (Growth #7, GSM #5, MBC #6, Dream Trigger #9). **UE `DECLARE_DYNAMIC_MULTICAST_DELEGATE`의 subscriber 호출 순서는 등록 순서와 일치하지만, 이 등록 순서는 비공개 계약** (UE documentation: "Order of execution not guaranteed" + implementation-defined).

**Day 21 시나리오 race**:
```
Card.ConfirmOffer(Day21CardId) 성공
  → Card.FOnCardOffered.Broadcast(CardId)  [각 subscriber 호출]
       ├─ Growth.OnCardOfferedHandler()     [태그 가산 + SaveAsync(ECardOffered)]
       │    └─ (Day 21이면) EvaluateFinalForm() → Growth.FOnFinalFormReached.Broadcast(FormId)
       ├─ GSM.OnCardOfferedHandler()        [Offering→Waiting 전환 시작]
       │    └─ (DayIndex == GameDurationDays + 카드 건넴 완료이면) Farewell P0 즉시 전환
       ├─ MBC.OnCardOfferedHandler()        [Reacting 상태 진입 — 시각 반응]
       └─ DreamTrigger.OnCardOfferedHandler() [CR-1 평가 — 희소성 게이트]
```

UE Multicast 호출 순서가 비결정적이면 다음 케이스에서 결과가 다름:

- **Growth 먼저 → GSM 나중**: 태그 벡터 Day 21 카드 반영 → `FinalFormId` 정확 결정 → GSM Farewell 전환 시 `FSessionRecord`에 올바른 FinalFormId 저장 ✅
- **GSM 먼저 → Growth 나중**: GSM이 Farewell P0 진입 → Resolved 상태 → Growth의 `FOnFinalFormReached`가 Farewell 상태에서 발행 → MBC의 FinalReveal이 Farewell 전환 후 발생 → **시각적 시퀀스 어긋남** + **`FSessionRecord.FinalFormId` 저장 전에 Farewell commit** 가능 → **마지막 카드가 최종 형태에 반영되지 않는 버그**

추가 race: Dream Trigger가 Growth의 TagVector를 pull할 때 Growth가 아직 Day 21 카드를 가산하지 않았으면 이전 벡터로 평가 — 꿈 선택 결과 왜곡.

### Constraints

- **UE Multicast Delegate 계약**: 호출 순서 비공개 (implementation-defined)
- **원자성**: Growth CR-1 step 2-5 + step 6 SaveAsync는 동일 game-thread 함수 안에서 (per-trigger atomicity, ADR-0009는 sequence atomicity 별도)
- **결정성**: Day 21 race는 플레이어 경험 핵심 (Pillar 4) — 비결정적 구현 절대 금지
- **결합도**: Card System이 downstream을 직접 인지하지 않아야 함 (loose coupling — delegate 패턴 유지)
- **AC 자동화**: AC-CS-16 (Day 21 cross-system), AC-GAE-06 (Score 계산 정확), AC-GSM-16 (Day 21 Farewell P0 전환) — 결정적 시퀀스로 자동화 가능해야 함

### Requirements

- Day 21에 Growth가 태그 가산 + FinalFormId 결정 완료 후에만 GSM Farewell P0 진입
- Dream Trigger가 최신 TagVector로 평가 (Growth 가산 후 pull)
- MBC Reacting (시각 반응)은 Growth 완료를 기다릴 필요 없음 — 시각 즉시성이 우선 (Pillar 2 "잘못된 선택은 없다" — 시각 피드백은 즉시)
- UE Multicast 등록 순서에 의존하지 않는 결정적 구현

## Decision

**2단계 Delegate 패턴 채택**:

1. **Stage 1 — `FOnCardOffered`** (Card 발행, Growth 유일 정식 subscriber + 시각 반응용 subscriber)
   - Card.ConfirmOffer 성공 시 `FOnCardOffered(CardId)` 발행
   - **정식 subscriber**: Growth (태그 가산 + FinalForm 결정 + SaveAsync(ECardOffered) + Growth 자체 delegate 발행)
   - **시각 반응 subscriber**: MBC (Reacting 상태 즉시 진입) — Growth 완료 대기 불필요 (시각 피드백의 독립성 보장)

2. **Stage 2 — `FOnCardProcessedByGrowth`** (**신규 delegate, Growth 발행, GSM + Dream Trigger subscriber**)
   - Growth가 CR-1 처리 완료 + (Day 21이면 FinalForm 결정 + `FOnFinalFormReached` 발행) 후 **마지막 step**에 발행
   - Payload: `FName CardId` + `FGrowthProcessResult` struct (IsDay21Complete, FinalFormId)
   - **subscriber**: GSM (Offering→Waiting / Day 21이면 Farewell P0), Dream Trigger (CR-1 평가 — 최신 TagVector 보장)

### Stage 분리 근거

```text
시각 반응 (MBC Reacting)              | 게임 로직 (GSM 전환, Dream 평가)
──────────────────────────────────────┼────────────────────────────────────
- 플레이어 즉시 피드백 필요            | - Growth 처리 완료 후에만 정확
- Growth 완료 대기 시 0.5-1.5ms       | - 비결정적 순서 시 Day 21 버그
  지연 체감 가능                       |
- 잘못된 선택 없음 (Pillar 2) —        | - Pillar 4 보호 (최종 형태 정확성)
  시각 반응 즉시성 > 정확성             |
                                       |
→ Stage 1 FOnCardOffered 구독          | → Stage 2 FOnCardProcessedByGrowth
                                       |   구독
```

### Key Interfaces

```cpp
// Growth Engine — 신규 delegate 정의
USTRUCT(BlueprintType)
struct FGrowthProcessResult {
    GENERATED_BODY()
    UPROPERTY() FName CardId;
    UPROPERTY() bool bIsDay21Complete = false;
    UPROPERTY() FName FinalFormId;  // Day 21 아니면 NAME_None
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnCardProcessedByGrowth, const FGrowthProcessResult&, Result);

UCLASS()
class UGrowthAccumulationSubsystem : public UGameInstanceSubsystem {
    // ... (existing)
public:
    UPROPERTY(BlueprintAssignable)
    FOnCardProcessedByGrowth OnCardProcessedByGrowth;  // Stage 2

private:
    void OnCardOfferedHandler(FName CardId) {
        // CR-1 step 2-5: 태그 가산, LastCardDay, TotalCardsOffered, MaterialHints
        TagVector.FindOrAdd(Tag) += 1.0f;  // 각 태그
        // ...

        // CR-4 단계 평가 (FOnDayAdvanced 경로 아님 — 본 handler는 FOnCardOffered 경로)
        // CR-5 Day 21 Final Form 결정
        FGrowthProcessResult Result;
        Result.CardId = CardId;
        if (CurrentDayIndex == GameDurationDays) {
            FName FormId = EvaluateFinalForm();  // F3 Score argmax
            FGrowthState.FinalFormId = FormId;
            Result.bIsDay21Complete = true;
            Result.FinalFormId = FormId;
            OnFinalFormReached.Broadcast(FormId);  // MBC가 FinalReveal
        }

        // CR-1 step 6: atomic SaveAsync (이미 in-memory mutation 완료)
        GetSaveLoadSubsystem()->SaveAsync(ESaveReason::ECardOffered);

        // Stage 2 발행 — GSM, Dream Trigger 처리
        OnCardProcessedByGrowth.Broadcast(Result);
    }
};

// GSM — Stage 1 FOnCardOffered 구독 제거, Stage 2 구독
UCLASS()
class UMossGameStateSubsystem : public UGameInstanceSubsystem {
    // ... (existing)
protected:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override {
        Super::Initialize(Collection);
        auto* Growth = GetGameInstance()->GetSubsystem<UGrowthAccumulationSubsystem>();
        Growth->OnCardProcessedByGrowth.AddDynamic(this, &OnCardProcessedByGrowthHandler);
    }

private:
    void OnCardProcessedByGrowthHandler(const FGrowthProcessResult& Result) {
        if (Result.bIsDay21Complete) {
            // GSM E13: Offering→Waiting 체류 없이 즉시 Farewell P0
            RequestStateTransition(EGameState::Farewell, EPriority::P0);
            // FOnFarewellReached는 Time이 별도 발행 경로 — 여기서는 GSM 상태 전환만
        } else {
            // 정상 Offering→Waiting 전환
            RequestStateTransition(EGameState::Waiting, EPriority::P1);
        }
    }
};

// Dream Trigger — Stage 2 구독 (최신 TagVector 보장)
UCLASS()
class UDreamTriggerSubsystem : public UGameInstanceSubsystem {
    // ... (existing)
protected:
    virtual void Initialize(FSubsystemCollectionBase&) override {
        // ...
        auto* Growth = GetGameInstance()->GetSubsystem<UGrowthAccumulationSubsystem>();
        Growth->OnCardProcessedByGrowth.AddDynamic(this, &OnCardProcessedByGrowthHandler);
    }

private:
    void OnCardProcessedByGrowthHandler(const FGrowthProcessResult& Result) {
        // Stage 2는 Growth가 태그 가산 완료 후 발행 → GetTagVector() 최신값 보장
        // (GSM이 Day 21 Farewell P0 진입 후면 GameState == Farewell → 평가 스킵 EC-6)
        if (GSM->GetCurrentState() == EGameState::Farewell) return;  // CR-1 Guard

        EvaluateRarityGates();
        // ... CR-2 ~ CR-4
    }
};

// MBC — Stage 1 FOnCardOffered 구독 유지 (시각 반응 즉시성)
void AMossBaby::OnCardOfferedHandler(FName CardId) {
    // Growth 완료 대기 없이 즉시 Reacting 진입
    EnterReactingState();  // Formula 4 지수 감쇠 시작
}

// Card System — 변경 없음 (Stage 1만 발행)
bool UCardSystemSubsystem::ConfirmOffer(FName CardId) {
    // ... (existing validation)
    OnCardOffered.Broadcast(CardId);  // 기존 그대로
    return true;
}
```

### Day 21 시퀀스 (결정적)

```text
[Frame N]  Player drags card → Card Hand UI → Card.ConfirmOffer(CardId)
     │
     ▼ Card.FOnCardOffered.Broadcast(CardId)
     │   ├─ Growth.OnCardOfferedHandler()    — 태그 가산, CR-1 전체, Day 21 → EvaluateFinalForm
     │   │                                      ├─ FOnFinalFormReached.Broadcast(FormId)
     │   │                                      │    └─ MBC.FinalReveal 시작
     │   │                                      └─ SaveAsync(ECardOffered)
     │   │                                      └─ OnCardProcessedByGrowth.Broadcast(Result)  ← Stage 2
     │   │                                             │
     │   │                                             ├─ GSM.OnCardProcessedByGrowthHandler
     │   │                                             │    └─ Day 21 → Farewell P0 전환
     │   │                                             └─ DreamTrigger.OnCardProcessedByGrowthHandler
     │   │                                                    └─ CR-1 (현 GameState==Farewell이면 skip)
     │   │
     │   └─ MBC.OnCardOfferedHandler()         — Reacting 즉시 (Stage 1 — 시각 반응 독립)
     ▼
[Same Frame] 모든 처리 동일 frame 완료
```

**핵심**: Growth가 Stage 2를 "자기 함수의 마지막 statement"로 발행 → GSM/Dream Trigger가 Growth 완료 후에만 실행됨이 C++ 호출 스택 순서로 보장.

## Alternatives Considered

### Alternative 1: Card → Growth 직접 호출 → Growth가 완료 후 delegate 발행

- **Description**: `FOnCardOffered` 없음. Card가 `Growth->OnCardOfferedImmediate(CardId)` 직접 호출 → Growth가 `OnCardProcessedByGrowth` 발행.
- **Pros**: Stage 1 delegate 불필요 — 2 delegate가 1 delegate로 감소
- **Cons**:
  1. **결합도 ↑**: Card가 Growth의 직접 참조 보유 (pull 패턴이 아닌 push) — loose coupling 원칙 위반
  2. **MBC 시각 반응 즉시성 소실**: MBC가 Stage 2를 구독해야 하므로 Growth 처리 완료 (최대 ~1.5ms) 후에 시각 반응 — Pillar 2 "즉시 피드백" 약화
  3. **Card → Growth 단방향 결합이 향후 확장 방해**: 미래에 Card System이 다른 use case 추가 시 Growth를 우회할 수 없음
- **Rejection Reason**: 결합도 ↑ + MBC 즉시성 손실이 2 delegate 복잡성 비용을 초과

### Alternative 2: 2단계 Delegate 패턴 (채택)

- 본 ADR의 §Decision

### Alternative 3: Phase A/B Priority Broadcast — UE 비공개 계약 의존

- **Description**: `FOnCardOffered` Broadcast 시 subscriber 등록 우선순위 매개변수 (`AddDynamicWithPriority`)로 Growth 먼저, GSM/Dream 나중
- **Pros**: 1 delegate 유지, 기존 구조 최소 변경
- **Cons**:
  1. **UE `FDelegateBase`에 priority 등록 public API 부재** — 커스텀 delegate wrapper 구현 필요
  2. **UE 업데이트 시 delegate 내부 구조 변경 가능** — priority wrapper가 의존하는 내부 API가 5.7+에서 변경될 위험
  3. **AC 자동화 가능하나 코드 복잡** — 커스텀 delegate + priority 테스트 인프라
  4. **결정성이 wrapper 구현에 의존** — wrapper 버그 시 발견 어려움
- **Rejection Reason**: 비공개 계약에 의존하지 않는 Stage 2 패턴이 더 안전. 내부 API drift 리스크 회피

### Alternative 4: Synchronous Queue — Card가 subscribers 명시적 순서로 호출

- **Description**: Card가 `TArray<ICardOfferListener*>` priority queue 보유, 순서대로 호출
- **Pros**: 결정성 명시
- **Cons**: **Interface pattern으로 전환 (UE Interface) — loose coupling 깨짐**. Card가 listener 클래스 직접 인지. Multicast Delegate의 blueprintable 이점 소실. 모든 subscriber class가 `ICardOfferListener` 구현 의무
- **Rejection Reason**: Multicast Delegate 계약 (blueprintable + loose coupling) 유지가 우선. Interface 전환 비용 > 이점

## Consequences

### Positive

- **결정적 Day 21 시퀀스**: Growth 태그 가산 + FinalFormId 결정 + SaveAsync 완료 → GSM Farewell P0 진입 — C++ 호출 스택 순서로 강제
- **Dream Trigger 최신 TagVector 보장**: Stage 2에서 Growth pull 시 이미 Day 21 카드 반영됨
- **MBC 즉시 시각 반응 유지**: Stage 1 구독으로 Growth 처리 대기 불필요 — Pillar 2 "즉시 피드백" 보호
- **UE Multicast 비공개 계약 의존 제거**: Delegate 등록 순서에 의존하지 않는 결정적 패턴
- **Loose Coupling 유지**: Card ↔ Growth ↔ GSM/Dream 간 delegate 패턴 — Card는 downstream 인지하지 않음 (Stage 1), Growth가 processed 후 발행 (Stage 2)
- **AC 자동화 가능**: AC-CS-16 Day 21 통합 테스트가 결정적 시퀀스로 자동화됨 — `[BLOCKED by OQ-CS-3]` 마킹 해소

### Negative

- **Delegate 2개 관리**: `FOnCardOffered` + `FOnCardProcessedByGrowth` — 구현·유지보수 복잡도 소폭 증가
- **Growth의 책임 확장**: Stage 2 delegate 소유 — Growth가 게임 시퀀스 orchestrator 역할 일부 담당 (단, 이는 Growth가 "태그 → 최종 형태 결정자"라는 core responsibility와 정합)
- **`FGrowthProcessResult` struct 신설**: 등록된 엔티티에 추가 (entities.yaml `structs` 섹션)
- **기존 GDD 갱신 필요**: Card GDD §CR-CS-4 + EC-CS-17 + OQ-CS-3, GSM GDD §Interaction 4 + E13, Dream Trigger GDD §CR-1 — ADR Accepted 후 "RESOLVED by ADR-0005" 표기

### Risks

- **Growth Handler 내 예외 발생 시 Stage 2 미발행**: CR-1 처리 중 throw 시 GSM Farewell 전환 누락 가능. **Mitigation**: Growth Handler를 `try { ... } catch { LogError + Stage 2 미발행 — 사용자 다음 상호작용에서 복구 가능 }` 또는 표준 UE 패턴 (throw 회피 + null check)
- **MBC가 Stage 1 구독 지속 → Growth Handler 중 MBC Tick 침투 리스크**: MBC가 Reacting 진입 후 Tick에서 Growth pull 시도 시 Growth가 처리 중 (동일 frame). **Mitigation**: MBC는 pull API 사용하지 않음 — `GetMaterialHints()` 호출 시점은 Reacting 후 별도 이벤트. MBC가 Growth 상태 직접 read 금지 (Layer Boundary Rule)
- **Card 외 다른 source가 FOnCardOffered 발행**: 미래에 다른 시스템이 FOnCardOffered 브로드캐스트 시도 가능. **Mitigation**: Card가 유일 writer — 다른 시스템이 Broadcast 호출 금지 (code review + grep AC — `grep "OnCardOffered.Broadcast" Source/MadeByClaudeCode/` = Card System 파일만)
- **FGrowthProcessResult struct 확장 시 호환성**: 미래에 struct 필드 추가 시 직렬화 호환 필요 — 하지만 본 struct는 delegate payload (직렬화 대상 아님) → 위험 없음

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `card-system.md` | §OQ-CS-3 Blocking — "EC-CS-17: `FOnCardOffered` 구독 순서 — UE Multicast Delegate 등록 순서는 비공개 계약. 명시적 호출 체인 또는 2단계 delegate 패턴 필요" | **본 ADR이 OQ-CS-3의 정식 결정** (2단계 delegate 채택) |
| `card-system.md` | §EC-CS-17 — "`FOnCardOffered` 처리 순서 — Day 21 최종 형태 정확성의 전제 조건" | 본 ADR §Day 21 시퀀스 diagram |
| `card-system.md` | §CR-CS-4 ConfirmOffer + FOnCardOffered broadcast | 본 ADR이 Card 측 변경 없음 — Stage 1만 발행 |
| `card-system.md` | AC-CS-16 `[BLOCKED by OQ-CS-3]` Day 21 크로스 시스템 | **본 ADR Accepted 시 AC-CS-16 BLOCKED 마킹 해소** |
| `growth-accumulation-engine.md` | §CR-1 (태그 가산 + atomic SaveAsync) + §CR-5 (Day 21 최종 형태 결정) | 본 ADR이 Stage 2 발행을 CR-1의 마지막 step에 통합 |
| `growth-accumulation-engine.md` | §Interactions §4 Card System — "FOnCardOffered → CR-1" | Stage 1 구독 유지 |
| `growth-accumulation-engine.md` | AC-GAE-02 CR-1 step 2-6 동일 call stack | Stage 2 broadcast도 같은 call stack 내 — atomicity 유지 |
| `growth-accumulation-engine.md` | AC-GAE-06 Day 21 Final Form 결정 정확성 | Stage 2 발행 전 `FinalFormId` 확정 |
| `game-state-machine.md` | §Interaction 4 Card System — "GSM은 Card System의 내부 로직에 관여하지 않음" | Stage 2 구독으로 Card 경유 간접 통지 — loose coupling 유지 |
| `game-state-machine.md` | §E13 Day 21 카드 건넴 직후 Farewell 즉시 도달 | Stage 2 `bIsDay21Complete` flag로 분기 — 결정적 |
| `game-state-machine.md` | AC-GSM-16 Day 21 Farewell P0 전환 | Stage 2 handler 내 RequestStateTransition(Farewell, P0) |
| `dream-trigger-system.md` | §CR-1 평가 트리거 — `FOnCardOffered` 수신 | **본 ADR이 Stage 2 구독으로 전환 권장** (최신 TagVector 보장) — Dream Trigger GDD §Interactions §1 업데이트 필요 |
| `moss-baby-character-system.md` | §Interactions §2 Card System — `FOnCardOffered` 즉시 Reacting | Stage 1 구독 유지 (시각 반응 즉시성) |
| `moss-baby-character-system.md` | AC-MBC-04 Reacting Formula 4 | Stage 1 handler로 즉시 진입 |
| **Cross-review D-2** | "Day 21 Final Form 순서 = OQ-CS-3" (2026-04-18) | **본 ADR이 D-2 정식 해소** |
| `architecture.md` | §Data Flow race condition #1 | 본 ADR이 race #1 mitigation |

## Performance Implications

- **CPU (Stage 1 + Stage 2)**: 매 카드 건넴 시 2개 Multicast Broadcast (기존 1개 대비 +50%). Multicast Broadcast 비용 ≈ 1-5μs per subscriber. 4 subscriber 기준 ≈ 4-20μs 추가 — 무시 가능 (16.6ms budget의 0.1%)
- **Memory**: `FGrowthProcessResult` struct ≈ 16 bytes per broadcast (stack-allocated, no heap). Delegate 추가 선언 ≈ 32 bytes per UObject 보유 — 무시 가능
- **Load Time**: 영향 없음
- **Network**: N/A

## Migration Plan

Card System 구현 직전 (MVP 구현 시점):

1. **Growth Engine 헤더 갱신**: `FGrowthProcessResult` struct 정의 + `FOnCardProcessedByGrowth` delegate 선언
2. **Growth `OnCardOfferedHandler` 갱신**: CR-1 완료 후 Stage 2 `OnCardProcessedByGrowth.Broadcast(Result)` 호출 — 마지막 statement로 배치
3. **GSM `Initialize` 갱신**: `Growth->OnCardProcessedByGrowth.AddDynamic(this, ...)` 구독 추가. 기존 `Card->OnCardOffered.AddDynamic` 구독 제거 (MBC 시각 반응과 혼선 방지)
4. **Dream Trigger `Initialize` 갱신**: 동일 — Stage 2 구독으로 전환
5. **MBC**: 변경 없음 — Stage 1 `OnCardOffered` 구독 유지
6. **Entity Registry 갱신** (`design/registry/entities.yaml`):
   - `structs:` 섹션에 `FGrowthProcessResult` 추가
   - `delegates:` 섹션에 `FOnCardProcessedByGrowth` 추가
   - `FOnCardOffered` `referenced_by` 갱신 — GSM, Dream Trigger 제거 (Stage 2로 이전) / Growth, MBC 유지
7. **GDD §Open Questions 갱신**: Card OQ-CS-3 → "RESOLVED by ADR-0005", Cross-review D-2 해소 메모
8. **AC 마킹 해소**: AC-CS-16 `[BLOCKED by OQ-CS-3]` → `BLOCKING`
9. **통합 테스트 구현**: AC-CS-16 자동화 (Day 21 시나리오 — Growth 완료 → GSM Farewell 순서 검증)

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| AC-CS-16 Day 21 순서 | Growth `EvaluateFinalForm` 완료 전에 GSM Farewell 진입 여부 log | Growth log 먼저 → GSM log 나중 (타임스탬프 검증) |
| Growth Handler 내 Stage 2 발행 | `grep "OnCardProcessedByGrowth.Broadcast" Growth*.cpp` | Growth `OnCardOfferedHandler` 마지막 statement에 1건 |
| Card 유일 FOnCardOffered writer | `grep "OnCardOffered.Broadcast\|FOnCardOffered" Source/MadeByClaudeCode/` | Card System 파일만 매치 |
| GSM/Dream Trigger Stage 2 구독 | Initialize log 또는 코드 검토 | `OnCardProcessedByGrowth.AddDynamic` 2건 (GSM + Dream) |
| MBC Stage 1 구독 유지 | Initialize log | `OnCardOffered.AddDynamic` 1건 (MBC) |
| MBC 즉시 Reacting | 수동 — 카드 건넴 후 MBC 반응 프레임 측정 | Growth 완료 대기 없이 동일 frame Reacting 진입 |
| Day 21 최종 형태 저장 | Day 21 카드 건넴 후 `FSessionRecord.FinalFormId` 확인 | 카드 반영된 FormId |
| Dream Trigger 최신 TagVector | Day 21 카드 건넴 후 Dream 평가 시 TagVector 확인 | Day 21 카드 태그 포함 |
| Cross-review D-2 해소 | `docs/architecture/architecture.md` §Document Status D-2 → "해소 by ADR-0005" | 본 ADR Accepted 후 수동 갱신 |

## Related Decisions

- **card-system.md** §OQ-CS-3 (Blocking ADR) — 본 ADR의 직접 출처
- **cross-review D-2** (2026-04-18) — 본 ADR이 Cross-Review blocker 해소
- **ADR-0009** (Compound Event Sequence Atomicity) — 본 ADR은 "단일 FOnCardOffered 처리 순서" — ADR-0009는 "compound event sequence atomicity"로 다른 문제. 단, Day 21에 carding + dream + narrative 동시 발생 시 두 ADR 협조 필요 (ADR-0009 GSM batch boundary가 본 ADR의 Stage 2를 감싸는 형태)
- **architecture.md** §Data Flow race condition #1 (Day 21) — 본 ADR이 직접 해소
- **architecture.md** §Architecture Principles §Principle 3 (Per-Trigger Atomicity, Sequence by GSM) — 본 ADR은 per-trigger atomicity의 subscriber 순서 레벨 보장 (sequence atomicity는 ADR-0009)
- **entities.yaml** `delegates` 섹션 — `FOnCardOffered` referenced_by 갱신 의무 (Migration Plan §6)
