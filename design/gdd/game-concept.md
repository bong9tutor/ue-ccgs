# Game Concept: Moss Baby (이끼 아이)

*Created: 2026-04-16*
*Status: Draft*

---

## Elevator Pitch

> 책상 위 유리병 속에 작은 이끼 영혼이 산다. 매일 한 번, 당신은 그에게 한 장의 **선물 카드**를 건네고, 때로 그가 꾸는 꿈의 조각을 받는다. 21일 후 — 당신이 건넨 것들의 패턴이 그 아이의 마지막 모습이 된다.

**10초 테스트**: 처음 듣는 사람이 "매일 작은 존재에게 선물 하나를 주고, 21일 후 이별한다"를 이해할 수 있다 → 통과.

---

## Core Identity

| Aspect | Detail |
| ---- | ---- |
| **Genre** | Cozy desktop companion / Card-ritual / Slow-life-sim |
| **Platform** | PC (Steam / itch.io) |
| **Target Audience** | Cozy 게임 팬, 저널링 취향, Tamagotchi 세대 (20–40대) |
| **Player Count** | Single-player |
| **Session Length** | 2–3분 × 21일 (실시간 Tamagotchi 모델) |
| **Monetization** | Premium ($4.99–$7.99 target) |
| **Estimated Scope** | Small (6–9개월, 솔로; MVP 4–8주) |
| **Comparable Titles** | Cozy Grove, Unpacking, Strange Horticulture, Tamagotchi |

---

## Core Fantasy

> *"내가 매일 건넨 작은 조각들이 모여, 이 존재가 이 존재가 되었다."*

프린세스 메이커의 긴 육성 DNA에서 핵심 감정만 증류한다: **돌봄의 누적이 눈에 보이는 결과로 맺히는 감각**. 하지만 육성 시뮬의 스프레드시트적 긴장(스탯 최적화)은 버리고, 그 자리에 **조용한 의식의 시학**을 둔다.

플레이어가 여기서 얻는 것: 자기 시간의 일부를 한 작은 존재에게 내어주는 경험. 그리고 21일 뒤, 그 시간이 **가시적인 형태로** 돌아오는 순간.

---

## Unique Hook

> *"Tamagotchi처럼 실시간으로 자라는 작은 존재를 돌보되, AND ALSO 숫자가 없다 — 카드는 시·계절·감정이고, 이끼 아이의 반응은 어쩌다 찾아오는 꿈의 조각으로 돌아온다."*

- **실시간 21일**이 주는 진짜 경과 감각 (세션 압축형 육성 게임들과 차별)
- **숫자·스탯 UI 배제** — 플레이어는 계산하지 않고 해석한다
- **대화는 희소하다** — 매일 반응하는 AI 친구가 아닌, 가끔 말하는 *존재*

---

## Player Experience Analysis (MDA Framework)

### Target Aesthetics (What the player FEELS)

| Aesthetic | Priority | How We Deliver It |
| ---- | ---- | ---- |
| **Submission** (relaxation, comfort zone) | **1** | 매일 2–3분의 짧고 일정한 의식. 실패 없음, 최적해 없음. |
| **Fantasy** (make-believe, role-playing) | **2** | 유리병 안의 이끼 영혼이라는 소재 자체. 스타일라이즈된 디오라마. |
| **Expression** (self-expression, creativity) | **3** | 21일 동안 건넨 카드 조합이 최종 형태를 결정. 결과물은 "나만의 이끼". |
| **Discovery** (exploration, secrets) | **4** | 꿈 조각이 드물게 주어짐 → 다음 꿈을 기다리는 호기심. |
| **Narrative** (drama, story arc) | 5 | 21일의 시작-중간-끝 구조에 내재. 명시적 플롯은 없음. |
| **Sensation** (sensory pleasure) | 6 | 황혼 Lumen 조명, 부드러운 ASMR 앰비언트, 카드 넘김 SFX. |
| **Challenge** (mastery) | N/A | 의도적 배제 (Pillar 1·2에 위배). |
| **Fellowship** (social) | N/A | 의도적 배제 (Anti-pillar). |

### Key Dynamics (Emergent behaviors we want)

- 플레이어는 카드를 고르기 전 **잠시 망설이는** 의식적 행동을 한다
- 플레이어는 이끼 아이의 꿈을 **다시 읽으려** 일기 화면을 연다
- 플레이어는 "어떤 조합이 어떤 결과를 낳을까" 궁금해서 **다른 플레이** 의향을 갖는다
- 21일 끝에 최종 형태를 **저장·공유하고 싶어한다** (소셜 기능은 없지만 스크린샷 욕구)

