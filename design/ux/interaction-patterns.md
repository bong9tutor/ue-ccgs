# Interaction Pattern Library: Moss Baby

> **Status**: Draft (2026-04-19 — initialized per `/gate-check` Technical Setup → Pre-Production 요구)
> **Author**: ux-designer (self, lean mode)
> **Linked Documents**: `design/gdd/input-abstraction-layer.md`, `design/gdd/card-hand-ui.md`, `design/gdd/dream-journal-ui.md`, `design/gdd/window-display-management.md`, `design/accessibility-requirements.md`, `design/art/art-bible.md §7`

> **Purpose**: 이 문서는 Moss Baby의 **cross-screen UX 패턴**을 정의한다. 개별 screen UX spec (`design/ux/[screen-name].md`)이 "이 screen이 어떻게 보이고 동작하는가"를 정의한다면, 이 library는 "모든 screen이 공유하는 인터랙션 원칙"을 정의한다. 새 UI 추가 시 이 library를 먼저 읽을 것.

---

## Core Principles (Pillar 연결)

| Principle | Pillar | 강제 장소 |
|---|---|---|
| **조용한 상태 전환** — 모달·팝업·알림 없음 | Pillar 1 | Window&Display Rule 10, Input OnInputModeChanged |
| **명시적 선택만** — Hover로 상태 변경 금지 | Pillar 1 | Input Rule 4 |
| **숫자 없는 UI** — 수치·확률·태그 레이블 표시 금지 | Pillar 2 | Card Hand UI AC-CHU-03, Dream Journal UI AC-DJ-21 |
| **희소한 응답** — 간헐적 시각 큐만 | Pillar 3 | Dream Journal 아이콘 펄스, GSM MPC blend |
| **Player-dismissed** — auto-dismiss UI 금지 | Pillar 1 | Dream Journal 이전/다음 버튼 |

---

## Pattern 1 — Drag-to-Offer (카드 건넴 의식)

**Intent**: 카드 선택 → 건넴의 물리적 제스처가 "건넴"이라는 의식적 행위로 느껴지게 한다.

**When to use**: 단 한 곳 — Card Hand UI의 카드 건넴. 다른 UI에서 드래그 인터랙션 도입 금지 (의식의 희소성).

**Input binding**:
- Mouse: LMB Hold (`OfferDragThresholdSec` 0.15s) → drag 시작 → Offer Zone (유리병 근처 80px 반지름) 내 LMB 릴리스 → 건넴 확정
- Gamepad (VS): D-pad 네비게이션으로 카드 포커스 → A Hold (`OfferHoldDurationSec` 0.5s) → 건넴 확정 (Offer Zone 판정 없음)

**Key invariants**:
- Hold 미달 릴리스 → `IA_Select` Released로 처리 (카드 밝아짐 → 선택 강조만, 건넴 아님)
- 드래그 중 Offer Zone 밖 릴리스 → 카드 원래 위치로 `CardReturnDurationSec` 0.3s 복귀 (EC-CHU-1)
- `ConfirmOffer(CardId)` false 반환 시 동일 복귀 (EC-CHU-2 — 검증 실패)
- 드래그 중 창 포커스 상실 → Hold 자동 취소 + 카드 복귀 (EC-CHU-3)
- **드래그 중 다른 카드 드래그 시도 무시** (EC-CHU-8 — 한 번에 1 카드)

**Visual feedback**:
- 드래그 카드: 커서 추적, 밝아짐 (EmissiveStrength +0.15)
- Offer Zone 진입: 카드 하이라이트 + Zone 파장 효과 (Stillness Budget `Particle` `Standard` 슬롯)
- 건넴 확정: 0.5s 페이드 아웃 + 유리병 방향 이동 + MBC Reacting 반응

**Do / Don't**:
- ✅ Do: 드래그 제스처의 물리성을 강화 (커서 이동에 즉각 반응)
- ✅ Do: Offer Zone 시각 피드백을 명확하게 (건넴 가능 vs 취소 경계)
- ❌ Don't: "건넴 진행률" progress bar 표시 (Pillar 2 "숫자 없음")
- ❌ Don't: "이 카드를 정말 건네시겠습니까?" 확인 모달 (Pillar 1)
- ❌ Don't: 드래그 외 "클릭 건넴" 대체 경로 (건넴은 의식 — 단일 제스처 유지)

**Related**: Card Hand UI §CR-CHU-2, §CR-CHU-3, §CR-CHU-4, §CR-CHU-5

