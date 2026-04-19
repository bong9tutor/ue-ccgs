# Systems Index: Moss Baby (이끼 아이)

> **Status**: Draft — Revision 2 (post-Director review)
> **Created**: 2026-04-16
> **Last Updated**: 2026-04-16 (R2)
> **Source Concept**: `design/gdd/game-concept.md`
> **Art Bible**: `design/art/art-bible.md`

---

## Director Review Outcome (R2 — 2026-04-16)

이 개정판은 creative-director (CD-SYSTEMS)와 technical-director (TD-SYSTEM-BOUNDARY) 선행 리뷰 결과를 반영한다. 두 디렉터 모두 **CONCERNS** 판정 — 10+ GDD 작성 들어가기 전 구조적 수정 13개 + 선택 3개 **모두 적용**됨.

### 적용된 주요 변경
- **+3 시스템 추가**: Stillness Budget (Meta/MVP), Input Abstraction Layer (Core/MVP), Window & Display Management (Meta/MVP)
- **−1 시스템 병합**: Visual State Director → Game State Machine에 흡수 (Component 서비스로 격하)
- **재분류**: Lumen Dusk Scene (Foundation → Presentation), Audio System (Foundation → Feature + VS로 연기)
- **경계 재정의**: Card System은 `OnCardOffered` 이벤트만 발행. Drag-to-offer 입력은 Card Hand UI 소유
- **강등**: Title & Settings UI → Vertical Slice, Audio System → Vertical Slice, Accessibility Layer 대부분 → Vertical Slice (단 Reduced Motion은 Stillness Budget의 공개 API로 MVP 유지)
- **Save/Load 강화**: `SaveSchemaVersion` + 마이그레이션 + Save Integrity(atomic write / 이중 슬롯 / 체크섬) 의무화, Save/Load GDD의 Edge Cases로 흡수
- **PSO Precaching 소유자**: Lumen Dusk Scene에 귀속
- **Prototype-first 전환**: Time & Session System, Dream Trigger System (설계 순서와 분리해 조기 프로토타입)
- **Pillar 3 (Speak Only When Moved) 수호 계약**: Dream Trigger GDD에 희소성 보호 통합 조항 명시 — 다른 반응 시스템(Game State/Audio/Card Hand UI)이 공모해야 달성
- **Day 카운터 영구 미표시**: Open Question #3 닫힘 (Pillar 1 위반 증거 발견 시에만 재검토)
- **MVP 종료 스크린 ↔ Farewell Archive 연결 명시**: MVP 7일차 종료는 Farewell Archive의 감정적 코어 1% (최소 스틸 + 한 줄 메시지)
- **일정 정직성**: 약 38+ 설계 세션 / MVP 구현 8–14주 (game-concept.md의 4–8주는 하한)

---

## Overview

Moss Baby는 책상 위 유리병 속 작은 이끼 영혼에게 21일간 매일 한 장의 선물 카드를 건네는 데스크톱 cozy companion 게임이다. 메커니즘 스코프는 작지만 **감정적 밀도는 높다** — 대부분의 시스템은 *복잡한 행동*이 아니라 *절제된 반응*을 만들기 위해 존재한다.

핵심 루프 3층(30초·세션·진행)을 뒷받침하는 시스템은 다섯 묶음으로 나뉜다:

1. **실시간 21일을 지속시키는 기반 시스템** (Time, Data Pipeline, Save/Load with Integrity, Input Abstraction) — Tamagotchi 모델의 뼈대
2. **상태와 캐릭터 표현의 코어** (Game State Machine+Visual Director, Moss Baby Character)
3. **카드 → 성장 → 꿈의 감정 엔진** (Growth Accumulation, Card, Dream Trigger) — Pillar 2·3의 수학적/내러티브적 증명
4. **조용함의 설계 계약** (Stillness Budget) — Pillar 1의 장기 수호자, 동시 이벤트·애니메이션·사운드 상한의 단일 출처
5. **다이어제틱 UI와 씬** (Lumen Dusk Scene, Card Hand UI, Dream Journal UI, Window & Display)

전체 **19개 시스템** 중 **14개가 MVP**로 분류되며 7일 축약 아크에서 핵심 루프를 검증한다. Vertical Slice에서 4개 시스템(Audio, Title & Settings UI, Farewell Archive, Accessibility 확장)이 더해져 21일 풀 경험이 완성되고, Companion Mode는 Polish-only로 Full Vision에 포함된다.

---

## Systems Enumeration

