# Story 008 — Withered Lifecycle + `FSessionRecord.bWithered` 영속화

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Rule 1 "메마름(Withered) 서브컨디션" + §Edge Cases E18 + §Withered Lifecycle (R3) — **Vocabulary Rule**.
- **TR-ID**: TR-gsm-003 (`bWithered` persistence + MPC path)
- **Governing ADR**: ADR-0005 §Stage 2 — `bWithered = false` 해제는 Growth Handler 경로 (Stage 2 수신 시) — 단, GDD §E18은 "첫 카드 건넴(`FOnCardOffered`)으로 해제" 명시하여 Stage 1/2 timing 이슈. 구현 선택: **Stage 2 수신 시 해제** (Growth 완료 후 영속화 보장). 시각 즉시성은 MBC가 Stage 1 Reacting으로 별도 제공.
- **Engine Risk**: LOW
- **Engine Notes**: `bWithered`는 `EGameState`의 새 값이 아니라 `FSessionRecord`의 `bool` 필드. `LONG_GAP_SILENT`는 항상 DayIndex=21이므로 withered 경로에 관여하지 않음.
- **Control Manifest Rules**: 
  - **Cross-Cutting Constraints**: SaveSchemaVersion bump 정책 (필드 추가 시 +1). USaveGame 필드 전부 `UPROPERTY()`.
  - **GSM §Withered Lifecycle Vocabulary Rule (GDD §R3 2026-04-19 추가)**: `bWithered` / `WitheredThresholdDays`는 기술적 영속화 identifier이며 `FSessionRecord`의 bool 필드. 플레이어 대면 vocabulary·시각 표현은 [MBC CR-5 "Quiet Rest / 고요한 대기"](../../../design/gdd/moss-baby-character-system.md) 의미론을 따른다. **금지**: drying, yellowing, 탈색, 건조화 시각 큐. `bWithered = true`는 MBC DryingFactor 상승의 영속화 플래그일 뿐, GSM이 독립적 visual layer를 추가하지 않음. MPC `MossBabySSSBoost` 감소는 "쉼"의 차분함(서리·차가운 톤) 표현이며 "메마름" 훼손 표현 아님.

## Acceptance Criteria

- **AC-GSM-20** [`AUTOMATED`/BLOCKING] — Waiting 상태 + `PrevDayIndex = 5`에서 `ADVANCE_SILENT` 수신 + `NewDayIndex = 10`(점프폭 5 > `WitheredThresholdDays` 3)일 때 `bWithered = true` 설정 + `FSessionRecord`에 영속화 + MPC `MossBabySSSBoost`가 Waiting 기본값(0.10)보다 감소 적용 (E18).
- **AC-GSM-21** [`AUTOMATED`/BLOCKING] — `bWithered = true` Waiting 상태에서 `FOnCardProcessedByGrowth` 수신 (Stage 2, ADR-0005) 시 `bWithered = false` 설정 + MPC 파라미터가 정상 Waiting 목표값으로 복원 + `FSessionRecord` 갱신 영속화.

## Implementation Notes

1. **`FSessionRecord.bWithered` 필드 추가** (Save/Load epic에서 이미 정의되거나 본 story에서 확장):
   - `UPROPERTY() bool bWithered = false;`
   - SaveSchemaVersion +1 필요 — Save/Load Schema versioning 규약 준수.
2. **Withered 진입 로직** (AC-GSM-20):
   - `HandleTimeActionResolved(ETimeAction::ADVANCE_SILENT)`에서 `const int32 Jump = NewDayIndex - PrevDayIndex;` 계산.
   - `if (Jump > GetSettings()->WitheredThresholdDays && CurrentState != EGameState::Farewell) { SetWithered(true); }`
   - `SetWithered(bool bNew)`: `Record.bWithered = bNew;` → `TriggerSave(ESaveReason::EWitheredChanged);` → MPC `MossBabySSSBoost` target 조정 (Waiting 기본 0.10 → withered 0.05 등 GDD 명시값).
3. **Withered 해제 로직** (AC-GSM-21):
   - Story 004의 `OnCardProcessedByGrowth(const FGrowthProcessResult&)` handler 확장: `if (Record.bWithered) { SetWithered(false); }` 선행.
   - MPC는 story 003의 TickMPC가 `Record.bWithered` 플래그 참조하여 target 조정.
4. **MPC 파라미터 조정 패턴** (GDD §Cross-Layer):
   - `TickMPC` 내 state target lookup 시 `if (Record.bWithered && NewState == EGameState::Waiting) { Target.SSS *= 0.5f; Target.ContrastLevel *= 0.8f; }` 식으로 감산. 리터럴 금지 (AC-GSM-06) — `UGameStateMPCAsset` 내 `FWitheredModifier` sub-struct 정의.
5. **Vocabulary 준수 (Pillar 1 보호)**:
   - MPC 조정은 "서리·차가운 톤" 시각 — drying/yellowing 키워드 코드/에셋 명명 금지. `WitheredModifier` 필드명은 기술 identifier로만 사용. UI·에셋 표시 vocabulary는 MBC CR-5 "Quiet Rest" 사용.

## Out of Scope

- MBC `DryingFactor` 계산 (Moss Baby epic story — CR-5)
- Save/Load Schema versioning 구현 자체 (Foundation Save/Load epic)
- LONG_GAP_SILENT(Day 21 직행) — story 007에서 처리 완료

## QA Test Cases

**Test Case 1 — AC-GSM-20 Withered entry**
- **Given**: `CurrentState = Waiting`, `PrevDayIndex = 5`, `WitheredThresholdDays = 3`, `Record.bWithered = false`.
- **When**: `ADVANCE_SILENT(NewDayIndex=10)` 수신 (점프폭 5 > 3).
- **Then**: `Record.bWithered == true`. `TriggerSave(EWitheredChanged)` 호출 검증. 다음 TickMPC에서 MPC `MossBabySSSBoost < 0.10` (Waiting 기본값 감소).

**Test Case 2 — AC-GSM-21 Withered clear on Stage 2**
- **Given**: `Record.bWithered = true`, `CurrentState = Waiting`.
- **When**: Growth broadcasts `FOnCardProcessedByGrowth({bIsDay21Complete=false})`.
- **Then**: `Record.bWithered == false`. MPC `MossBabySSSBoost = 0.10` (정상 Waiting). `TriggerSave` 호출 검증.

**Test Case 3 — Vocabulary grep (Pillar 1 보호)**
- **Setup**: `grep -rniE "drying|yellowing|wilting|wither" Source/MadeByClaudeCode/Core/GameState/ Content/` 
- **Verify**: 매치는 기술 identifier (`bWithered`, `WitheredThresholdDays`, `WitheredModifier`)에만 한정. UI string / 에셋 경로에 `drying*` / `yellowing*` 0건.
- **Pass**: grep 결과가 기술 식별자 범위 내.

## Test Evidence

- [ ] `tests/integration/game-state/withered_entry_test.cpp` — AC-GSM-20
- [ ] `tests/integration/game-state/withered_clear_stage2_test.cpp` — AC-GSM-21
- [ ] `tests/unit/game-state/vocabulary_grep.md` — Pillar 1 vocabulary 검증

## Dependencies

- **Depends on**: story-004 (Stage 2 subscription), story-003 (MPC tick for SSS path)
- **Unlocks**: Moss Baby epic CR-5 Quiet Rest coordination