---

## Pattern 2 — Hover-Only Prohibition (Hover로 상태 변경 금지)

**Intent**: 모든 게임 상태 변경은 **명시적 Select/Confirm**으로만 일어난다. Hover는 시각 피드백 전용.

**When to use**: 모든 interactive UI 요소 (카드, 버튼, 아이콘).

**Implementation rule (Input Abstraction Rule 4)**:
- Hover → 시각 하이라이트 OK (tooltip, 카드 밝아짐)
- Hover만으로 상태 변경 금지 — 카드 자동 선택, 버튼 자동 실행, 페이지 자동 넘김 등 금지

**Rationale**: Steam Deck 컨트롤러 모드에서 hover 불가능 (cursor 숨김). Hover-only UI는 Input method 전환 시 완전 차단됨 → Mouse-only completeness (Rule 5) 위반.

**Do / Don't**:
- ✅ Do: Hover 시 tooltip, highlight, breathing scale 허용
- ❌ Don't: Hover 3초로 카드 자동 선택 (AC-CHU-11 grep 금지)
- ❌ Don't: Hover로 Dream Journal 아이콘 자동 열기

**Related**: Input Abstraction §Rule 4, Card Hand UI AC-CHU-11

---

## Pattern 3 — Hold vs Select 공유 키 (Select/OfferCard Implicit Trigger)

**Intent**: 동일 물리 키가 context별로 다른 의미를 가지나 UE Enhanced Input의 Implicit Trigger 우선순위로 결정적 처리.

**Binding structure**:
- Mouse LMB: `IA_Select` (Released, Hold 미달) OR `IA_OfferCard` (Hold Triggered ≥0.15s)
- Gamepad A: `IA_Select` (Released, Hold 미달) OR `IA_OfferCard` (Hold Triggered ≥0.5s)

**Implementation rule**:
- Hold 성립 시 Released 억제 (UE Enhanced Input이 자동 처리)
- 카드 위에서 짧은 클릭 → 카드 선택 강조 (시각만, 건넴 아님)
- 카드 위에서 긴 클릭 → drag 시작

**Do / Don't**:
- ✅ Do: 동일 키의 short-press vs long-press 의미 분리
- ❌ Don't: 별도 "Select 모드" / "Offer 모드" toggle 추가 (복잡도)

**Related**: Input Abstraction §Rule 2a, Formula 2 Hold Duration

---

## Pattern 4 — Silent State Transitions (상태 전환은 조용하게)

**Intent**: 모든 게임 상태 전환 (GSM 6 states, Input mode, Window focus)은 **빛·색·아이콘 교체**로만 표현. 알림·팝업·모달·sound alert 금지.

**When to use**: 모든 state change 발생 지점.

**Implementation rule (cross-system)**:
- GSM state transition: MPC scalar Lerp → 머티리얼 자동 반영 + Light Actor Temperature (ADR-0004 hybrid)
- Input mode 전환 (Mouse ↔ Gamepad): `OnInputModeChanged` delegate → UI 프롬프트 아이콘 조용히 교체
- Window focus loss/gain: 게임 pause 없음, 알림 없음 (Window&Display Rule 6)
- Save/Load degraded: silent rollback — 에러 다이얼로그 절대 없음 (AC-DJ-17 grep 강제)

**Do / Don't**:
- ✅ Do: 빛·색·카메라 DoF·vignette로 상태 변화 전달
- ✅ Do: 아이콘 shape·크기·emissive pulse로 신호
- ❌ Don't: "Waiting 상태로 전환되었습니다" 문자 알림
- ❌ Don't: state transition 시 sound cue (단, state 자체가 의도한 ambient 변화는 OK)
- ❌ Don't: "게임패드 감지됨" 팝업

**Related**: Window&Display Rule 6, Input Abstraction Rule 3, Save/Load §Visual/Audio Requirements, Pillar 1 전역

---

## Pattern 5 — Modal Prohibition (모달 절대 금지)

**Intent**: 플레이어의 플레이를 block하는 dialog 생성 자체 금지. 선택은 항상 비-차단적으로.

**When to use**: 어떠한 경우에도 모달 생성 금지 — error, confirmation, settings change, farewell.

