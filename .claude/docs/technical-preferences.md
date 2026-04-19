# Technical Preferences

<!-- Populated by /setup-engine. Updated as the user makes decisions throughout development. -->
<!-- All agents reference this file for project-specific standards and conventions. -->

## Engine & Language

- **Engine**: Unreal Engine 5.6
- **Language**: C++ (primary), Blueprint (gameplay prototyping)
- **Rendering**: Deferred (Lumen enabled — for Terrarium Dusk 조명 연출)
- **Physics**: Chaos Physics (UE 기본)

## Input & Platform

<!-- Written by /setup-engine. Read by /ux-design, /ux-review, /test-setup, /team-ui, and /dev-story -->
<!-- to scope interaction specs, test helpers, and implementation to the correct input methods. -->

- **Target Platforms**: PC (Windows — Steam / itch.io)
- **Input Methods**: Keyboard/Mouse
- **Primary Input**: Mouse (카드 선택·건넴 주력 상호작용)
- **Gamepad Support**: Partial (Steam Deck 호환 고려; 필수 아님)
- **Touch Support**: None
- **Platform Notes**: 데스크톱 cozy 오브제. 작은 창 모드(800×600 이하) 지원 + 항상 위 고정(Always-on-top) 옵션 고려. Hover-only 상호작용 금지 (Steam Deck 컨트롤러 모드 대응). 알림 팝업 없음 (Pillar 1: Quiet Presence).

## Naming Conventions

- **Classes**: Prefixed PascalCase (`A` Actor, `U` UObject, `F` struct, `E` enum, `I` interface, `T` template) — 예: `AMossBaby`, `UGiftCardComponent`, `FDreamEntry`
- **Variables**: PascalCase (예: `GrowthStage`, `CurrentDay`)
- **Functions**: PascalCase (예: `OfferCard()`, `TriggerDream()`)
- **Booleans**: `b` prefix (예: `bHasDreamToday`, `bIsSleeping`)
- **Files**: 클래스에서 prefix 뺀 이름 (예: `MossBaby.h`, `GiftCardComponent.h`)
- **Folders**: PascalCase 카테고리 (예: `Source/MadeByClaudeCode/Gameplay/`, `Source/MadeByClaudeCode/UI/`)

## Performance Budgets

- **Target Framerate**: 60 fps (locked)
- **Frame Budget**: 16.6 ms/frame
- **Draw Calls**: < 500 (데스크톱 미니어처 씬. Nanite 미사용 권장 — 오버헤드)
- **Memory Ceiling**: 1.5 GB (데스크톱 상주형 앱 고려. Lumen 사용 시 상향 검토)

## Testing

- **Framework**: Unreal Automation Framework (UE 내장)
- **Minimum Coverage**: 핵심 시스템(카드 선택, 성장 누적 로직, 세이브/로드, 꿈 트리거) — 각 시스템별 Functional Test 최소 1개
- **Required Tests**: Balance formulas (카드 태그 벡터 → 최종 형태 매핑), 세이브 파일 왕복, 21일 실시간 경과 처리
- **Headless CI Flag**: `-nullrhi` (렌더링 없이 로직 테스트)

## Forbidden Patterns

<!-- Add patterns that should never appear in this project's codebase -->
- [None configured yet — add as architectural decisions are made]

## Allowed Libraries / Addons

<!-- Add approved third-party dependencies here -->
- [None configured yet — add as dependencies are approved]

## Architecture Decisions Log

<!-- Quick reference linking to full ADRs in docs/architecture/ -->
- [No ADRs yet — use /architecture-decision to create one]

## Engine Specialists

<!-- Written by /setup-engine when engine is configured. -->
<!-- Read by /code-review, /architecture-decision, /architecture-review, and team skills -->
<!-- to know which specialist to spawn for engine-specific validation. -->

- **Primary**: unreal-specialist
- **Language/Code Specialist**: ue-blueprint-specialist (Blueprint graphs) 또는 unreal-specialist (C++)
- **Shader Specialist**: unreal-specialist (전용 shader specialist 없음 — primary가 머티리얼 담당)
- **UI Specialist**: ue-umg-specialist (UMG widgets, CommonUI, input routing, widget styling)
- **Additional Specialists**: ue-gas-specialist (Gameplay Ability System — 이번 프로젝트는 미사용 가능성 높음), ue-replication-specialist (네트워킹 — 이번 프로젝트는 해당 없음, 순수 오프라인)
- **Routing Notes**: 일반 C++ 아키텍처·엔진 결정은 primary. Blueprint 그래프 설계·BP/C++ 경계 설계는 Blueprint specialist. UMG 위젯 모든 구현은 UMG specialist. GAS·Replication 전문가는 이 프로젝트에서 필요할 가능성 낮지만, 미래 기능 추가 시 참조.

### File Extension Routing

<!-- Skills use this table to select the right specialist per file type. -->
<!-- If a row says [TO BE CONFIGURED], fall back to Primary for that file type. -->

| File Extension / Type | Specialist to Spawn |
|-----------------------|---------------------|
| Game code (.cpp, .h files) | unreal-specialist |
| Shader / material files (.usf, .ush, Material assets) | unreal-specialist |
| UI / screen files (.umg, UMG Widget Blueprints) | ue-umg-specialist |
| Scene / prefab / level files (.umap, .uasset) | unreal-specialist |
| Native extension / plugin files (Plugin .uplugin, modules) | unreal-specialist |
| Blueprint graphs (.uasset BP classes) | ue-blueprint-specialist |
| General architecture review | unreal-specialist |
