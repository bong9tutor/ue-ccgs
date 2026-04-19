# Accessibility Requirements: Moss Baby (이끼 아이)

> **Status**: Committed (Draft 2026-04-19)
> **Author**: ux-designer + producer (self-review, lean mode)
> **Last Updated**: 2026-04-19
> **Accessibility Tier Target**: **Standard**
> **Platform(s)**: PC (Steam / itch.io), Steam Deck partial
> **External Standards Targeted**:
> - WCAG 2.1 Level AA (body text)
> - AbleGamers CVAA Guidelines (partial — 음성 소통 기능 없음)
> - Xbox Accessibility Guidelines (XAG) — N/A (Xbox 미지원)
> - PlayStation Accessibility — N/A (PS 미지원)
> - Apple / Google Accessibility — N/A (mobile 미지원)
> **Accessibility Consultant**: None engaged (solo dev project)
> **Linked Documents**: `design/gdd/systems-index.md`, `design/ux/interaction-patterns.md`, `design/art/art-bible.md §4 + §7.8`

> **Why this document exists**: per-screen accessibility annotations belong in UX specs (`design/ux/`); project-wide commitments live here. This is the authoritative source for the game's accessibility tier and the feature matrix. If a feature conflicts with a commitment here, the commitment wins unless revised by the producer.

---

## Accessibility Tier Definition

### This Project's Commitment

**Target Tier**: **Standard**

**Rationale**:

Moss Baby는 싱글 플레이어 데스크톱 cozy companion 게임으로, 전투·시간 압박·연속 버튼 입력이 전혀 없다 — 전통적인 모터 accessibility barrier가 구조적으로 부재. 반면 **읽기 중심 narrative** (꿈 일기 텍스트, 시스템 내러티브)와 **색온도 기반 상태 전달** (6 GameState 색온도 2,200K–4,200K)이 visual accessibility의 핵심 barrier다. Standard tier는 이 두 barrier (text 가독성 + 색각 이상 대응)를 정면으로 다루면서, 존재하지 않는 기능 (combat assist, QTE timing adjustment)에 노력을 낭비하지 않는다.