| # | System Name | Category | Priority | Status | Design Doc | Depends On |
|---|---|---|---|---|---|---|
| 1 | Time & Session System (inferred) | Core | MVP | **Approved** (R3 — lean review APPROVED 2026-04-17. R2 6 BLOCKER 전부 해소 + 8 RECOMMENDED 적용 + OQ-4/OQ-6 RESOLVED. RECOMMENDED 2건은 구현 단계 이관. see review-log) | [time-session-system.md](time-session-system.md) | — |
| 2 | Data Pipeline (inferred) | Core | MVP | **Approved** (R3 — lean review APPROVED 2026-04-17. R1의 8 BLOCKER 해소 확인 + R3 인코딩 수정·AC-DP-18 추가. R1 RECOMMENDED 5건은 구현/downstream 이관. see review-log) | [data-pipeline.md](data-pipeline.md) | — |
| 3 | Save/Load Persistence (with Integrity) | Persistence | MVP | **Approved** (R3 + cross-review C-1 해소 2026-04-18: PlaythroughSeed 필드 추가) | [save-load-persistence.md](save-load-persistence.md) | — |
| 4 | Input Abstraction Layer (new — R2) | Core | MVP | **Approved** (R2 — lean review APPROVED 2026-04-17. R1의 2 BLOCKER 해소 확인. RECOMMENDED 2건은 구현/ADR 이관. see review-log) | [input-abstraction-layer.md](input-abstraction-layer.md) | — |
| 5 | Game State Machine (inferred; absorbs Visual State Director) | Core | MVP | **Approved** (R3 + cross-review C-2 해소 2026-04-18: FOnDayAdvanced 발행자 수정) | [game-state-machine.md](game-state-machine.md) | Time & Session, Save/Load |
| 6 | Moss Baby Character System | Gameplay | MVP | **Approved** (R3 — full review APPROVED 2026-04-17. R1+R2 10 BLOCKER 전부 해소. R3 신규 blocking 0건. RECOMMENDED 6건+Nice-to-Have 9건은 구현/아트 파이프라인 이관. see review-log) | [moss-baby-character-system.md](moss-baby-character-system.md) | Data Pipeline |
| 7 | Growth Accumulation Engine | Gameplay | MVP | **Approved** (R4 — lean review APPROVED 2026-04-18. R1-R3 총 20 BLOCKER 전부 해소 확인. RECOMMENDED 8건은 구현/downstream 이관. see review-log) | [growth-accumulation-engine.md](growth-accumulation-engine.md) | Data Pipeline, Save/Load, Moss Baby Character |
| 8 | Card System (drag removed — R2) | Gameplay | MVP | **Approved** (R3 — full review APPROVED 2026-04-18. R1+R2 총 6 BLOCKER 해소 확인 + R3 AC 분류·누락 4건 수정. RECOMMENDED 4건+Nice-to-Have 3건은 구현/downstream 이관. see review-log) | [card-system.md](card-system.md) | Data Pipeline, Time & Session, Game State Machine |
| 9 | Dream Trigger System | Narrative | MVP | **Approved** (lean review 2026-04-18. 4 blocking 해소. 꿈 빈도 3-5회/21일 확정) | [dream-trigger-system.md](dream-trigger-system.md) | Growth Accumulation, Data Pipeline, Save/Load |
| 10 | Stillness Budget (new — R2) | Meta | MVP | **Approved** (R1 — lean review APPROVED 2026-04-17. RECOMMENDED 3건은 구현 이관. see review-log) | [stillness-budget.md](stillness-budget.md) | Data Pipeline (UStillnessBudgetAsset 로드 — Data Pipeline GDD §C Interactions) — publishes limits; Reduced Motion toggle는 공개 API |
| 11 | Lumen Dusk Scene (re-layered — R2; owns PSO Precaching) | Core (Env Assets) | MVP | **Approved** (lean review 2026-04-18. 5 blocking 해소. ULumenDuskAsset 확정) | [lumen-dusk-scene.md](lumen-dusk-scene.md) | Game State Machine |
| 12 | Card Hand UI (owns drag-to-offer — R2) | UI | MVP | **Approved** (lean review 2026-04-18. 2 blocking 해소. OQ-CHU-1은 ADR 단계 이관) | [card-hand-ui.md](card-hand-ui.md) | Card System, Input Abstraction, Game State Machine, Stillness Budget |
| 13 | Dream Journal UI | UI | MVP | **Approved** (lean review 2026-04-18. 3 blocking 해소. OQ-DJ-1 RESOLVED) | [dream-journal-ui.md](dream-journal-ui.md) | Data Pipeline (GetDreamBodyText), Dream Trigger, Save/Load, Stillness Budget |
| 14 | Window & Display Management (new — R2) | Meta | MVP | **Approved** (lean review 2026-04-18. blocking 0건. APPROVED) | [window-display-management.md](window-display-management.md) | Input Abstraction |
| 15 | Audio System (demoted — R2) | Audio | Vertical Slice | Not Started | — | Game State Machine, Card System, Dream Trigger |
| 16 | Title & Settings UI (demoted — R2) | UI | Vertical Slice | Not Started | — | Game State Machine, Save/Load, Accessibility |
| 17 | Farewell Archive | Narrative | Vertical Slice | Not Started | — | Growth Accumulation, Dream Trigger, Save/Load, Game State Machine |
| 18 | Accessibility Layer (Text Scale/Colorblind — split R2) | Meta | Vertical Slice | Not Started | — | Stillness Budget (Reduced Motion is already MVP via that system), UI |
| 19 | Companion Mode | Meta | Full Vision | Not Started | — | Window & Display, UI systems, Windows native API |

