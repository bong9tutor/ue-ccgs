# Epic: Moss Baby Character System

> **Layer**: Core
> **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md)
> **Architecture Module**: Core / MBC — 이끼 아이 Actor + Material driver + 5-state character animation
> **Status**: Ready
> **Stories**: 8 stories — see Stories table
> **Manifest Version**: 2026-04-19

## Overview

Moss Baby Character System은 유리병 속 이끼 영혼의 시각 존재와 반응을 담당한다. MVP는 정적 메시 + MaterialInstanceDynamic으로 구현 (블렌드 셰이프·스켈레탈 리깅은 VS/Full 범위 밖). 5-state 상태 머신(Idle·Reacting·GrowthTransition·Drying·FinalReveal)이 우선순위(FinalReveal > GrowthTransition > Reacting > Drying/Idle)로 동작. 카드 건넴 즉시 Stage 1 `FOnCardOffered` 구독으로 Reacting 진입(시각 즉시성, Pillar 2). `DryingFactor`(고요한 대기)는 DayGap ≥ 2 시 상승 — Pillar 1 준수 vocabulary ("Quiet Rest", drying/yellowing 금지). `UMossBabyPresetAsset`의 성장 단계별 머티리얼 프리셋(CR-2 테이블)을 Lerp.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0004](../../../docs/architecture/adr-0004-mpc-light-actor-sync.md) | MBC 머티리얼은 GSM MPC `AmbientMoodBlend` 등 자동 참조 (read-only) | HIGH (Lumen 영향 상속) |
| [ADR-0005](../../../docs/architecture/adr-0005-foncardoffered-processing-order.md) | MBC는 Stage 1 `FOnCardOffered` 구독 유지 — 시각 즉시성 (Growth 대기 없이 Reacting 진입) | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-mbc-001 | AC-MBC-03 Material param 리터럴 금지 — `UMossBabyPresetAsset` 전용 | GDD contract ✅ |
| TR-mbc-002 | Priority: FinalReveal > GrowthTransition > Reacting > Drying/Idle | GDD contract ✅ |
| TR-mbc-003 | CR-5 "Quiet Rest" vocabulary — drying/yellowing 시각 큐 금지 (Pillar 1) | GDD contract + control-manifest Cross-Cutting ✅ |
| TR-mbc-004 | Formula 4 Reacting 지수 감쇠 (≈1.65s = 5τ_sss) | GDD contract ✅ |
| TR-mbc-005 | Formula 5 DryingOverlay (성장 단계 프리셋 ↔ QuietRest 선형 보간) | GDD contract ✅ |

## Key Interfaces

- **Publishes**: 없음 (MBC는 subscriber 중심)
- **Consumes**: `FOnCardOffered` (Stage 1), `FOnGrowthStageChanged`, `FOnFinalFormReached`, `FOnGameStateChanged`, Growth `GetMaterialHints()` pull API (Reacting 후 별도 이벤트)
- **Owned types**: `EMossCharacterState`, `UMossBabyPresetAsset`, `FGrowthMaterialHints` (Growth가 source, MBC consume)
- **Settings**: 없음 (콘텐츠 자산은 `UMossBabyPresetAsset`, 시스템 전역 knob 필요 시 `UMossBabyCharacterSettings` 추가)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [`AMossBaby` Actor + MID 세팅 + `UMossBabyPresetAsset` 스키마](story-001-mossbaby-actor-mid-preset-asset.md) | Logic | Ready | ADR-0011 |
| 002 | [5-State 상태 머신 + 우선순위](story-002-five-state-machine-priority.md) | Logic | Ready | — (GDD contract) |
| 003 | [Reacting 상태 + Stage 1 `FOnCardOffered` 구독 + Formula 4 지수 감쇠](story-003-reacting-stage1-decay-formula4.md) | Logic | Ready | ADR-0005 |
| 004 | [GrowthTransition Mesh 교체 + Formula 1 SmoothStep Lerp + E1 Multi-skip](story-004-growthtransition-mesh-swap-lerp-formula1.md) | Integration | Ready | — (GDD contract) |
| 005 | [QuietRest (고요한 대기) + `DryingFactor` Formula 2/5 + Vocabulary Rule](story-005-quietrest-dryingfactor-formulas-2-5.md) | Logic | Ready | ADR-0012 |
| 006 | [브리딩 사인파 Formula 3 + Idle 분기 내부 전용](story-006-breathing-sine-wave-formula3-idle-only.md) | Logic | Ready | — (GDD contract) |
| 007 | [FinalReveal + Mesh Load Failure E8 Fallback](story-007-finalreveal-e8-fallback-mesh-load.md) | Integration | Ready | ADR-0005 |
| 008 | [E4 QuietRest + FOnCardOffered + 동시 FOnGrowthStageChanged 순서](story-008-e4-quietrest-card-growth-compound.md) | Integration | Ready | ADR-0005 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-MBC-01~16)
- AC-MBC-03 grep — Material parameter 리터럴 직접 설정 0건 (Preset asset 경유만)
- AC-MBC-04 Reacting Formula 4 지수 감쇠 결정성 테스트
- AC-MBC-15 E4 QuietRest + FOnCardOffered + 동시 FOnGrowthStageChanged 순서 테스트
- AC-MBC-16 DryingOverlay Formula 5 Mature+QuietRest 경계 테스트
- 호흡 사인파 grep — Idle 상태 분기 내부에서만 호출 (CR-4)

## Next Step

Run `/create-stories core-moss-baby-character` to break this epic into implementable stories.
