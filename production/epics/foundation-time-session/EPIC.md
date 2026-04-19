# Epic: Time & Session System

> **Layer**: Foundation
> **GDD**: [design/gdd/time-session-system.md](../../../design/gdd/time-session-system.md)
> **Architecture Module**: Foundation / Time — 8-Rule Classifier + IMossClockSource injection seam
> **Status**: Ready
> **Stories**: 7 stories created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Time & Session System은 앱 시작·세션 내 1Hz tick·앱 재시작 간의 시간 경과를 8-Rule Classifier로 분류하여 `ETimeAction` 신호(START_DAY_ONE, ADVANCE_SILENT, ADVANCE_WITH_NARRATIVE, HOLD_LAST_TIME, LONG_GAP_SILENT, LOG_ONLY)를 발행한다. 모든 시간 입력은 `IMossClockSource` 추상 인터페이스를 통해서만 진입하여 production(FRealClockSource)과 test(FMockClockSource) 교체를 가능케 한다. Tamagotchi 모델의 실시간 21일을 안정적으로 구현하는 기반 시스템이며, Pillar 1·4의 인프라 약속을 수행한다.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0001](../../../docs/architecture/adr-0001-forward-time-tamper-policy.md) | Forward time tamper를 정상 경과와 bit-exact 동일 처리 — 탐지·차단·로깅 금지 | MEDIUM (`FPlatformTime::Seconds()` Windows suspend — OQ-IMP-1 실기) |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossTimeSessionSettings : UDeveloperSettings` — Tuning Knob 일관 노출 | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-time-001 | `NO_FORWARD_TAMPER_DETECTION_ADR` AC — Forward tamper 수용 정책 기록 | ADR-0001 ✅ |
| TR-time-002 | OQ-5 `UDeveloperSettings` knob exposure (NarrativeCap, DefaultEpsilonSec, WitheredThresholdDays 등) | ADR-0011 ✅ |
| TR-time-003 | `IMossClockSource` injection seam — 4 virtual methods (GetUtcNow, GetMonotonicSec, OnPlatformSuspend, OnPlatformResume) | GDD contract (R3 RESOLVED) ✅ |
| TR-time-004 | Windows suspend/resume 실기 검증 | OQ-IMP-1 Implementation task ✅ |

## Key Interfaces

- **Publishes**: `FOnTimeActionResolved(ETimeAction)`, `FOnDayAdvanced(int32 NewDayIndex)`, `FOnFarewellReached(EFarewellReason)`
- **Consumes**: `IMossClockSource` implementations
- **Owned types**: `FSessionRecord`, `ETimeAction`, `EBetweenSessionClass`, `EInSessionClass`, `EFarewellReason`, `NarrativeCapPerPlaythrough`

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | IMossClockSource 인터페이스 + FRealClockSource + FMockClockSource | Logic | Ready | ADR-0001 |
| 002 | FSessionRecord 구조체 + double precision round-trip | Logic | Ready | ADR-0001 |
| 003 | UMossTimeSessionSubsystem 뼈대 + enums + DeveloperSettings | Logic | Ready | ADR-0011 |
| 004 | Between-session Classifier (Rules 1-4) + Formulas 1, 4, 5 | Logic | Ready | ADR-0001 |
| 005 | In-session 1Hz Tick + Rules 5-8 + Formulas 2, 3 + FTSTicker | Logic | Ready | ADR-0001 |
| 006 | Farewell 상태 전환 + LONG_GAP_SILENT_IDEMPOTENT (Save/Load integration) | Integration | Ready | ADR-0001 |
| 007 | Forward Tamper CI grep hook + Windows Suspend/Resume 실기 검증 | Config/Data | Ready | ADR-0001 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- `design/gdd/time-session-system.md`의 모든 Acceptance Criteria 검증됨 (AUTOMATED + INTEGRATION + CODE_REVIEW + MANUAL)
- `AC NO_FORWARD_TAMPER_DETECTION` CI grep hook 구현 + 0 매치 강제
- OQ-IMP-1 Windows 10/11 `FPlatformTime::Seconds()` suspend 실기 결과 문서화
- 모든 Logic stories가 `tests/unit/time-session/` 하위 passing test 보유
- Integration stories가 `tests/integration/time-session/` 또는 evidence 문서 보유

## Next Step

Run `/create-stories foundation-time-session` to break this epic into implementable stories.