**Note on R2 markers**:
- "(new — R2)" : 개정판에서 추가된 시스템
- "(demoted — R2)" : MVP에서 더 나중 티어로 강등됨
- "(re-layered — R2)" : Layer 재분류됨
- "(inferred)" : game-concept.md에 명시 없이 도출됨

---

## Categories

| Category | Description | Systems in This Project |
|---|---|---|
| **Core** | Foundation 시스템 전체가 의존하는 뼈대 | Time & Session, Data Pipeline, Input Abstraction, Game State Machine (+Visual Director), Lumen Dusk Scene |
| **Gameplay** | 게임을 재미있게 만드는 시스템 | Moss Baby Character, Growth Accumulation, Card System |
| **Persistence** | 저장 상태와 연속성 | Save/Load Persistence (with Integrity) |
| **UI** | 플레이어 대면 정보 표시 | Card Hand UI, Dream Journal UI, Title & Settings UI |
| **Audio** | 사운드·음악 시스템 | Audio System |
| **Narrative** | 이야기·대화 전달 | Dream Trigger System, Farewell Archive |
| **Meta** | 핵심 루프 바깥의 시스템 | Stillness Budget, Window & Display Management, Accessibility Layer, Companion Mode |

**제외된 카테고리**: Progression (성장 시스템 자체가 진행), Economy (자원·통화 없음 — Anti-pillar)

---

## Priority Tiers

| Tier | Definition | Target Milestone | Systems |
|---|---|---|---|
| **MVP** | 7일 축약 루프 검증 + 구조적 기반 | 8–14주 (현실적 추정, R2 개정), 잼 데모 | **14** (1–14) |
| **Vertical Slice** | 21일 전체 경험. 외부 플레이테스트 준비 | +2–4개월, Shippable Demo (Steam Next Fest) | **4** (15–18) |
| **Alpha** | 해당 없음 — 규모상 VS에서 Full로 직접 | — | 0 |
| **Full Vision** | Polish. 소장 가치 있는 데스크톱 오브제 완성 | 6–9개월, Steam 출시 | **1** (19) |

**Timeline honesty (R2)**: game-concept.md의 MVP "4–8주"는 하한. 실제로는 약 38 설계 세션 + 리뷰 루프 + 프로토타입 + 아키텍처 결정 반영 시 **MVP 구현 8–14주가 현실적**. 외부 플레이테스트·콘텐츠 확장까지 포함한 Shippable Demo는 3–6개월.

---

## Dependency Map (R2 — re-layered)

### Foundation Layer (no dependencies — 병렬 설계 OK)

1. **Time & Session System** — 순수 로직. 시스템 클럭만 읽음. 모든 시간 관련 동작의 기반. **Prototype-first 전환 (R2)**
2. **Data Pipeline** — DataTable/DataAsset 로더. 독립적 로딩 레이어
3. **Save/Load Persistence (with Integrity)** — `USaveGame` 프레임워크 + `SaveSchemaVersion` 마이그레이션 + atomic write + 이중 슬롯 + 체크섬
4. **Input Abstraction Layer (NEW R2)** — Mouse / Partial Gamepad / Steam Deck 입력 통합. `OnInputAction(EInputType)` 수준의 추상화 발행