**Implementation rule (Window&Display Rule 10)**:
- 창 모드 변경: 즉시 적용, 확인 모달 없음 (AC-WD-13)
- Save/Load 양 슬롯 invalid: `FOnLoadComplete(bFreshStart=true, bHadPreviousData=true)` → Title UI가 "이끼 아이는 깊이 잠들었습니다" **inline 분기** (모달 아님)
- LONG_GAP 자동 farewell: `OnFarewellReached(LongGapAutoFarewell)` → 정상 Farewell 화면 (모달 아님)
- Day 21 종결: 자동 Farewell 전환 — "게임을 종료하시겠습니까?" 모달 없음

**Do / Don't**:
- ✅ Do: 선택이 필요하면 별도 화면 전환 (GSM state로 표현)
- ❌ Don't: `UBlueprintFunctionLibrary::ShowMessageBox()` 류 모달 생성
- ❌ Don't: "설정이 저장되었습니다" 팝업

**Related**: Window&Display Rule 10, Save/Load §Visual/Audio, Pillar 1 전역

---

## Pattern 6 — Reduced Motion Fallback

**Intent**: `IsReducedMotion() == true` 시 모든 motion/particle 효과를 즉시 적용 상태로 대체. 정보 전달은 보존, 애니메이션은 생략.

**When to use**: 모든 Stillness Budget `Motion` / `Particle` 채널 소비자.

**Standard consumer pattern**:
```cpp
FStillnessHandle Handle;
if (auto* Budget = GetStillnessBudget()) {
    if (Budget->IsReducedMotion()) {
        ApplyTargetValueImmediate();  // Lerp 생략, 최종 상태 즉시 적용
        return;
    }
    Handle = Budget->Request(EStillnessChannel::Motion, EStillnessPriority::Standard);
    if (!Handle.bValid) {
        ApplyTargetValueImmediate();  // Budget Deny — 즉시 적용
        return;
    }
    StartLerpAnimation(OnComplete: [&Handle]{ Budget->Release(Handle); });
}
```

**Systems that must implement this pattern**:
- Card Hand UI: Reveal staggered → 즉시 표시, Dragging 카드 복귀 → 즉시, Offering 확정 → 즉시 Hide (AC-CHU-13)
- Dream Journal UI: Opening/Closing 트랜지션 → 즉시 전환, 페이지 넘김 → 즉시, 아이콘 펄스 → 정적 하이라이트 (AC-DJ-19)
- MBC: GrowthTransition → V_target 즉시, 카드 Reacting → Emission 즉시 설정 없음 (Motion 범주에서 제외 — state 값 유지) (AC-MBC-11)
- Lumen Dusk Scene: 앰비언트 파티클 미생성 (AC-LDS-15), 조명 Lerp → 즉시 적용 (E6)
- GSM: MPC Lerp 중 `bReducedMotionEnabled = true` 토글 → 즉시 목표값 적용 (AC-GSM-10)

**Do / Don't**:
- ✅ Do: Sound 효과는 ReducedMotion 영향 없음 (청각 접근성 독립 — Rule 6)
- ✅ Do: 최종 상태값은 보존 (e.g., Dream의 이끼 아이 빛남 SSS는 유지, 전환 애니메이션만 생략)
- ❌ Don't: ReducedMotion ON에서 애니메이션 부분 축소 (풀 생략 또는 유지 — 중간 없음)

**Related**: Stillness Budget §Rule 6 + F2, Accessibility Requirements §Motion/animation reduction mode

---

## Pattern 7 — Silent Rollback (세이브 실패 침묵 복원)

**Intent**: Save/Load 실패는 플레이어에게 절대 노출되지 않는다. 직전 정상 상태로 silent 복원.

**When to use**: Save/Load의 모든 failure case.

**Implementation rule (Save/Load §Visual/Audio Requirements)**:
- Save worker thread 실패: 재시도 후 degraded flag (silent — 다음 정상 저장 시 해제)
- 단일 슬롯 손상 (CRC mismatch): 반대 슬롯 fallback (AC E8-E9)
- 양 슬롯 손상: `bHadPreviousData=true` 신호 → Title UI "이끼 아이는 깊이 잠들었습니다" 화면 (모달 아님 — inline 분기)
- 디스크 풀 3회 연속: `bSaveDegraded = true` 플래그 (silent)
- 어떤 경우에도: 에러 다이얼로그, "저장 실패", 재시도 버튼 금지

**Do / Don't**:
- ✅ Do: 진단 로그에 세부 정보 기록 (`UE_LOG`)
- ✅ Do: 직전 정상 슬롯으로 자동 복원
- ❌ Don't: "세이브 데이터가 손상되었습니다" 다이얼로그 (AC-WD-13, NO_SAVE_INDICATOR_UI)
- ❌ Don't: 진행 인디케이터 ("저장 중...") 표시 (AC NO_SAVE_INDICATOR_UI)