Comprehensive tier는 screen reader + mono audio + 자막 풍부 customization이 필요한데, Moss Baby는 **음성 대사 0개** (모든 텍스트는 `FText` + RichTextBlock) — screen reader는 메뉴 단위에서만 의미 있고, mono audio는 현재 ambient music 1개 + SFX 수 개 규모에서 overkill. 음성 부재로 자막·caption 섹션의 대부분이 N/A. Standard tier + VS 단계 Accessibility Layer (#18) 확장으로 충분.

Pillar 1 (조용한 존재)이 이미 **모달·팝업·알림 절대 금지**를 강제하고 있어 cognitive accessibility의 핵심 (인지 부하·방해 요소 최소화)은 기본값으로 달성된다. Stillness Budget #10의 `IsReducedMotion()` API는 MVP 공개 — motion accessibility는 Standard 요구 중 선제적으로 충족.

**Features explicitly in scope (beyond tier baseline)**:
- **Reduced Motion API** — Stillness Budget #10 공개 (MVP). Motion/Particle 슬롯 EffLimit 0 전환 (Sound 영향 없음)
- **Hover-only 금지 강제** — Input Abstraction Rule 4 (모든 상호작용은 명시적 Select/Confirm)
- **Mouse-only 완결성** — Input Abstraction Rule 5 (키보드 없이 전체 게임 기능 접근)
- **800×600 최소 창 텍스트 가독성** — Card Hand UI F-CHU-1, Dream Journal UI F-DJ-1 formulas
- **모달 절대 금지** — Window&Display Rule 10 (Pillar 1 인프라 강제)
- **색각 이상 안전 팔레트** — Art Bible §4 (이끼 녹 / 꿀빛 / 라벤더 — 색각 이상 3 modes 통과)

**Features explicitly out of scope** (VS 또는 Full Vision 연기):
- **Text Scale UI** — Accessibility Layer #18 (VS)로 연기. MVP는 16px minimum 고정 (Dream Journal UI `FontSizeBody`)
- **Colorblind modes 3종 UI toggle** — Accessibility Layer #18 (VS). MVP는 Art Bible §4 팔레트로 모든 상태가 색각 안전 (non-color 형태·실루엣 차이로 전달)
- **Screen reader 통합** — out of scope. Moss Baby는 음성 대사 0개 + 시적 narrative (screen reader로 전달해도 literary nuance 손실). 대신 모든 UI 버튼은 UMG focus navigation (Gamepad D-pad) 지원 — 시각 장애보다 motor impairment에 접근성 제공
- **Full subtitle customization (font/color/background)** — N/A (음성 대사 부재)
- **Platform accessibility APIs** — N/A (Xbox/PS 미지원, Steam Input 의존)

---

## Visual Accessibility

| Feature | Target Tier | Scope | Status | Implementation Notes |
|---------|-------------|-------|--------|---------------------|
| Minimum text size — Dream Journal body | Standard | Dream Journal UI | **MVP에서 이미 명시** | 16px @ 800×600 (Dream Journal F-DJ-1). Art Bible §7.2 |
| Minimum text size — Card DisplayName/Description | Standard | Card Hand UI | **MVP에서 이미 명시** | Card Hand UI AC-CHU-01 — 800×600에서 판독 가능 |
| Minimum text size — HUD | Standard | N/A | — | Moss Baby는 HUD가 없음 (Pillar 1) |
| Text contrast — UI text on backgrounds | Standard | All UI | Not Started | Art Bible §4.260 색 비중 표 준수. 최소 4.5:1 (WCAG AA). CI 시 자동 contrast checker 권장 |
| Text contrast — Dream Journal | Standard | Dream Journal UI | Not Started | 꿈 텍스트는 의식적 읽기 대상 — 7:1 (WCAG AAA) 목표. 배경 불투명 종이 텍스처 |
| Colorblind safety — single palette 설계 | Standard | All color-coded state (6 GameState) | **Art Bible §4.260에서 명시** | 이끼 녹·꿀빛·라벤더 세 코어 톤이 protanopia/deuteranopia/tritanopia 3 modes 통과 (Art Bible 시뮬 확인) |
| Color-as-only-indicator audit | Basic | All UI and gameplay | **CR-CS-6 + MBC CR-3에서 partial** | 6 GameState 시각 변화는 색온도 + 광원 각도 + SSS 강도 조합 (색 단독 없음). 상세 audit는 아래 |
| UI scaling | Standard | All UI | VS로 연기 | MVP는 1.0 고정 + 800×600 creature. Accessibility Layer #18이 75%-150% 범위 구현 |
| High contrast mode | Comprehensive | Menus | Out of scope | Pillar 1 시각 정체성과 충돌 (Terrarium Dusk 부드러운 톤 필수) |
| Brightness/gamma controls | Basic | Global | Not Started | Window&Display §Tuning Knobs에 추가 검토 (지금 미포함 — VS Title&Settings UI에서 구현) |
| Screen flash / strobe warning | Basic | All VFX | **구조적 부재** | Pillar 1 + Stillness Budget 상한으로 flash 유발 VFX가 존재할 수 없음. Photosensitivity 경고 screen도 불필요 (Moss Baby는 flash 없음) |
| Motion/animation reduction mode | Standard | All UI transitions, particle, light blend | **MVP 완성** | **Stillness Budget #10 `IsReducedMotion()` 공개 API**. Motion/Particle EffLimit=0 전환 — Sound 영향 없음 (Rule 6) |
| Subtitles — on/off | Basic | N/A | — | 음성 대사 부재 |
| Subtitles — speaker identification | Standard | N/A | — | 동일 |
| Subtitles — style customization | Comprehensive | N/A | — | 동일 |

### Color-as-Only-Indicator Audit

| Location | Color Signal | What It Communicates | Non-Color Backup | Status |
|----------|-------------|---------------------|-----------------|--------|
| 6 GameState 색온도 (2,200K Farewell ~ 4,200K Dream) | 색온도 변화 | 현재 상태 (Menu/Dawn/Offering/Waiting/Dream/Farewell) | **광원 각도 + SSS 강도 + 카메라 DoF** — Dream은 차가운 달빛 + 40° 위 각도 + DoF 강화 (Lumen Dusk Rule 3 + ADR-0004) | Resolved |
| 이끼 아이 성장 단계 (Sprout/Growing/Mature) HueShift | Season 태그 기반 색조 오프셋 | 어느 계절 태그가 지배적인지 | **메시 교체** (Sprout → Growing → Mature 형태 자체 변화) + SSS offset | Resolved (MBC CR-1/CR-2) |
| 메마름 (Withered) 서브컨디션 | HueShift -0.02 + SSS 약화 | 장기 부재 후 복귀 | **머티리얼 Roughness 증가 + 서리 낀 텍스처** (CR-5 "차가운 톤 + 빛 감소") — 색 단독 아님 | Resolved (MBC CR-5) |
| Card Hand UI Offer Zone 피드백 | 카드 EmissiveStrength +0.15 | 건넴 가능 상태 | **카드 크기 미세 scale up + Offer Zone 파장 VFX** (Stillness Budget Particle Standard 슬롯) | Resolved (CR-CHU-3) |
| Dream Journal 아이콘 "새 꿈" 신호 | 펄스 애니메이션 + 미세 빛남 | 새 꿈 있음 | **펄스 애니메이션 shape 변화** (색 단독 아님 — 아이콘 크기 호흡) + RM OFF 시 정적 하이라이트 | Resolved (CR-6 + AC-DJ-14) |

모든 항목 Resolved (MVP 현재). VS tier에서 colorblind mode UI toggle 추가 시 사용자가 직접 3 modes 시뮬 선택 가능.

---

## Motor Accessibility

| Feature | Target Tier | Scope | Status | Implementation Notes |
|---------|-------------|-------|--------|---------------------|
| Full input remapping | Standard | All gameplay inputs | Out of scope (UI 없음) | Enhanced Input System 자체가 remapping 지원 — 단, in-game UI는 VS Title&Settings UI (#16). MVP는 default bindings 고정 (6 InputActions) |
| Input method switching | Standard | PC (Mouse/Keyboard ↔ Gamepad) | **MVP 완성** | **Input Abstraction Layer #4 Rule 3 — `OnInputModeChanged(EInputMode)`**. Hysteresis (3px threshold). 알림 없음 (Pillar 1) |
| One-hand mode | Standard | Drag-to-offer (Card Hand UI) | **구조적 충족** | 드래그는 단일 pointer (mouse or touch trackpad) 동작. Gamepad 대체 (Hold A button 0.5s, CR-CHU-9) VS에서 구현 |
| Hold-to-press alternatives | Standard | `IA_OfferCard` Hold | **구조적 충족** | `IA_OfferCard`는 Hold trigger (Mouse 0.15s / Gamepad 0.5s). Hold는 제스처의 본질 ("의식적 건넴" — Pillar 2). 짧은 클릭은 자동으로 `IA_Select` (Input Abstraction Rule 2a) — 별도 toggle 불필요 |
| Rapid input alternatives | Standard | N/A | — | 연속 버튼 누르기 입력 없음 (하루 1장 건넴) |
| Input timing adjustments | Standard | `OfferDragThresholdSec` / `OfferHoldDurationSec` | Not Started | Input Abstraction Tuning Knobs (0.08-0.3s / 0.2-1.5s 범위). VS Title&Settings UI에서 slider 노출 |
| Aim assist | Standard | N/A | — | 조준 없음 |
| Auto-sprint / movement assists | Standard | N/A | — | 이동 없음 |
| Platforming / traversal assists | N/A | N/A | — | 플랫포머 요소 없음 |
| HUD element repositioning | Comprehensive | N/A | — | HUD 없음 (Pillar 1) |

---

## Cognitive Accessibility

| Feature | Target Tier | Scope | Status | Implementation Notes |
|---------|-------------|-------|--------|---------------------|
| Difficulty options | Standard | N/A | — | 난이도 개념 없음 (Game Concept §Flow State Design — "난이도 개념 없음") |
| Pause anywhere | Basic | All gameplay states | **구조적 충족** | Moss Baby는 실시간 pressure 없음. 앱 닫기 자체가 자연스러운 pause. Time & Session #1이 wall clock 경과로 날짜 판정 — 화면 열림 여부와 무관 (Pillar 4 "이끼 아이가 살아낸 시간") |
| Tutorial persistence | Standard | N/A | **구조적 부재** | Tutorial 자체가 존재하지 않음 — Game Concept §Onboarding "설명 없이 제스처로 건넴" 명시 (Pillar 1) |
| Quest / objective clarity | Standard | N/A | **구조적 부재** | Quest 시스템 없음 (Anti-pillar — Game Concept §Anti-Pillars "game화된 진척 지표 금지") |
| Visual indicators for audio-only information | Standard | Stillness Budget Sound events | **부분 충족** | 현재 Sound는 MVP에 placeholder (VS Audio System #15). Dream 도착 시각 큐: 아이콘 펄스 (청각 아닌 시각). 카드 건넴 SFX: 카드 VFX와 동시 (시각 backup 구조적) |
| Reading time for UI | Standard | Dream Journal body text | **구조적 충족** | Dream Journal은 player-dismissed — auto-dismiss 없음 (CR-3). 탐색 버튼 있으면 계속 read 가능 |
| Cognitive load documentation | Comprehensive | Per system | Not Started | systems-index.md에 시스템별 동시 추적 항목 수 audit 추가 (MVP 이후). Moss Baby는 "한 번에 1 카드 선택" — 구조적 저부하 |
| Navigation assists | Standard | N/A | — | 월드 내비게이션 없음 |

**핵심 관찰**: Moss Baby는 Pillar 1 (조용한 존재) + Anti-pillars (game화된 지표 배제)가 cognitive accessibility barrier의 대부분을 **구조적으로 제거**. 이 게임을 "접근하기 어려운" 방식으로 만드는 것이 디자인상 불가능.

---

## Auditory Accessibility

| Feature | Target Tier | Scope | Status | Implementation Notes |
|---------|-------------|-------|--------|---------------------|
| Subtitles for all spoken dialogue | Basic | N/A | — | 음성 대사 0개 |
| Closed captions for gameplay-critical SFX | Comprehensive | VS Audio System | Not Started | VS에서 Audio System #15 구현 시 gameplay-critical SFX (꿈 chime, 카드 건넴) 중 **게임플레이에 영향을 주는** 소리 audit 필요. Moss Baby는 대부분 SFX가 ambient/reinforcing (gameplay-critical 아님 — 시각 backup 이미 존재) |
| Mono audio option | Comprehensive | VS 검토 | Not Started | Ambient 1 + SFX 10개 규모 — mono option 구현 비용 낮음. VS Audio System에서 ini 옵션 추가 가능 |
| Independent volume controls | Basic | Music / SFX / UI audio | VS | 현재 MVP는 placeholder 1 track. VS Audio System + Title&Settings UI에서 4 sliders 구현 |
| Visual representations for directional audio | Comprehensive | N/A | **구조적 부재** | 카메라 고정 + 단일 씬 — 화면 밖 이벤트 없음 |
| Hearing aid compatibility mode | Standard | High-frequency audio cues | VS | VS Audio System 구현 시 ambient drone의 frequency range audit (100Hz-2kHz 권장) |

### Gameplay-Critical SFX Audit (MVP 기준)

| Sound Effect | What It Communicates | Visual Backup | Caption Required | Status |
|-------------|---------------------|--------------|-----------------|--------|
| 카드 건넴 SFX | 건넴 확정 | 카드 페이드 아웃 + MBC Reacting + GSM 상태 전환 (Offering→Waiting) — 시각 충분 | No | Resolved (구조적 — 시각 backup 풍부) |
| 꿈 chime (Dream Journal 알림) | 새 꿈 도착 | 아이콘 펄스 애니메이션 (Dream Journal UI CR-6) + RM OFF 시 정적 하이라이트 | No | Resolved |
| Dream 상태 진입 ambient shift | Dream 상태 시작 | 색온도 2,800K→4,200K 전환 + DoF 강화 + 파티클 선점 — 시각 충분 | No | Resolved |
| Farewell ambient | Day 21 종결 | Vignette 0→0.6 + 색온도 2,200K + 파티클 소멸 — 시각 충분 | No | Resolved |

**결론**: MVP 현재 gameplay-critical audio cue 없음 — 모든 state change가 시각 우선 설계 (Pillar 1 "빛과 색으로만"). VS Audio System 구현 시에도 이 원칙 유지.

---

## Platform Accessibility API Integration

| Platform | API / Standard | Features Planned | Status | Notes |
|----------|---------------|-----------------|--------|-------|
| Steam (PC) | Steam Input + SDL | Controller remapping via Steam Input; Big Picture UI | Not Started | Steam Input이 시스템 레벨 remapping 담당 — in-game UI는 VS에서. Steam Deck compatibility 검증 (OQ-IMP-5 Implementation task) |
| PC (Screen Reader) | Windows Narrator / JAWS | 메뉴 단위 announcements (VS Title&Settings UI) | Out of scope MVP | Moss Baby는 음성 narrative + 시적 텍스트 — screen reader 통합 시 literary nuance 손실 우려. VS에서 Title&Settings UI 메뉴 한정 고려 |
| Xbox / PlayStation | — | — | N/A | 이 프로젝트 미지원 |

---

## Per-Feature Accessibility Matrix

| System | Visual Concerns | Motor Concerns | Cognitive Concerns | Auditory Concerns | Addressed |
|--------|----------------|---------------|-------------------|------------------|-----------|
| Time & Session (#1) | None (인프라 — UI 없음) | None | NarrativeCap (3회 cap) 낮은 cognitive load | None | Resolved |
| Data Pipeline (#2) | None | None | None | None | Resolved |
| Save/Load (#3) | None (silent — Pillar 1) | None | "이끼 아이는 깊이 잠들었습니다" 문구 cognitive load 낮음 | None | Resolved |
| Input Abstraction (#4) | N/A | **Mouse-only 완결성 Rule 5 + Hover-only 금지 Rule 4 + Input mode 자동 전환 Rule 3** | None | None | **Resolved — MVP 구조적 충족** |
| Game State Machine (#5) | **6 GameState 색온도 색각 audit 완료** (Art Bible §4) | None | State 전환 자동 — 플레이어 입력 요구 없음 | None | Resolved |
| Moss Baby Character (#6) | **성장 단계 메시 교체 (색 단독 아님)** | None | Withered 시각 = 쉼 (Pillar 1 — 죄책감 없음) | None | Resolved |
| Growth Accumulation (#7) | None (인프라 — UI 없음) | None | 숫자 없음 (Pillar 2) — cognitive load 최저 | None | Resolved |
| Card System (#8) | 카드 DisplayName/Description 16px+ | Drag-to-offer (CR-CHU-9 Gamepad VS) | 하루 1장 — 최소 선택 부하 | None | Partial (Gamepad VS) |
| Dream Trigger (#9) | None (인프라) | None | 희소성 (3-5회/21일) — 정보 과부하 없음 | None | Resolved |
| Stillness Budget (#10) | **ReducedMotion API (MVP 공개)** | N/A | N/A | ReducedMotion은 Sound 영향 없음 (Rule 6) | **Resolved — 모든 accessibility의 인프라** |
| Lumen Dusk Scene (#11) | **색온도 color-coded but + 광원 각도 + DoF + SSS 복합 시각 cue** | None | 환경 — 인지 요구 없음 | None | Resolved |
| Card Hand UI (#12) | Drag VFX 색 단독 아님 (scale + 파장) + Hover-only 금지 | Hover-only 금지 Rule 4 | 3장 중 1장 — 단순 | None | Resolved |
| Dream Journal UI (#13) | 16px minimum text + RichTextBlock (font adjustable in UMG) | Keyboard + Mouse 완결 (AC-DJ-10) + 이전/다음 버튼 숨김/표시 | **Player-dismissed 읽기 + 통계 없음** (AC-DJ-21) | N/A | Resolved |
| Window & Display (#14) | DPI scaling (Formula 1) + letterbox (Formula 2) | N/A | **모달 금지 Rule 10 + focus 시 pause 없음 (Pillar 4)** | N/A | Resolved |

**결과**: MVP 14 시스템 중 13건 Resolved, 1건 Partial (Card System Gamepad one-hand mode — VS 구현).

---

## Accessibility Test Plan

| Feature | Test Method | Test Cases | Pass Criteria | Responsible | Status |
|---------|------------|------------|--------------|-------------|--------|
| Text contrast ratios | Automated — WCAG contrast analyzer on UI screenshots | Dream Journal body, Card DisplayName, Window 모달(금지) | Body ≥ 4.5:1, Dream Journal ≥ 7:1 | ux-designer | Not Started |
| Colorblind modes (simulator) | Manual — Coblis on 6 GameState screenshots | 6 상태 각 3 modes (Protanopia/Deuteranopia/Tritanopia) | 상태 식별 가능 (색온도 외 광원 각도·DoF·SSS로 구분) | ux-designer | Not Started |
| Input method switching | Manual — Mouse 사용 중 Gamepad 입력 → 자동 전환 확인 | Offering 상태 중 Mouse→Gamepad 전환 | `OnInputModeChanged` 1회 발행 + 프롬프트 아이콘 조용히 교체 (알림 없음) | qa-tester | Not Started |
| Hover-only 금지 | Manual — 카드 위 mouse hover 3초 (클릭 없이) | Offering 상태 | 시각 하이라이트만, 건넴 미발생 (AC-CHU-11) | qa-tester | Not Started |
| Mouse-only completeness | Manual — 키보드·게임패드 미사용, 전체 게임 플레이 | Day 1 → Day 7 MVP 전체 | 모든 기능 접근 가능 (AC-CHU-12) | qa-tester | Not Started |
| Reduced Motion | Manual — `bReducedMotionEnabled = true` + 전체 플레이 | Card Hand UI reveal + transitions, Dream Journal open/close, Lumen Dusk particle | 모든 Motion/Particle 슬롯 Deny, 정적 표시, Sound 정상 | ux-designer | Not Started |
| 최소 창 800×600 가독성 | Manual — 창 크기 800×600, 카드 3장 + Dream Journal open | 모든 UI 화면 | 텍스트 잘림 없음, 카드 겹침 없음 (AC-CHU-01 + AC-DJ-18) | qa-tester | Not Started |
| DPI scaling 144/192 DPI | Manual — 시스템 DPI 150%·200% 환경 | 모든 UI 화면 | Formula 1 `DPIScale` 적용, 레이아웃 유지 (AC-WD-04) | qa-tester | Not Started |
| 모달 절대 금지 | Code review — `SWindow` modal 생성 grep | Save/Load degraded, Window mode change, Input mode change | grep 0건 (AC-WD-13, Pillar 1) | ux-designer | Not Started |

---

## Known Intentional Limitations

| Feature | Tier Required | Why Not Included | Risk / Impact | Mitigation |
|---------|--------------|-----------------|--------------|------------|
| Screen reader for game world | Comprehensive | Moss Baby는 시적 narrative + 음성 없음 — screen reader 구현 시 literary nuance (띄어쓰기·리듬·이미지) 손실 우려 | 전맹 시각장애 플레이어 접근 제한 | VS Title&Settings UI 메뉴 한정 screen reader 고려 (문자 읽기만) |
| Full subtitle customization | Comprehensive | 음성 대사 0개 — 자막 자체가 의미 낮음 | N/A (영향 없음) | N/A |
| Tactile/haptic alternatives | Exemplary | MVP에 Platform haptic API 통합 out of scope | Deaf 플레이어의 audio 대체 경로 부재 (단, Moss Baby는 gameplay-critical audio 0개 — 구조적 불필요) | VS에서 검토, Audio System 구현 시 재평가 |
| Text Scale UI toggle | Standard | Accessibility Layer #18 VS 연기. MVP는 16px fixed | 저시력 플레이어 text 가독성 한계 | 800×600 최소 해상도에서 16px minimum 강제. Window 확대 (1920×1080)에서 DPI scaling 자동 적용 (Formula 1) |
| Colorblind mode UI toggle (3 modes 선택) | Standard | Accessibility Layer #18 VS. MVP는 Art Bible §4 기본 팔레트가 이미 색각 안전 | 특정 color vision 조건에서 미세 톤 차이 감지 저하 가능 | Art Bible §4 팔레트 (이끼 녹·꿀빛·라벤더)가 3 modes 모두에서 distinct하도록 설계됨 + 색 단독 전달 항목 0건 (color-as-only audit Resolved) |

---

## Audit History

| Date | Auditor | Type | Scope | Findings Summary | Status |
|------|---------|------|-------|-----------------|--------|
| 2026-04-19 | Internal — self-review (lean mode) | Initial tier commitment | 본 문서 작성 | Standard tier 확정, 14 MVP 시스템 accessibility matrix 13/14 Resolved | Committed |

---

## External Resources

| Resource | URL | Relevance |
|----------|-----|-----------|
| WCAG 2.1 | https://www.w3.org/TR/WCAG21/ | Text contrast, sizing (body ≥ 4.5:1, AAA 7:1) |
| Game Accessibility Guidelines | https://gameaccessibilityguidelines.com | Cozy genre 섹션 참조 |
| Colour Blindness Simulator (Coblis) | https://www.color-blindness.com/coblis-color-blindness-simulator/ | 6 GameState + Art Bible §4 팔레트 시뮬 도구 |
| AbleGamers Player Panel | https://ablegamers.org/player-panel/ | VS 이후 external audit 시 |

---

## Open Questions

| Question | Owner | Deadline | Resolution |
|----------|-------|----------|-----------|
| Steam Deck에서 Input Abstraction Layer + Always-on-top 상호작용 | ux-designer + unreal-specialist | Implementation OQ-IMP-5 | Implementation 단계 실기 검증 |
| VS Audio System에서 gameplay-critical SFX는 정말 0개로 유지 가능한가? | audio-director (VS) | Audio System GDD 작성 시 | Pillar 1 "빛과 색으로만" 원칙 유지 — caption 불필요 설계 목표 |
| Dream Journal UI screen reader 통합 범위 (메뉴만 vs 본문 포함) | ux-designer | VS Title&Settings UI 작성 시 | 본 문서 §Known Intentional Limitations 1번 관련 — 메뉴 한정으로 결정 중 |
