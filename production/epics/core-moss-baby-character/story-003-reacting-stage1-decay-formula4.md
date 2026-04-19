# Story 003 — Reacting 상태 + Stage 1 `FOnCardOffered` 구독 + Formula 4 지수 감쇠

> **Epic**: [core-moss-baby-character](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3h

## Context

- **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md) §CR-3 (카드 반응) + §Formula 4 (카드 반응 지수 감쇠)
- **TR-ID**: TR-mbc-004 (Formula 4 Reacting 지수 감쇠 ≈1.65s = 5τ_sss)
- **Governing ADR**: **ADR-0005** §Decision §Stage 1 — MBC는 Stage 1 `FOnCardOffered` 구독 유지 (시각 즉시성, Growth 완료 대기 없이 Reacting 즉시 진입).
- **Engine Risk**: LOW
- **Engine Notes**: Reacting은 Formula 4 지수 감쇠 기반 — 5τ 시점 (Emission 0.5s, SSS 1.65s)에서 실질 복귀. Reacting 상태 지속 시간 = max(5τ_emission, 5τ_sss) ≈ 1.65s.
- **Control Manifest Rules**: Feature Layer Rules §Required Patterns — **카드 반응성 즉시 보장** — MBC는 Stage 1 `FOnCardOffered` 구독으로 Growth 처리 대기 없이 즉시 Reacting 진입. Feature Layer Rules §Forbidden — "Never subscribe MBC to Stage 2" (시각 즉시성 우선). "Never call Growth pull API from MBC inside Growth Handler window".

## Acceptance Criteria

- **AC-MBC-04** [`AUTOMATED`/BLOCKING] — Reacting 상태 진입 (Delta_E=0.15, τ_E=0.1)에 Formula 4 순수 함수에 t=0, 0.3, 0.5, 1.65 입력 시 Emission 출력이 각각 V₀+0.15, V₀+0.0075(±0.001), V₀+0.0011(±0.001), V₀(±0.001). 렌더링 불필요 — 내부 float 값 검증.
- **AC-MBC-05** [`INTEGRATION`/BLOCKING] — Budget Deny 상태에서 카드 반응 트리거 시도 시 Emission·SSS 불변, 반응 타이머 미시작 (E7).
- **AC-MBC-17** [`AUTOMATED`/BLOCKING] — Reacting 상태 활성 중 (t=0.5s, Emission 감쇠 진행 중) 두 번째 `FOnCardOffered` 수신 시 EmissionStrength 재부스트 없음 + 첫 감쇠 곡선 유지. Stillness Budget 핸들 중복 Request Deny 확인.

## Implementation Notes

1. **Stage 1 Subscription** (ADR-0005):
   - `AMossBaby::BeginPlay` 내 `CardSubsystem->OnCardOffered.AddDynamic(this, &AMossBaby::OnCardOfferedHandler);`
   - **절대 금지**: `OnCardProcessedByGrowth.AddDynamic(...)` (Stage 2) — CI grep 검증.
2. **`OnCardOfferedHandler(FName CardId)`**:
   - `FStillnessHandle ReactingHandle = StillnessBudget->Request(EStillnessChannel::Motion, EStillnessPriority::Standard);`
   - Deny: `if (!ReactingHandle.IsValid()) { return; }` — AC-MBC-05 (Emission·SSS 불변).
   - `if (!RequestStateChange(EMossCharacterState::Reacting)) { ReactingHandle.Release(); return; }` — AC-MBC-17 (priority reject — 이미 Reacting 중이면 두 번째 요청 skip).
   - Timer 시작: `GetWorld()->GetTimerManager().SetTimer(DecayTickTimer, ..., 1.0f/60.0f, true);` — 60Hz tick.
3. **Formula 4 순수 함수** (`MossBabyFormulas.h`):
   ```cpp
   static float CardReactionDecay(float VBase, float Delta, float Tau, float T) {
       return VBase + Delta * FMath::Exp(-T / Tau);
   }
   ```
4. **Decay Tick handler**:
   - `float T = GetWorld()->GetTimeSeconds() - ReactingStartTime;`
   - `if (T >= 5.0f * TauSSS) { ExitReacting(); return; }`
   - `float NewEmission = Formula4(EBase, 0.15f, 0.1f, T);` → `ApplyPreset` 경유 SetScalar.
   - `float NewSSS = Formula4(SSSBase, 0.08f, 0.33f, T);` → `ApplyPreset` 경유 SetScalar.
5. **ExitReacting**:
   - `ReactingHandle.Release(); RequestStateChange(EMossCharacterState::Idle);`
   - 감쇠 종료 후 QuietRest 복귀 시 story 005의 `DryingFactor` 재적용 (Formula 5).
6. **카드 종류 무관 동일 반응 (MVP)** — 카드별 차등 반응은 VS 검토 (OQ-2).

## Out of Scope

- QuietRest → Reacting 전환 시 `V_base` 계산 (story 005 covers Formula 4 + DryingOverlay order)
- GrowthTransition Lerp (story 004)
- Stillness Budget 구현 자체 (Feature epic)

## QA Test Cases

**Test Case 1 — AC-MBC-04 Formula 4 math**
- **Given**: `V_base = 0.03, Delta = 0.15, Tau = 0.1`.
- **When**: `t = 0, 0.3, 0.5, 1.65` 입력.
- **Then**: 출력 `0.18, 0.0375, 0.03010, 0.03` (±0.001). e^(-3)≈0.0498, e^(-5)≈0.0067.

**Test Case 2 — AC-MBC-05 Budget Deny**
- **Given**: Mock Stillness Budget이 `Motion/Standard` slot 점유 중 (Request returns invalid handle).
- **When**: `FOnCardOffered(Card001)` 발행.
- **Then**: `CurrentState` 여전히 `Idle`. `MID`의 EmissionStrength/SSS_Intensity 불변. DecayTickTimer 미시작.

**Test Case 3 — AC-MBC-17 이중 요청 방지**
- **Given**: Reacting 상태 t=0.5s (첫 번째 `FOnCardOffered` 후). 감쇠 진행 중.
- **When**: 두 번째 `FOnCardOffered` 발행.
- **Then**: `RequestStateChange(Reacting)` returns false (이미 Reacting 중). `ReactingStillnessHandle` 중복 Request 없음. 첫 감쇠 곡선 계속 — `EmissionStrength` 재부스트 없음 (V₀+Δ로 리셋 안 됨).

**Test Case 4 — Stage 1 subscription grep**
- **Setup**: `grep -rn "OnCardProcessedByGrowth" Source/MadeByClaudeCode/Core/MossBaby/`
- **Verify**: 매치 0건 — MBC는 Stage 2 구독 금지.
- **Pass**: grep empty.

## Test Evidence

- [ ] `tests/unit/moss-baby/formula4_decay_test.cpp` — AC-MBC-04 결정론적 수학
- [ ] `tests/integration/moss-baby/budget_deny_test.cpp` — AC-MBC-05
- [ ] `tests/integration/moss-baby/double_offer_test.cpp` — AC-MBC-17
- [ ] `tests/unit/moss-baby/stage1_subscription_grep.md` — ADR-0005 위반 grep

## Dependencies

- **Depends on**: story-001 (MossBaby + MID), story-002 (state machine)
- **Unlocks**: story-005 (QuietRest → Reacting transition order), story-008 (E4 compound event)
