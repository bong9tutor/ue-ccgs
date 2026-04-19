# Input Abstraction Layer

> **Status**: In Review (R1 수정 완료 2026-04-17, 재리뷰 대기)
> **Author**: bong9tutor + claude-code session
> **Last Updated**: 2026-04-17
> **Implements Pillar**: Pillar 1 (조용한 존재 — Hover-only 금지, 알림 팝업 금지), Pillar 2 (선물의 시학 — 카드 건넴의 감각적 품질)
> **Priority**: MVP | **Layer**: Foundation | **Category**: Core

## Overview

Input Abstraction Layer는 UE 5.6 Enhanced Input System 위에 놓이는 **의미적 입력 계층**이다. 하드웨어 입력(마우스, 키보드, 게임패드)을 6개의 **게임 액션**(`IA_Select`, `IA_OfferCard`, `IA_Back` 등)으로 변환하고, 입력 장치에 따른 매핑 컨텍스트(`IMC_MouseKeyboard`, `IMC_Gamepad`)를 관리한다. Downstream 시스템(Card Hand UI, Window & Display, Dream Journal UI)은 원시 키 입력이 아닌 게임 액션만 구독하므로, 입력 장치가 바뀌어도 게임플레이 코드는 변경되지 않는다.

플레이어는 이 시스템을 의식하지 않는다. 그러나 이 계층 없이는 (1) Mouse와 Gamepad 간 매끄러운 전환, (2) Steam Deck 호환, (3) Hover-only 상호작용 금지(Pillar 1) 강제가 각 UI 시스템에 분산되어 일관성이 깨진다.

## Player Fantasy

플레이어는 이 시스템을 "입력 추상화"로 인식하지 않는다. 경험하는 것은 결과뿐이다: **카드를 집어 건네는 동작이 마우스든 게임패드든 동일하게 자연스럽다**는 것. 키보드 단축키를 모르더라도 마우스만으로 모든 상호작용이 완결되고, Steam Deck을 집어 들면 컨트롤러로도 같은 의식을 치를 수 있다.

이 시스템이 지키는 감각: **"이 게임은 나에게 아무것도 외우라고 하지 않았다."** 복잡한 키바인딩이나 입력 모드 전환의 마찰이 없는 것이 Pillar 1(조용한 존재)의 입력 계층 표현이다.

**Pillar 연결**:
- **Pillar 1 (조용한 존재)** — Hover-only 금지. 입력 장치 전환 시 알림·팝업 없음. 게임패드 없어도 완전한 경험
- **Pillar 2 (선물의 시학)** — 카드 건넴의 물리적 제스처(클릭-드래그 또는 버튼 홀드)가 "건넨다"는 의식적 행위로 느껴져야 함. 이 감각의 기반은 Input이 제공하고, 구체적 구현은 Card Hand UI가 소유

## Detailed Rules

### Core Rules

**Rule 1 — Game Action 정의**: 모든 플레이어 입력은 아래 6개 Game Action 중 하나로 변환된다. Raw key/button 이벤트를 직접 구독하는 게임플레이 코드는 금지.

| Action | ID | ValueType | 설명 | 주 소비자 |
|---|---|---|---|---|
| Pointer Move | `IA_PointerMove` | `Axis2D` | 커서/포인터 위치 변화 | Card Hand UI |
| Select | `IA_Select` | `bool` | 카드 선택, UI 확인. OfferCard와 동일 키 공유 — Hold 미달 릴리스 시 발화 | Card Hand UI, Dream Journal UI |
| Offer Card | `IA_OfferCard` | `bool` (Hold trigger, 컨텍스트별 threshold 상이) | 카드 건넴 — Mouse: LMB Hold(0.15s) → drag 시작, Gamepad: A Hold(0.5s) → 건넴 확정. Card Hand UI가 spatial logic 소유 | Card Hand UI |
| Navigate UI | `IA_NavigateUI` | `Axis2D` | UI 요소 간 이동 (Gamepad D-pad/스틱, 키보드 Arrow Keys) | Card Hand UI, Dream Journal UI |
| Open Journal | `IA_OpenJournal` | `bool` | 꿈 일기 토글 | Dream Journal UI |
| Back | `IA_Back` | `bool` | 맥락별 뒤로가기: UI 열림 → 닫기, 최상위 → 메뉴 토글, 메뉴 내 → 뒤로. GSM 라우팅 | GSM, Dream Journal UI, Title UI |