### Core Layer (depends on foundation)

5. **Game State Machine** — depends on: Time & Session, Save/Load. Dawn/Offering/Waiting/Dream/Farewell/Menu 상태 전환 + **Visual State Director (R2 흡수)**: LUT·Niagara·Light 파라미터를 상태별 MPC로 매핑
6. **Moss Baby Character System** — depends on: Data Pipeline. 이끼 아이 Actor + Morph + Material 드라이버
7. **Lumen Dusk Scene (re-layered R2)** — depends on: Game State Machine (상태별 씬 파라미터). 환경 에셋·조명·카메라·DoF + **PSO Precaching 소유**

### Feature Layer (depends on core)

8. **Growth Accumulation Engine** — depends on: Data Pipeline, Save/Load, Moss Baby Character. 태그 벡터 → 성장 단계/최종 형태
9. **Card System** (R2 재정의) — depends on: Data Pipeline, Time & Session, Game State Machine. 일일 3장 제시 + `FOnCardOffered(FName)` 이벤트 발행만. **Drag-to-offer 입력 로직 제외 (Card Hand UI 소유)**
10. **Dream Trigger System** — depends on: Growth Accumulation, Data Pipeline, Save/Load. 꿈 언록 + 희소성 관리. **Prototype-early 전환 (R2)**
11. **Stillness Budget (NEW R2)** — depends on: 최소. 동시 애니메이션/파티클/사운드 이벤트 상한의 단일 출처. **Reduced Motion 토글은 이 시스템의 공개 API**. 다른 시스템들은 Budget을 구독·준수

### Presentation Layer (depends on feature)

12. **Card Hand UI (R2 확장)** — depends on: Card System, **Input Abstraction**, Game State Machine, Stillness Budget. **Drag-to-offer 인터랙션 소유**
13. **Dream Journal UI** — depends on: Dream Trigger, Save/Load, Stillness Budget

### Meta Layer (cross-cutting, VS/Full로 일부 연기)

14. **Window & Display Management (NEW R2, MVP)** — depends on: Input Abstraction (focus events). 800×600 min, 리사이즈, DPI, 포커스 상실 반응

**Vertical Slice 추가 분포**:
15. **Audio System (VS)** — depends on: Game State Machine, Card System, Dream Trigger. Feature Layer
16. **Title & Settings UI (VS)** — depends on: Game State Machine, Save/Load, Accessibility. Presentation Layer
17. **Farewell Archive (VS)** — depends on: Growth, Dream, Save/Load, Game State Machine. Presentation Layer
18. **Accessibility Layer (VS — split)** — depends on: Stillness Budget (Reduced Motion), UI widgets (Text Scale/Colorblind). Meta Layer

### Polish Layer

19. **Companion Mode** — depends on: Window & Display, UI systems, Windows native API

---

## Recommended Design Order (R2)

Foundation 4개는 병렬 설계 가능. Stillness Budget은 Feature지만 조기 명시 필요 (다른 시스템이 구독).

| Order | System | Priority | Layer | Agent(s) | Est. Effort | R2 Note |
|---|---|---|---|---|---|---|
| **P1** | **Time & Session System** (prototype) | MVP | Foundation | unreal-specialist | Spike 1–2일 | **Prototype-first** |
| **P2** | **Dream Trigger rarity prototype** | MVP | (Feature) | systems-designer | Spike 1–2일 | **Early playtest-driven** |
| 1 | Time & Session System (GDD) | MVP | Foundation | game-designer + systems-designer | M | Prototype 결과 반영 |
| 2 | Data Pipeline | MVP | Foundation | game-designer | S | |
| 3 | Save/Load Persistence (with Integrity) | MVP | Foundation | systems-designer | **M+** | R2: Integrity 흡수로 노력 증가 |
| 4 | Input Abstraction Layer (NEW) | MVP | Foundation | unreal-specialist + ux-designer | S | |
| 5 | Stillness Budget (NEW, 조기 명시) | MVP | Feature | game-designer + art-director | S | 다른 시스템이 구독하므로 조기에 |
| 6 | Game State Machine (+ Visual Director) | MVP | Core | game-designer + systems-designer + technical-artist | **L** | R2: Visual Director 흡수로 노력 증가 |
| 7 | Moss Baby Character System | MVP | Core | game-designer + art-director | M | |
| 8 | Lumen Dusk Scene (+ PSO Precaching) | MVP | Presentation | art-director + technical-artist | M | R2: PSO 소유권 포함 |
| 9 | **Growth Accumulation Engine** | MVP | Feature | **systems-designer** (공식 중심) | **L** | |
| 10 | Card System (R2 재정의) | MVP | Feature | game-designer + systems-designer | M | R2: drag 제외로 노력 ↓ |
| 11 | **Dream Trigger System (GDD)** | MVP | Feature | **systems-designer + narrative-director** | **L** | Prototype P2 결과 반영 |
| 12 | Card Hand UI (drag 포함) | MVP | Presentation | ux-designer + ue-umg-specialist | **M+** | R2: drag 흡수로 노력 ↑ |
| 13 | Dream Journal UI | MVP | Presentation | ux-designer + writer | M | |
| 14 | Window & Display Management (NEW) | MVP | Meta | unreal-specialist | S | |
| 15 | Audio System (VS) | VS | Feature | audio-director + sound-designer | M | Demoted; MVP는 placeholder |
| 16 | Title & Settings UI (VS) | VS | Presentation | ux-designer | M | Demoted |
| 17 | Accessibility Layer (VS — Text/Colorblind) | VS | Meta | accessibility-specialist | S | Reduced Motion은 Stillness Budget 내장 |
| 18 | Farewell Archive (VS) | VS | Presentation | narrative-director + art-director | M | MVP 종료 스크린의 감정 코어 1%와 연결 |
| 19 | Companion Mode | Full | Polish | unreal-specialist | S | |