**Related**: Save/Load §Player Fantasy + §Visual/Audio Requirements, Pillar 1 전역

---

## Pattern 8 — 조용한 아이콘 알림 (Quiet Icon Indicator)

**Intent**: 새 꿈 도착 같은 간헐적 이벤트를 **single icon pulse**로만 전달. Badge 숫자·팝업·sound cue 금지.

**When to use**: 현재 유일한 사례 — Dream Journal 아이콘 (CR-6).

**Implementation rule (Dream Journal UI CR-6)**:
- `bHasUnseenDream = true` 시 아이콘 1회 펄스 애니메이션 (Stillness Budget `Motion Standard` 슬롯)
- 펄스는 플레이어가 일기를 열 때까지 지속 (다음 세션까지도 유지 — CR-7 `bHasUnseenDream` 세이브 영속화)
- 플레이어가 꿈 페이지 열람 → `bHasUnseenDream = false` in-memory (다음 저장 시 영속 반영)

**Visual**:
- Badge 숫자 ("새 꿈 3개") 금지 — Pillar 3 "희소성 보존"
- Toast 알림 금지 — Pillar 1
- 진동 알림 금지 — 플랫폼 호환 + Pillar 1
- ReducedMotion ON: 정적 하이라이트로 대체 (AC-DJ-14)

**Do / Don't**:
- ✅ Do: 간헐적 이벤트를 아이콘 shape·크기·emissive로 신호
- ❌ Don't: "새 꿈 X개" 숫자 표시 (AC-DJ-21 grep 금지)
- ❌ Don't: 화면 상단 toast 배너

**Related**: Dream Journal UI §CR-6 + §CR-7, Pillar 3

---

## Pattern 9 — Player-Dismissed Reading (auto-dismiss 금지)

**Intent**: 시적 narrative (꿈 텍스트)는 플레이어가 원하는 만큼 읽을 수 있어야 함. 자동 닫힘·타이머 금지.

**When to use**: Dream Journal 꿈 페이지, 미래 narrative UI.

**Implementation rule (Dream Journal UI §CR-3)**:
- 꿈 페이지 표시 → 플레이어 닫기(ESC/닫기 버튼)까지 지속
- 이전/다음 버튼 상시 표시 (숨김 or 활성/비활성 — 단, 숫자 "페이지 1/5" 금지)
- Dream 상태 이탈 (GSM state change)로 강제 닫기는 가능하나 이는 state 자체의 종결 (CR-1 Access Control)

**Do / Don't**:
- ✅ Do: player-controlled close button
- ✅ Do: 키보드 ESC + UI 닫기 버튼 둘 다 지원 (Mouse-only completeness)
- ❌ Don't: "5초 후 자동으로 닫힙니다" 카운트다운 (Pillar 1)
- ❌ Don't: 페이지 자동 넘김

**Related**: Dream Journal UI §CR-3, §CR-4

---

## Pattern 10 — Focus-Independent Session Tracking

**Intent**: 게임은 창 포커스와 **무관**하게 시간을 추적한다. Alt-Tab, 창 최소화, 다른 앱 위 작업 중에도 Time & Session 1Hz tick 계속.

**When to use**: Time & Session System의 tick 로직.

**Implementation rule (Time §States + architecture §Data Flow §3.2)**:
- `FTSTicker` 사용 — engine-level ticker, pause-independent
- `FTimerManager` 사용 금지 — World 귀속 + pause 시 정지
- 창 focus loss (WM_ACTIVATE LOSS) → Time `Active` 상태 유지 + tick 계속 (AC `FOCUS_LOSS_CONTINUES_TICK`)
- OS sleep (PBT_APMSUSPEND) → Time `Suspended` 상태, tick 일시정지 (복귀 시 재분류)

**Rationale**: Pillar 4 "이끼 아이가 살아낸 시간" — 플레이어 출석 여부와 무관.

**Do / Don't**:
- ✅ Do: 모든 실시간 추적 logic에 FTSTicker 사용
- ❌ Don't: 창 minimize 시 게임 pause
- ❌ Don't: 창 focus 상실 시 "돌아오세요" notification (Pillar 1)

**Related**: Time §Window Focus vs OS Sleep, Window&Display §Rule 6

---

