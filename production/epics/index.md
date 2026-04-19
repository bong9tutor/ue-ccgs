# Epics Index

> **Last Updated**: 2026-04-19
> **Engine**: Unreal Engine 5.6 (hotfix 5.6.1)
> **Manifest Version**: 2026-04-19 ([control-manifest.md](../../docs/architecture/control-manifest.md))
> **Total Epics**: 14 (MVP) — Foundation 4 + Core 3 + Feature 4 + Presentation 2 + Meta 1

14 MVP 시스템을 layer별로 Epic 파일화. 각 Epic은 해당 시스템의 scope + governing ADRs + GDD requirements + Definition of Done을 정의. Story 분해는 `/create-stories [epic-slug]`로 진행.

**Review mode**: lean — PR-EPIC director gate skipped.

---

## Foundation Layer (병렬 구현 가능 — 의존 0건)

| Epic | System | GDD | Governing ADRs | Engine Risk | Stories |
|---|---|---|---|---|---|
| [foundation-time-session](foundation-time-session/EPIC.md) | Time & Session System (#1) | [time-session-system.md](../../design/gdd/time-session-system.md) | ADR-0001, 0011 | MEDIUM | Not yet created |
| [foundation-data-pipeline](foundation-data-pipeline/EPIC.md) | Data Pipeline (#2) | [data-pipeline.md](../../design/gdd/data-pipeline.md) | ADR-0002, 0003, 0010, 0011 | MEDIUM | Not yet created |
| [foundation-save-load](foundation-save-load/EPIC.md) | Save/Load Persistence (#3) | [save-load-persistence.md](../../design/gdd/save-load-persistence.md) | ADR-0001, 0009, 0011 | MEDIUM | Not yet created |
| [foundation-input-abstraction](foundation-input-abstraction/EPIC.md) | Input Abstraction Layer (#4) | [input-abstraction-layer.md](../../design/gdd/input-abstraction-layer.md) | ADR-0011 | LOW | Not yet created |

## Core Layer

| Epic | System | GDD | Governing ADRs | Engine Risk | Stories |
|---|---|---|---|---|---|
| [core-game-state-machine](core-game-state-machine/EPIC.md) | Game State Machine (+ Visual Director) (#5) | [game-state-machine.md](../../design/gdd/game-state-machine.md) | ADR-0004, 0005, 0009, 0011 | HIGH (Lumen 영향 상속) |  Not yet created |
| [core-moss-baby-character](core-moss-baby-character/EPIC.md) | Moss Baby Character System (#6) | [moss-baby-character-system.md](../../design/gdd/moss-baby-character-system.md) | ADR-0004, 0005 | LOW | Not yet created |
| [core-lumen-dusk-scene](core-lumen-dusk-scene/EPIC.md) | Lumen Dusk Scene (+ PSO Precaching) (#11) | [lumen-dusk-scene.md](../../design/gdd/lumen-dusk-scene.md) | ADR-0004, 0013, 0011 | **HIGH** | Not yet created |

## Feature Layer

| Epic | System | GDD | Governing ADRs | Engine Risk | Stories |
|---|---|---|---|---|---|
| [feature-growth-accumulation](feature-growth-accumulation/EPIC.md) | Growth Accumulation Engine (#7) | [growth-accumulation-engine.md](../../design/gdd/growth-accumulation-engine.md) | ADR-0005, 0010, 0012, 0011 | LOW | Not yet created |
| [feature-card-system](feature-card-system/EPIC.md) | Card System (#8) | [card-system.md](../../design/gdd/card-system.md) | ADR-0005, 0006, 0012, 0011 | LOW | Not yet created |
| [feature-dream-trigger](feature-dream-trigger/EPIC.md) | Dream Trigger System (#9) | [dream-trigger-system.md](../../design/gdd/dream-trigger-system.md) | ADR-0005, 0011 | MEDIUM (AssetManager PrimaryAssetType) | Not yet created |
| [feature-stillness-budget](feature-stillness-budget/EPIC.md) | Stillness Budget (#10) | [stillness-budget.md](../../design/gdd/stillness-budget.md) | ADR-0007, 0011 | LOW (VS 전까지 game-layer only) | Not yet created |

## Presentation Layer

| Epic | System | GDD | Governing ADRs | Engine Risk | Stories |
|---|---|---|---|---|---|
| [presentation-card-hand-ui](presentation-card-hand-ui/EPIC.md) | Card Hand UI (#12) | [card-hand-ui.md](../../design/gdd/card-hand-ui.md) | ADR-0008 | LOW | Not yet created |
| [presentation-dream-journal-ui](presentation-dream-journal-ui/EPIC.md) | Dream Journal UI (#13) | [dream-journal-ui.md](../../design/gdd/dream-journal-ui.md) | (standard UMG patterns) | LOW | Not yet created |

## Meta Layer

| Epic | System | GDD | Governing ADRs | Engine Risk | Stories |
|---|---|---|---|---|---|
| [meta-window-display](meta-window-display/EPIC.md) | Window & Display Management (#14) | [window-display-management.md](../../design/gdd/window-display-management.md) | ADR-0011 | MEDIUM (Steam Deck Proton OQ-IMP-5) | Not yet created |

---

## Recommended Implementation Order

1. **Foundation sprint** (병렬 가능):
   - foundation-time-session, foundation-data-pipeline, foundation-save-load, foundation-input-abstraction
   - 의존 0건 — 4 epic 병렬 구현 가능
2. **Core sprint** (Foundation 완료 후):
   - core-game-state-machine (Foundation 전부 의존) → core-moss-baby-character → core-lumen-dusk-scene (MVP 가장 HIGH risk — 첫 milestone GPU 프로파일링 게이트)
3. **Feature sprint** (Core 완료 후):
   - feature-growth-accumulation → feature-card-system → feature-dream-trigger (Stage 2 의존) → feature-stillness-budget (VS까지 stub)
4. **Presentation sprint** (Feature 완료 후):
   - presentation-card-hand-ui → presentation-dream-journal-ui
5. **Meta** (Foundation 완료 후 언제든):
   - meta-window-display (Input Abstraction만 의존)

## Next Steps

각 Epic별로 Story 분해 실행:

```
/create-stories foundation-time-session
/create-stories foundation-data-pipeline
/create-stories foundation-save-load
/create-stories foundation-input-abstraction
... (14개 순차 또는 layer 단위)
```

Story 생성 후 `/sprint-plan new`로 첫 sprint backlog 작성.

---

## Untraced Requirements Summary

모든 14 epic의 주요 TR이 Accepted ADR 또는 GDD contract로 커버됨. 30 TR spot-check 100% Covered — [architecture-traceability.md](../../docs/architecture/architecture-traceability.md) 참조.

**Implementation 단계 검증 대상 (5.6-VERIFY + OQ-IMP)**:
- AC-DP-09/10 (Pipeline T_init + 3-단계 임계) — ADR-0003
- AC-DP-18 (Cooked PrimaryAssetTypesToScan) — ADR-0002 + ADR-0010
- AC-LDS-04 (Lumen GPU 5ms budget) — ADR-0004
- AC-LDS-11 (PSO precaching 첫 프레임 히칭) — ADR-0013
- OQ-IMP-1 (`FPlatformTime::Seconds()` Windows suspend Win10/11)
- OQ-IMP-2 (`GameInstance::Init` 순서 보장 예외 플랫폼)
- OQ-IMP-3 (Cook FSoftObjectPath 누락 → Error 격상)
- OQ-IMP-4 (CI Validator 자동화 훅)
- OQ-IMP-5 (Steam Deck Proton Always-on-top)
