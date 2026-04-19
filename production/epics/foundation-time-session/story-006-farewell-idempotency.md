# Story 006: Farewell 상태 전환 + LONG_GAP_SILENT_IDEMPOTENT (Save/Load integration)

> **Epic**: Time & Session System
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/time-session-system.md`
**Requirement**: `TR-time-001` (NO_FORWARD_TAMPER_DETECTION + Farewell idempotency)
**ADR Governing Implementation**: ADR-0001 (Forward Time Tamper → LONG_GAP도 silent Farewell)
**ADR Decision Summary**: LONG_GAP_SILENT 후 앱 재시작 시 `FarewellExit` 상태 유지 (재진입 불가). `ESaveReason::EFarewellReached` 직렬화 경유로 Time + Save/Load round-trip.
**Engine**: UE 5.6 | **Risk**: LOW
**Engine Notes**: Save/Load epic이 실제 직렬화 소유 — 본 story는 Time 측 `FarewellExit` 플래그 및 delegate 계약만 구현. Integration test는 Save/Load story-003 완료 후 실행 가능.

**Control Manifest Rules (Foundation layer)**:
- Required: "Save/Load는 per-trigger atomicity만 보장" — Farewell도 단일 SaveAsync 호출 (ADR-0009)
- Forbidden: Core Rules "Farewell → Any 상태 전환 분기 금지" — AC-GSM-05 grep 0건 (GSM 경계, 본 story는 Time 내부 멱등성)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] FAREWELL_NORMAL_COMPLETION: `DayIndex == 21` + 카드 건넴 이벤트 수신 시 `OnFarewellReached(NormalCompletion)` 발행 + `FarewellExit` 전환 + 이후 `ClassifyOnStartup` 재호출 시 `FarewellExit` 유지 (재진입 불가)
- [ ] FAREWELL_LONG_GAP_SILENT: LONG_GAP 시 `OnFarewellReached(LongGapAutoFarewell)` 1회 발행 + `EFarewellReason` 두 값 `NormalCompletion`, `LongGapAutoFarewell` 분리 정의
- [ ] LONG_GAP_SILENT_IDEMPOTENT: 이미 `FarewellExit` + `LastSaveReason == "EFarewellReached"` 상태에서 앱 재시작 시 `ClassifyOnStartup` 재호출이 추가 `LONG_GAP_SILENT` 미발행 + 추가 `OnFarewellReached` 미발행 (INTEGRATION — Time + Save/Load cross-system round-trip)

---

## Implementation Notes

*Derived from GDD §Acceptance Criteria FAREWELL* + §States and Transitions:*

```cpp
// Source/MadeByClaudeCode/Time/MossTimeSessionSubsystem.h — 추가
public:
    bool IsInFarewellExit() const { return bFarewellReached; }
private:
    bool bFarewellReached = false;

// Source/MadeByClaudeCode/Time/MossTimeSessionSubsystem.cpp — ClassifyOnStartup 보강
ETimeAction UMossTimeSessionSubsystem::ClassifyOnStartup(const FSessionRecord* PrevRecord) {
    // Idempotency guard — LONG_GAP_SILENT_IDEMPOTENT
    if (PrevRecord && PrevRecord->LastSaveReason == TEXT("EFarewellReached")) {
        bFarewellReached = true;
        CurrentRecord = *PrevRecord;
        // 재진입 방지: FarewellExit 유지, delegate 재발행 없음
        return ETimeAction::HOLD_LAST_TIME;  // 또는 별도 ENO_ACTION 고려 (GDD 참조)
    }

    // ... 기존 FIRST_RUN / BACKWARD / LONG_GAP / ACCEPTED 분기 (Story 004)

    // LONG_GAP 시 bFarewellReached 설정
    if (WallDeltaSec > GameDurationSec) {
        CurrentRecord.DayIndex = Settings->GameDurationDays;
        CurrentRecord.LastSaveReason = TEXT("EFarewellReached");
        bFarewellReached = true;
        OnTimeActionResolved.Broadcast(ETimeAction::LONG_GAP_SILENT);
        OnFarewellReached.Broadcast(EFarewellReason::LongGapAutoFarewell);
        // Save/Load epic이 FarewellReached reason으로 SaveAsync 호출 (GSM 라우팅)
        return ETimeAction::LONG_GAP_SILENT;
    }
    // ...
}

// Day 21 NORMAL_COMPLETION — GSM이 카드 건넴 이벤트 수신 후 이 메서드 호출
void UMossTimeSessionSubsystem::OnDay21CardOffered() {
    if (CurrentRecord.DayIndex == UMossTimeSessionSettings::Get()->GameDurationDays
        && !bFarewellReached) {
        bFarewellReached = true;
        CurrentRecord.LastSaveReason = TEXT("EFarewellReached");
        OnFarewellReached.Broadcast(EFarewellReason::NormalCompletion);
    }
}
```

- `EFarewellReason`은 Story 003에서 정의
- Save/Load 측 `FSessionRecord.LastSaveReason` 직렬화는 `foundation-save-load` epic story-002/003 소관
- Integration test는 Save epic 완료 후 `tests/integration/time-session/` 하위에 작성

---

## Out of Scope

- Save/Load 직렬화 구현 (`foundation-save-load` epic)
- GSM `FarewellExit` 상태 머신 (Core layer `core-game-state-machine` epic)
- Story 007: CI grep hook + Windows suspend manual

---

## QA Test Cases

**For Integration story:**
- **FAREWELL_NORMAL_COMPLETION**
  - Given: `CurrentRecord.DayIndex == 21`, not yet farewell
  - When: `OnDay21CardOffered()` 호출 → SaveAsync 발행 → 앱 재시작 → Save/Load가 `FSessionRecord` 역직렬화 → `ClassifyOnStartup(&Loaded)` 재호출
  - Then: 두 번째 호출에서 추가 `OnFarewellReached` 미발행, `bFarewellReached == true` 유지
- **FAREWELL_LONG_GAP_SILENT**
  - Given: `Prev.DayIndex = 5`, `LastWallUtc = T₀`
  - When: WallDelta = 30일로 `ClassifyOnStartup`
  - Then: `OnFarewellReached(LongGapAutoFarewell)` 1회, `EFarewellReason` enum 두 값 확인
- **LONG_GAP_SILENT_IDEMPOTENT (INTEGRATION)**
  - Given: 이미 `FarewellExit` + `LastSaveReason == "EFarewellReached"` 세이브
  - When: 앱 재시작 → Save/Load가 `FSessionRecord` 로드 → `ClassifyOnStartup(&Loaded)`
  - Then: 두 번째 실행 `LONG_GAP_SILENT` 카운트 == 0, `OnFarewellReached` 카운트 == 0, 상태 `FarewellExit`
  - Edge cases: 3회 이상 재시작해도 같은 결과 (idempotent)

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- `tests/integration/time-session/farewell_idempotency_test.cpp` (Time + Save/Load round-trip, Save/Load story-003 완료 후)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 004 (LONG_GAP branch), Story 005 (Subsystem lifecycle), **foundation-save-load Story 003** (Save/Load 직렬화 — 크로스-에픽 의존)
- Unlocks: Story 007
