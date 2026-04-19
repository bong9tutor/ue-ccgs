# Story 005: GSM Prepare/Ready 프로토콜 + Degraded Fallback

> **Epic**: feature-card-system
> **Layer**: Feature
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2시간

## Context

- **GDD Reference**: `design/gdd/card-system.md` §CR-CS-3 GSM Prepare/Ready + §CR-CS-7 카탈로그 크기별 + EC-CS-1/2/4/5 + AC-CS-04/05/11/12/18
- **TR-ID**: TR-card-003 (Eager + Prepare 응답)
- **Governing ADR**: [ADR-0006](../../../docs/architecture/adr-0006-card-pool-regeneration-timing.md) — Eager로 즉시 Ready 응답 보장
- **Engine Risk**: LOW
- **Engine Notes**: 5초 `CardSystemReadyTimeoutSec` 내 응답 필수. GSM E9 timeout 경로는 GSM 측 구현.
- **Control Manifest Rules**:
  - Foundation Layer §Required Patterns — "`FTSTicker` 또는 `FTimerManager` 중 선택" — GameInstanceSubsystem 초기화 시 `GetWorld()` null 가능하므로 `FTSTicker` 권장 (card-system.md Implementation Notes)

## Acceptance Criteria

1. **AC-CS-04** (INTEGRATION/BLOCKING) — `FOnDayAdvanced(3)` 후 Ready 상태, GSM이 `OnPrepareRequested()` 호출 → `OnCardSystemReady(bDegraded = false)` 즉시 응답, 경과 시간 < 5.0초
2. **AC-CS-05** (INTEGRATION/BLOCKING) — Uninitialized (`FOnDayAdvanced` 미수신), GSM `OnPrepareRequested()` 호출 → On-demand 샘플링 시작 → 완료 시 `OnCardSystemReady()` 응답
3. **AC-CS-11** (AUTOMATED/BLOCKING) — Pipeline DegradedFallback, 카탈로그 0장, `FOnDayAdvanced(1)` + `OnPrepareRequested()` → `DailyHand.Num()=0`, `UE_LOG(Error)` 1회, `OnCardSystemReady(bDegraded=true)`, `ConfirmOffer()` → false (EC-CS-1)
4. **AC-CS-12** (AUTOMATED/BLOCKING) — 카탈로그 2장만 반환, `FOnDayAdvanced(1)` → `DailyHand.Num()=2`, `OnCardSystemReady(bDegraded=true)`, DailyHand 내 CardId로 `ConfirmOffer` → true (EC-CS-2)
5. **AC-CS-18** (INTEGRATION/BLOCKING) — Pipeline 응답 5초 초과 지연 시뮬, GSM `OnPrepareRequested()` 호출 후 `CardSystemReadyTimeoutSec`(5.0초) 경과 → GSM이 `bDegraded=true`로 Offering 강제 진입 (GSM E9 계약). Card System `OnCardSystemReady` 미도달. Card Hand UI 빈/부분 슬롯 표시
6. `OnPrepareRequested()` 3-way 분기 구현:
   - Ready → 즉시 `OnCardSystemReady(bDegraded = DailyHand.Num() < HandSize)`
   - Preparing → 샘플링 완료 후 자동 응답
   - Uninitialized → On-demand 샘플링 시작 (EC-CS-4 — `FSessionRecord.DayIndex` 직접 read)

## Implementation Notes

- **`OnPrepareRequested` 구현** (ADR-0006 + GDD §CR-CS-3):
  ```cpp
  void UCardSystemSubsystem::OnPrepareRequested() {
      switch (CurrentState) {
      case Ready:
          GSM->OnCardSystemReady(DailyHand.Num() < HandSize);  // bDegraded
          break;
      case Preparing:
          // 이미 진행 중 — 완료 후 자동 응답 (sampling sync 보장)
          break;
      case Uninitialized:
          // EC-CS-4: FSessionRecord.DayIndex 직접 read 후 on-demand
          const int32 DayIdx = SessionRecord->DayIndex;
          OnDayAdvancedHandler(DayIdx);
          GSM->OnCardSystemReady(DailyHand.Num() < HandSize);
          break;
      default:
          break;
      }
  }
  ```
- **카탈로그 크기별 동작 (CR-CS-7 + EC-CS-1/2)**:
  - 0장 → `DailyHand={}`, `UE_LOG(Error)`, `bDegraded=true`
  - 1-2장 → 전부 포함, `bDegraded=true`
  - ≥3장 → 가중 샘플링 3장
- **5초 timeout (GSM E9 + AC-CS-18)**: GSM 측 `FTSTicker` 타이머 — Card System은 응답만 책임, timeout은 GSM 주도
- **Eager의 이점 (ADR-0006)**: 정상 경로에서 `OnPrepareRequested` 호출 시 이미 Ready — 즉시 응답, 5초 timeout 여유 충분

## Out of Scope

- GSM Prepare/Ready 타이머 구현 (core-game-state-machine)
- Card Hand UI 빈 슬롯 표시 (presentation-card-hand-ui)

## QA Test Cases

**Given** `FOnDayAdvanced(3)` 후 Ready, **When** GSM `OnPrepareRequested()`, **Then** `OnCardSystemReady(false)` 즉시 응답, < 1ms 경과 (AC-CS-04 + Eager).

**Given** Uninitialized + `FSessionRecord.DayIndex=3`, **When** GSM `OnPrepareRequested()` (FOnDayAdvanced 미수신 상태), **Then** on-demand 샘플링 → `OnCardSystemReady()` 응답 (AC-CS-05, EC-CS-4).

**Given** Pipeline DegradedFallback (카탈로그 0장), **When** `FOnDayAdvanced(1)` + `OnPrepareRequested()`, **Then** DailyHand 빈, `UE_LOG(Error)` 1회, `bDegraded=true` (AC-CS-11).

**Given** 카탈로그 2장, **When** `FOnDayAdvanced(1)`, **Then** `DailyHand.Num()==2`, `bDegraded=true`, DailyHand 내 CardId로 ConfirmOffer 성공 (AC-CS-12).

**Given** Pipeline 5초 초과 지연, **When** GSM 타이머 발동, **Then** GSM 측 `bDegraded=true` + Offering 강제 진입 (AC-CS-18, GSM E9).

## Test Evidence

- **Integration test**: `tests/integration/Card/test_gsm_prepare_ready.cpp` (AC-CS-04/05/18)
- **Edge test**: `tests/unit/Card/test_catalog_size_edge_cases.cpp` (AC-CS-11/12)

## Dependencies

- Story 001 (Subsystem + ECardSystemState)
- Story 002 (Eager 샘플링)
- `core-game-state-machine` (OnPrepareRequested / OnCardSystemReady API)