## Pattern 11 — Cross-Resolution Scale Preservation

**Intent**: 800×600 최소 해상도부터 4K까지 UI + 씬이 일관된 비율로 보이고 판독 가능.

**When to use**: 모든 UMG 위젯, Lumen Dusk Scene 카메라 설정.

**Implementation rule**:
- UMG: UE DPI scaling (`UUserInterfaceSettings::GetDPIScaleBasedOnSize()`) 활용. 최소 폰트 16px @ 800×600 (Dream Journal F-DJ-1)
- Scene: 4:3 고정 카메라 비율 + 16:9 와이드에서 letterbox (Window&Display Formula 2 — `SceneAspectRatio 4:3`)
- 카드 배치: F-CHU-1 `SlotX(i) = ScreenCenterX + (i-1) × (CardWidth + Spacing)` — 비율 기반
- DPI 스케일 보정: 마우스 Offer Zone 반지름은 DPI 스케일 무관 픽셀 절대값 (OQ-CHU-3 — 검토 중)

**Do / Don't**:
- ✅ Do: UMG anchor 기반 스케일링
- ✅ Do: 최소 해상도 (800×600)에서 수동 검증 (AC-CHU-01, AC-DJ-18)
- ❌ Don't: 하드코딩된 픽셀 좌표 (Tuning Knob으로 외부화)
- ❌ Don't: 800×600 미만 창 허용 (Window&Display Rule 2 + OS level 강제)

**Related**: Window&Display Rule 5 + Formula 1 + Formula 2, Dream Journal F-DJ-1, Card Hand UI F-CHU-1

---

## Pattern Adoption Checklist (신규 UI 추가 시)

새로운 UI screen 또는 interactive 요소 추가 시 다음 checklist 통과 확인:

- [ ] Pattern 1 Drag-to-Offer: 이 screen이 drag 인터랙션을 추가하는가? → Card Hand UI 외 도입 불가 (의식의 희소성)
- [ ] Pattern 2 Hover-Only Prohibition: 모든 상태 변경이 명시적 Select로? Hover는 시각만?
- [ ] Pattern 4 Silent State Transitions: 상태 전환 시 알림·모달·팝업 없음?
- [ ] Pattern 5 Modal Prohibition: 모달 다이얼로그 생성 0건?
- [ ] Pattern 6 Reduced Motion Fallback: Stillness Budget 사용 시 RM OFF 분기 구현?
- [ ] Pattern 8 Quiet Icon Indicator: 이벤트 신호 시 숫자·팝업·sound 금지?
- [ ] Pattern 9 Player-Dismissed: auto-dismiss timer 없음?
- [ ] Pattern 11 Cross-Resolution: 800×600 + 1920×1080 + 3840×2160 검증?

---

## Related Documents

- `design/gdd/input-abstraction-layer.md` — 6 InputActions + Rule 4 Hover 금지
- `design/gdd/card-hand-ui.md` — Pattern 1 + Pattern 3 구현
- `design/gdd/dream-journal-ui.md` — Pattern 8 + Pattern 9 구현
- `design/gdd/window-display-management.md` — Pattern 4 + Pattern 5 + Pattern 11
- `design/gdd/stillness-budget.md` — Pattern 6 인프라
- `design/gdd/save-load-persistence.md` — Pattern 7 인프라
- `design/gdd/time-session-system.md` — Pattern 10 인프라
- `design/accessibility-requirements.md` — Standard tier commitment, cross-reference
- `design/art/art-bible.md §7 UI/HUD Visual Direction` — 시각 문법
- `docs/architecture/architecture.md §Architecture Principles` — Principle 1 (Pillar-First) + Principle 4 (Layer Boundary)

---

## Open Questions

| Question | Owner | Resolves In |
|---|---|---|
| Pattern 11 Offer Zone DPI 보정 | ux-designer + ue-umg-specialist | Card Hand UI OQ-CHU-3 구현 단계 |
| Pattern 10 Time tick이 `bDisableWorldRendering = true` (최소화) 상태에서도 진행? | unreal-specialist | Window&Display OQ-2 Implementation |
| Pattern 3 Gamepad Hold 0.5s vs Mouse Hold 0.15s 차이가 "의식 품질"에 동일하게 느껴지는가? | game-designer | VS Playtest |
| VS 단계에서 추가 Pattern 필요 (Audio cue, Text Scale, Colorblind toggle UI)? | ux-designer | Title & Settings UI GDD 작성 시 |
