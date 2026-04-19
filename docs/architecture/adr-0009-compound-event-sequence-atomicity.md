# ADR-0009: Compound Event Sequence Atomicity — GSM `BeginCompoundEvent` / `EndCompoundEvent` Boundary

## Status

**Accepted** (2026-04-19 — 결정 명확. AC 검증은 Day 13 compound event 시나리오 발생 시점)

## Date

2026-04-19

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | Core / State Management / Persistence |
| **Knowledge Risk** | LOW — UE Subsystem + Multicast Delegate 표준 패턴 |
| **References Consulted** | save-load-persistence.md §8 per-trigger atomicity qualifier + §OQ-8, game-state-machine.md §Detailed Design + §Interactions, ADR-0005 (FOnCardOffered 2-Stage Delegate) |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | Day 13 compound event 시나리오 시 통합 테스트 (자동) + AC `COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY` (Save/Load 기존 negative AC) 갱신 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0005 (FOnCardOffered 2-Stage Delegate — Stage 2 Growth → GSM/Dream 패턴이 본 ADR의 기반) |
| **Enables** | Day 13 compound event 시나리오 안전 처리 (카드 + 꿈 + narrative 동시 트리거 시) |
| **Blocks** | Day 13 compound event 시나리오가 발생하는 sprint — Save/Load §8 negative AC만으로는 불충분 |
| **Ordering Note** | Day 13 시나리오 발견 시점에 Acceptance 검증. 본 ADR은 상세 결정 미리 기록 |

## Context

### Problem Statement

Save/Load는 **per-trigger atomicity**만 보장 (Save/Load §5 + §8). 그러나 Day 13 시나리오처럼 **여러 시스템이 같은 frame에 in-memory mutation을 만들고 각각 SaveAsync 호출**하는 경우, sequence atomicity (모두 commit 또는 모두 rollback) 보장 안 됨:

```text
Frame N: Player offers card on Day 13 (long-gap return + tag-trigger dream + threshold narrative)
  → Card.ConfirmOffer → FOnCardOffered (Stage 1)
       → Growth.OnCardOfferedHandler
            → tag accumulation
            → SaveAsync(ECardOffered)              [commit 1]
            → FOnCardProcessedByGrowth (Stage 2)
                 → GSM.OnCardProcessedByGrowth
                      → bWithered = false
                      → Offering→Waiting 전환
                 → Dream.OnCardProcessedByGrowth
                      → CR-1 trigger evaluation → SelectDream → save dream
                      → SaveAsync(EDreamReceived)  [commit 2]
                      → FOnDreamTriggered → GSM Waiting→Dream
                           → narrative emit (Time)
                           → Time.IncrementNarrativeCountAndSave
                                → SaveAsync(ENarrativeEmitted)  [commit 3]
```

3개의 commit이 순차 — 만약 commit 2가 fail (worker thread 타이밍 이슈 등)하면 commit 1은 disk에 반영, commit 2 미반영, commit 3 disk 반영 → **disk 상태 부정합** (꿈은 미수령으로 보이는데 narrative count는 +1 되어 있음).

이를 atomic하게 묶으려면 GSM 또는 Save/Load 측에서 sequence boundary가 필요.

### Constraints

- **Save/Load §5 atomicity contract**: per-trigger만 보장 — 변경 시 dual-slot ping-pong + atomic rename 알고리즘 재설계 → 비현실적
- **Architecture Principle 3 (Per-Trigger Atomicity, Sequence by GSM)**: 책임 분리 — Save/Load = storage durability, GSM = game logic ordering
- **Day 13 시나리오 빈도**: 21일 중 1-2회 발생 가능 (long-gap + dream + narrative 모두 충족 case). 0회도 가능 (희소)
- **GSM이 sequence orchestrator 자연스러움**: GSM은 이미 multi-system event coordinator (Card/Growth/Dream/Time/Stillness Budget cross-cutting)
- **Save/Load coalesce 정책**: 진행 중 SaveAsync에 새 SaveAsync 도착 시 latest in-memory 승리 (Save/Load §5 마지막 문단) — 본 ADR이 활용

### Requirements

- 3+ in-memory mutation을 단일 disk commit으로 결합
- Per-trigger atomicity는 유지 (Save/Load 변경 없음)
- API는 단순 — GSM의 in-memory mutation 로직 침투 최소화
- AC 자동화 가능

## Decision

**GSM `BeginCompoundEvent` / `EndCompoundEvent` boundary 채택** — Save/Load의 coalesce 정책을 활용하여 GSM이 multiple in-memory mutation을 batch한 후 마지막 단 1회 SaveAsync만 호출.

### API