**Rule 2 — 매핑 컨텍스트**: 두 개의 `UInputMappingContext`가 존재한다.

| Context | Priority | 활성 조건 | MVP 상태 |
|---|---|---|---|
| `IMC_MouseKeyboard` | 1 (기본) | 마우스/키보드 입력 감지 시 | **구현** |
| `IMC_Gamepad` | 1 (기본) | 게임패드 입력 감지 시 | **구조만 정의, VS 구현** |

두 컨텍스트는 **동시 활성** — Priority 동일, 충돌 해소는 "마지막 입력 장치 우선" (Rule 3).

**Rule 2a — 기본 키 바인딩**: 각 매핑 컨텍스트의 기본 하드웨어 바인딩. 에디터에서 `UInputMappingContext` 에셋으로 구성.

**`IMC_MouseKeyboard`**:

| Action | Key | Trigger | Notes |
|---|---|---|---|
| `IA_PointerMove` | Mouse XY | 매 프레임 | Axis2D 연속 |
| `IA_Select` | LMB | Released (Hold 미달 시) | `OfferDragThresholdSec` 미만 릴리스 = Select |
| `IA_OfferCard` | LMB | Hold (`OfferDragThresholdSec` = 0.15s) | Hold Started → Card Hand UI drag 시작 |
| `IA_NavigateUI` | Arrow Keys | Axis2D | 키보드 접근성 보조. 마우스는 PointerMove 사용 |
| `IA_OpenJournal` | J | Pressed | 편의 단축키. UI 버튼으로도 접근 가능 |
| `IA_Back` | Esc | Pressed | GSM 맥락별 라우팅 |

**`IMC_Gamepad`** (MVP: 구조 정의만, VS에서 구현):

| Action | Key | Trigger | Notes |
|---|---|---|---|
| `IA_PointerMove` | — | — | Gamepad 모드: 커서 숨김, NavigateUI로 대체 |
| `IA_Select` | A | Released (Hold 미달 시) | `OfferHoldDurationSec` 미만 릴리스 = Select |
| `IA_OfferCard` | A | Hold (`OfferHoldDurationSec` = 0.5s) | Hold Completed = 건넴 확정 |
| `IA_NavigateUI` | D-pad / Left Stick | Axis2D | UI 포커스 이동 |
| `IA_OpenJournal` | Y | Pressed | |
| `IA_Back` | B | Pressed | GSM 맥락별 라우팅 |

> **Select/OfferCard 공유 키 해소**: LMB(Mouse)와 A(Gamepad) 모두에 `IA_Select`(Released)와 `IA_OfferCard`(Hold)가 바인딩됨. UE Enhanced Input의 Implicit Trigger 우선순위가 Hold threshold 판정 후 Released 발화 여부를 결정 — Hold 성립 시 Released 억제.

**Rule 3 — 입력 모드 자동 감지**: 마지막으로 사용된 입력 장치를 `EInputMode { Mouse, Gamepad }` enum으로 추적. 전환 시:
- `OnInputModeChanged(EInputMode NewMode)` delegate 발행
- **플레이어 대면 알림 없음** (Pillar 1). UI 아이콘(프롬프트 표시 등)만 조용히 교체
- 전환 감지: 마우스 이동 → `Mouse`, 게임패드 버튼/스틱 → `Gamepad`
- **Hysteresis**: 마우스 미세 진동(1px 이하)으로 모드 전환 방지. 최소 이동 거리 `InputModeMouseThreshold`(기본 3px) 초과 시에만 Mouse 모드로 전환

**Rule 4 — Hover-only 금지 강제**: 모든 상호작용은 **명시적 Select/Confirm 동작**(클릭, 버튼 프레스)으로 완결되어야 한다.
- Hover 상태(마우스 올림)는 **시각 피드백**(하이라이트, 툴팁)에만 사용 가능
- Hover 상태만으로 게임 상태를 변경하는 동작은 금지 (예: hover로 카드 자동 선택)
- 이 규칙은 Card Hand UI, Dream Journal UI 등 모든 downstream에 적용되는 **설계 강제**

