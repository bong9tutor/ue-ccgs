# Story 008 — E4 QuietRest + FOnCardOffered + 동시 FOnGrowthStageChanged 순서

> **Epic**: [core-moss-baby-character](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2h

## Context

- **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md) §Edge Case E4 (QuietRest 상태에서 카드 건넴 + 동시 성장 전환)
- **TR-ID**: TR-mbc-002 (priority) + TR-mbc-003 (Quiet Rest vocabulary)
- **Governing ADR**: ADR-0005 §Decision — Stage 1 (MBC Reacting) + Stage 2 (Growth → MBC `FOnGrowthStageChanged` publish).
- **Engine Risk**: LOW
- **Engine Notes**: E4 순서: `FOnCardOffered`로 QuietRest 즉시 해제 → Reacting → Reacting 완료 후 GrowthTransition 처리. QuietRest 시각이 잔존하지 않음.
- **Control Manifest Rules**: Vocabulary rule — "Quiet Rest" 시각. drying/yellowing 금지 (MBC CR-5).

## Acceptance Criteria

- **AC-MBC-14** [`INTEGRATION`/BLOCKING] — QuietRest 상태(DryingFactor > 0) + FOnCardOffered + 동시 FOnGrowthStageChanged 순서대로 처리 시 DryingFactor 즉시 0 리셋 → Reacting 진입 → Reacting 완료 후 GrowthTransition 진입. QuietRest 시각이 잔존하지 않음 (E4).

## Implementation Notes

1. **순서 보장** (ADR-0005 Stage 1/2 분리):
   - Stage 1 `FOnCardOffered`: MBC handler (story 003 확장)에서 QuietRest 해제 — `if (CurrentState == QuietRest) { DryingFactor = 0.0f; ApplyPreset(StagePresets[CurrentStage]); }` → `RequestStateChange(Reacting)`.
   - Stage 2 `FOnCardProcessedByGrowth`는 GSM/Dream만 — MBC는 Stage 2 구독 금지 (ADR-0005).
   - Growth가 **Stage 2 chain 내에서** `FOnGrowthStageChanged(NewStage)` 별도 발행 (단계 변경 시) — MBC가 이 별도 delegate를 구독 (story 004).
2. **Reacting → GrowthTransition 순서 (E4)**:
   - Stage 1 → MBC Reacting 진입 (story 003, priority 2).
   - Stage 2 delegate chain 중 Growth의 `OnGrowthStageChanged.Broadcast(Mature)` 발행 시 (story 004).
   - MBC `OnGrowthStageChangedHandler`가 `RequestStateChange(GrowthTransition)` 호출 — priority 3 > Reacting 2 → **즉시 인터럽트** (AC-MBC-13 pattern).
   - **이 즉시 인터럽트는 E4 의도와 충돌** — GDD §E4 "Reacting 완료 후 GrowthTransition 진입" 명시.
   - **해결 (구현 결정)**: Reacting 상태에서 `OnGrowthStageChangedHandler` 수신 시 **즉시 전환하지 않고 defer** — `PendingGrowthTransitionStage = NewStage;` 저장. Reacting 종료 (`ExitReacting`) 시점에 `if (PendingGrowthTransitionStage.IsSet()) { HandleGrowthTransition(...); }` 실행.
   - 이 defer 패턴은 AC-MBC-13 (Reacting → GrowthTransition 즉시 인터럽트)과 **E4 (QuietRest compound) 차이를 구분** — 일반 GrowthTransition은 priority interrupt, **QuietRest→Reacting 진입 직후 수신된 GrowthTransition은 defer**.
3. **Defer 규칙 명확화**:
   - `if (CurrentState == Reacting && PreviousState == QuietRest) { PendingGrowthTransitionStage = NewStage; return; }`
   - 아니면 정상 priority interrupt.
4. **Vocabulary test (CR-5 Pillar 1)**:
   - Test 녹화에서 QuietRest → Reacting → GrowthTransition 전환이 "미안" 감정 유발 안 함 검증 (MANUAL — story 009 visual).

## Out of Scope

- Compound event GSM boundary (GSM epic story 011 — disk commit coalesce)
- QuietRest entry 조건 (story 005)
- Reacting decay (story 003)
- GrowthTransition Lerp (story 004)

## QA Test Cases

**Test Case 1 — AC-MBC-14 E4 Compound sequence**
- **Given**: `CurrentState = QuietRest`, `DryingFactor = 0.6`, Growth가 Day 6 (Sprout→Growing 전환 조건) 충족.
- **When**: 플레이어 카드 건넴 → Stage 1 `FOnCardOffered` 발행 → Growth Handler 처리 → `FOnGrowthStageChanged(Growing)` + `FOnCardProcessedByGrowth` 발행.
- **Then (순서)**: 
  1. `FOnCardOffered` 수신 → `DryingFactor = 0.0`, `ApplyPreset(Sprout)` 즉시 → `CurrentState = Reacting`.
  2. 동일 frame 내 `FOnGrowthStageChanged(Growing)` 수신 → `PendingGrowthTransitionStage = Growing` (defer — QuietRest 직후 Reacting이므로).
  3. Reacting 감쇠 1.65s 완료 → `ExitReacting` → Pending check → `RequestStateChange(GrowthTransition)` → Sprout mesh → Growing mesh swap + Lerp 1.5s.
  4. 최종 `CurrentState = Idle`, `MeshComponent->GetStaticMesh() == GrowingMesh`.
- **Verify**: QuietRest 시각 (DryingFactor > 0 효과) 잔존하지 않음 — Reacting 진입 시점부터 Stage preset 반영.

**Test Case 2 — 일반 GrowthTransition은 priority interrupt**
- **Given**: `CurrentState = Reacting`, `PreviousState = Idle` (일반 카드, QuietRest 아님).
- **When**: `FOnGrowthStageChanged(Growing)` 수신.
- **Then**: **즉시 인터럽트** (AC-MBC-13 pattern) — `PendingGrowthTransitionStage` 미사용. `CurrentState = GrowthTransition` 즉시.

## Test Evidence

- [ ] `tests/integration/moss-baby/e4_quietrest_compound_test.cpp` — AC-MBC-14
- [ ] `tests/integration/moss-baby/reacting_interrupt_vs_defer_test.cpp` — defer vs interrupt 분기 확인

## Dependencies

- **Depends on**: story-003 (Reacting handler), story-004 (GrowthTransition handler), story-005 (QuietRest)
- **Unlocks**: Day 21 cross-system integration (cross-epic)
