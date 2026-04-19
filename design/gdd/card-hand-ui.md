# Card Hand UI (카드 패 UI)

> **Status**: In Design
> **Author**: bong9tutor + claude-code session
> **Last Updated**: 2026-04-18
> **Implements Pillar**: Pillar 2 (선물의 시학 — 카드는 이미지와 감정, 숫자 없음), Pillar 1 (조용한 존재 — 타이머 없음, 재촉 없음)
> **Priority**: MVP | **Layer**: Presentation | **Category**: UI
> **Effort**: M+ (3 세션)
> **Depends On**: Card System (#8), Input Abstraction Layer (#4), Game State Machine (#5), Stillness Budget (#10)

---

## 1. Overview

Card Hand UI는 이끼 아이에게 선물 카드를 건네는 **플레이어 대면 인터랙션 레이어**다. Card System(#8)으로부터 매일 3개의 카드 ID를 수신하고, `FGiftCardRow`의 일러스트·이름·설명을 시각 카드로 표시하며, 드래그-투-오퍼(drag-to-offer) 제스처를 처리해 `ConfirmOffer(CardId)`를 Card System 데이터 레이어에 전달한다. GSM(#5)의 `Offering` 상태 진입 시 카드를 보여주고, 이탈 시 숨긴다. 이 시스템은 게임의 유일한 의사결정 순간인 2–3초의 카드 건넴 의식을 플레이어가 체험하는 물리적 표면이다. 게임패드 등 마우스 이외의 입력 방식에 대한 대체 선택 경로도 제공한다.

---

## 2. Player Fantasy

카드 세 장이 손 앞에 펼쳐진다. 어떤 것을 건네야 할지 공식은 없다. 오늘은 이 그림이, 이 계절이, 이 감각이 어울린다는 느낌뿐이다.

카드 하나를 향해 손이 움직인다. 손가락이 닿는 순간부터 유리병 쪽으로 밀어내는 그 짧은 거리가 — 건넴의 전부다. 빠르게 결정해도 좋고, 잠시 머물러도 좋다. 재촉하는 소리는 없다. 카드 위에 숫자는 없다.

이 시스템이 만드는 경험: **"나는 지금 선물을 건넸다."** 게임 액션이 아니라 행위 자체의 무게감. 마우스 드래그라는 물리적 제스처가 "건넨다"는 의식적 의미를 갖도록 UX가 뒷받침한다.

**Pillar 연결**:
- **Pillar 1 (조용한 존재)** — 카드에 타이머가 없다. 조급함을 유발하는 카운트다운, 진행 바, 빠른 반응 요구가 없다. 플레이어는 언제든 원하는 만큼 카드를 바라볼 수 있다
- **Pillar 2 (선물의 시학)** — 카드는 이름과 시적 설명, 일러스트만 표시한다. 수치 효과, 확률, 태그 레이블은 절대 표시하지 않는다. 선택의 근거는 직관과 감정이다

---

## 3. Detailed Design

### 3A. Core Rules

#### CR-CHU-1 — 카드 레이아웃

Offering 상태 진입 시 카드 3장을 화면에 배치한다.

- 카드는 화면 하단 영역에 수평으로 펼쳐진다 (F-CHU-1 참조)
- 각 카드는 FGiftCardRow의 `DisplayName`, `Description`, `IconPath` 텍스처를 표시한다
- 수치 효과, 태그 레이블(`Season.Spring` 등), 확률 정보는 **절대 표시하지 않는다** (Pillar 2)
- 카드 크기는 800×600 최소 해상도에서도 텍스트가 읽힐 수 있어야 한다 (§Accessibility 검증 기준)
- Degraded 모드(카드 0–2장) 시 빈 슬롯은 비어있는 상태로 표시 (CR-CHU-8 참조)

#### CR-CHU-2 — 드래그-투-오퍼 제스처 (마우스)

게임의 핵심 인터랙션. Input Abstraction Layer(#4)로부터 `IA_OfferCard` Hold Triggered 신호를 받아 드래그를 처리한다.

**드래그 흐름**:

1. **Hold 시작**: `IA_OfferCard` Hold Triggered (LMB 0.15초 유지) → 해당 카드가 드래그 모드 진입. 카드가 커서를 따라 움직이기 시작한다
2. **드래그 중**: 카드가 커서 위치를 추적. Offer Zone(유리병 근처 원형 영역)으로 진입 시 시각 피드백(카드 밝아짐, Offer Zone 하이라이트)
3. **릴리스 — 건넴 확정**: Offer Zone 내에서 LMB 릴리스 → `ConfirmOffer(CardId)` 호출 → 성공 시 건넴 확정 애니메이션 → Hide 시퀀스
4. **릴리스 — 취소**: Offer Zone 밖에서 LMB 릴리스 → 카드가 원래 위치로 부드럽게 복귀 (CR-CHU-5 참조)

드래그 중 카드는 원래 위치에서 분리되어 커서를 따른다. 나머지 두 카드는 제자리에 유지된다.

#### CR-CHU-3 — Offer Zone(건넴 구역)

유리병을 향한 건넴 제스처의 판정 영역.

- 위치: 유리병 Actor의 스크린 투영 좌표 중심, 반지름 `OfferZoneRadiusPx`(기본 80px) 원형
- 드래그 중 카드의 중심점이 이 원 안에 들어오면 "건넴 가능" 상태로 전환
- 건넴 가능 상태의 시각 피드백: 카드 밝아짐(EmissiveStrength +0.15), Offer Zone 테두리 파장 효과 (Stillness Budget `Particle` Standard 슬롯 사용)
- 릴리스 위치가 Zone 내부 → 건넴 확정. 외부 → 드래그 취소

#### CR-CHU-4 — 건넴 확정 시퀀스

`ConfirmOffer(CardId)` 가 `true`를 반환한 후:

1. 카드가 유리병 방향으로 이동하며 페이드 아웃 (건넴 애니메이션, 약 0.5초)
2. Stillness Budget `Motion` Standard 슬롯 Request → Deny 시 즉각 페이드 아웃
3. 나머지 두 카드 동시 페이드 아웃
4. GSM이 `FOnCardOffered` 수신 후 Offering→Waiting 전환 시작
5. 카드 패 전체 Hide 완료

건넴 확정 후 드래그 취소·선택 변경은 불가능하다 — `ConfirmOffer`는 단방향 계약 (Card System CR-CS-4).

#### CR-CHU-5 — 드래그 취소 및 카드 복귀

드래그 중 다음 조건에서 드래그가 취소되고 카드가 원래 위치로 복귀한다:

- LMB 릴리스 위치가 Offer Zone 밖인 경우
- `ConfirmOffer` 가 `false`를 반환한 경우 (Card System 검증 실패)
- 창 포커스 상실 (E5 참조)

복귀 애니메이션: SmoothStep, 지속시간 `CardReturnDurationSec`(기본 0.3초). Stillness Budget `Motion` Standard 슬롯 Request → Deny 시 즉각 복귀(애니메이션 없음).

#### CR-CHU-6 — 카드 공개(Reveal) 애니메이션

GSM Offering 진입 시 카드 3장을 순차 공개한다.

- 카드가 화면 밖(하단)에서 슬라이드 업 (또는 페이드 인)
- 공개 순서: 왼쪽 → 가운데 → 오른쪽, 각 카드 간 `CardRevealStaggerSec`(기본 0.08초) 간격
- Stillness Budget `Motion` Standard 슬롯 Request. Deny 시 즉각 표시(애니메이션 없음)
- ReducedMotion ON: 즉각 표시 (Stillness Budget `IsReducedMotion()` 조회)

#### CR-CHU-7 — 숨김(Hide) 시퀀스

GSM Offering 이탈(Waiting 전환) 시:

- 건넴 확정: CR-CHU-4 건넴 확정 시퀀스가 Hide를 처리
- 취소·상태 강제 이탈(`FOnGameStateChanged(Offering, Waiting)` 수신): 카드 패 전체 페이드 아웃 (`CardHideDurationSec` 기본 0.25초)
- ReducedMotion ON: 즉각 Hide

#### CR-CHU-8 — Degraded 모드 (카드 0–2장)

Card System이 Degraded Ready(`bDegraded = true`)를 전달한 경우:

- **카드 2장**: 2장을 표시, 남은 슬롯 1개는 빈 카드 프레임(회색 테두리, 내용 없음)으로 표시
- **카드 1장**: 1장 표시, 빈 슬롯 2개
- **카드 0장**: 빈 슬롯 3개 + 카드 패가 Show 상태 유지(Offering은 진행). `ConfirmOffer` 호출 불가 → GSM이 자동 Waiting 전환 (GSM E9 계약)
- 빈 슬롯에는 드래그·클릭 반응 없음
- MVP 10장 환경에서 Degraded는 에디터 설정 오류 — 배포 빌드에서 발생해서는 안 된다

#### CR-CHU-9 — 게임패드 대체 인터랙션

Input Abstraction Layer(#4) `EInputMode::Gamepad` 감지 시 대체 선택 흐름으로 전환한다.

MVP에서는 구조만 정의, Vertical Slice에서 구현.

- `IA_NavigateUI` (D-pad / 스틱)로 카드 포커스 이동 (좌우 3개 순환)
- `IA_OfferCard` A버튼 Hold (`OfferHoldDurationSec` 0.5초) → 포커스된 카드 건넴 확정
- 포커스된 카드는 시각 강조 (테두리 밝아짐, 미세 스케일 업)
- `IA_Back`으로 선택 취소 (카드 패 유지, 포커스 해제)
- Offer Zone 판정 없음 — Hold 완료 즉시 `ConfirmOffer` 호출

#### CR-CHU-10 — 입력 모드 전환 대응

`OnInputModeChanged(EInputMode)` 수신 시:

- Mouse → Gamepad: 진행 중인 드래그가 있으면 취소 + 카드 복귀 → 포커스 네비게이션 모드 전환
- Gamepad → Mouse: 포커스 해제 → 드래그 모드 대기

프롬프트 아이콘은 조용히 교체된다. 알림·팝업 없음 (Pillar 1).

---

### 3B. States and Transitions

Card Hand UI는 6개의 내부 상태를 갖는다.

| 상태 | 설명 | 진입 조건 | 이탈 조건 |
|---|---|---|---|
| `Hidden` | 카드 패 비표시. UI 위젯 Collapsed | 기본값 / Hide 완료 / GSM Offering 미진입 | GSM Offering 진입 → Revealing |
| `Revealing` | 카드 공개 애니메이션 중 | GSM `Offering` 진입 신호 | 공개 완료 → Idle |
| `Idle` | 카드 3장 표시, 입력 대기 | Reveal 완료 / 드래그 취소 복귀 완료 | 드래그 시작 → Dragging, GSM 이탈 → Hiding |
| `Dragging` | 카드 1장 드래그 중 | `IA_OfferCard` Hold Triggered | 릴리스 → Offering 또는 Idle (복귀) |
| `Offering` | 건넴 확정 애니메이션 중 | `ConfirmOffer` 성공 | 애니메이션 완료 → Hiding |
| `Hiding` | 카드 패 Hide 애니메이션 중 | GSM 이탈 또는 Offering 완료 | Hide 완료 → Hidden |

**전환 표**:

| From | To | 트리거 | Guard | 부작용 |
|---|---|---|---|---|
| `Hidden` | `Revealing` | `FOnGameStateChanged(_, Offering)` | — | `GetDailyHand()` pull, 카드 위젯 구성 |
| `Revealing` | `Idle` | Reveal 애니메이션 완료 | — | 입력 수락 시작 |
| `Idle` | `Dragging` | `IA_OfferCard` Hold Triggered (카드 위에서) | `bHandOffered == false` | 해당 카드 Drag 상태 시작 |
| `Dragging` | `Offering` | LMB 릴리스, Offer Zone 내 | `ConfirmOffer` → `true` | 건넴 애니메이션 시작 |
| `Dragging` | `Idle` | LMB 릴리스, Offer Zone 밖 OR `ConfirmOffer` → `false` | — | 카드 복귀 애니메이션 |
| `Idle` | `Hiding` | `FOnGameStateChanged(Offering, _)` (강제 이탈) | — | 전체 카드 페이드 아웃 |
| `Offering` | `Hiding` | 건넴 애니메이션 완료 | — | — |
| `Hiding` | `Hidden` | Hide 애니메이션 완료 | — | 위젯 Collapsed |

---

### 3C. Other Systems와의 상호작용

#### 1. Card System (#8) — 양방향

| 방향 | API / 이벤트 | 시점 | 용도 |
|---|---|---|---|
| UI → Card | `GetDailyHand()` → `TArray<FName>` | Offering 진입 시 1회 pull | 카드 3장 ID 획득 → 각 카드 `GetCardRow(CardId)` 조회 |
| UI → Card | `ConfirmOffer(FName CardId)` → `bool` | 드래그 완료·Gamepad Hold 완료 | 건넴 확정 시도. `false` = 드래그 롤백 |

Card Hand UI는 Card System의 내부 상태(DailyHand, bHandOffered)를 직접 읽지 않는다. 카드 표시에 필요한 `FGiftCardRow` 데이터는 `Pipeline.GetCardRow(CardId)`를 통해 Data Pipeline(#2)에서 직접 pull한다.

#### 2. Input Abstraction Layer (#4) — 인바운드

| 입력 신호 | 시점 | Card Hand UI 처리 |
|---|---|---|
| `IA_OfferCard` Hold Triggered | LMB 0.15초 유지 | 카드 위에서 → Dragging 시작. 카드 밖 → 무시 |
| `IA_Select` Released | LMB 0.15초 미만 릴리스 | 카드 위에서 → 선택 강조 (시각만, 건넴 아님). Gamepad 포커스 이동 대기 |
| `IA_PointerMove` Axis2D | 매 프레임 | Dragging 상태에서 카드 위치 갱신. Hit Test용 위치 추적 |
| `IA_NavigateUI` Axis2D | 게임패드 입력 | Gamepad 모드에서 카드 포커스 이동 |
| `OnInputModeChanged(EInputMode)` | 장치 전환 시 | CR-CHU-10 참조. 진행 중 드래그 취소 등 |

- Hover-only 인터랙션 없음. 시각 하이라이트는 Hover 허용, 게임 상태 변경은 명시적 선택만 (Input Abstraction Rule 4)
- 드래그의 공간적 판정(Offer Zone 진입/이탈)은 Card Hand UI가 소유 — Input Abstraction은 Hold 트리거만 제공

#### 3. Game State Machine (#5) — 인바운드

| 이벤트 | Card Hand UI 반응 |
|---|---|
| `FOnGameStateChanged(Dawn, Offering)` | Revealing 시작. `GetDailyHand()` pull + 카드 위젯 구성 |
| `FOnGameStateChanged(Offering, Waiting)` (카드 건넴 후 정상 전환) | Offering → Hiding (CR-CHU-4에서 이미 처리 중일 수 있음) |
| `FOnGameStateChanged(Offering, _)` (강제 이탈) | 진행 중 드래그 취소 + 전체 Hide |

Card Hand UI는 GSM 상태를 주도적으로 변경하지 않는다. `ConfirmOffer` 성공이 `FOnCardOffered`를 발행하고, GSM이 그것을 수신해 Offering→Waiting 전환을 스스로 결정한다.

#### 4. Stillness Budget (#10) — 아웃바운드

| 상황 | Channel | Priority | Deny 시 대체 |
|---|---|---|---|
| 카드 Reveal 애니메이션 | Motion | Standard | 즉각 표시 |
| 카드 wobble (Hover 강조) | Motion | Standard | 정적 하이라이트 |
| 드래그 중 카드 이동 | Motion | Standard | 이동은 유지 (Budget은 추가 wobble만 제어) |
| 건넴 VFX (Offer Zone 파장) | Particle | Standard | 파장 생략 |
| 카드 건넴 애니메이션 | Motion | Standard | 즉각 Hide |
| 카드 복귀 애니메이션 | Motion | Standard | 즉각 복귀 |

`IsReducedMotion()` = `true` 시: 모든 애니메이션 건너뜀, 정적 표시·즉각 전환만 사용.

#### 5. Moss Baby Character System (#6) — 간접 관계

Card Hand UI는 MBC에 직접 메시지를 보내지 않는다. `ConfirmOffer` 성공 → Card System이 `FOnCardOffered` 발행 → MBC가 Reacting 상태 진입. UI와 MBC의 시각 반응은 동시에 일어나되 독립적으로 처리된다.

---

## 4. Formulas

### F-CHU-1 — 카드 수평 배치 위치 (CardSlotPosition)

화면 하단 영역에 3장을 균등 간격으로 배치한다.

`SlotX(i) = ScreenCenterX + (i − 1) × (CardWidthPx + CardSpacingPx)`

`SlotY = ScreenHeight × CardBaselineRatio`

여기서 `i ∈ {0, 1, 2}` (왼쪽=0, 가운데=1, 오른쪽=2). `(i − 1)`이 오프셋 역할을 한다: i=0 → -1(좌), i=1 → 0(중), i=2 → +1(우).

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| 화면 중앙 X | ScreenCenterX | float | [0, ScreenWidth] | 뷰포트 수평 중앙 픽셀 |
| 화면 높이 | ScreenHeight | float | [600, ∞) | 뷰포트 높이 픽셀 |
| 카드 너비 | CardWidthPx | float | [80, 200] | 카드 UI 위젯 너비. Tuning Knob |
| 카드 간격 | CardSpacingPx | float | [8, 60] | 카드 간 수평 여백. Tuning Knob |
| 기준선 비율 | CardBaselineRatio | float | [0.55, 0.85] | 화면 높이 대비 카드 Y 위치. 기본 0.72 |
| 슬롯 인덱스 | i | int | {0, 1, 2} | 0=좌, 1=중, 2=우 |

**Output Range**: `SlotX` ∈ [0, ScreenWidth], `SlotY` ∈ [330, ScreenHeight×0.85]

**Example** (1280×720, CardWidthPx=120, CardSpacingPx=20):
- i=0 (왼쪽): SlotX = 640 + (0−1) × 140 = **500**, SlotY = 720 × 0.72 = **518**
- i=1 (가운데): SlotX = 640 + 0 × 140 = **640**, SlotY = **518**
- i=2 (오른쪽): SlotX = 640 + 140 = **780**, SlotY = **518**

**800×600 최소 해상도 검증**: CardWidthPx=100, CardSpacingPx=12 기준:
- 3장 총 너비 = 100×3 + 12×2 = 324px. 800px 내 여유 있음 ✓

---

### F-CHU-2 — 드래그 판정 (DragThreshold)

Input Abstraction이 이미 Hold 판정을 수행하므로, Card Hand UI는 추가 드래그 거리 임계를 검사하지 않는다. `IA_OfferCard` Hold Triggered 이후 모든 커서 이동이 드래그로 간주된다.

---

### F-CHU-3 — Offer Zone 진입 판정 (OfferZoneHit)

드래그 중인 카드의 중심점이 Offer Zone 원 안에 있는지 판정한다.

`IsInOfferZone = (Distance(CardCenter, JarScreenPos) ≤ OfferZoneRadiusPx)`

`Distance(A, B) = sqrt((A.X − B.X)² + (A.Y − B.Y)²)`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| 카드 중심 스크린 좌표 | CardCenter | FVector2D | [0, ScreenSize] | 드래그 중인 카드 위젯의 중심 |
| 유리병 스크린 좌표 | JarScreenPos | FVector2D | [0, ScreenSize] | 유리병 Actor의 월드→스크린 투영 좌표. 매 프레임 갱신 |
| Offer Zone 반지름 | OfferZoneRadiusPx | float | [40, 150] | 기본 80px. Tuning Knob |

**Output Range**: `bool`. `true` = 건넴 판정, `false` = 드래그 계속·취소 대기.

**Example** (OfferZoneRadiusPx=80, JarScreenPos=(640, 360)):
- CardCenter=(620, 400) → Distance = sqrt(400+1600) = 44.7 → **Zone 내** (건넴 가능)
- CardCenter=(500, 300) → Distance = sqrt(19600+3600) = 152.3 → **Zone 밖** (드래그 계속)

**800×600 스케일 참고**: 화면 해상도에 따라 OfferZoneRadiusPx를 DPI 스케일 factor로 보정 가능 (Tuning Knob).

---

## 5. Edge Cases

### EC-CHU-1 — 드래그 도중 LMB 릴리스 (Offer Zone 밖)

**상황**: 플레이어가 카드를 들었다가 Offer Zone 밖에서 놓음.
**처리**: 드래그 취소. 카드가 `CardReturnDurationSec`(기본 0.3초) 동안 원래 슬롯 위치로 복귀. `ConfirmOffer` 미호출. `Dragging` → `Idle` 전환. 취소 후 다른 카드 다시 선택 가능.

### EC-CHU-2 — ConfirmOffer 실패 (false 반환)

**상황**: 드래그 Offer Zone 내 릴리스 시 `ConfirmOffer`가 `false` 반환 (Card System 검증 실패 — Uninitialized, 이미 건넴 완료, CardId 불일치 등).
**처리**: 카드를 원래 슬롯으로 복귀. `UE_LOG(Warning, "ConfirmOffer failed for CardId=%s")`. Idle 상태로 복귀. 재시도 허용.

### EC-CHU-3 — 드래그 도중 창 포커스 상실 (Alt-Tab 등)

**상황**: 드래그 중 창이 포커스를 잃음 (Alt-Tab, 시스템 알림 등).
**처리**: `IA_OfferCard` Hold가 Input Abstraction E5에 따라 리셋됨. Card Hand UI는 `Dragging` → `Idle` 강제 전환. 카드 원래 위치 복귀. Offering 상태는 유지 — 플레이어가 창으로 돌아오면 다시 카드 선택 가능.

### EC-CHU-4 — Offering 중 강제 상태 전환 (LONG_GAP_SILENT 등)

**상황**: 카드를 아직 건네지 않았는데 GSM이 Farewell 또는 다른 상태로 강제 전환.
**처리**: `FOnGameStateChanged(Offering, _)` 수신 → 진행 중 드래그 즉시 취소 → 카드 패 전체 Hide. `ConfirmOffer` 미호출. 드래그 중이었어도 카드는 건네진 것으로 처리되지 않는다.

### EC-CHU-5 — 창 리사이즈 중 드래그

**상황**: 드래그 중 창 크기 변경.
**처리**: 유리병 스크린 좌표 JarScreenPos와 카드 슬롯 위치를 다음 프레임 뷰포트 크기로 재계산. 드래그는 취소하지 않음 — 커서 위치는 새 해상도 좌표계로 재매핑. Offer Zone 반지름은 픽셀 절대값이므로 DPI 스케일 변화 시 보정 필요 (OQ-CHU-3).

### EC-CHU-6 — 카드 0장 (완전 Degraded)

**상황**: Card System이 카드 0장으로 Degraded Ready를 전달.
**처리**: 카드 패에 빈 슬롯 3개 표시. `ConfirmOffer` 호출 불가. GSM E9 계약에 따라 GSM이 자동 Offering→Waiting 전환. `UE_LOG(Error, "CardHandUI: 0 cards in Degraded mode")`.

### EC-CHU-7 — Gamepad 홀드 중 Mouse 입력 전환

**상황**: A버튼 Hold 중 마우스 이동으로 EInputMode가 Gamepad→Mouse로 전환.
**처리**: CR-CHU-10 — Hold 취소. `IA_OfferCard` Gamepad Hold가 Input Abstraction E5와 유사하게 리셋됨. 포커스 해제 → Mouse 드래그 모드 대기. 카드 선택 취소.

### EC-CHU-8 — Offering 중 이중 드래그 시도

**상황**: 카드 A를 드래그하는 도중 다른 마우스 버튼으로 카드 B 드래그 시도 (멀티 포인터 등).
**처리**: `Dragging` 상태에서 새 `IA_OfferCard` Hold는 무시. 한 번에 하나의 카드만 드래그 가능. 현재 드래그 완료 또는 취소 후 다음 드래그 가능.

### EC-CHU-9 — 800×600 이하 해상도

**상황**: 플레이어가 800×600 미만 해상도로 실행.
**처리**: F-CHU-1의 CardWidthPx/CardSpacingPx 최솟값 적용으로 화면 내 배치 유지. 카드 텍스트가 최소 폰트 크기 이하로 축소되지 않도록 최소 카드 크기 보장. Window & Display Management(#14)의 최소 해상도 계약과 연동. CardWidthPx 최솟값=80px, 카드 3장 최소 총 너비=80×3+8×2=256px (800px에 여유).

### EC-CHU-10 — Reveal 애니메이션 중 입력

**상황**: 카드가 Reveal 애니메이션 중(Revealing 상태)에 플레이어가 클릭·드래그 시도.
**처리**: Revealing 상태에서는 드래그 입력을 수락하지 않음. 애니메이션 완료 → Idle 진입 후 입력 활성화. Reveal 중 클릭은 무시 (대기 없이 자동 완료 트리거는 하지 않음 — 의식의 리듬 보호).

---

## 6. Dependencies

| 시스템 | 방향 | 강도 | 인터페이스 | 티어 |
|---|---|---|---|---|
| **Card System (#8)** | 양방향 | **Hard** | `GetDailyHand()`, `ConfirmOffer(FName)` | MVP |
| **Input Abstraction Layer (#4)** | 인바운드 | **Hard** | `IA_OfferCard`, `IA_Select`, `IA_PointerMove`, `IA_NavigateUI`, `OnInputModeChanged` | MVP |
| **Game State Machine (#5)** | 인바운드 | **Hard** | `FOnGameStateChanged` — Offering 진입/이탈 | MVP |
| **Stillness Budget (#10)** | 아웃바운드 | **Hard** | `Request(Motion/Particle, Standard)`, `IsReducedMotion()` | MVP |
| **Data Pipeline (#2)** | 아웃바운드 (pull) | **Hard** | `Pipeline.GetCardRow(FName)` — 카드 표시 데이터 | MVP |
| **Moss Baby Character (#6)** | 없음 (간접) | — | `FOnCardOffered`를 통해 MBC가 독립적으로 반응 | MVP |
| **Audio System (#15)** | 아웃바운드 (간접) | **Soft** | `FOnCardOffered` 구독으로 Audio가 SFX 재생 | VS |

**양방향 일관성 확인**:
- Card System GDD §Interaction 6: `GetDailyHand()`, `ConfirmOffer(FName)` ✓
- Input Abstraction GDD §Interaction 1: Card Hand UI는 4 Action 바인딩 + `OnInputModeChanged` 구독 ✓
- GSM GDD §Interaction 5: Card Hand UI가 `FOnGameStateChanged` 구독, Offering 시 Show/Hide ✓
- Stillness Budget GDD §Interaction 2: `Request(Motion, Standard)` — wobble, Reveal, `Request(Particle, Standard)` — 건넴 VFX ✓

---

## 7. Tuning Knobs

| 노브 | 타입 | 기본값 | 안전 범위 | 영향 | 극단 시 |
|---|---|---|---|---|---|
| `CardWidthPx` | float | 120 | [80, 200] | 카드 크기. 텍스트 가독성에 직접 영향 | < 80: 800×600에서 텍스트 읽기 불가. > 200: 3장이 화면 밖 |
| `CardHeightPx` | float | 170 | [110, 280] | 카드 높이. 일러스트 비율 유지 권장 | 비율 깨지면 일러스트 왜곡 |
| `CardSpacingPx` | float | 20 | [8, 60] | 카드 간 수평 간격 | < 8: 카드 겹침. > 60: 1024px 미만에서 화면 밖 이탈 |
| `CardBaselineRatio` | float | 0.72 | [0.55, 0.85] | 카드 Y 위치(화면 높이 비율). 유리병과의 시각 균형 | < 0.55: 유리병과 너무 가까움. > 0.85: 화면 하단 잘림 |
| `OfferZoneRadiusPx` | float | 80 | [40, 150] | Offer Zone 원 반지름. 건넴 판정 너그러움 | < 40: 정확한 드래그 어려움. > 150: 조금만 움직여도 건넴 (의식 감 훼손) |
| `CardRevealStaggerSec` | float | 0.08 | [0.0, 0.2] | 카드 공개 간격 | = 0: 동시 공개. > 0.2: 마지막 카드까지 대기 시간이 길어짐 |
| `CardReturnDurationSec` | float | 0.3 | [0.1, 0.6] | 드래그 취소 시 카드 복귀 속도 | < 0.1: 너무 순식간, 자연스럽지 않음. > 0.6: 플레이어가 기다리는 느낌 |
| `CardOfferDurationSec` | float | 0.5 | [0.2, 0.8] | 건넴 확정 애니메이션 속도 | < 0.2: 급함. > 0.8: 너무 느림 |
| `CardHideDurationSec` | float | 0.25 | [0.1, 0.5] | 강제 Hide 페이드 아웃 속도 | — |
| `CardRevealDurationSec` | float | 0.3 | [0.1, 0.5] | 카드 1장 공개 애니메이션 속도 | — |

**DataAsset 위치**: `UCardHandUIConfigAsset` (`UPrimaryDataAsset` 상속). Data Pipeline 경유 로드.

**PostLoad 검증**:
- `OfferZoneRadiusPx > 0` — 0 이하 → `UE_LOG(Warning)` + clamp(80)
- 3장 총 너비 `CardWidthPx * 3 + CardSpacingPx * 2 ≤ 800` — 800px 밖 → `UE_LOG(Warning)`
- `CardRevealStaggerSec ∈ [0.0, 0.2]` — 범위 외 → clamp

---

## 8. Acceptance Criteria

### 레이아웃 기본

**AC-CHU-01** | `MANUAL` | ADVISORY
- **GIVEN** Offering 상태, 카드 3장 정상 로드, 해상도 800×600
- **WHEN** 카드 패 Reveal 완료
- **THEN** 카드 3장이 화면 내에 완전히 표시됨. 텍스트(DisplayName, Description)가 읽힐 수 있는 크기. 카드 간 겹침 없음

**AC-CHU-02** | `AUTOMATED` | BLOCKING
- **GIVEN** 카드 3장 ID 배열, 해상도 1280×720, 기본 Tuning Knob 값
- **WHEN** F-CHU-1 SlotX(i) 계산 (i=0,1,2)
- **THEN** SlotX(1) = ScreenCenterX. SlotX(0) = SlotX(1) − (CardWidthPx + CardSpacingPx). SlotX(2) = SlotX(1) + (CardWidthPx + CardSpacingPx). 세 X 좌표 모두 [0, ScreenWidth] 범위 내

**AC-CHU-03** | `CODE_REVIEW` | BLOCKING
- **GIVEN** Card Hand UI 위젯 구현 코드 (`Source/MadeByClaudeCode/UI/`)
- **WHEN** `FGiftCardRow` 필드를 카드 위젯에 바인딩하는 코드 검색
- **THEN** Tags, 수치 필드, 확률 정보를 UI에 노출하는 코드 0건. DisplayName + Description + IconPath만 바인딩

### 드래그-투-오퍼 제스처

**AC-CHU-04** | `MANUAL` | ADVISORY
- **GIVEN** Offering 상태, 카드 3장 표시
- **WHEN** 카드 위에서 LMB를 0.15초 이상 유지 후 드래그 → 유리병 위로 릴리스
- **THEN** 카드가 커서를 따라 이동. 유리병 근처에서 Offer Zone 시각 피드백. 릴리스 후 건넴 확정 애니메이션 → 카드 패 Hide

**AC-CHU-05** | `INTEGRATION` | BLOCKING
- **GIVEN** Offering 상태, Ready Card System, `DailyHand = ["Card_A", "Card_B", "Card_C"]`
- **WHEN** 카드 A 드래그 → Offer Zone 내 릴리스
- **THEN** `ConfirmOffer("Card_A")` 호출 → `true` 반환. `FOnCardOffered("Card_A")` 발행 1회. 건넴 애니메이션 시작 → Hide 시퀀스

**AC-CHU-06** | `INTEGRATION` | BLOCKING
- **GIVEN** Offering 상태, 카드 드래그 중 (Dragging 상태)
- **WHEN** Offer Zone 밖에서 LMB 릴리스
- **THEN** `ConfirmOffer` 미호출. 카드가 `CardReturnDurationSec` 내 원래 슬롯으로 복귀. Card Hand UI가 Idle 상태 복귀 — 재선택 가능

**AC-CHU-07** | `AUTOMATED` | BLOCKING
- **GIVEN** OfferZoneRadiusPx=80, JarScreenPos=(640, 360)
- **WHEN** F-CHU-3 IsInOfferZone 계산. CardCenter=(620, 400) 및 (500, 300) 각각 테스트
- **THEN** (620, 400) → Distance≈44.7 → `true`. (500, 300) → Distance≈152.3 → `false`

### 상태 전환

**AC-CHU-08** | `INTEGRATION` | BLOCKING
- **GIVEN** Hidden 상태
- **WHEN** `FOnGameStateChanged(Dawn, Offering)` 수신
- **THEN** `GetDailyHand()` pull. Revealing 상태 진입. 공개 순서: 좌→중→우, `CardRevealStaggerSec` 간격. 완료 후 Idle

**AC-CHU-09** | `INTEGRATION` | BLOCKING
- **GIVEN** Idle 상태
- **WHEN** `FOnGameStateChanged(Offering, Waiting)` 수신 (카드 건넴 완료)
- **THEN** Hiding 상태 진입. `CardHideDurationSec` 내 카드 패 Hide. 완료 후 Hidden

**AC-CHU-10** | `INTEGRATION` | BLOCKING
- **GIVEN** Dragging 상태 (카드 드래그 중)
- **WHEN** `FOnGameStateChanged(Offering, _)` 강제 이탈 수신
- **THEN** 드래그 즉시 취소. 전체 카드 패 Hide. `ConfirmOffer` 미호출

### 피드백 및 접근성

**AC-CHU-11** | `MANUAL` | ADVISORY
- **GIVEN** Offering 상태, 카드 3장 표시
- **WHEN** 마우스를 카드 위에 올리기만 (클릭 없이)
- **THEN** 시각 하이라이트(카드 테두리 밝아짐)만 발생. 카드 건넴·선택 게임 상태 변화 없음 (Hover-only 금지)

**AC-CHU-12** | `MANUAL` | ADVISORY
- **GIVEN** 게임패드 미연결, 키보드 미사용
- **WHEN** 마우스만으로 카드 선택 → 드래그 → 건넴
- **THEN** 모든 인터랙션 완결. 키보드·게임패드 없이 차단되는 기능 없음

**AC-CHU-13** | `AUTOMATED` | BLOCKING
- **GIVEN** `bReducedMotionEnabled = true`
- **WHEN** Offering 진입 (카드 Reveal), 드래그 취소 (복귀), 건넴 (확정 애니메이션)
- **THEN** 모든 경우 애니메이션 없이 즉각 표시/Hide. Stillness Budget `IsReducedMotion()` = true 경로 확인

### Degraded 모드

**AC-CHU-14** | `AUTOMATED` | BLOCKING
- **GIVEN** Card System Degraded Ready, `DailyHand.Num() = 0`
- **WHEN** `FOnGameStateChanged(Dawn, Offering)` 수신
- **THEN** 빈 슬롯 3개 표시. 드래그 입력 없음. `UE_LOG(Error)` 1회

**AC-CHU-15** | `AUTOMATED` | BLOCKING
- **GIVEN** Card System Degraded Ready, `DailyHand.Num() = 2`
- **WHEN** Offering 진입
- **THEN** 카드 2장 정상 표시. 빈 슬롯 1개. 2장 중 1장 드래그 → `ConfirmOffer` 정상 호출

### Offer Zone

**AC-CHU-16** | `MANUAL` | ADVISORY
- **GIVEN** Offering 상태, 카드 드래그 중
- **WHEN** 드래그 카드 중심이 Offer Zone 경계를 통과
- **THEN** 진입 시 Offer Zone 시각 피드백(카드 밝아짐, Zone 파장) 활성. 이탈 시 피드백 비활성

### AC 요약

| 유형 | 수량 | Gate |
|---|---|---|
| AUTOMATED | 5 | BLOCKING |
| INTEGRATION | 5 | BLOCKING |
| CODE_REVIEW | 1 | BLOCKING |
| MANUAL | 5 | ADVISORY |
| **합계** | **16** | — |

---

## Implementation Notes

다음은 GDD 규칙이 아닌 구현 단계 권장 사항이다.

- **UMG 위젯 구조**: `UCardHandWidget` (전체 패 컨테이너) → `UCardSlotWidget` × 3 (개 카드). `UCardSlotWidget`은 `FGiftCardRow` 데이터를 바인딩하는 View. Dragging 상태는 UMG DragDropOperation 또는 Widget Transform 직접 제어로 구현 가능 — ue-umg-specialist 협의 필요
- **Offer Zone 시각화**: `JarScreenPos`는 유리병 Actor의 `UWidgetInteractionComponent` 또는 월드 좌표를 `UGameViewportClient::WorldToScreen()`으로 변환. 매 프레임 갱신 — Tick 또는 `FTSTicker`
- **드래그 구현 방식 (ADR 후보)**: UMG의 `NativeOnMouseButtonDown` / `NativeOnMouseMove` 직접 오버라이드 vs. Input Abstraction의 `IA_PointerMove` + `IA_OfferCard` 구독. 후자가 Input Abstraction 계약에 부합. `/architecture-decision` 결정 필요
- **텍스처 로딩**: `FGiftCardRow.IconPath`는 `FSoftObjectPath` — 동기 로드 금지. `UAssetManager::GetStreamableManager().AsyncLoad()` 또는 `TSoftObjectPtr<UTexture2D>` 사용. 로딩 중 플레이스홀더(회색 사각형) 표시. Data Pipeline GDD R6 비동기 로드 책임 참조
- **Gamepad 포커스 네비게이션**: UMG Widget Focus System (`SetUserFocus`, `SetFocus`) 또는 CommonUI의 포커스 관리. VS 구현 시 ue-umg-specialist와 협의
- **800×600 레이아웃 테스트**: `UGameViewportClient::SetViewportSize(800, 600)` 에디터 내 테스트로 검증. 자동화는 PIE headless로 가능

---

## Open Questions

| # | 질문 | 소유자 | 해결 대상 | 상태 |
|---|---|---|---|---|
| ~~OQ-CHU-1~~ | ~~UMG 드래그 구현 방식~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0008](../../docs/architecture/adr-0008-umg-drag-implementation.md) — Enhanced Input `IA_OfferCard` Hold + `IA_PointerMove` 구독 채택. UMG NativeOnMouse* override + UDragDropOperation 사용 금지 — Input Abstraction Rule 1 준수) |
| OQ-CHU-2 | 카드 공개 애니메이션 방향 — 하단 슬라이드 업 vs. 페이드 인. Art Bible §Terrarium Dusk 분위기와 어울리는 방향 | `art-director` | Card Hand UI 구현 전 | **Open** |
| OQ-CHU-3 | DPI 스케일 변화 시 OfferZoneRadiusPx 보정 — `UUserInterfaceSettings::GetDPIScaleBasedOnSize()`로 자동 보정 필요 여부. 현재는 픽셀 절대값 | `ue-umg-specialist` | 구현 단계 | **Open** |
| OQ-CHU-4 | Offer Zone 위치 — 유리병 스크린 좌표를 매 프레임 계산할 때 `FTSTicker` vs. UMG Tick 중 어느 것이 적합한가. 씬 렌더와 UI 렌더 순서 의존성 있음 | `ue-umg-specialist` | 구현 단계 | **Open** |
| OQ-CHU-5 | 카드 시각 디자인 확정 — 카드 크기 비율(120×170), 카드 테두리 스타일, 배경 텍스처가 Art Bible과 일치하는지 Art Director 검토 필요 | `art-director` | `/art-bible` 실행 후 | **Open** |
| OQ-CHU-6 | Input Abstraction OQ-3 연동 — `IA_NavigateUI`의 Gamepad D-pad vs. 좌측 스틱 바인딩이 Card Hand UI의 포커스 이동에 자연스러운가. VS 구현 시 | `ux-designer` + `game-designer` | VS Gamepad 구현 시 | **Open** |
