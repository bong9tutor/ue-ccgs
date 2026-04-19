# Story 002: CR-1 Atomic Chain — 태그 가산 + LastCardDay + SaveAsync

> **Epic**: feature-growth-accumulation
> **Layer**: Feature
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2–3시간

## Context

- **GDD Reference**: `design/gdd/growth-accumulation-engine.md` §CR-1 (태그 벡터 누적) + §F1 (Tag Accumulation) + AC-GAE-01/02/12/23
- **TR-ID**: TR-growth-001 (CR-1 atomicity)
- **Governing ADR**: [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) (Stage 1 subscriber + Stage 2 publisher)
- **Engine Risk**: LOW
- **Engine Notes**: `Card->OnCardOffered.AddDynamic(this, &UGrowthAccumulationSubsystem::OnCardOfferedHandler)` 등록. `co_await` / `AsyncTask` / `Tick` 분기 금지 — CR-1 step 2-6은 동일 game-thread 함수.
- **Control Manifest Rules**:
  - Feature Layer §Required Patterns — Growth는 Stage 1 `FOnCardOffered` 구독
  - Foundation Layer §Required Patterns — Save/Load는 per-trigger atomicity, `SaveAsync(reason)` 동일 game-thread 함수
  - Feature Layer §Forbidden Approaches — "Never broadcast `FOnCardOffered` outside Card System"

## Acceptance Criteria

1. **AC-GAE-01** (AUTOMATED/BLOCKING) — Accumulating 상태 + `TagVector = {}` 초기 + 카드 `Tags = [Season.Spring, Emotion.Calm]`, `FOnCardOffered(CardId)` 발행 시: `TagVector["Season.Spring"] == 1.0f`, `TagVector["Emotion.Calm"] == 1.0f`, `TotalCardsOffered == 1`, `LastCardDay == CurrentDayIndex`
2. **AC-GAE-02** (CODE_REVIEW/BLOCKING) — CR-1 구현 소스에서 Step 2–5 + step 6 `SaveAsync(ECardOffered)` 사이에 `co_await` / `AsyncTask` / `Tick` 분기 grep = 0건
3. **AC-GAE-12** (AUTOMATED/BLOCKING) — F1 누적 확인: `TagVector = {Season.Spring: 2.0, Emotion.Calm: 1.0}` + 카드 `[Season.Spring, Emotion.Calm]` 건넴 → `{3.0, 2.0}`
4. **AC-GAE-23** (AUTOMATED/BLOCKING) — F1 W_cat 경로: `CategoryWeightMap = {Season: 2.0f, Emotion: 0.5f}` + 카드 `[Season.Spring, Emotion.Calm]` → `TagVector["Season.Spring"] == 2.0f`, `TagVector["Emotion.Calm"] == 0.5f`
5. **AC-GAE-18** (AUTOMATED/BLOCKING) — EC-8 `GetCardRow(CardId)` → nullptr 시 CR-1 전체 스킵, `UE_LOG(LogGrowth, Warning)` 1회

## Implementation Notes

- **CR-1 step 순서** (ADR-0005 + GDD §CR-1):
  1. `Pipeline->GetCardRow(CardId)` pull — nullptr이면 early return + Warning log
  2. `Row.Tags` 순회 → 각 태그별 `W_card(=1.0f) × W_cat[cat(tag)]` 계산 후 `FGrowthState.TagVector.FindOrAdd(Tag) += ComputedWeight`
  3. `FGrowthState.LastCardDay = CurrentDayIndex`
  4. `FGrowthState.TotalCardsOffered += 1`
  5. MaterialHints 재계산 트리거 (Story 006 — 현재는 TODO 마커)
  6. `GetSaveLoadSubsystem()->SaveAsync(ESaveReason::ECardOffered)` — 동일 함수 스택
  7. Stage 2 broadcast 자리 (Story 005)
- `cat(tag)` = FName의 점(.) 접두사 파싱 (`"Season.Spring"` → `"Season"`)
- `CategoryWeightMap`은 `UGrowthConfigAsset`에서 pull (per-content — ADR-0011 UDeveloperSettings 예외)
- **ADR-0005 준수**: Growth는 `FOnCardOffered` 구독자 중 하나 — MBC도 동시 구독하지만 Growth 완료를 기다리지 않음 (시각 즉시성)
- EC-6 guard: `if (InternalState != Accumulating) return;` — Resolved 상태에서 이벤트 무시

## Out of Scope

- MaterialHints 실제 계산 (Story 006 — F4)
- Day 21 EvaluateFinalForm 분기 (Story 004)
- Stage 2 `OnCardProcessedByGrowth.Broadcast` (Story 005)
- CR-4 단계 평가 (Story 003)

## QA Test Cases

**Given** Accumulating 상태 + `TagVector={}` + 카드 `Tags=[Season.Spring, Emotion.Calm]`, **When** `FOnCardOffered("Card_SpringBreeze")` 발행, **Then** `TagVector["Season.Spring"]==1.0f`, `TagVector["Emotion.Calm"]==1.0f`, `TotalCardsOffered==1`, `LastCardDay==CurrentDayIndex`.

**Given** Resolved 상태 + `TagVector={Season.Spring: 3.0}`, **When** `FOnCardOffered` 수신, **Then** TagVector 불변, SaveAsync 미호출 (EC-6).

**Given** `GetCardRow("NonExistent")` → nullptr, **When** `FOnCardOffered("NonExistent")`, **Then** TagVector 불변, `LogGrowth Warning` 1회 (AC-GAE-18).

**Given** `CategoryWeightMap = {Season: 2.0f, Emotion: 0.5f}`, **When** 카드 `[Season.Spring, Emotion.Calm]` 건넴, **Then** `TagVector["Season.Spring"]==2.0f`, `TagVector["Emotion.Calm"]==0.5f` (F1 W_cat 경로).

**Given** CR-1 구현 소스, **When** `grep -E "co_await|AsyncTask|Tick" OnCardOfferedHandler`, **Then** 매치 0건.

## Test Evidence

- **Unit test**: `tests/unit/Growth/test_cr1_atomic_accumulation.cpp`
- **Static analysis**: `grep "OnCardOffered.Broadcast" Source/MadeByClaudeCode/` = Card System 파일만 매치 (Feature Layer §Forbidden Approaches)
- **CODE_REVIEW gate**: CR-1 handler 내 async 분기 grep 0건

## Dependencies

- Story 001 (Subsystem 골격)
- `foundation-data-pipeline` (GetCardRow API)
- `foundation-save-load` (SaveAsync + ESaveReason::ECardOffered)
- `feature-card-system` Story (FOnCardOffered publisher — 실 구현 의존 없음, stub 충분)
