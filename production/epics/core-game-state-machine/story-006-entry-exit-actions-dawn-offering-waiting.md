# Story 006 — 6-State Entry/Exit Actions + `DawnMinDwellSec` Guard

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §상태별 진입/이탈 액션 + §Formula 3 (Dawn 최소 체류 시간)
- **TR-ID**: TR-gsm-005 (Farewell 종단), TR-gsm-006 (Day 21 P0)
- **Governing ADR**: ADR-0011 §Decision — `UMossGameStateSettings : UDeveloperSettings` with `meta=(ConfigRestartRequired=true)` where applicable.
- **Engine Risk**: LOW
- **Engine Notes**: Subsystem lazy-init 패턴 — `TWeakObjectPtr` 캐싱 + null check + re-acquire. Dawn "새로운 날" 감지 — `FOnLoadComplete` + `FOnTimeActionResolved` 순차 수신.
- **Control Manifest Rules**: Cross-Layer Rules §Tuning Knob — `UMossGameStateSettings : UDeveloperSettings` with `UCLASS(Config=Game, DefaultConfig)`. Global Rules §Forbidden APIs — `FTimerManager` for 21일 실시간 추적 금지 (Dawn dwell은 단기 타이머는 OK).

## Acceptance Criteria

- **AC-GSM-12** [`AUTOMATED`/BLOCKING] — DawnMinDwellSec=3.0, Menu→Dawn 블렌드 0.9s일 때 Formula 3 계산 시 추가 대기 2.1s, 블렌드 완료 후 2.1s 전에 Offering 전환 미시작.
- **AC-GSM-17** [`AUTOMATED`/BLOCKING] — Dawn 상태에서 `CardSystemReadyTimeoutSec` 초과 시 Offering 강제 전환 + Degraded 플래그 전달.

## Implementation Notes

1. **`UMossGameStateSettings : UDeveloperSettings`** (`Source/MadeByClaudeCode/Core/GameState/MossGameStateSettings.h`):
   - `UCLASS(Config=Game, DefaultConfig)`.
   - `UPROPERTY(Config, EditAnywhere, Category="Game State", meta=(ClampMin=0.0, ClampMax=1.0)) float BlendDurationBias = 0.5f;`
   - `UPROPERTY(Config, EditAnywhere, Category="Game State", meta=(ClampMin=2.0, ClampMax=5.0)) float DawnMinDwellSec = 3.0f;`
   - `UPROPERTY(Config, EditAnywhere, Category="Game State", meta=(ClampMin=3.0, ClampMax=10.0)) float CardSystemReadyTimeoutSec = 5.0f;`
   - `UPROPERTY(Config, EditAnywhere, Category="Game State", meta=(ClampMin=30.0, ClampMax=120.0)) float DreamDeferMaxSec = 60.0f;`
   - `UPROPERTY(Config, EditAnywhere, Category="Game State", meta=(ClampMin=2, ClampMax=7)) int32 WitheredThresholdDays = 3;`
2. **각 상태 Entry/Exit handlers**:
   - `HandleMenuEntry()`, `HandleDawnEntry()`, `HandleOfferingEntry()`, `HandleWaitingEntry()`, `HandleDreamEntry()`, `HandleFarewellEntry()`.
   - 각 handler는 MPC target 지정 + `OnGameStateChanged.Broadcast(Old, New)`.
3. **Dawn 진입 시 `DawnMinDwellSec` 타이머**:
   - `GetWorld()->GetTimerManager().SetTimer(DawnDwellTimer, this, &UMossGameStateSubsystem::TryAdvanceToOffering, GetSettings()->DawnMinDwellSec, false);`
   - `TryAdvanceToOffering`: (1) 블렌드 완료 확인, (2) CardSystem Ready 확인, (3) 둘 다 true → `RequestStateTransition(Offering, P1)`.
4. **`CardSystemReadyTimeoutSec` guard (AC-GSM-17)**:
   - 별도 타이머 `CardReadyTimeout`. 만료 시 `bDegradedFallback = true` + 강제 Offering 전환.
5. **Menu → Dawn/Waiting/Offering/Farewell 분기 (GDD §전환 표)**:
   - `FOnLoadComplete(bFreshStart, bHadPreviousData)` 수신 handler에서 `LastPersistedState` 기반 분기.
6. **Formula 3 SmoothStep 유사 — 이 formula는 선형**: `T_wait = max(0, DawnMinDwellSec − D_blend_elapsed)`. 순수 함수로 unit test.

## Out of Scope

- Narrative overlay (Rule 5) — 세부 구현은 story 009 (E11 Narrative)
- Dream Budget request (story 007 Dream defer E5)
- `FOnLoadComplete` publish는 Save/Load epic

## QA Test Cases

**Test Case 1 — AC-GSM-12 Dawn dwell Formula 3**
- **Given**: `DawnMinDwellSec = 3.0, D_blend_elapsed = 0.9`.
- **When**: `Formula3` 순수 함수 호출.
- **Then**: `T_wait = 2.1s` (±1e-6). `D_blend_elapsed = 5.0` → `T_wait = 0` (clamp).

**Test Case 2 — AC-GSM-17 CardSystem timeout**
- **Given**: `CurrentState = Dawn`, `CardSystemReadyTimeoutSec = 5.0`, Mock CardSystem은 Ready 시그널 미발행.
- **When**: 5.1초 경과 (시뮬 시간).
- **Then**: `CurrentState = Offering`, `bDegradedFallback = true`, downstream에 Degraded 플래그 전달 확인.

**Test Case 3 — Settings class integration**
- **Setup**: Editor → Project Settings → Game → Moss Baby → Game State 카테고리.
- **Verify**: 5개 UPROPERTY 모두 편집 가능. `meta=(ClampMin/ClampMax)` 범위 강제.
- **Pass**: Editor UI에서 모든 knob 편집 + range clamp 확인.

## Test Evidence

- [ ] `tests/unit/game-state/formula3_dawn_dwell_test.cpp` — Formula 3 순수 수학
- [ ] `tests/integration/game-state/card_ready_timeout_test.cpp` — AC-GSM-17 시뮬
- [ ] `production/qa/evidence/game-state-settings-[date].md` — Project Settings UI 스크린샷

## Dependencies

- **Depends on**: story-002 (transition table), story-004 (delegate broadcast)
- **Unlocks**: story-007 (edge cases E5 Dream defer uses DreamDeferMaxSec)
