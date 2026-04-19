# Epic: Save/Load Persistence (with Integrity)

> **Layer**: Foundation
> **GDD**: [design/gdd/save-load-persistence.md](../../../design/gdd/save-load-persistence.md)
> **Architecture Module**: Foundation / Persistence — USaveGame + atomic dual-slot ping-pong + CRC32 + schema migration
> **Status**: Ready
> **Stories**: 8 stories created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Save/Load Persistence는 21일 실시간 세션 상태를 atomic dual-slot ping-pong + CRC32 integrity 검증으로 영속화한다. 4-trigger 라이프사이클 패턴(T1 CloseRequested, T2 ActivationStateChanged FlushOnly, T3 OnExit, T4 GameInstance Shutdown)으로 Windows 플랫폼 lifecycle을 커버. Per-trigger atomicity만 보장(`ESaveReason` 단일 commit)하며, Compound event sequence atomicity는 GSM boundary 책임(ADR-0009)으로 분리. `SaveSchemaVersion`(uint8) + migration chain으로 빌드 간 호환성 유지. 저장 인디케이터·모달 절대 금지(Pillar 1).

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0001](../../../docs/architecture/adr-0001-forward-time-tamper-policy.md) | E27/E28 외부 편집·forward tamper 비정책 — anti-tamper hash 추가 금지 | MEDIUM |
| [ADR-0009](../../../docs/architecture/adr-0009-compound-event-sequence-atomicity.md) | Compound atomicity는 GSM `Begin/EndCompoundEvent` boundary. Save/Load는 coalesce 정책만 제공 | LOW |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossSaveLoadSettings` — MinCompatibleSchemaVersion 등 knob | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-save-001 | OQ-8 compound event sequence atomicity (Day 13) | ADR-0009 (P1 — Day 13 시나리오 시 검증) ✅ |
| TR-save-002 | `NO_ANTITAMPER_LOGIC` CODE_REVIEW AC | ADR-0001 ✅ |
| TR-save-003 | `NO_SAVE_INDICATOR_UI` AC — Pillar 1 | GDD contract ✅ |
| TR-save-004 | 4-trigger lifecycle (T1 CloseRequested / T2 FlushOnly / T3 OnExit / T4 Shutdown) | GDD contract ✅ |
| TR-save-005 | SaveSchemaVersion (uint8) + migration chain | GDD contract ✅ |

## Key Interfaces

- **Publishes**: `FOnLoadComplete(bool bFreshStart, bool bHadPreviousData)`
- **Consumes**: `FCoreDelegates::OnSystemSettingsChanged`, worker thread for disk write
- **Owned types**: `UMossSaveData` (USaveGame subclass), `ESaveReason`, `FSessionRecord` (Time이 field 정의, Save/Load가 직렬화 소유), `FGrowthState`, `FDreamState`
- **Settings**: `UMossSaveLoadSettings`
- **API**: `SaveAsync(ESaveReason)`, `FlushOnly()` (T2), coalesce 정책 (진행 중 요청은 latest in-memory 승리)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | UMossSaveData USaveGame 서브클래스 + ESaveReason enum + Developer Settings | Logic | Ready | ADR-0011 |
| 002 | UMossSaveLoadSubsystem 뼈대 + 4-trigger lifecycle (T1/T2/T3/T4) + coalesce 정책 | Logic | Ready | ADR-0009 |
| 003 | Header block + CRC32 + Formula 1-3 + Dual-slot Load | Logic | Ready | ADR-0001 |
| 004 | FMossSaveSnapshot POD + Atomic write (Step 2-10) + Ping-pong rename + E4 temp crash | Logic | Ready | ADR-0001 |
| 005 | Migration chain (Formula 4) + Sanity check + Exception rollback + Engine archive fallback | Logic | Ready | ADR-0001 |
| 006 | NarrativeCount Atomic Commit + Compound Event Negative AC (ADR-0009 qualifier) | Logic | Ready | ADR-0009 |
| 007 | E14 Disk Full Degraded flag + E15 Non-ASCII path + bSaveDegraded accessor | Logic | Ready | ADR-0001 |
| 008 | NO_ANTITAMPER_LOGIC + NO_SAVE_INDICATOR_UI CODE_REVIEW CI grep hooks | Config/Data | Ready | ADR-0001 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증
- `NO_SAVE_INDICATOR_UI` + `NO_ANTITAMPER_LOGIC` CODE_REVIEW grep = 0건
- Atomic rename + dual-slot ping-pong 테스트 (partial write interrupt 시나리오)
- `SaveSchemaVersion` V1 → V2 mock migration 왕복 테스트
- `NARRATIVE_COUNT_ATOMIC_COMMIT` — Time `IncrementNarrativeCountAndSave` private 강제
- Day 13 compound event atomicity 테스트 (ADR-0009 `Begin/EndCompoundEvent` boundary — 3 SaveAsync coalesce → 단일 commit)
- CRC32 실패 시 dual-slot fallback 테스트

## Next Step

Run `/create-stories foundation-save-load` to break this epic into implementable stories.