**Rule 5 — Mouse-only 완결성**: 키보드 단축키(`J`, `Esc`)는 편의 기능. 마우스 클릭만으로 모든 게임 기능에 접근 가능해야 한다. `IA_OpenJournal`과 `IA_Back`은 UI 버튼을 통해서도 접근 가능 (해당 UI GDD 책임). 즉, 일기 열기·메뉴 진입·UI 닫기 모두 마우스 클릭 경로가 존재해야 함.

### States and Transitions

| 상태 | 설명 | 진입 조건 | 유효 전환 |
|---|---|---|---|
| `Mouse` | 마우스/키보드 활성. 커서 표시, 게임패드 프롬프트 숨김 | 기본 상태 / 마우스 이동 `> InputModeMouseThreshold` | → `Gamepad` (게임패드 입력 감지) |
| `Gamepad` | 게임패드 활성. 커서 숨기거나 UI 포커스 네비게이션으로 대체, 게임패드 프롬프트 표시 | 게임패드 버튼/스틱 입력 | → `Mouse` (마우스 이동 감지) |

- 전환은 **즉시** (프레임 내). 딜레이·확인·모달 없음 (Pillar 1)
- `OnInputModeChanged` delegate로 UI 시스템에 전파
- MVP에서는 `Mouse` 상태만 사용. `Gamepad` 상태 진입은 VS부터

> **구현 참고 (UE 5.6)**: `APlayerController`에서 `UEnhancedInputLocalPlayerSubsystem` 통해 매핑 컨텍스트 관리. 입력 모드 감지는 `UEnhancedPlayerInput::GetLastInputDeviceType()` 또는 raw input event의 `FInputDeviceId` 활용. 커서 표시/숨김은 `APlayerController::SetShowMouseCursor()`.

### Interactions with Other Systems

#### 1. Card Hand UI (#12) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Input 제공 | `IA_Select` (bool, Released) | 카드 클릭/선택 시 (Hold 미달 릴리스) |
| Input 제공 | `IA_OfferCard` (bool, Hold Triggered) | 카드 건넴 제스처 시작 (Mouse: drag, Gamepad: hold 완료) |
| Input 제공 | `IA_PointerMove` (Axis2D) | 매 프레임 커서 위치 |
| Input 제공 | `IA_NavigateUI` (Axis2D) | Gamepad UI 네비게이션 |
| Input 이벤트 | `OnInputModeChanged(EInputMode)` | 입력 장치 전환 시 — UI 프롬프트 아이콘 교체 |

- Card Hand UI는 `IA_OfferCard`의 Hold Triggered를 drag-to-offer 인터랙션의 시작으로 매핑. **구체적 드래그 로직은 Card Hand UI 소유**
- Mouse: LMB Hold(`OfferDragThresholdSec` = 0.15초) → drag 시작. Card Hand UI가 릴리스 위치로 건넴/취소 판정
- Gamepad: A Hold(`OfferHoldDurationSec` = 0.5초) → 건넴 확정
- 두 threshold 모두 이 GDD의 Tuning Knob (§Tuning Knobs 참조)

#### 2. Window & Display Management (#14) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Input 이벤트 | `OnInputModeChanged(EInputMode)` | 창 포커스 연동 시 커서 가시성 결정 |

- Window & Display는 focus loss/gain 이벤트를 자체적으로 OS API에서 수신 (Time GDD §Window Focus 참조). Input은 입력 장치 모드만 전달

#### 3. Dream Journal UI (#13) — 아웃바운드

| 방향 | 데이터 | 시점 |
|---|---|---|
| Input 제공 | `IA_OpenJournal` (bool, Triggered) | 일기 토글 |
| Input 제공 | `IA_Back` (bool, Triggered) | 일기 닫기 |
| Input 제공 | `IA_NavigateUI` (Axis2D) | 일기 페이지 넘기기 (Gamepad) |

#### 4. Game State Machine (#5) — 아웃바운드 (간접)

| 방향 | 데이터 | 시점 |
|---|---|---|
| Input 제공 | `IA_Back` (bool, Triggered) | 맥락별: 메뉴 토글 / UI 닫기 / 뒤로 |
| Input 이벤트 | `OnInputModeChanged(EInputMode)` | GSM이 UI 어댑테이션에 활용 가능 |