### Core Mechanics (Systems we build)

1. **일일 카드 제공 & 선택** — 매일 랜덤 + 시즌 가중치로 3장이 제시됨. 1장 선택·건넴.
2. **누적 성장 엔진** — 건네진 카드들의 태그(계절·원소·감정) 벡터가 쌓여 최종 형태를 결정.
3. **꿈 조각 트리거** — 특정 카드 조합·누적 임계점에서 드물게 꿈 텍스트 1개 언록.
4. **황혼 씬 렌더** — 유리병·이끼 아이·UI가 하나의 Lumen 라이트 씬으로 구성.
5. **작별의 아카이브** — 21일 종료 시 최종 형태 + 꿈 일기 + 카드 기록이 하나의 이미지/페이지로 저장.

---

## Player Motivation Profile

### Primary Psychological Needs Served

| Need | How This Game Satisfies It | Strength |
| ---- | ---- | ---- |
| **Autonomy** | 매일 완전한 자유 선택. 정답 없음. 스탯 시스템 없음. | **Core** |
| **Competence** | 꿈 일기가 채워지는 것 = "나는 이 이끼를 읽을 줄 안다" 증거. | Supporting |
| **Relatedness** | 침묵과 간헐적 대화의 리듬이 조용한 친밀감을 만든다. | **Core** |

### Player Type Appeal (Bartle Taxonomy)

- [x] **Explorers** — 다음 꿈은 뭘까? 어떤 카드 조합이 뭘 만들까? → **Primary**
- [x] **Socializers** (내면형) — 이끼 아이와의 일대일 관계. 외부 소셜은 배제. → **Primary**
- [ ] **Achievers** — 의도적으로 약화. 업적·완주 보상 없음.
- [ ] **Killers/Competitors** — 의도적 배제. Pillar와 충돌.

### Flow State Design

- **Onboarding (첫 3분)**: 유리병이 책상에 놓이고, 이끼 아이가 깨어난다. 첫 카드 1장이 손 위에 도착. 설명 없이 제스처로 건넴. "건넴" 1회 경험 후 게임은 끝난다. 내일 돌아오게 만든다.
- **Difficulty scaling**: 난이도 개념 없음. 대신 **감정 커브**를 설계 — 1–5일 탐색기, 6–14일 깊어짐, 15–20일 징조, 21일 작별.
- **Feedback clarity**: 카드를 건네면 즉시 시각 변화(빛·색·자세). 꿈은 드물게 (평균 4–7일 간격, 21일 중 3–5회 — Dream Trigger GDD 채택값), 일기 아이콘이 조용히 빛나는 것으로 알림.
- **Recovery from failure**: 실패라는 개념 자체가 없음. 하루를 놓쳐도 이끼 아이는 기다린다(여러 날 놓친 경우 약한 "메마름" 상태 → 첫 카드로 복구).

---

## Core Loop

### Moment-to-Moment (30초)

카드 3장 중 1장을 선택 → 이끼 아이에게 건넴 → 짧은 시각 반응(빛·흔들림) → (간헐적) 꿈 알림 확인.

### Short-Term (5–15분)

**해당 없음** — 이 게임은 하루 1회 2–3분 세션이 최소 단위. "한 번 더"가 없는 구조가 의도. 세션 길이 초과 유도는 Pillar 1("조용한 존재") 위반.

### Session-Level (일일 세션)

하루 1세션. 앱을 열고, 카드를 건네고, 꿈이 왔다면 한 번 읽고, 앱을 닫는다. 자연스러운 의식적 종료.

### Long-Term Progression (21일 아크)

- **Days 1–5**: 탐색. 이끼 아이의 기본 성질을 본다. 꿈은 드물다.
- **Days 6–14**: 깊어짐. 성장 단계가 눈에 띄게 바뀐다. 꿈이 구체화된다.
- **Days 15–20**: 징조. 최종 형태가 암시되기 시작. 일기에 "곧"이라는 단어가 등장.
- **Day 21**: 작별. 최종 형태 공개 + 마지막 꿈 + 아카이브 저장. 게임 종료.

### Retention Hooks

- **Curiosity**: "내일 어떤 카드가 올까? 꿈은 언제 또 올까?"
- **Investment**: 13일 돌봐준 이 존재를 버리긴 어렵다.
- **Social**: **의도적 없음** (Anti-pillar).
- **Mastery**: **의도적 없음** (Pillar 2).
- **Ritual anchor**: 아침 커피 옆 / 자기 전 2분 — 일상에 자연스럽게 끼워넣는 시간.

---

## Game Pillars

### Pillar 1: 조용한 존재 / Quiet Presence