**Effort Legend**: S = 1 세션, M = 2–3 세션, M+ = 3 세션, L = 4+ 세션

**총 추정 설계 노력 (R2)**:
- S (5개) = 5
- M (8개) = 20
- M+ (2개) = 6
- L (3개) = 12
- Spikes: 2 × 1–2일 = 별도
- **합계 ~43 설계 세션** (원본 26–34 대비 증가는 (1) 추가 3 시스템, (2) Game State 확장, (3) Save Integrity 흡수, (4) Card Hand UI 확장 반영)

---

## Circular Dependencies

**없음.** Stillness Budget은 다른 시스템이 구독하는 Publisher이지만 Budget 자체는 Foundation의 Data Pipeline만 사용 (튜닝 값 로드). 순환 없음.

Accessibility Layer 분할로 이전 우려 해소:
- Reduced Motion → Stillness Budget (MVP)의 공개 API
- Text Scale / Colorblind → Accessibility Layer (VS), UI widget이 소비

---

## High-Risk Systems

Prototype-first / early-prototype 권장. 설계 순서와 독립적으로 검증.

| System | Risk Type | Risk Description | Mitigation |
|---|---|---|---|
| **Time & Session System** | Technical | 시스템 시간 조작(time travel 플레이), suspend/resume 드리프트, NTP 동기화 점프, DST, 앱 닫힌 중의 경과. `FDateTime::UtcNow()` 단독 사용 시 감지 한계 | **P1 Spike**: monotonic clock (`FPlatformTime::Seconds()`) 병행 저장 + 이상 감지 알고리즘 실측. 완료 후 GDD 작성 |
| **Growth Accumulation Engine** | Design | **핵심 판타지의 수학적 증명**. 태그 벡터 → 형태 매핑이 "의미있다"고 느껴지지 않으면 게임 전체 실패 | `/prototype growth-mapping` — 3종 최종 형태로 블라인드 A/B 테스트. "같은 카드를 줬는데 다른 결과"가 읽혀야 함 |
| **Dream Trigger System** | Design | 희소성 튜닝이 모든 것. 너무 잦으면 Pillar 3 위반, 너무 드물면 이탈. Dream Trigger GDD 채택값 3–5회/21일(평균 4–7일 간격) | **P2 Early Spike**: 공식보다 튜닝 테이블 기반 (`AC_DreamFrequency` DataAsset). 플레이테스트 반복 후 GDD |
| **Save/Load Persistence (with Integrity)** | Technical | 21일 진행 중인 세이브가 패치 후 로드 실패 시 플레이어 투자 증발. atomic write 실패 시 부분 쓰기 | `SaveSchemaVersion : uint8` + `FMossSaveData::MigrateFrom(uint8 OldVersion)` 설계. 이중 슬롯 A/B ping-pong + 체크섬 |
| **Lumen Dusk Scene** | Performance | HWRT BVH + Surface Cache 150MB + Farewell vignette·Dream Reveal 동시 피크 | 설계 후 첫 구현 마일스톤에 GPU 프로파일링 gate 삽입 |