```cpp
UCLASS()
class UMossGameStateSubsystem : public UGameInstanceSubsystem {
public:
    /**
     * Compound event boundary — 같은 frame 내 여러 시스템의 in-memory mutation을 묶어
     * disk commit을 단일 SaveAsync로 결합한다.
     *
     * 사용 예 (Day 13 시나리오):
     *   GSM->BeginCompoundEvent(ESaveReason::ECardOffered);  // primary reason
     *   // Growth/Dream/Time 등이 in-memory mutation + SaveAsync 호출
     *   GSM->EndCompoundEvent();
     *   // → 모든 SaveAsync 호출이 coalesce되어 단일 commit 발생
     */
    void BeginCompoundEvent(ESaveReason PrimaryReason);
    void EndCompoundEvent();

    bool IsInCompoundEvent() const { return bCompoundEventActive; }

private:
    bool bCompoundEventActive = false;
    ESaveReason CompoundPrimaryReason;
    int32 CoalescedSaveCount = 0;  // 디버깅용
};
```

### Save/Load 측 변경 — 최소

Save/Load는 변경 없음. 기존 coalesce 정책 (§5 마지막 문단: "이전 저장 완료 전 재호출되면 요청은 coalesce됨 — 새 요청이 이전 요청을 superseed; worker 스레드 pickup 시점의 in-memory 상태가 직렬화됨")이 본 ADR의 핵심 mechanism. GSM은 단지 SaveAsync 호출이 coalesce되도록 frame 내 호출을 묶기만 함.

### GSM 사용 패턴 (Day 13 예시)

```cpp
// GSM이 ETimeAction::ADVANCE_WITH_NARRATIVE + Day 13 분기 진입
void UMossGameStateSubsystem::OnTimeActionResolved(ETimeAction Action) {
    if (Action == ETimeAction::ADVANCE_WITH_NARRATIVE && IsCompoundEventTrigger()) {
        BeginCompoundEvent(ESaveReason::ENarrativeEmitted);
    }

    // 기존 분기 로직 — Card/Growth/Dream/Time이 자연스럽게 SaveAsync 호출
    // (Stage 2 delegate chain 등)

    if (bCompoundEventActive) {
        EndCompoundEvent();
    }
}

void UMossGameStateSubsystem::BeginCompoundEvent(ESaveReason PrimaryReason) {
    if (bCompoundEventActive) {
        UE_LOG(LogGSM, Warning, TEXT("BeginCompoundEvent 재진입 — 무시"));
        return;
    }
    bCompoundEventActive = true;
    CompoundPrimaryReason = PrimaryReason;
    CoalescedSaveCount = 0;
}

void UMossGameStateSubsystem::EndCompoundEvent() {
    if (!bCompoundEventActive) return;
    // Coalesced된 SaveAsync는 worker thread가 latest in-memory 상태로 단일 commit
    // 명시적 commit trigger 불필요 — Save/Load §5 정책이 자동 처리
    UE_LOG(LogGSM, Verbose, TEXT("Compound event 종료 — %d SaveAsync 호출 coalesced (primary=%s)"),
        CoalescedSaveCount, *UEnum::GetValueAsString(CompoundPrimaryReason));
    bCompoundEventActive = false;
}
```

### IsCompoundEventTrigger() 판단 기준

GSM이 다음 조건 중 ≥2개 동시 만족하면 compound event:
1. `ADVANCE_WITH_NARRATIVE` 도착
2. Day = MultipleOf(`MinDreamIntervalDays`) — Dream Trigger 가능성
3. `bWithered = true` 상태에서 카드 건넴 (withered 해제 + 일반 진행)

### Failure Semantics

- Compound event 중 어느 한 commit이라도 fail (worker thread error) → 전체 batch latest in-memory가 disk에 미반영
- Save/Load §5 coalesce 정책 + per-trigger atomicity 결합으로 다음 정상 SaveAsync에서 latest in-memory가 disk에 commit 시도 → eventual consistency
- **단 1-commit loss는 여전히 가능** (Save/Load per-trigger 보장의 한계 내) — Day 13 시나리오의 narrative count +1이 손실되더라도 cap=3 정책으로 자연 회복

## Alternatives Considered

### Alternative 1: GSM `BeginCompoundEvent` / `EndCompoundEvent` boundary (채택)

- 본 ADR §Decision

### Alternative 2: Save/Load sequence-level API

- **Description**: `Save/Load.BeginTransaction()` / `CommitTransaction()` API 추가, multi-reason commit 단일 disk write
- **Pros**: Save/Load가 sequence atomicity의 owner — Architecture cleaner
- **Cons**:
  - **Save/Load §5 contract 변경** — atomic rename 알고리즘이 single payload 전제. Multi-reason batch는 새 algorithm
  - GSM의 game logic ordering 책임이 Save/Load로 이동 — Architecture Principle 3 위반
  - 기존 14개 시스템의 SaveAsync(reason) 호출 패턴 모두 영향
- **Rejection Reason**: Architecture Principle 3 위반 + 비용

### Alternative 3: Reason별 즉시 commit + 부분 손실 허용

- **Description**: Sequence atomicity 자체를 buy하지 않고, Day 13 시나리오에서 부분 disk 부정합을 허용 (per-trigger atomicity만)
- **Pros**: 구현 비용 0
- **Cons**: 
  - Disk 부정합 사용자 경험 — 꿈 미수령인데 narrative count는 +1 (희박하나 가능)
  - **Pillar 4 보호 약화** — 일관 disk 상태가 21일 보존의 일부
