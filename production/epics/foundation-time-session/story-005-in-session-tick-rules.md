# Story 005: In-session 1Hz Tick + Rules 5-8 + Formulas 2, 3 + FTSTicker

> **Epic**: Time & Session System
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 0.5 days (~3-4 hours)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/time-session-system.md`
**Requirement**: `TR-time-001` (Rule precedence first-match-wins) + `TR-time-004` (Suspend/resume behaviour — FTSTicker 기반 pause·focus loss 무관)
**ADR Governing Implementation**: ADR-0001 (Forward Time Tamper — Rule 5-8도 동일 정책 — 탐지 로직 없음)
**ADR Decision Summary**: In-session 1Hz tick은 `FTSTicker`에 등록 (engine-level, pause/focus 무관). Rule 5-8은 `MonoDelta` + `|Discrepancy|`로 판정. Rule 6(SUSPEND_RESUME) 우선, Rule 8(DISCREPANCY)은 first-match-wins 보호.
**Engine**: UE 5.6 | **Risk**: MEDIUM
**Engine Notes**: `FTSTicker::GetCoreTicker().AddTicker()` — `FTimerManager` 금지 (Tick pause 시 중단). 등록 해제는 `FTSTicker::GetCoreTicker().RemoveTicker(Handle)`. `FPlatformTime::Seconds()`는 Windows suspend 동작이 드라이버 의존 (OQ-IMP-1).

**Control Manifest Rules (Foundation layer)**:
- Required: Forbidden APIs "`FTimerManager` for 21일 실시간 추적" 회피 — `FDateTime::UtcNow()` 기반 경과 계산 + 세이브
- Required: first-match-wins 전제 — Rule 6 Suspend 우선, Rule 8 Discrepancy 후순위
- Forbidden: "compare `WallDelta > expected_max` for suspicion" (ADR-0001)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] MONOTONIC_DELTA_CLAMP_ZERO: 앱 재시작 후 `CurrentMonotonicSec = 0.5` (부팅 리셋) 시 `MonoDelta = max(0, M₁ − M₀)` = 0 + 크래시 없음 (Formula 2)
- [ ] NORMAL_TICK_IN_SESSION: 1Hz tick에서 `WallDelta = 1초`, `MonoDelta = 1초` (`|Δ| ≤ 5s`) → `ADVANCE_SILENT` (Rule 5)
- [ ] MINOR_SHIFT_NTP: NTP 재동기 `|Δ| = 15s` (5s < 15s ≤ 90min) → `ADVANCE_SILENT` + 진단 로그에 `MINOR_SHIFT` (Rule 7)
- [ ] MINOR_SHIFT_DST: DST spring-forward `|Δ| = 1h` → `ADVANCE_SILENT` + 플레이어 대면 알림 없음 (Rule 7)
- [ ] IN_SESSION_DISCREPANCY_LOG_ONLY: `|Δ| = 4h 58min` (>90min) → `LOG_ONLY` + `DayIndex` 변경 없음 (Rule 8)
- [ ] RULE_PRECEDENCE_FIRST_MATCH_WINS: `MonoΔ < 5s AND WallΔ = 2h` (Rule 6 + Rule 8 동시 match) → Rule 6 `ADVANCE_SILENT` (NOT Rule 8 LOG_ONLY), 로그 `SUSPEND_RESUME` 기록
- [ ] TICK_CONTINUES_WHEN_PAUSED: `GetWorld()->SetGamePaused(true)` 5초 대기 → `TickInSession()` ≥ 4회 호출 (FTSTicker engine-level)
- [ ] FOCUS_LOSS_CONTINUES_TICK: `WM_ACTIVATE` LOSS 후 3초 → `TickInSession()` ≥ 2회 호출 + 상태 `Active` 유지
- [ ] SESSION_COUNT_TODAY_RESET_CONTRACT: WallDelta = 25h → `DayIndex = 6` 전진 시 `SessionCountToday` = 0 리셋

---

## Implementation Notes

*Derived from GDD §Core Rules §Rule 5-8 + §Formulas 2, 3:*

```cpp
// Source/MadeByClaudeCode/Time/MossTimeSessionSubsystem.cpp
void UMossTimeSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    if (!Clock) { Clock = MakeShared<FRealClockSource>(); }  // production default
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UMossTimeSessionSubsystem::TickInSession),
        1.0f);  // 1Hz
}

