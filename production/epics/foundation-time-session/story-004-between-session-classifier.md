# Story 004: Between-session Classifier (Rules 1-4) + Formulas 1, 4, 5

> **Epic**: Time & Session System
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/time-session-system.md`
**Requirement**: `TR-time-001` (NO_FORWARD_TAMPER_DETECTION_ADR — Forward tamper를 정상 경과처럼 silent 수용)
**ADR Governing Implementation**: ADR-0001 (Forward Time Tamper 명시적 수용 정책)
**ADR Decision Summary**: Rule 4 (ACCEPTED_GAP)에서 `0 ≤ WallDelta ≤ 21일`이면 forward tamper도 정상 경과처럼 `DayIndex` 전진. Rule 3 (LONG_GAP_SILENT)에서 `WallDelta > 21일`이면 즉시 Farewell. 탐지·분기·로깅 금지.
**Engine**: UE 5.6 | **Risk**: MEDIUM
**Engine Notes**: `FSessionRecord` 서브밀리초 정밀도는 Formula 2 `double` 필드로 보장 (Story 002). Formula 4 `floor((WallDelta seconds) / 86400)` + clamp 상한 `GameDurationDays`.

**Control Manifest Rules (Foundation layer)**:
- Required: "Forward time tamper는 정상 경과와 bit-exact 동일 처리 — 분기·탐지·로깅 일체 없음" (ADR-0001)
- Forbidden: "변수·함수 이름에 `tamper`/`cheat`/`bIsForward`/`bIsSuspicious`/`DetectClockSkew`/`IsSuspiciousAdvance` 패턴" (ADR-0001)
- Forbidden: "compare `WallDelta > expected_max` for suspicion" — algorithmically indistinguishable (ADR-0001)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] FIRST_RUN: `PrevRecord == nullptr` 시 `START_DAY_ONE` + `DayIndex == 1` + 신규 `FGuid SessionUuid` 생성 (Rule 1)
- [ ] BACKWARD_GAP_REJECT: `CurrentWallUtc < Prev.LastWallUtc`이면 `HOLD_LAST_TIME` + `DayIndex` 유지 + `LastWallUtc` 갱신 없음 (Rule 2)
- [ ] BACKWARD_GAP_REPEATED: BACKWARD_GAP 2회 연속 시 두 번째도 `HOLD_LAST_TIME` + 모두 불변
- [ ] LONG_GAP_SILENT_BOUNDARY_OVER: `WallDelta = 21일 + 1초`에서 `LONG_GAP_SILENT` 발행 + `DayIndex == 21` clamp + `OnFarewellReached(LongGapAutoFarewell)` 즉시 발행 + `FarewellExit` 상태 전환 (Rule 3)
- [ ] LONG_GAP_BOUNDARY_EXACT: `WallDelta = 정확히 21일`에서 `ADVANCE_WITH_NARRATIVE` (`NarrativeCount < cap`) + `DayIndex == 21` + `LONG_GAP_SILENT` 미발행
- [ ] ACCEPTED_GAP_SILENT: `WallDelta = 12h` (24h 이하) 시 `ADVANCE_SILENT` + `DayIndex` 유지 (Rule 4 between)
- [ ] ACCEPTED_GAP_NARRATIVE_THRESHOLD: `WallDelta = 24h + 1초` + `NarrativeCount = 0` 시 `ADVANCE_WITH_NARRATIVE` + `DayIndex == 4` + `NarrativeCount == 1` 영속화
- [ ] NARRATIVE_THRESHOLD_EXACT_BOUNDARY: `WallDelta = 정확히 24h`에서 `ADVANCE_SILENT` (Formula 5 strict `>`, cap 영향 없음)
- [ ] NARRATIVE_CAP_ENFORCED: `NarrativeCount = 3`(cap 도달) 상태 + 24h 초과 시 `ADVANCE_SILENT` (cap 강제 차단) + 진단 로그 "cap reached, narrative suppressed"
- [ ] DAYINDEX_FORMULA_FLOOR: `WallDelta = 48h 30min` + `Prev.DayIndex = 3` → `DayIndex == 5` (floor(48.5/24) = 2)
- [ ] DAYINDEX_CLAMP_UPPER: `Prev.DayIndex = 20` + `WallDelta = 48h` → `DayIndex == 21` (clamp)
- [ ] DAYINDEX_CORRUPTION_CLAMP: `DayIndex = 25` (범위 외) 주입 시 `clamp(25, 1, 21) = 21` 보정 + 로그

---

## Implementation Notes

*Derived from GDD §Formulas + ADR-0001 §Decision:*

```cpp
// Source/MadeByClaudeCode/Time/MossTimeSessionSubsystem.cpp
ETimeAction UMossTimeSessionSubsystem::ClassifyOnStartup(const FSessionRecord* PrevRecord) {
    // Rule 1 — FIRST_RUN
    if (!PrevRecord) {
        CurrentRecord.SessionUuid = FGuid::NewGuid();
        CurrentRecord.DayIndex = 1;
        CurrentRecord.LastWallUtc = Clock->GetUtcNow();
        CurrentRecord.LastMonotonicSec = Clock->GetMonotonicSec();
        OnTimeActionResolved.Broadcast(ETimeAction::START_DAY_ONE);
        return ETimeAction::START_DAY_ONE;
    }

    CurrentRecord = *PrevRecord;
    // E13 corruption clamp (sanity check before classification)
    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    CurrentRecord.DayIndex = FMath::Clamp(CurrentRecord.DayIndex, 1, Settings->GameDurationDays);

    const FDateTime Now = Clock->GetUtcNow();
    const FTimespan WallDelta = Now - PrevRecord->LastWallUtc;
    const double WallDeltaSec = WallDelta.GetTotalSeconds();

    // Rule 2 — BACKWARD_GAP_REJECT (유일하게 시각 거부)
    if (WallDeltaSec < 0.0) {
        OnTimeActionResolved.Broadcast(ETimeAction::HOLD_LAST_TIME);
        return ETimeAction::HOLD_LAST_TIME;  // LastWallUtc 갱신 없음
    }

    // Rule 3 — LONG_GAP_SILENT (> 21일)
    const double GameDurationSec = Settings->GameDurationDays * 86400.0;
    if (WallDeltaSec > GameDurationSec) {
        CurrentRecord.DayIndex = Settings->GameDurationDays;  // clamp 21
        OnTimeActionResolved.Broadcast(ETimeAction::LONG_GAP_SILENT);
        OnFarewellReached.Broadcast(EFarewellReason::LongGapAutoFarewell);
        // 상태 머신은 GSM epic이 소비 — 이 subsystem은 FarewellExit 전환만 기록
        return ETimeAction::LONG_GAP_SILENT;
    }

    // Rule 4 — ACCEPTED_GAP (Forward tamper도 여기로 silent 수용 — ADR-0001)
    // Formula 4: DayIndex 전진
    const int32 DaysElapsed = FMath::FloorToInt32(WallDeltaSec / 86400.0);
    const int32 NewDayIndex = FMath::Clamp(PrevRecord->DayIndex + DaysElapsed, 1, Settings->GameDurationDays);
    CurrentRecord.DayIndex = NewDayIndex;
    CurrentRecord.LastWallUtc = Now;

    // Formula 5 — Narrative threshold (strict `>` + cap)
    const double NarrativeThresholdSec = Settings->NarrativeThresholdHours * 3600.0;
    const bool bCrossThreshold = WallDeltaSec > NarrativeThresholdSec;  // strict
    const bool bUnderCap = PrevRecord->NarrativeCount < Settings->NarrativeCapPerPlaythrough;

    if (bCrossThreshold && bUnderCap) {
        IncrementNarrativeCountAndSave();  // atomic wrapper (Save/Load §5 R2)
        OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_WITH_NARRATIVE);
        return ETimeAction::ADVANCE_WITH_NARRATIVE;
    }
    if (bCrossThreshold && !bUnderCap) {
        UE_LOG(LogMossTime, Verbose, TEXT("cap reached, narrative suppressed"));
    }
    OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_SILENT);
    return ETimeAction::ADVANCE_SILENT;
}
```

- **중요**: 이 코드 어디에도 "tamper"/"bIsForward"/"expected_max" 분기 없음 (ADR-0001)
- `IncrementNarrativeCountAndSave`는 Story 003에서 선언한 single public method (Save/Load §5 contract)
- `SessionUuid`는 앱 재시작마다 새로 생성 (§FIRST_RUN; BETWEEN-SESSION classification도 Uuid 갱신 여부는 GDD §Rule 1에 따름 — FIRST_RUN만 신규)

---

## Out of Scope

- Story 005: In-session Rules 5-8 (1Hz tick) + Formula 2, 3
- Story 006: Farewell idempotency + `FAREWELL_NORMAL_COMPLETION`
- Story 007: CI grep hook `NO_FORWARD_TAMPER_DETECTION`

---

## QA Test Cases

**For Logic story:**
- **FIRST_RUN**
  - Given: `FMockClockSource` set, `PrevRecord = nullptr`
  - When: `ClassifyOnStartup(nullptr)`
  - Then: `START_DAY_ONE` delegate + `DayIndex == 1` + UUID 생성
  - Edge cases: 즉시 재호출 시 FIRST_RUN 재발행 (PrevRecord nullptr이면 idempotent)
- **BACKWARD_GAP_REJECT**
  - Given: `Prev.LastWallUtc = T₀`, `DayIndex = 5`
  - When: Mock UTC = T₀ - 48h, `ClassifyOnStartup(&Prev)`
  - Then: `HOLD_LAST_TIME` + `DayIndex == 5` 불변 + `CurrentRecord.LastWallUtc` 갱신 없음
- **LONG_GAP_SILENT_BOUNDARY_OVER**
  - Given: `Prev.DayIndex = 5`, `Prev.LastWallUtc = T₀`
  - When: Mock UTC = T₀ + 21일 + 1초
  - Then: `LONG_GAP_SILENT` delegate 1회 + `OnFarewellReached(LongGapAutoFarewell)` 1회 + `DayIndex == 21`
- **ACCEPTED_GAP_NARRATIVE_THRESHOLD**
  - Given: `Prev.DayIndex = 3`, `NarrativeCount = 0`, cap = 3
  - When: Mock UTC = T₀ + 24h + 1초
  - Then: `ADVANCE_WITH_NARRATIVE` + `DayIndex == 4` + `NarrativeCount == 1`
  - Edge cases: 24h 정각 → `ADVANCE_SILENT`, `NarrativeCount` 불변 (strict `>`)
- **NARRATIVE_CAP_ENFORCED**
  - Given: `NarrativeCount = 3`, cap = 3
  - When: WallDelta = 48h
  - Then: `ADVANCE_SILENT` + `NarrativeCount` 불변 + 진단 로그 "cap reached"
- **DAYINDEX_CORRUPTION_CLAMP**
  - Given: Injected `FSessionRecord.DayIndex = 25`
  - When: `ClassifyOnStartup(&Prev)`
  - Then: clamp 21 보정 + 보정 로그 + crash 없음

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/time-session/between_session_classifier_test.cpp` (12 AUTOMATED AC 케이스 각각 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 003 (Subsystem 뼈대 + Settings)
- Unlocks: Story 005, 007