- GSM은 `IA_Back`을 맥락별로 라우팅: 최상위 게임 뷰 → `Menu` 상태 전환, UI 열림 → 닫기, 메뉴 내 → 뒤로
- Input은 GSM 상태를 직접 읽지 않음 (단방향)

## Formulas

### Formula 1 — Input Mode Hysteresis

마우스 미세 진동에 의한 불필요한 모드 전환 방지.

`ShouldSwitchToMouse = (MouseDelta > InputModeMouseThreshold) AND (CurrentMode == Gamepad)`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| MouseDelta | ΔM | `float` (pixels) | `[0, +∞)` | 이전 프레임 대비 마우스 이동 거리 (screen space) |
| InputModeMouseThreshold | T | `float` (pixels, const) | 3.0 (기본) | §Tuning Knob |
| CurrentMode | — | `EInputMode` | `{Mouse, Gamepad}` | 현재 입력 모드 |

**Output**: `true` → `EInputMode::Mouse` 전환 + `OnInputModeChanged` 발행. `false` → 유지.
**Example**: 게임패드 모드에서 마우스가 1px 움직임 → 3.0 미만 → 유지. 5px 움직임 → 전환.

### Formula 2 — Offer Hold Duration

카드 건넴 제스처의 Hold 판정 시간. **컨텍스트별 threshold가 다르다**:
- **Mouse**: `OfferDragThresholdSec` (0.15s) — 클릭과 드래그 의도를 구분하는 최소 시간
- **Gamepad**: `OfferHoldDurationSec` (0.5s) — "의식적 건넴"의 무게감 (Pillar 2)

`OfferTriggered = (HoldElapsed ≥ HoldThreshold)`

여기서 `HoldThreshold`는 컨텍스트에 따라 `OfferDragThresholdSec` 또는 `OfferHoldDurationSec`.

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| HoldElapsed | t | `float` (seconds) | `[0, +∞)` | 바인딩 키 누르기 시작 후 경과 시간 |
| OfferDragThresholdSec | D | `float` (seconds, const) | 0.15 (기본) | Mouse 전용. §Tuning Knob |
| OfferHoldDurationSec | H | `float` (seconds, const) | 0.5 (기본) | Gamepad 전용. §Tuning Knob |

**Output**: `true` → `IA_OfferCard` Triggered 이벤트 발행. Mouse에서는 drag 시작 (Card Hand UI가 이후 spatial logic 관리), Gamepad에서는 건넴 확정. `false` → 대기 또는 취소(릴리스 시 `IA_Select` Released로 처리).
**Example (Mouse)**: LMB 0.1초 후 릴리스 → Select. LMB 0.15초 이상 유지 → OfferCard Triggered (drag 시작).
**Example (Gamepad)**: A버튼 0.3초 후 릴리스 → Select. 0.5초 이상 유지 → 건넴 확정.

> **구현 참고**: UE Enhanced Input의 `UInputTriggerHold`가 이 로직을 기본 제공. `HoldTimeThreshold` 프로퍼티에 각 컨텍스트의 threshold 값 매핑. 별도 코드 구현 불필요.

> **Formula 1과의 경계 비대칭**: Formula 1(`ShouldSwitchToMouse`)은 `>` (strict — 정확히 threshold = 전환 안 됨), Formula 2는 `≥` (inclusive — 정확히 threshold = 트리거). 의도적 설계: 입력 모드 전환은 보수적(경계에서 유지), Hold 판정은 관대(경계에서 인정).

## Edge Cases