void UMossTimeSessionSubsystem::Deinitialize() {
    if (TickHandle.IsValid()) {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
    Super::Deinitialize();
}

bool UMossTimeSessionSubsystem::TickInSession(float /*DeltaTime*/) {
    const FDateTime Now = Clock->GetUtcNow();
    const double MonoNow = Clock->GetMonotonicSec();
    const double WallDeltaSec = (Now - CurrentRecord.LastWallUtc).GetTotalSeconds();
    const double MonoDelta = FMath::Max(0.0, MonoNow - CurrentRecord.LastMonotonicSec);  // Formula 2
    const double DiscrepancyAbs = FMath::Abs(WallDeltaSec - MonoDelta);

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();

    // Rule 6 — SUSPEND_RESUME (first-match-wins vs Rule 8)
    if (MonoDelta < Settings->SuspendMonotonicThresholdSec
        && WallDeltaSec > Settings->SuspendWallThresholdSec) {
        UE_LOG(LogMossTime, Verbose, TEXT("classification: SUSPEND_RESUME"));
        AdvanceDayIfNeeded(WallDeltaSec);
        OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_SILENT);
        CurrentRecord.LastWallUtc = Now;
        CurrentRecord.LastMonotonicSec = MonoNow;
        return true;
    }

    // Rule 5 — NORMAL_TICK
    if (DiscrepancyAbs <= Settings->DefaultEpsilonSec) {
        OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_SILENT);
        CurrentRecord.LastWallUtc = Now;
        CurrentRecord.LastMonotonicSec = MonoNow;
        return true;
    }

    // Rule 7 — MINOR_SHIFT (NTP/DST tolerance)
    const double ToleranceSec = Settings->InSessionToleranceMinutes * 60.0;
    if (DiscrepancyAbs <= ToleranceSec) {
        UE_LOG(LogMossTime, Verbose, TEXT("classification: MINOR_SHIFT"));
        OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_SILENT);
        CurrentRecord.LastWallUtc = Now;
        CurrentRecord.LastMonotonicSec = MonoNow;
        return true;
    }

    // Rule 8 — IN_SESSION_DISCREPANCY
    UE_LOG(LogMossTime, Warning, TEXT("classification: IN_SESSION_DISCREPANCY"));
    OnTimeActionResolved.Broadcast(ETimeAction::LOG_ONLY);
    // DayIndex 변경 없음, WallUtc·Mono 갱신 없음 (다음 tick에서 재평가)
    return true;
}