이끼 아이는 항상 거기 있지만 결코 주의를 요구하지 않는다. 알림도, 재촉도, 죄책감도 없다.

*Design test*: "오늘 돌아오세요!" 푸시 알림 추가를 고민한다면 → **배제**. "못 해서 미안"이라는 감정은 이 게임의 적.

### Pillar 2: 선물의 시학 / The Poetry of Gifts

카드는 숫자가 아니라 이미지·계절·감정이다. 플레이어는 스탯을 계산하지 않고 의미를 읽는다.

*Design test*: 카드 위에 "+3 생명력" 같은 수치 텍스트가 나오면 → **배제**. "어떤 계절의 바람"이 더 낫다.

### Pillar 3: 말할 때만 말한다 / Speak Only When Moved

이끼 아이는 매일 대답하지 않는다. 대부분의 날은 고요하고, 어쩌다 한 번의 꿈이 더 소중해진다.

*Design test*: "매일 반드시 한 줄 대사"가 고민된다면 → **배제**. 희소성이 감정을 만든다.

### Pillar 4: 끝이 있다 / It Ends

21일 후, 게임은 끝난다. 무한 루프 없음. 작별은 서사의 일부다.

*Design test*: "30일/무한 모드로 늘려달라"는 요청 → **신중히 검토**. 끝의 무게를 지킬 수 있는가를 먼저 물어라.

### Anti-Pillars

- **NOT 게임화된 진척 지표** (XP 바·레벨·별★): Pillar 2를 깨뜨린다. 스탯의 존재 자체가 시학을 파괴.
- **NOT 일일 로그인 보상·스트릭·FOMO**: Pillar 1을 깨뜨린다. "놓치면 손해"는 조용한 존재의 반대말.
- **NOT 소셜 비교·리더보드·공유 강요**: "내 이끼가 예쁜가?"는 틀린 질문. 플레이어의 여정은 그와 이끼 둘만의 것.

---

## Inspiration and References

| Reference | What We Take From It | What We Do Differently | Why It Matters |
| ---- | ---- | ---- | ---- |
| **Cozy Grove** | 실시간 daily 세션의 감정적 리듬 | 캐릭터 ∞명 대신 **단 1명**의 이끼 아이에 집중 | 일일 의식형 cozy가 유료로 성립 ($14.99, 180k+ 소유) |
| **Unpacking** | 대사 없는 서사, 배치로 이야기를 쓰기 | 공간 배치 대신 **시간에 걸친 누적** | 내러티브 UI가 대사 UI를 이길 수 있음 (200만+) |
| **Strange Horticulture** | 조용하고 예술적인 호기심 게임 | 미스터리 해결 대신 **호기심의 순수한 형태** | 소규모 cozy에 시장이 있음 (10만+) |
| **Tamagotchi** | 실시간 돌봄의 애착 구조 | 죽음·스트레스 페널티 **제거** | 노스탤지아 + 현대적 재해석의 정당성 |
| **Princess Maker** | 긴 호흡의 육성 DNA (**원체험**) | 수년 → 21일, 스탯 → 카드, 전투 → 대화로 **증류** | 육성 장르의 감정 코어를 새 문법으로 |

**Non-game inspirations**:
- Wabi-sabi 미학 (불완전·일시적·고요함)
- 다도 · 사계 엽서 · 이시이 신지의 오브제 삽화
- *My Neighbor Totoro* (지브리) — 정령과의 작은 만남의 감각
- 자잘한 다이어리·잔념 일기 문화 (1cm 일기, Hobonichi Techo)

---

## Target Player Profile

| Attribute | Detail |
| ---- | ---- |
| **Age range** | 22–40 |
| **Gaming experience** | Casual / Mid-core. 대작보다 작은 아트 게임에 관심 있음. |
| **Time availability** | 평일 2–5분짜리 짧은 세션을 선호. 긴 집중 시간을 낼 수 없는 일상. |
| **Platform preference** | PC 데스크톱 (업무용 노트북 포함) |
| **Current games they play** | Stardew Valley, A Short Hike, Unpacking, Cozy Grove |
| **What they're looking for** | 하루를 부드럽게 여닫는 **작고 의미있는 의식**. 요구 없는 동반자. |
| **What would turn them away** | 알림 스팸, 과금 유도, 스트레스 유발 페일 스테이트, 긴 온보딩. |

---

## Technical Considerations