- **E1 — 게임패드 연결 없이 시작**: `Mouse` 모드 기본. `IMC_Gamepad`는 등록되어 있으나 입력 미감지 → 기능적 영향 없음
- **E2 — 세션 중 게임패드 연결/분리**: 연결 시 입력 대기 (모드 전환 안 함 — 실제 입력이 있을 때만 전환). 분리 시 `Mouse` 모드 유지. **알림 없음** (Pillar 1)
- **E3 — Mouse와 Gamepad 동시 입력**: 마지막 입력 장치 우선 (Rule 3). 같은 프레임 내 동시 입력 시 Mouse 우선 (마우스가 더 정밀)
- **E4 — Steam Deck 트랙패드**: Steam Input이 트랙패드를 마우스로 에뮬레이션 → `Mouse` 모드로 동작. 별도 처리 불필요
- **E5 — IA_OfferCard Hold 중 포커스 상실**: Hold 타이머 리셋. 건넴 취소. Card Hand UI가 시각 피드백 롤백 담당. *구현 검증 필요*: UE `UInputTriggerHold`가 포커스 상실 시 자동 리셋하는지 확인. 자동 미지원 시 `OnWindowFocusLost`에서 수동 `UInputTriggerHold` 리셋 또는 `FlushPressedKeys()` 호출
- **E6 — 800×600 미만 해상도에서 마우스 정밀도**: `InputModeMouseThreshold`가 3px — 저해상도에서도 의도된 이동과 진동 구분 가능. DPI 스케일링은 OS 수준 → UE가 스크린 좌표로 변환
- **E7 — 키보드 없이 마우스만 사용**: Rule 5에 의해 완전 작동. `IA_OpenJournal`, `IA_Back`은 UI 버튼으로 접근 가능
- **E8 — `IA_OfferCard` Hold 경계**: Formula 2의 `≥` — 정확히 threshold = 트리거됨. Mouse: 0.15초 = drag 시작. Gamepad: 0.5초 = 건넴 확정. threshold 미만 릴리스 = `IA_Select` 발화
- **E9 — 입력 모드 전환 경계**: Formula 1의 `>` — 정확히 `InputModeMouseThreshold`(3.0px) 이동 시 전환 **미발생**. 3.0px 초과(예: 3.1px)에서만 전환. Formula 2의 `≥`와 의도적 비대칭 (Formula 2 참조)

## Dependencies

### Upstream Dependencies (이 시스템이 의존하는 대상)

| 의존 대상 | 의존 강도 | 인터페이스 | 비고 |
|---|---|---|---|
| UE 5.6 Enhanced Input Plugin | Hard — engine | `UInputAction`, `UInputMappingContext`, `UEnhancedInputLocalPlayerSubsystem`, `UInputTriggerHold`, `UInputModifier` | 표준 UE 5.6 플러그인. breaking change 없음 (2026-04-17 확인) |
| `APlayerController` | Hard — engine | 매핑 컨텍스트 등록 (`AddMappingContext`), 커서 가시성 (`SetShowMouseCursor`), input mode 감지 | 단일 PlayerController (레벨 전환 없음) |

Foundation Layer 시스템 간 의존 없음 (Time, Data Pipeline, Save/Load, Input Abstraction은 병렬 설계 가능).

### Downstream Dependents (이 시스템에 의존하는 시스템)

