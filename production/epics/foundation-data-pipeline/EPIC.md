# Epic: Data Pipeline

> **Layer**: Foundation
> **GDD**: [design/gdd/data-pipeline.md](../../../design/gdd/data-pipeline.md)
> **Architecture Module**: Foundation / Data — UDataPipelineSubsystem + 4 카탈로그 + C1/C2 schema guards
> **Status**: Ready
> **Stories**: 7 stories created (2026-04-19)
> **Manifest Version**: 2026-04-19

## Overview

Data Pipeline은 디자이너 편집 콘텐츠(카드, 꿈, 최종 형태, Stillness Budget)를 코드와 분리된 데이터 자산으로 관리하고, 런타임에서 동기 조회 API(`GetCardRow`, `GetAllDreamAssets`, `GetGrowthFormRow`, `GetStillnessBudgetAsset`)로 모든 downstream 시스템에 제공한다. 4-state 상태 머신(Uninitialized → Loading → Ready / DegradedFallback)으로 로드 실패를 fail-close 처리. C1 schema gate(카드 수치 필드 금지)·C2 schema gate(꿈 임계 외부 노출 의무)로 Pillar 2·3을 타입·자산 수준에서 강제한다.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| [ADR-0002](../../../docs/architecture/adr-0002-data-pipeline-container-selection.md) | Card=UDataTable / FinalForm=UMossFinalFormAsset (ADR-0010) / Dream=UDreamDataAsset / Stillness=UStillnessBudgetAsset. UDataRegistrySubsystem 미사용 | MEDIUM (cooked PrimaryAssetType) |
| [ADR-0003](../../../docs/architecture/adr-0003-data-pipeline-loading-strategy.md) | Sync 일괄 로드 — 4단계 순차. HDD cold-start Known Compromise | MEDIUM (GameInstance::Init timing) |
| [ADR-0010](../../../docs/architecture/adr-0010-ffinalformrow-storage-format.md) | FFinalFormRow → UMossFinalFormAsset per-form (TMap 편집 위해) | LOW |
| [ADR-0011](../../../docs/architecture/adr-0011-tuning-knob-udeveloper-settings.md) | `UMossDataPipelineSettings` — MaxInitTimeBudgetMs 등 knob | LOW |

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-dp-001 | OQ-ADR-1 container selection (Card/FinalForm/Dream/Stillness) | ADR-0002 ✅ |
| TR-dp-002 | OQ-ADR-2 loading strategy (Sync vs Async) | ADR-0003 ✅ |
| TR-dp-003 | CR-8 UMossFinalFormAsset TMap 편집 | ADR-0010 ✅ |
| TR-dp-004 | AC-DP-18 [5.6-VERIFY] Cooked PrimaryAssetTypesToScan | ADR-0002 + ADR-0010 ✅ |

## Key Interfaces

- **Publishes (pull API)**: `GetCardRow(FName)`, `GetAllCardIds()`, `GetGrowthFormRow(FName)`, `GetAllGrowthFormIds()`, `GetDreamAsset(FName)`, `GetAllDreamAssets()`, `GetDreamBodyText(FName)`, `GetStillnessBudgetAsset()`
- **Consumes**: `UAssetManager::LoadPrimaryAssetsWithType`, `UDataTable::LoadSynchronous`
- **Owned types**: `UDataPipelineSubsystem`, `EDataPipelineState`, `FGiftCardRow`, `UMossFinalFormAsset`, `UDreamDataAsset` (schema guard owner), `UStillnessBudgetAsset`
- **Settings**: `UMossDataPipelineSettings` (MaxInitTimeBudgetMs, CatalogOverflowWarn/Error/FatalMultiplier)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | Schema Types (FGiftCardRow, UMossFinalFormAsset, UDreamDataAsset, UStillnessBudgetAsset) + Developer Settings | Logic | Ready | ADR-0002 |
| 002 | DefaultEngine.ini PrimaryAssetTypesToScan 등록 + AC-DP-18 Cooked build 검증 | Integration | Ready | ADR-0002 |
| 003 | UDataPipelineSubsystem 뼈대 + 4-state machine + pull API contract | Logic | Ready | ADR-0003 |
| 004 | Catalog Registration Loading (Card → FinalForm → Dream → Stillness) + DegradedFallback | Logic | Ready | ADR-0003 |
| 005 | Edge case policies (DuplicateCardId, FTextEmpty, RefreshCatalog 재진입, GetFailedCatalogs) | Logic | Ready | ADR-0011 |
| 006 | T_init budget 실측 + 3단계 overflow 임계 로그 분기 + D1 memory smoke | Logic | Ready | ADR-0003 |
| 007 | 단조성 Shipping-safe guard + C1/C2 schema gate CI grep hooks | Logic | Ready | ADR-0002 |

## Definition of Done

이 Epic은 다음 조건이 모두 충족되면 완료:
- 모든 stories가 구현·리뷰·`/story-done`으로 종료됨
- 모든 GDD Acceptance Criteria 검증 (AC-DP-01~18)
- AC-DP-09/10 [5.6-VERIFY] T_init 실측 + 3단계 임계 분기 로그
- AC-DP-18 [5.6-VERIFY] Cooked build `GetPrimaryAssetIdList` 비어있지 않음 검증
- C1 schema gate grep `int32|float|double` in `FGiftCardRow` = 0건
- C2 schema gate grep Dream 코드 리터럴 임계 = 0건
- DefaultEngine.ini `PrimaryAssetTypesToScan` 3종 등록 (DreamData, FinalForm, StillnessBudget)
- R3 fail-close contract 테스트 통과 (TOptional empty / TArray empty 반환)

## Next Step

Run `/create-stories foundation-data-pipeline` to break this epic into implementable stories.
