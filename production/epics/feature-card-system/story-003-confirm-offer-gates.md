# Story 003: ConfirmOffer 검증 게이트 + Stage 1 Broadcast

> **Epic**: feature-card-system
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/card-system.md` §CR-CS-4 (ConfirmOffer + FOnCardOffered) + AC-CS-06/07
- **TR-ID**: TR-card-005 (CR-CS-4 EC-CS-9 이중 호출 방지)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) — Card가 Stage 1 `FOnCardOffered` 유일 publisher
- **Engine Risk**: LOW
- **Engine Notes**: `ConfirmOffer`는 sync return `bool` — UI 드래그 롤백에 사용. Broadcast 이후 취소 불가 (Pillar 1).
- **Control Manifest Rules**:
  - Feature Layer §Forbidden Approaches — "Never broadcast `FOnCardOffered` outside Card System"

## Acceptance Criteria

1. **AC-CS-06** (AUTOMATED/BLOCKING) — Ready 상태 + DailyHand = ["Card_A","Card_B","Card_C"], `ConfirmOffer("Card_A")` 호출 → `return true`, `bHandOffered = true`, `FOnCardOffered("Card_A")` 정확히 1회 브로드캐스트, CurrentState → Offered
2. **AC-CS-07** (AUTOMATED/BLOCKING) — 세 경로 모두 `return false`, `FOnCardOffered` 미발행:
   - (1) `ConfirmOffer("Card_D")` (DailyHand 없음 — EC-CS-8)
   - (2) 성공 후 즉시 재호출 (이중 호출 — EC-CS-9)
   - (3) Preparing 상태에서 호출 (EC-CS-7)
3. CR-CS-4 검증 게이트 3단:
   - Gate 1: `if (CurrentState != Ready) return false`
   - Gate 2: `if (!DailyHand.Contains(CardId)) return false`
   - Gate 3: `if (bHandOffered) return false`
4. Broadcast 순서: `bHandOffered = true` → `OfferedCardId = CardId` → `CurrentState = Offered` → `FOnCardOffered.Broadcast(CardId)` → `return true`

## Implementation Notes

```cpp
bool UCardSystemSubsystem::ConfirmOffer(FName CardId) {
    if (CurrentState != ECardSystemState::Ready) return false;  // EC-CS-7
    if (!DailyHand.Contains(CardId)) {
        UE_LOG(LogCard, Warning, TEXT("ConfirmOffer unknown CardId: %s"), *CardId.ToString());
        return false;  // EC-CS-8
    }
    if (bHandOffered) return false;  // EC-CS-9

    bHandOffered = true;
    OfferedCardId = CardId;
    CurrentState = ECardSystemState::Offered;
    OnCardOffered.Broadcast(CardId);  // Stage 1 — ADR-0005
    return true;
}
```

- **ADR-0005 준수**: Growth + MBC가 Stage 1 구독 (Growth는 CR-1 + Stage 2 forward, MBC는 시각 즉시성). GSM + Dream은 Stage 2 경유 (Story 004 Growth side)
- `FOnCardOffered` 하루 1회 보장 — Growth CR-1 "하루 1회 태그 가산" 계약의 상류 방어
- EC-CS-18: `GetCardRow`가 nullptr 반환해도 Card System은 ConfirmOffer 성공 처리 완료 후이므로 `bHandOffered = true` 유지

## Out of Scope

- Card Hand UI 드래그 구현 (presentation-card-hand-ui)
- Growth CR-1 태그 가산 (feature-growth-accumulation Story 002)
- Stage 2 downstream (feature-growth-accumulation Story 005)

## QA Test Cases

**Given** Ready + DailyHand=["A","B","C"], **When** `ConfirmOffer("A")`, **Then** `return true`, `FOnCardOffered("A")` 1회, CurrentState=Offered, bHandOffered=true (AC-CS-06).

**Given** Ready + DailyHand=["A","B","C"], **When** `ConfirmOffer("D")`, **Then** `return false`, FOnCardOffered 미발행 (EC-CS-8).

**Given** 성공적으로 ConfirmOffer("A") 수행 완료, **When** 즉시 재호출 `ConfirmOffer("A")`, **Then** `return false`, FOnCardOffered 재발행 없음 (EC-CS-9).

**Given** Preparing 상태, **When** `ConfirmOffer("A")`, **Then** `return false` (EC-CS-7).

## Test Evidence

- **Unit test**: `tests/unit/Card/test_confirm_offer_gates.cpp`
- 3-gate 각 실패 경로 독립 테스트

## Dependencies

- Story 001 (Subsystem + FOnCardOffered delegate)
- Story 002 (DailyHand 구성)