---

## Progress Tracker

| Metric | Count |
|---|---|
| Total systems identified | **19** (R2) |
| Spikes required before GDD | 2 (Time & Session ✓, Dream Trigger pending) |
| Design docs started | **14** (Time R3, Save/Load R3, Data Pipeline R3, Input Abstraction R2, Stillness Budget R1, Game State Machine R3, Moss Baby Character R3, Growth Accumulation R4, Card System R3, **Lumen Dusk Scene D1, Dream Trigger D1, Card Hand UI D1, Dream Journal UI D1, Window & Display D1**) |
| Design docs reviewed | 9 (Time R1+R2+R3, Save/Load R1+R2+R3, Data Pipeline R1+R3, Input Abstraction R1+R2, Stillness Budget R1, Moss Baby Character R1+R2+R3, Game State Machine R1+R2+R3, Growth Accumulation R1+R2+R3+R4, **Card System R1+R2+R3**) |
| Design docs approved | **14** (Time R3 + Data Pipeline R3 + Save/Load R3+C1 + Input Abstraction R2 + Stillness Budget R1 + Moss Baby Character R3 + Game State Machine R3+C2 + Growth Accumulation R4 + Card System R3 + **Dream Trigger L1 + Lumen Dusk L1 + Card Hand UI L1 + Dream Journal UI L1 + Window Display L1**) |
| MVP systems designed | **14/14 (14 APPROVED)** — **전체 MVP GDD 설계·리뷰·승인 완료** |
| Vertical Slice systems designed | 0/4 |
| Full Vision systems designed | 0/1 |

---

## Open Questions (R2 Update)

| # | Question | Status | Resolves In |
|---|---|---|---|
| 1 | Day 21 이후 UX (restart ritual 포함) | Open | Farewell Archive GDD |
| 2 | Gamepad/Steam Deck 드래그 대체 입력 | Open | Input Abstraction Layer + Card Hand UI GDDs |
| ~~3~~ | ~~Day 카운터 노출 여부~~ | **CLOSED (R2)**: 영구 미표시. Pillar 1 위반 증거가 발견되었을 때만 재검토 | — |
| 4 | Text Scale 시 카드 크기 씬 구성 영향 | Open | Accessibility Layer + Card Hand UI GDDs |
| 5 | MVP placeholder audio 구현 방식 (NEW R2) | Open | Audio System 강등으로 MVP는 단일 루프 wav or 무음. Audio System GDD에서 확정 |

---

## Next Steps

### Immediate (prototype-first for 2 high-risk systems)
- [ ] **P1**: `/prototype time-session` — 1–2일 spike. FDateTime + monotonic clock + suspend/resume 검증
- [ ] **P2**: `/prototype dream-rarity` — 1–2일 spike. 희소성 튜닝의 감정 기준선 확인

### MVP Foundation GDDs (전부 Approved)
- [x] `/design-system time-session-system` (R3 APPROVED 2026-04-17)
- [x] `/design-system data-pipeline` (R3 APPROVED 2026-04-17)
- [x] `/design-system save-load-persistence` (R3 APPROVED 2026-04-17)
- [x] `/design-system input-abstraction-layer` (R2 APPROVED 2026-04-17)

### Continue per Recommended Design Order
- Stillness Budget 조기 명시 (다른 시스템이 구독)
- Game State Machine (+ Visual Director) → Moss Baby Character → Lumen Dusk Scene (+ PSO) → Growth → Card System → Dream Trigger (P2 결과 반영) → UI들 → Window & Display

### Quality Gates
- [ ] `/design-review [path]` 각 GDD 완성 후
- [ ] `/review-all-gdds` MVP GDD 일괄 완성 후
- [ ] `/gate-check pre-production` MVP GDD 리뷰·수정 완료 후

---

## Review Mode Notes

Lean mode로 이 세션에서 자동 skip된 Director 게이트:
- **TD-SYSTEM-BOUNDARY** (Phase 3 후) — Lean mode auto-skip
- **PR-SCOPE** (Phase 4 후) — Lean mode auto-skip
- **CD-SYSTEMS** (Phase 5 후) — Lean mode auto-skip

**단, 사용자 명시 요청으로 Phase 7에서 두 디렉터 선행 리뷰를 수동 실행** (2026-04-16). CONCERNS 판정 수용하여 이 R2 개정 반영.

다음 Phase Gate (`/gate-check pre-production`)에서 Director 리뷰가 일괄 재실행된다.