- **Rejection Reason**: Pillar 4 보호 약화. Game design 차원에서 trade-off 명시 필요 — Save/Load OQ-8이 정확히 이 거부

## Consequences

### Positive

- **Sequence atomicity 달성**: Day 13 compound event 시나리오 disk 부정합 0 (latest in-memory가 단일 commit)
- **Save/Load 변경 없음**: per-trigger atomicity contract 유지 — 14개 시스템 영향 없음
- **Architecture Principle 3 강화**: GSM이 game logic ordering의 owner — Save/Load는 storage durability
- **Coalesce 정책 활용**: Save/Load 기존 mechanism의 자연스러운 사용 — 새 algorithm 불필요

### Negative

- **GSM Compound Event boundary 호출 의무**: GSM이 Day 13 시나리오 분기에서 BeginCompoundEvent / EndCompoundEvent 호출 누락 시 본 ADR 효과 무효
- **`IsCompoundEventTrigger()` 휴리스틱**: 정확성은 game design에 의존 — 미래 새 compound 시나리오 추가 시 갱신 필요. **Mitigation**: GSM Open Question으로 추적

### Risks

- **재진입 race**: Compound event 중 추가 compound trigger 발생 시 (희박). **Mitigation**: `bCompoundEventActive` flag 재진입 차단 + Warning 로그
- **EndCompoundEvent 누락 (early return path)**: Compound event 진행 중 예외/early return 시 flag set 상태 영구. **Mitigation**: RAII guard pattern — `FCompoundEventScope` struct 권장 (Implementation 단계)

```cpp
// 권장 사용 — RAII
{
    FCompoundEventScope Scope(GSM, ESaveReason::ENarrativeEmitted);
    // ... compound mutation
}  // Scope destructor automatically calls EndCompoundEvent
```

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `save-load-persistence.md` | §OQ-8 "Compound event sequence atomicity 책임 분배 — GSM batch boundary" | **본 ADR이 OQ-8의 정식 결정 (GSM batch boundary)** |
| `save-load-persistence.md` | §8 per-trigger atomicity qualifier — "sequence atomicity가 필요한 경우 GSM이 BeginCompoundEvent/EndCompoundEvent boundary로 batch coalesce" | 본 ADR이 그 패턴의 구체 API |
| `save-load-persistence.md` | AC `COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY` (negative AC) | 본 ADR Accepted 후 AC 갱신 — sequence atomicity가 GSM batch로 *제공됨* (positive case 추가) |
| `game-state-machine.md` | §Detailed Design — GSM이 multi-system orchestrator | 본 ADR이 GSM 책임 확장 |

## Performance Implications

- **CPU**: BeginCompoundEvent/EndCompoundEvent 호출 ~ 100ns (flag 변경). 무시 가능
- **CPU (coalesce 효과)**: 3 SaveAsync 호출 → 1 disk commit → ~3× I/O 비용 절감 (worker thread)
- **Memory**: int32 flag + ESaveReason enum + counter — 무시 가능

## Migration Plan

Day 13 compound event 시나리오가 발생하는 sprint 진입 시 (또는 사전 작업):
1. `UMossGameStateSubsystem::BeginCompoundEvent` / `EndCompoundEvent` 구현
2. `IsCompoundEventTrigger()` 휴리스틱 구현 + 단위 테스트
3. `FCompoundEventScope` RAII guard struct 추가 (권장 사용 패턴)
4. Day 13 시나리오 통합 테스트 — Mock Save/Load + 3 SaveAsync coalesce 검증
5. Save/Load `COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY` AC 갱신 — sequence atomicity가 GSM batch로 제공됨을 명시

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| Compound event coalesce | Mock Save/Load — Day 13 시나리오 3 SaveAsync 호출 후 disk write count | 1회 |
| Per-trigger atomicity 유지 | Save/Load 기존 AC 모두 통과 | 영향 없음 |
| BeginCompoundEvent 재진입 차단 | Compound event 중 다시 BeginCompoundEvent 호출 | Warning 로그 + 무시 |
| RAII guard pattern | `FCompoundEventScope` 사용 시 destructor에서 EndCompoundEvent 자동 호출 | 자동 종료 확인 |
| Save/Load §8 negative AC 갱신 | AC `COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY` 변경 — GSM batch로 sequence atomicity *제공됨* | AC 텍스트 갱신 + 통합 테스트 추가 |

## Related Decisions

- **save-load-persistence.md** §OQ-8 — 본 ADR의 직접 출처
- **save-load-persistence.md** §8 R2 NEW Per-trigger atomicity qualifier — 본 ADR이 §8의 GSM batch 위임을 구체 API로
- **ADR-0005** (FOnCardOffered 2-Stage Delegate) — Stage 2 Growth → GSM/Dream chain이 본 ADR의 기반 (Day 13 chain의 일부)
- **architecture.md** §Architecture Principles §Principle 3 (Per-Trigger Atomicity, Sequence by GSM) — 본 ADR이 Principle 3의 sequence 부분 정식 구현
- **architecture.md** §Data Flow race condition #3 — 본 ADR이 race #3 mitigation