| 시스템 | 의존 강도 | 인터페이스 | 데이터 방향 |
|---|---|---|---|
| **Card Hand UI (#12)** | Hard | 4 Action 바인딩 (`IA_Select`, `IA_OfferCard`, `IA_PointerMove`, `IA_NavigateUI`) + `OnInputModeChanged` | Input → Card Hand UI (단방향) |
| **Window & Display (#14)** | Soft | `OnInputModeChanged(EInputMode)` | Input → Window (단방향) |
| **Dream Journal UI (#13)** | Hard | 3 Action 바인딩 (`IA_OpenJournal`, `IA_Back`, `IA_NavigateUI`) | Input → Dream Journal (단방향) |
| **Game State Machine (#5)** | Soft | `IA_Back` + `OnInputModeChanged` | Input → GSM (단방향) |
| **Title & Settings UI (#16, VS)** | Soft | `IA_Back` + `OnInputModeChanged` | Input → Title UI (단방향) |

### Bidirectional Consistency Notes

모든 downstream 연결은 **단방향 (Input → Consumer)**. 역방향 데이터 흐름 없음.

각 downstream GDD 작성 시 다음 참조 명시 필요:
- **Card Hand UI GDD**: "Input Abstraction Layer (#4)의 `IA_Select`, `IA_OfferCard`, `IA_PointerMove`, `IA_NavigateUI` 구독. `OnInputModeChanged`로 프롬프트 아이콘 교체. Select/OfferCard 공유 키 해소(Hold vs Released)는 Rule 2a 참조."
- **Window & Display GDD**: "Input Abstraction Layer (#4)의 `OnInputModeChanged`로 커서 가시성 결정."
- **Dream Journal UI GDD**: "Input Abstraction Layer (#4)의 `IA_OpenJournal`, `IA_Back`, `IA_NavigateUI` 구독."
- **GSM GDD**: "Input Abstraction Layer (#4)의 `IA_Back`을 맥락별 라우팅 — 최상위 → Menu 전환, UI 열림 → 닫기, 메뉴 내 → 뒤로."

### Data Ownership

| 데이터/타입 | 소유 시스템 | 읽기 허용 | 쓰기 허용 |
|---|---|---|---|
| `EInputMode` enum | Input Abstraction (정의) | 모든 구독자 | Input Abstraction (유일) |
| `UInputAction` 에셋 6개 | Input Abstraction (정의) | 모든 바인딩 소비자 | — |
| `UInputMappingContext` 2개 | Input Abstraction (정의) | 디버깅 시 읽기 | Input Abstraction (유일) |
| `OnInputModeChanged` delegate | Input Abstraction (정의) | 모든 구독자 | — |

## Tuning Knobs

| 노브 | 기본값 | 안전 범위 | 너무 높으면 | 너무 낮으면 | 참조 |
|---|---|---|---|---|---|
| `InputModeMouseThreshold` | 3.0 px | `[1, 10]` | 마우스를 사용해도 Gamepad 모드 유지 — 전환이 안 되어 답답 | 진동으로 불필요한 전환 빈발, 커서 깜빡임 | Formula 1 |
| `OfferDragThresholdSec` | 0.15초 | `[0.08, 0.3]` | 클릭이 자주 drag로 오판 — 카드 선택이 어려워짐 | 진짜 drag도 click으로 처리 — 건넴 불가 | Formula 2 |
| `OfferHoldDurationSec` | 0.5초 | `[0.2, 1.5]` | 건넴이 느려 답답함, 의식적 행위가 지루한 대기로 전락 | 실수로 건넴 발생, 의식적 행위 감각 훼손 (Pillar 2 위반) | Formula 2 |

### 노브 상호작용

- `InputModeMouseThreshold`와 `OfferDragThresholdSec`는 독립적 — 상호 영향 없음
- `OfferDragThresholdSec`와 `OfferHoldDurationSec`는 각각 Mouse/Gamepad 전용 — 동시 적용 안 됨
- `OfferHoldDurationSec`은 Card Hand UI의 드래그 거리 threshold와 연동될 수 있음 — Card Hand UI GDD에서 관계 정의
- MVP에서는 `OfferDragThresholdSec`만 활성 (Mouse 전용). `OfferHoldDurationSec`은 VS (Gamepad 구현 시)에서 활성화

### Playtest Tuning Priorities

- **1순위**: `OfferDragThresholdSec` — MVP에서 바로 체감. "클릭했는데 drag됐다" vs "drag하려는데 click됐다" 경계. 0.1초 ~ 0.25초 범위 테스트
- **2순위**: `OfferHoldDurationSec` — VS Gamepad 구현 후 카드 건넴의 "무게감" (Pillar 2). 0.3초 ~ 0.8초 범위 A/B 테스트
- **3순위**: `InputModeMouseThreshold` — Steam Deck 실기에서 트랙패드 진동 측정 후 조정

## Visual/Audio Requirements

이 시스템은 **인프라 레이어**로 직접적인 Visual/Audio 출력이 없다. 입력 장치 전환 시 시각·음향 피드백은 없음 (Pillar 1: 알림 없음).

유일한 간접 연결:
- `IA_OfferCard` Hold 진행 중 시각 피드백(프로그레스 표시)은 **Card Hand UI** 소유
- 입력 모드 전환 시 UI 프롬프트 아이콘 교체는 각 **UI GDD** 소유

## UI Requirements

이 시스템은 UI를 **소유하지도, 발행하지도 않는다**. 입력 장치별 프롬프트 아이콘(예: "LMB" vs "A 버튼")은 각 UI 시스템이 `OnInputModeChanged` 구독하여 자체 교체.

## Acceptance Criteria

> Given-When-Then 형식. QA 테스터가 GDD 없이 독립 검증 가능.

### Criterion ACTION_COUNT_COMPLETE
- **GIVEN** Input Abstraction Layer 구현 완료
- **WHEN** `UInputAction` 에셋 목록을 열거
- **THEN** 6개 Action 정확히 존재 (`IA_PointerMove`, `IA_Select`, `IA_OfferCard`, `IA_NavigateUI`, `IA_OpenJournal`, `IA_Back`)
- **Evidence**: Content Browser에서 InputAction 에셋 6개 확인. 이름과 ValueType 일치
- **Type**: CODE_REVIEW — **Source**: Rule 1

### Criterion NO_RAW_KEY_BINDING
- **GIVEN** 전체 게임플레이 소스 코드 (`Source/MadeByClaudeCode/` — Input 서브시스템 자체 제외)
- **WHEN** raw key/button 바인딩 패턴 검색 (`BindKey`, `BindAxis` legacy, `EKeys::` 직접 참조)
- **THEN** 매치 0건. 모든 입력은 `UInputAction` 경유
- **Evidence**: `grep -r "BindKey\|BindAxis\|EKeys::" Source/MadeByClaudeCode/ --include="*.cpp" --include="*.h"` 결과 0건 (Input 서브시스템 파일 제외)
- **Type**: CODE_REVIEW — **Source**: Rule 1

### Criterion BINDING_TABLE_COMPLETE
- **GIVEN** `IMC_MouseKeyboard` 매핑 컨텍스트 에셋
- **WHEN** 바인딩 목록 열거
- **THEN** Rule 2a 테이블의 6개 Action이 명시된 Key + Trigger로 바인딩되어 있음
- **Evidence**: Content Browser에서 `IMC_MouseKeyboard` 열어 바인딩 확인. 각 Action의 Key와 Trigger Type이 Rule 2a 일치
- **Type**: CODE_REVIEW — **Source**: Rule 2a

### Criterion MOUSE_ONLY_COMPLETENESS
- **GIVEN** 게임패드 미연결, 키보드 미사용 환경
- **WHEN** 마우스만으로 전체 게임 기능 접근 시도: 카드 3장 중 선택 → 건넴 → 일기 열기/닫기 → 메뉴 진입/퇴장
- **THEN** 모든 기능에 접근 가능. 키보드 없이 차단되는 기능 없음
- **Evidence (체크리스트)**:
  - [ ] 카드 선택: 마우스 클릭으로 카드 선택 가능
  - [ ] 카드 건넴: 마우스 드래그로 건넴 가능
  - [ ] 일기 열기: UI 버튼 클릭으로 열기 가능
  - [ ] 일기 닫기: UI 닫기 버튼 또는 외부 클릭으로 닫기 가능
  - [ ] 메뉴 진입: UI 메뉴 버튼 클릭으로 진입 가능
- **Type**: MANUAL — **Source**: Rule 5, E7

### Criterion HOVER_ONLY_PROHIBITION
- **GIVEN** Card Hand UI에 카드 3장 표시됨
- **WHEN** 마우스를 카드 위에 올려놓기만 (클릭 없이) 3초 대기
- **THEN** 시각 피드백(하이라이트)만 발생. 게임 상태 변경 없음. 카드 선택·건넴 미발생
- **Evidence**: 카드 시각 상태가 '하이라이트'이나 `OnCardSelected` delegate 미발행; `FSessionRecord` 불변
- **Type**: MANUAL — **Source**: Rule 4

### Criterion INPUT_MODE_AUTO_DETECT
- **GIVEN** `Mouse` 모드 활성, 게임패드 연결됨
- **WHEN** 게임패드 A버튼 누름
- **THEN** `OnInputModeChanged(EInputMode::Gamepad)` delegate 발행 1회
- **Evidence**: delegate 수신 카운트 == 1; `CurrentInputMode == Gamepad`
- **Type**: AUTOMATED — **Source**: Rule 3

### Criterion INPUT_MODE_HYSTERESIS
- **GIVEN** `Gamepad` 모드 활성, `InputModeMouseThreshold = 3.0`
- **WHEN** 마우스가 2px 이동 (threshold 미만)
- **THEN** `Mouse` 모드 전환 **미발생**. `OnInputModeChanged` 미발행. `CurrentInputMode == Gamepad` 유지
- **Evidence**: delegate 수신 카운트 == 0; 모드 쿼리 `Gamepad`
- **Type**: AUTOMATED — **Source**: Formula 1, Rule 3

### Criterion OFFER_HOLD_BOUNDARY_GAMEPAD
- **GIVEN** `OfferHoldDurationSec = 0.5`, Gamepad 모드, 카드 포커스 상태
- **WHEN** A버튼 정확히 0.5초 유지 후 릴리스
- **THEN** `IA_OfferCard` Triggered 이벤트 발행 (건넴 확정)
- **Evidence**: `IA_OfferCard` Triggered 이벤트 수신 1회
- **Type**: AUTOMATED — **Source**: Formula 2, E8

### Criterion OFFER_HOLD_CANCEL_GAMEPAD
- **GIVEN** `OfferHoldDurationSec = 0.5`, Gamepad 모드
- **WHEN** A버튼 0.3초 유지 후 릴리스 (threshold 미만)
- **THEN** `IA_OfferCard` Triggered 미발행. `IA_Select` Released로 처리됨
- **Evidence**: `IA_OfferCard` Triggered 카운트 == 0; `IA_Select` Released 카운트 == 1
- **Type**: AUTOMATED — **Source**: Formula 2

### Criterion OFFER_DRAG_THRESHOLD_MOUSE
- **GIVEN** `OfferDragThresholdSec = 0.15`, Mouse 모드, 카드 위 커서
- **WHEN** LMB 정확히 0.15초 유지
- **THEN** `IA_OfferCard` Triggered 이벤트 발행 (drag 시작 가능)
- **Evidence**: `IA_OfferCard` Triggered 카운트 == 1
- **Type**: AUTOMATED — **Source**: Formula 2, E8

### Criterion OFFER_CLICK_SELECT_MOUSE
- **GIVEN** `OfferDragThresholdSec = 0.15`, Mouse 모드
- **WHEN** LMB 0.1초 유지 후 릴리스 (threshold 미만)
- **THEN** `IA_OfferCard` Triggered 미발행. `IA_Select` Released 발행
- **Evidence**: `IA_OfferCard` Triggered 카운트 == 0; `IA_Select` Released 카운트 == 1
- **Type**: AUTOMATED — **Source**: Formula 2, Rule 2a

### Criterion GAMEPAD_DISCONNECT_SILENT
- **GIVEN** `Gamepad` 모드 활성, 게임 진행 중
- **WHEN** 게임패드 USB 케이블 물리적 분리
- **THEN** 알림·팝업·모달 미생성 (Pillar 1). 마우스 이동 시 `Mouse` 모드로 정상 전환. 게임 진행 차단 없음
- **Evidence (체크리스트)**:
  - [ ] 팝업/모달/알림 없음
  - [ ] 마우스 이동 후 커서 표시
  - [ ] 카드 선택/건넴 정상 작동
- **Type**: MANUAL — **Source**: E2, Pillar 1

---

### Coverage 요약

| 분류 | Criterion 수 |
|---|---|
| Action 정의 + 바인딩 완결성 | 3 (ACTION_COUNT_COMPLETE, NO_RAW_KEY_BINDING, BINDING_TABLE_COMPLETE) |
| Mouse-only 완결성 | 1 (MOUSE_ONLY_COMPLETENESS) |
| Hover 금지 | 1 (HOVER_ONLY_PROHIBITION) |
| Input 모드 전환 | 2 (AUTO_DETECT, HYSTERESIS) |
| Offer Hold/Drag 로직 | 4 (HOLD_BOUNDARY_GAMEPAD, HOLD_CANCEL_GAMEPAD, DRAG_THRESHOLD_MOUSE, CLICK_SELECT_MOUSE) |
| 장치 분리 | 1 (GAMEPAD_DISCONNECT_SILENT) |
| **Total** | **12** |
| CODE_REVIEW | 3 (25.0%) |
| AUTOMATED | 6 (50.0%) |
| MANUAL | 3 (25.0%) |

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|---|---|---|
| OQ-1 | Steam Deck 실기에서 트랙패드 마우스 에뮬레이션의 `InputModeMouseThreshold` 적정값은? | `qa-lead` | MVP 구현 후 Steam Deck 실기 테스트 |
| OQ-2 | `IA_OfferCard`의 Gamepad Hold 시간(0.5초)이 "의식적 건넴"으로 느껴지는가? | `game-designer` | VS Gamepad 구현 후 플레이테스트 |
| OQ-3 | `IA_NavigateUI`의 Gamepad D-pad vs 왼쪽 스틱 매핑 — 어느 쪽이 자연스러운가? | `ux-designer` | Card Hand UI GDD 작성 시 |