| Consideration | Assessment |
| ---- | ---- |
| **Recommended Engine** | **Unreal Engine 5.6 (C++)** — 저장소 기 예약. 장르에는 드문 선택이지만 Lumen(황혼 조명) · Niagara(포자/반딧불) · Sequencer(작별 씬)가 시각적 강점. |
| **Key Technical Challenges** | (1) UE5 2.5D/스타일라이즈 렌더 세팅 (2) 21일 실시간 상태의 로컬 세이브·복구 (3) 최소 메모리로 데스크톱 상주 (4) 꿈 텍스트 데이터-드리븐 파이프라인 |
| **Art Style** | Stylized 3D diorama (종이 공예 + 수채화 톤). 전체 화면 = 유리병 하나를 담은 미니어처 씬. |
| **Art Pipeline Complexity** | Medium. 이끼 아이 리그는 간소화 (블렌드 셰이프 위주, 풀 리깅 없음). |
| **Audio Needs** | Moderate. 앰비언트 드론 1개 + 시간대별 레이어 3개 + 카드 SFX 10개 + 꿈 챠임 1개. VO 없음. |
| **Networking** | None (순수 오프라인). |
| **Content Volume** | 카드 30장, 최종 형태 8–12종, 꿈 텍스트 30–50개 (Full Release 기준). |
| **Procedural Systems** | 없음 (수작업 콘텐츠). 카드 제시는 시즌 가중 랜덤. |

---

## Risks and Open Questions

### Design Risks

- **리텐션 설계 vs. Anti-FOMO의 균형** — Pillar 1을 지키면서도 21일 중 이탈을 줄일 내재적 후크가 모호. 중간 비트(Day 6, 14)의 감정 페이스 프로토타입 필요.
- **"의미가 있다"는 감각의 증명** — 카드 조합이 정말 다른 결과를 만드는지 플레이어가 **느낄** 수 있어야 함. 8–12종 최종 형태가 서로 구분 가능해야.
- **꿈의 희소성 튜닝** — 너무 드물면 잊혀짐, 너무 잦으면 Pillar 3 파괴. Dream Trigger GDD 채택값 3–5회/21일(평균 4–7일 간격)의 플레이테스트 검증이 필요.

### Technical Risks

- **UE5 + 솔로 + 작은 게임**: 엔진 오버헤드가 장르 규모에 과함. 패키지 크기, 빌드 시간, 데스크톱 상주 시 메모리가 Unity/Godot 대비 큰 점. 완화: Lumen 대신 정적 조명 + 미니멀 씬.
- **21일 실시간 상태의 버그**: 시간 조작(시스템 시간 변경) 대응, 세이브 파일 손상 복구.

### Market Risks

- **UE5 브랜딩 기대치 괴리**: "언리얼 게임"이라는 라벨이 대작을 기대하게 만들 수 있음. 스토어 페이지·스크린샷으로 cozy 톤을 강하게 전달 필요.
- **21일 실시간의 진입 장벽**: "당장 결과를 못 봄"이 유튜브/스트리머 친화적이지 않음. 마케팅은 "21일 뒤의 최종 컷"에 집중.

### Scope Risks

- **"몇 주" 목표와 MVP의 불일치**: MVP도 현실적으로 4–8주. 잼용 크래시 프로토타입(1주)을 먼저 만들어 루프만 확인하는 단계가 권장됨.
- **글쓰기 부담**: 꿈 30–50개는 2–3주 별도 작가 시간. 엔지니어링과 병행이 아닌 **사전 작업**으로 분리해야 함.
- **아트 일관성 (Lean 모드)**: Art Director 리뷰 skipped — `/art-bible` 조기 실행이 위험 완화에 필수.

### Open Questions

1. **최종 형태 다양성의 감지성**: 플레이어가 "내 이끼는 다른 이끼와 다르다"를 알 수 있는가? → **프로토타입**으로 3종 최종 형태를 보고, 외부 테스터 2–3명에게 블라인드 비교 테스트.
2. **실시간 vs. 압축 모드 제공 여부**: 21일이 너무 길다는 피드백이 나오면 "시간 압축 모드"를 옵션으로 제공할지. → **결정 보류**, Alpha 플레이테스트 후 결정.
3. **데스크톱 상주 형태**: 별도 실행 앱 vs. 항상 떠 있는 작은 창(Always-on-top). → `/architecture-decision`로 처리.

---

## MVP Definition

**Core hypothesis**:
> *"하루 2–3분의 카드-건넴-꿈 루프가, 기능적 보상 없이도 플레이어를 7일 연속 돌아오게 만들 수 있다."*

**Required for MVP** (4–8주 목표):