void UMossTimeSessionSubsystem::AdvanceDayIfNeeded(double WallDeltaSec) {
    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    const int32 DaysElapsed = FMath::FloorToInt32(WallDeltaSec / 86400.0);
    if (DaysElapsed > 0) {
        const int32 NewDay = FMath::Clamp(CurrentRecord.DayIndex + DaysElapsed, 1, Settings->GameDurationDays);
        if (NewDay != CurrentRecord.DayIndex) {
            CurrentRecord.DayIndex = NewDay;
            CurrentRecord.SessionCountToday = 0;  // SESSION_COUNT_TODAY_RESET_CONTRACT
            OnDayAdvanced.Broadcast(NewDay);
        }
    }
}
```

- `TickInSession` 반환 `true`: `FTSTicker`가 계속 호출
- `FTSTicker`는 engine-level — `SetGamePaused(true)` 및 focus loss에 영향 없음 (GDD §Window Focus vs OS Sleep 표)
- Rule 5-8 순서는 Rule 6 first-match-wins (RULE_PRECEDENCE AC), 그 후 Rule 5 → 7 → 8 순

---

## Out of Scope

- Story 004: Between-session classifier (선행)
- Story 006: Farewell idempotency
- Story 007: Forward Tamper CI grep + Windows suspend manual

---

## QA Test Cases

**For Logic story:**
- **NORMAL_TICK_IN_SESSION**
  - Given: Session Active, `LastWallUtc`/`LastMonotonicSec` set
  - When: Mock tick — WallDelta 1s, MonoDelta 1s
  - Then: `ADVANCE_SILENT` + `DayIndex` 불변
- **RULE_PRECEDENCE_FIRST_MATCH_WINS**
  - Given: Session Active, Mock: WallDelta=2h, MonoDelta=0s
  - When: `TickInSession()`
  - Then: `ADVANCE_SILENT` (Rule 6) — NOT `LOG_ONLY` (Rule 8); 로그 `SUSPEND_RESUME`
- **IN_SESSION_DISCREPANCY_LOG_ONLY**
  - Given: Session Active, 2분 경과
  - When: WallDelta=5h, MonoDelta=2min (|Δ| ≈ 4h58min > 90min)
  - Then: `LOG_ONLY` + `DayIndex` 불변 + 플레이어 알림 없음
- **MONOTONIC_DELTA_CLAMP_ZERO**
  - Given: `LastMonotonicSec = 600.0`
  - When: Mock `MonoNow = 0.5` (부팅 리셋)
  - Then: MonoDelta = 0 (clamp 음수), process 정상 종료, 분류 결과는 WallDelta에 따라
- **TICK_CONTINUES_WHEN_PAUSED**
  - Given: Session Active, 1Hz FTSTicker registered
  - When: `SetGamePaused(true)` 5초
  - Then: TickInSession 호출 ≥ 4회 (±1)
  - 대조군: `FTimerManager` 기반 타이머 = 0회
- **SESSION_COUNT_TODAY_RESET_CONTRACT**
  - Given: `Prev.DayIndex = 5`, `SessionCountToday = 3`
  - When: WallDelta = 25h → NewDayIndex = 6
  - Then: `SessionCountToday == 0`

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/time-session/in_session_tick_test.cpp` (Rule 5-8 AUTOMATED + FTSTicker pause independence test)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 003 (Subsystem 뼈대), Story 004 (Between-session 부분 구현)
- Unlocks: Story 006

---

## Completion Notes

**Completed**: 2026-04-19
**Criteria**: 7/9 AUTOMATED + 2 deferred integration (TICK_CONTINUES_WHEN_PAUSED, FOCUS_LOSS_CONTINUES_TICK)

**Files delivered**:
- `Time/MossTimeSessionSubsystem.h/.cpp` (수정) — FTSTicker 1Hz 등록/해제 + TickInSession + AdvanceDayIfNeeded + Testing 훅
- `Tests/TimeSessionTickRulesTests.cpp` (신규, ~8 tests)
- `tests/unit/time-session/README.md` Story 1-18 섹션 append

**Test Evidence**: 8 UE Automation — `MossBaby.Time.Tick.*`

**AC 커버**:
- [x] MONOTONIC_DELTA_CLAMP_ZERO (Formula 2 max(0, delta))
- [x] NORMAL_TICK_IN_SESSION (Rule 5, |Δ| ≤ DefaultEpsilonSec)
- [x] MINOR_SHIFT_NTP (Rule 7, 15s)
- [x] MINOR_SHIFT_DST (Rule 7, 3600s)
- [x] IN_SESSION_DISCREPANCY_LOG_ONLY (Rule 8, >90min)
- [x] RULE_PRECEDENCE_FIRST_MATCH_WINS (Rule 6 over Rule 8)
- [x] SESSION_COUNT_TODAY_RESET_CONTRACT (DayIndex 전진 시 0 리셋)
- [~] TICK_CONTINUES_WHEN_PAUSED: TD-012 (실제 GameWorld + SetGamePaused 필요)
- [~] FOCUS_LOSS_CONTINUES_TICK: TD-012

**ADR-0001 준수**:
- 금지 패턴 신규 코드 0 매치
- WallDelta > expected_max 의심 분기 없음
- Rule 5-8 동일 정책 (forward tamper 탐지 로직 부재)

**Engine Idiom**:
- FTSTicker::GetCoreTicker().AddTicker (engine-level, pause/focus 무관)
- FTimerManager 0건 사용
- Formula 2 FMath::Max(0.0, MonoNow - Last) strict clamp

**Deferred (TD-012)**:
- TICK_CONTINUES_WHEN_PAUSED / FOCUS_LOSS_CONTINUES_TICK — 실제 GameWorld + SetGamePaused(true) 시나리오 integration test