1. 유리병 + 이끼 아이 **1종** (성장 단계 3개 + 최종 형태 1개)
2. 카드 **10장** (스타일라이즈 일러스트 + 태그 메타데이터)
3. 일일 카드 제시 & 선택 UI (3장 중 1장)
4. 누적 성장 로직 (카드 태그 벡터 → 성장 단계 전환)
5. **7일** 축약 아크 (21일이 아닌)
6. 꿈 텍스트 **5개** (임계점 트리거)
7. 꿈 일기 UI
8. 최소 앰비언트 루프 1개 + 카드 SFX 3개
9. 세이브/로드 (실시간 경과 기록)
10. 종료 스크린 (7일차 최종 형태 + 일기 요약)

**Explicitly NOT in MVP**:

- 8–12종 최종 형태 (1종만)
- 꿈 30개 이상 (5개로 충분)
- 포토 모드 / 아카이브 공유 기능
- 시간대별 조명 레이어 (단일 황혼 고정)
- 작별 시네마틱 (간단한 페이드 아웃으로)
- 이끼 아이 리깅·블렌드 셰이프 (정적 메시 + 머티리얼 애니메이션)

### Scope Tiers

| Tier | Content | Features | Timeline |
| ---- | ---- | ---- | ---- |
| **MVP** | 이끼 1종·최종 1, 카드 10, 꿈 5, 7일 | 핵심 루프만 | 4–8주 |
| **Vertical Slice** | + 최종 4종, 카드 20, 꿈 15, 21일 | + 감정 페이싱 비트 | +4–6주 |
| **Shippable Demo** (Steam Next Fest) | 최종 4종, 카드 30, 꿈 30 | + 시간대 조명·작별 시퀀스 | 2–4개월 총 |
| **Full Release** | 최종 8종, 카드 40, 꿈 50, 포토 모드 | + 4 앰비언트 변주·시네마틱 | 6–9개월 총 |

---

## Visual Identity Anchor (잠정)

> ⚠️ **주의**: Lean 리뷰 모드로 AD-CONCEPT-VISUAL 게이트를 skip했습니다. 이 섹션은 대화 맥락에서 도출한 **잠정 방향**이며, `/art-bible` 실행 시 Art Director가 정식화합니다.

### Visual Direction Name
**"Terrarium Dusk / 유리병 해질녘"**

### One-line Visual Rule
> *"작은 유리 디오라마, 언제나 황혼 빛, 부드러운 이끼 녹·꿀빛·라벤더 그림자."*

### Supporting Visual Principles

1. **스케일은 손바닥** — 모든 것은 작고 가까워야 한다. 카메라가 멀어지면 마법이 깨진다.
   *Design test*: 풀 스크린 와이드 앵글이 고민된다면 → **배제**. 언제나 클로즈 샷.
2. **조명은 늘 황혼 또는 새벽** — 강한 정오 빛 배제. 경사진 따뜻한 측광.
   *Design test*: 푸른 차가운 오전 조명이 고민된다면 → **배제**.
3. **표면은 부드러움** — 날카로운 하드 엣지 금지. 이끼·수채·유리·종이 질감.
   *Design test*: 메탈·플라스틱·선명한 기하가 고민된다면 → **배제**.

### Color Philosophy
- **코어 3톤**: 이끼 녹 / 꿀빛 호박 / 저물녘 라벤더
- **채도**: 중간 이하
- **대비**: 부드럽게 (하이라이트는 작고 따뜻하게)

---

## Next Steps

- [ ] `/setup-engine` — UE 5.6 C++ 기반 구성 + 버전 레퍼런스 문서 생성 (`.claude/docs/engine-reference/`)
- [ ] `/art-bible` — Terrarium Dusk 방향을 정식 Art Bible로 확장 (**GDD 작성 전 필수**)
- [ ] `/design-review design/gdd/game-concept.md` — 이 문서 완결성 검증
- [ ] `/map-systems` — 카드 시스템 / 성장 엔진 / 꿈 트리거 / 렌더 / 세이브 등 서브시스템으로 분해
- [ ] `/design-system [각 시스템]` — 시스템별 GDD 작성
- [ ] `/review-all-gdds` — 시스템 간 일관성 검토
- [ ] `/gate-check pre-production` — 아키텍처 단계 진입 전 게이트 검증
- [ ] `/create-architecture` — 마스터 아키텍처 + Required ADR 리스트 생성
- [ ] `/architecture-decision` ×N — 실시간 상태 지속성, 데스크톱 상주 형태 등 결정 기록
- [ ] `/prototype card-ritual` — 핵심 루프 검증 (7일 축약 모드)
- [ ] `/playtest-report` — 프로토타입 플레이테스트 1–2회
- [ ] `/sprint-plan new` — MVP를 위한 첫 스프린트 계획
