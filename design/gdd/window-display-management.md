# Window & Display Management

> **Status**: Not Started → In Review
> **Author**: bong9tutor + claude-code session
> **Created**: 2026-04-18
> **Last Updated**: 2026-04-18
> **Implements Pillar**: Pillar 1 (조용한 존재 — 알림 없음, 강요 없음, 데스크톱 오브제), Pillar 2 (선물의 시학 — 카드 건넴 의식은 창이 어떻게 놓여 있든 온전히 치러진다)
> **Priority**: MVP | **Layer**: Meta | **Category**: Meta
> **System #**: 14
> **Effort**: S (1 세션)

---

## A. Overview

Window & Display Management는 Moss Baby가 **데스크톱 동반자 앱**으로서 올바르게 존재하도록 만드는 창 인프라 계층이다. 일반적인 전체화면 게임과 달리 이 게임은 플레이어의 업무 화면 한켠에 작은 창으로 조용히 앉아있는 오브제다. 이 시스템은 (1) 최소 800×600의 창 크기 제한과 기본 창 모드 설정, (2) 고DPI 디스플레이에서의 올바른 UI 스케일링, (3) 포커스 상실/획득 감지와 Time & Session System 연동, (4) "항상 위에 표시(Always-on-top)" 선택지 제공, (5) 창 크기 변경 시 레이아웃 적응, (6) 창 모드 옵션(창 모드·테두리 없는 창·전체화면), (7) 다중 모니터 간 이동 시 DPI 대응을 담당한다. 플레이어는 이 시스템의 존재를 인식하지 않는다 — 창이 책상 위 진짜 유리병처럼 그냥 거기 있을 뿐이다.

---

## B. Player Fantasy

플레이어의 책상 위에 작은 테라리움 유리병이 놓여 있다. 탭 하나를 여닫는 것처럼 가볍게 열고, 이끼 아이에게 오늘의 카드를 건네고, 다시 옆으로 밀어두는 것이 자연스럽다. 카드 건넴이라는 작은 의식은 포커스가 어디 있든, 창이 얼마나 작든, 4K 모니터에서 1080p 모니터로 옮겨갔든 — 언제나 온전하게 치러진다.

이 시스템이 지키는 감각: **"이 창은 내 방해물이 아니라 내 책상의 일부다."** 다른 앱 위로 불쑥 튀어나오거나 배경으로 물러나며 소리를 지르지 않는다. 알림도, 점멸도, "돌아오세요" 팝업도 없다. 있어도 좋고 없어도 좋은 것처럼 — 그냥 거기 있다.

**Pillar 연결**:
- **Pillar 1 (조용한 존재)** — 창이 배경으로 물러날 때 소리나 알림 없음. 태스크바 깜빡임 없음. 항상 위에 표시는 강요가 아닌 플레이어의 선택. "지금 돌아오세요" 모달 없음
- **Pillar 2 (선물의 시학)** — 카드 건넴 의식은 창 크기·DPI·모니터 종류에 관계없이 의도한 시각·입력 품질을 유지

---

## C. Detailed Design

### Core Rules

**Rule 1 — 기본 창 모드**: 게임은 **창 모드(Windowed)**로 시작한다. 전체화면이 기본값이 아니다. 이 선택은 "데스크톱 동반자" 정체성의 물리적 표현이다.

**Rule 2 — 최소 창 크기**: 창 크기는 **800×600px** 미만으로 줄어들지 않는다. 800×600은 "책상 오브제 모드"의 최소 단위이며, 이 크기에서 모든 UI 요소(카드 3장, 일기 버튼, 이끼 아이 씬)가 판독 가능해야 한다.

**Rule 3 — 기본 창 크기**: 첫 실행 시 기본 창 크기는 **1024×768px**이다. 이전 세션의 창 크기·위치는 Save/Load(#3)가 `FWindowState`로 영속화하고, 다음 실행 시 복원된다.

**Rule 4 — 창 모드 옵션**: 다음 세 가지 창 모드를 지원한다. 설정은 Title & Settings UI(#16, VS)에서 변경 가능하다.
| 모드 | 설명 | 기본값 |
|---|---|---|
| `Windowed` | 표준 창. 크기 조절·이동 가능. 태스크바 버튼 표시 | **기본** |
| `BorderlessWindowed` | 테두리 없는 창. 전체화면처럼 보이나 다른 앱으로 즉시 전환 가능. 창 이동 불가 | 선택 |
| `Fullscreen` | 독점 전체화면. Steam Deck 게임 모드에서 자동 적용 | Steam Deck 자동 |

**Rule 5 — DPI 인식 및 UI 스케일링**: 고DPI 디스플레이에서 UI와 씬이 흐릿해지거나 너무 작아지지 않도록 시스템 DPI를 읽어 UE ViewportClient의 DPI 스케일 인수를 계산한다(Formula 1 참조). 사용자가 `DPIScaleOverride`를 설정하면 자동 계산을 무시하고 고정값을 사용한다.

**Rule 6 — 포커스 관리**: 창이 포커스를 잃거나 획득할 때 이 시스템이 관련 시스템에 알린다.
- 포커스 상실 → 게임 오디오 **자동 음소거 없음** (플레이어가 선택). Time & Session System의 `OnWindowFocusLost` 콜백 호출 (배경에서도 실시간 경과는 계속).
- 포커스 상실 → **게임 일시정지 없음**. 이끼 아이는 포커스와 무관하게 살아있다 (Pillar 4: "끝이 있다"와 동일 — 이끼 아이가 살아낸 시간은 플레이어 출석이 아니다).
- 포커스 획득 → 커서 가시성 복원 (Input Abstraction Layer(#4)의 현재 `EInputMode` 기준).
- **알림 없음**: 포커스 상실/획득 시 어떠한 팝업·모달·알림도 생성하지 않는다 (Pillar 1).

**Rule 7 — 항상 위에 표시 (Always-on-top)**: 플레이어가 선택하면 이 창이 다른 앱 위에 항상 표시된다. 이것은 "책상 위 진짜 유리병"을 모방하는 선택적 기능이다.
- 기본값: **비활성** (Tuning Knob `bAlwaysOnTopDefault`).
- 변경: 우클릭 컨텍스트 메뉴 또는 Title & Settings UI(#16, VS)에서 토글.
- `bAlwaysOnTop == true`일 때: `FGenericWindow::SetWindowMode` + OS API(`SetWindowPos(HWND_TOPMOST)`, Windows)로 Always-on-top 설정.
- **Always-on-top 상태에서도 포커스를 강제하지 않는다.** "위에 표시"는 보이는 순서일 뿐, 포커스를 빼앗지 않는다.

**Rule 8 — 창 크기 변경 시 레이아웃 적응**: 플레이어가 창 크기를 변경하면 UI와 씬이 적응한다.
- 800×600 최소값 아래로는 OS 수준에서 리사이즈가 차단된다 (Rule 2).
- 800×600 이상에서: UI는 앵커 기반 스케일링 (UMG 뷰포트 기준). 씬(테라리움 씬)은 비율 유지 + 레터박스(Formula 2 참조).
- 리사이즈 완료(마우스 드래그 종료) 후 최종 창 크기를 영속화 (Save/Load #3).

**Rule 9 — Steam Deck 호환**: Steam Deck 게임 모드에서는 `Fullscreen` 모드로 자동 전환된다. 1280×800 해상도가 적용되며, DPI 스케일은 1.0으로 고정한다. 모든 UI 요소가 이 해상도에서 판독 가능해야 한다 (Rule 2의 800×600 기준 충족 확인).

**Rule 10 — 모달 금지**: 이 시스템은 어떠한 경우에도 플레이어를 차단하는 모달 다이얼로그를 생성하지 않는다 (Pillar 1). "정말 종료하시겠습니까?", "창 크기를 변경하시겠습니까?" 등 모든 확인 팝업 금지. 설정 변경은 즉시 적용되며 되돌리기는 설정 메뉴에서 직접 처리한다.

---

### Window States

| 상태 | 설명 | 조건 | 유효 전환 |
|---|---|---|---|
| `Active` | 포커스 있음. 플레이어 입력 정상 수신 | 창이 포커스를 가짐 | → `Background` (Alt-Tab 등 포커스 상실) / → `Minimized` (최소화) |
| `Background` | 포커스 없음. 창은 보임. 이끼 아이는 계속 살아있음 | 포커스 상실 | → `Active` (포커스 획득) / → `Minimized` (최소화) |
| `Minimized` | 창이 최소화됨. 이끼 아이는 계속 살아있음 | 창 최소화 | → `Active` (복원 시 포커스 획득) / → `Background` (복원 시 포커스 없음) |

**전환 부수효과**:
- `Active → Background`: `OnWindowFocusLost()` 콜백 발행. 오디오·렌더 변경 없음(별도 설정 없을 시).
- `Background → Active`: `OnWindowFocusGained()` 콜백 발행. Input Abstraction(#4)에서 커서 가시성 재적용.
- `* → Minimized`: UE `UGameViewportClient::bDisableWorldRendering = true` (선택적 최적화 — Tuning Knob). 렌더 중단으로 CPU/GPU 절약.
- `Minimized → Active/Background`: 렌더 재개.

---

### Interactions with Other Systems

#### 1. Input Abstraction Layer (#4) — 수신

| 방향 | 데이터 | 시점 |
|---|---|---|
| 수신 | `OnInputModeChanged(EInputMode)` | 포커스 복귀 시 커서 가시성 결정에 활용 |

- 포커스가 돌아올 때 현재 `EInputMode`가 `Mouse`이면 커서 표시, `Gamepad`이면 커서 숨김.
- Window & Display는 Input에게 명령하지 않는다 — 단방향 수신.

#### 2. Time & Session System (#1) — 발행

| 방향 | 데이터 | 시점 |
|---|---|---|
| 발행 | `OnWindowFocusLost()` 콜백 | 창 포커스 상실 시 |
| 발행 | `OnWindowFocusGained()` 콜백 | 창 포커스 획득 시 |

- Time & Session System은 이 신호로 suspend/resume 감지를 **보조**한다. OS suspend(PBT_APMSUSPEND)와는 별개의 이벤트.
- 포커스 상실 ≠ 세션 일시정지. Time & Session System은 실시간 경과를 계속 추적한다.

#### 3. Save/Load Persistence (#3) — 발행

| 방향 | 데이터 | 시점 |
|---|---|---|
| 발행 | `FWindowState { Position, Size, bAlwaysOnTop, WindowMode, DPIScaleOverride }` | 창 이동·리사이즈 완료 시, 설정 변경 시 |

- Save/Load가 이 데이터를 `FMossSaveData`에 포함해 영속화.
- 다음 실행 시 `FWindowState`를 읽어 창 위치·크기 복원.

#### 4. Game State Machine (#5) — 간접

- Window & Display는 GSM을 직접 구독하지 않는다.
- `Background`/`Minimized` 상태에서 UE의 `FWorldDelegates::OnWorldCleanup` 같은 생명주기 이벤트가 발생해도 GSM은 독립 진행한다.
- 예외: `Minimized` 상태의 렌더 중단(`bDisableWorldRendering`)은 이 시스템이 직접 설정. GSM 협의 불필요.

---

## D. Formulas

### Formula 1 — DPI 스케일 계산

OS에서 보고하는 디스플레이 DPI를 기준 96 DPI 대비 비율로 변환해 UMG ViewportClient에 적용하는 인수를 산출한다.

```
DPIScale = SystemDPI / ReferenceDPI
```

단, `DPIScaleOverride > 0.0`이면 자동 계산을 무시하고 `DPIScaleOverride`를 직접 사용한다.

| 변수 | 기호 | 타입 | 범위 | 설명 |
|---|---|---|---|---|
| SystemDPI | D_sys | `float` | `[72, 384]` (실용 범위) | OS에서 읽은 현재 디스플레이 DPI. Windows: `GetDpiForWindow()` 또는 `GetDeviceCaps(LOGPIXELSX)` |
| ReferenceDPI | D_ref | `float` (상수) | 96.0 (고정) | 100% 스케일의 기준 DPI (Windows 표준). 튜닝 불필요 |
| DPIScaleOverride | D_ov | `float` | `[0.5, 2.0]` (0 = 자동) | 플레이어가 수동으로 설정한 스케일. 0이면 Formula 1 자동 사용 |
| DPIScale | S | `float` | `[0.75, 4.0]` (자동 시 이론 범위) | UMG에 전달하는 최종 스케일 인수 |

**클램프**: `DPIScale = clamp(DPIScale, 0.5, 3.0)` — 극단적 DPI 환경(초저 DPI 오래된 모니터, 초고 DPI 전문 모니터)에서 레이아웃 붕괴 방지.

**예시 계산**:
- 96 DPI (기본): `96 / 96 = 1.0` → 스케일 변경 없음.
- 144 DPI (150% 스케일): `144 / 96 = 1.5` → UI가 1.5배 스케일업.
- 192 DPI (200% 스케일, 4K 노트북): `192 / 96 = 2.0` → UI가 2배 스케일업.
- 수동 오버라이드 `D_ov = 1.25` 설정 시: 시스템 DPI 무시, `DPIScale = 1.25` 고정.

> **구현 참고 (UE 5.6)**: `UUserInterfaceSettings::GetDPIScaleBasedOnSize()` 또는 `FSlateApplication::Get().GetApplicationScale()`을 통해 플랫폼 DPI를 읽을 수 있다. 다중 모니터 환경에서 창이 이동하면 `FDisplayMetrics`를 재조회한다. UMG는 내부적으로 DPI Scale을 `GetDPIScaleBySize()`로 처리하므로, 커스텀 스케일 적용 시 `UUserInterfaceSettings` 에셋의 DPI Curve를 조정하거나 `SWindow::SetDPIScaleFactor()`를 호출한다.

---

### Formula 2 — 씬 비율 레터박스 계산

테라리움 씬(이끼 아이 + 유리병)은 **16:9 이상 와이드 화면**에서 좌우 레터박스를 추가해 씬의 가로세로 비율을 유지한다. 씬 자체를 찌그러뜨리지 않는다.

```
// 씬의 고정 종횡비 (기본: 4:3)
TargetAR = SceneWidth_px / SceneHeight_px

// 현재 뷰포트 비율
ViewportAR = ViewportWidth_px / ViewportHeight_px

// 레터박스 계산
IF ViewportAR > TargetAR THEN
    // 뷰포트가 더 넓음 → 좌우 레터박스
    RenderHeight = ViewportHeight_px
    RenderWidth  = floor(RenderHeight * TargetAR)
    OffsetX      = floor((ViewportWidth_px - RenderWidth) / 2)
    OffsetY      = 0
ELSE IF ViewportAR < TargetAR THEN
    // 뷰포트가 더 좁음 → 상하 레터박스 (필박스)
    RenderWidth  = ViewportWidth_px
    RenderHeight = floor(RenderWidth / TargetAR)
    OffsetX      = 0
    OffsetY      = floor((ViewportHeight_px - RenderHeight) / 2)
ELSE
    // 정확히 일치 → 레터박스 없음
    RenderWidth  = ViewportWidth_px
    RenderHeight = ViewportHeight_px
    OffsetX      = 0
    OffsetY      = 0
```

| 변수 | 타입 | 범위 | 설명 |
|---|---|---|---|
| `SceneWidth_px` / `SceneHeight_px` | `int32` | 기본 4:3 (800:600) | 씬의 기준 해상도. Tuning Knob `SceneAspectRatioNumerator/Denominator` |
| `ViewportWidth_px` / `ViewportHeight_px` | `int32` | `[800, ∞) × [600, ∞)` | 현재 창 내부 뷰포트 크기 |
| `RenderWidth` / `RenderHeight` | `int32` | 뷰포트 이하 | 씬이 실제로 렌더되는 영역 크기 |
| `OffsetX` / `OffsetY` | `int32` | `[0, ∞)` | 씬 렌더 영역의 뷰포트 내 오프셋 (레터박스 폭) |

**예시 계산** (씬 TargetAR = 4/3 ≈ 1.333):
- 뷰포트 1280×720 (AR = 1.778 > 1.333) → 좌우 레터박스: `RenderHeight = 720, RenderWidth = 960, OffsetX = 160, OffsetY = 0`
- 뷰포트 800×600 (AR = 1.333 = 1.333) → 레터박스 없음: `RenderWidth = 800, RenderHeight = 600`
- 뷰포트 1024×768 (AR = 1.333 = 1.333) → 레터박스 없음: `RenderWidth = 1024, RenderHeight = 768`

> **구현 참고**: UE의 `UGameViewportClient`를 상속하거나 `FViewport` 리사이즈 콜백에서 이 계산을 실행. 또는 UMG의 `SConstraintCanvas`와 `UCanvasPanel`의 앵커/오프셋으로 씬 영역을 제한하는 접근도 유효.

---

## E. Edge Cases

### EC-01 — 모니터 분리 (다중 모니터 환경)
창이 표시되던 모니터가 분리되면 OS가 창을 기본(primary) 모니터로 이동시킨다. 이 시스템은 `FDisplayMetrics` 변경을 감지하고:
1. 새 모니터의 DPI로 Formula 1 재계산.
2. 창 위치가 유효 모니터 범위 내인지 확인 — 범위 밖이면 기본 모니터 중앙으로 이동.
3. `FWindowState`를 새 위치·DPI 스케일로 갱신 후 영속화.
알림 없음 (Pillar 1).

### EC-02 — 세션 중 DPI 변경
사용자가 Windows 디스플레이 설정에서 DPI 스케일을 변경하면 `WM_DPICHANGED` 메시지 수신. 이 시스템은:
1. 새 `SystemDPI`로 Formula 1 재계산.
2. UMG DPI 스케일 인수 즉시 갱신.
3. 창 크기를 새 DPI 기준 권장 크기로 조정 (OS 제안값 우선, 단 최소 800×600 강제).
재시작 불필요, 플레이어에게 알림 없음 (Pillar 1).

### EC-03 — Alt-Tab 빠른 전환 (연속 포커스 상실/획득)
짧은 시간 내 Alt-Tab 다회 반복 시:
- `OnWindowFocusLost` / `OnWindowFocusGained` 콜백이 연속 발행될 수 있다.
- Time & Session System은 이 이벤트를 수신하지만, 포커스 전환은 시간 경과 분류에 영향을 주지 않는다 (세션은 포커스와 무관하게 진행).
- **디바운스**: 50ms 이내 연속 포커스 이벤트는 하나로 병합. `FTimerHandle`로 구현.
- 이끼 아이 씬 렌더는 중단 없이 계속된다.

### EC-04 — Always-on-top + 다른 앱 전체화면
사용자가 `bAlwaysOnTop = true` 상태에서 다른 앱을 전체화면으로 실행하면:
- 다른 앱의 독점 전체화면(`EWindowMode::Fullscreen`)은 OS 수준에서 Always-on-top 창을 가린다. **의도된 동작** — OS 정책 우선.
- 다른 앱이 테두리 없는 창(BorderlessWindowed)이라면 Always-on-top이 유지된다.
- 이 시스템은 이 상황에서 어떠한 알림도 생성하지 않는다. 사용자가 Alt-Tab으로 Moss Baby로 돌아오면 Always-on-top이 다시 적용된다.

### EC-05 — Steam Deck 창 모드 전환
Steam Deck에서 게임 모드(전체화면)를 벗어나 데스크톱 모드로 전환 시:
- `Fullscreen` → `Windowed` 전환을 감지하면 1024×768 기본 창 크기로 복원.
- 저장된 `FWindowState`가 있으면 복원값 우선.
- DPI 스케일 재계산 (데스크톱 모드에서 연결된 외부 모니터 기준).

### EC-06 — 창 최소화 중 카드 건넴 이벤트 불가
창이 최소화 상태(`Minimized`)에서는 플레이어 입력이 없으므로 카드 건넴이 발생하지 않는다. 이끼 아이의 실시간 경과(Time & Session System)는 계속된다. 창 복원 시 정상 상태로 진입. 별도 처리 불필요.

### EC-07 — 저장된 창 위치가 현재 모니터 밖
이전 세션에서 다중 모니터를 사용했고, 현재 세션에서 보조 모니터가 없어 `FWindowState.Position`이 화면 밖을 가리킬 때:
1. `FDisplayMetrics::PrimaryDisplayWidth/Height`와 비교.
2. 창의 적어도 절반(50%)이 화면 안에 있어야 함. 그렇지 않으면 기본 모니터 중앙으로 이동.
알림 없음 (Pillar 1).

### EC-08 — 창 크기 800×600 미만 시도
UE `SWindow::SetSizingRule(ESizingRule::UserSized)` + `SetContent(...)`으로 최소 크기를 강제한다. OS 리사이즈 핸들로 800×600 이하로 드래그 시도 시 800×600에서 고정.
- `FWindowSizeConstraints`를 `SWindow` 생성 시 설정: `MinWidth = 800, MinHeight = 600`.

### EC-09 — 다중 모니터 DPI 혼합 (모니터별 DPI 상이)
창이 96 DPI 모니터에서 192 DPI 모니터로 이동할 때 `WM_DPICHANGED` 수신. EC-02와 동일 처리. 이동 중간(창이 두 모니터에 걸친 상태)에서는 창 중심이 위치한 모니터의 DPI를 기준으로 한다.

---

## F. Dependencies

### Upstream Dependencies (이 시스템이 의존하는 대상)

| 의존 대상 | 의존 강도 | 인터페이스 | 비고 |
|---|---|---|---|
| UE 5.6 `SWindow` / `FGenericWindow` | Hard — engine | `SetWindowMode`, `SetSizingRule`, `SetMinimumSize`, `GetDPIScaleFactor`, `SetDPIScaleFactor`, `HWND SetWindowPos` (Windows) | OS 창 관리의 기반. UE 5.6에서 변경 없음 (2026-04-18 확인) |
| UE 5.6 `UGameViewportClient` | Hard — engine | `bDisableWorldRendering`, 뷰포트 리사이즈 콜백 | 최소화 시 렌더 중단에 사용 |
| `FDisplayMetrics` (UE 플랫폼) | Hard — engine | `PrimaryDisplayWidth/Height`, DPI 정보 | 다중 모니터 감지·DPI 읽기 |
| Input Abstraction Layer (#4) | Soft | `OnInputModeChanged(EInputMode)` delegate 구독 | 포커스 복귀 시 커서 가시성 결정 |
| Save/Load Persistence (#3) | Soft | `FWindowState` 구조체 영속화 / 복원 | 창 위치·크기·설정 저장. Save/Load GDD §Data Schema에 `FWindowState` 추가 필요 |

### Downstream Dependents (이 시스템에 의존하는 시스템)

| 시스템 | 의존 강도 | 인터페이스 | 데이터 방향 |
|---|---|---|---|
| **Time & Session System (#1)** | Soft | `OnWindowFocusLost()` / `OnWindowFocusGained()` 콜백 | Window → Time (단방향) |
| **Companion Mode (#19, Full)** | Hard | 창 핸들(HWND), Always-on-top 상태, `FWindowState` | Window → Companion (이 시스템이 기반 제공) |

### Bidirectional Consistency Notes

- **Input Abstraction Layer (#4) GDD 참조 확인**: "Window & Display (#14)는 `OnInputModeChanged`를 구독해 포커스 복귀 시 커서 가시성을 결정한다" — Input GDD §Downstream Dependents에 이미 명시됨 (확인).
- **Time & Session System (#1) GDD 참조 확인**: `OnWindowFocusLost` 콜백은 Time GDD의 §States — `Suspended` 상태 진입 트리거와 구분된다. 포커스 상실은 세션을 일시정지하지 않으며, OS suspend(PBT_APMSUSPEND)와 별개이다. Time GDD에 Window와의 관계 명시 권장.
- **Save/Load GDD (#3)**: `FMossSaveData`에 `FWindowState` 필드 추가가 필요하다. Save/Load GDD 작성 시 이 의존성을 명시할 것.

### Data Ownership

| 데이터/타입 | 소유 시스템 | 읽기 허용 | 쓰기 허용 |
|---|---|---|---|
| `EWindowDisplayMode` enum | Window & Display (정의) | Title & Settings UI, Save/Load | Window & Display (유일) |
| `FWindowState` 구조체 | Window & Display (정의) | Save/Load (영속화) | Window & Display (유일) |
| `bAlwaysOnTop` 런타임 상태 | Window & Display | Title & Settings UI (읽기 전용 표시) | Window & Display (유일) |
| `CurrentDPIScale` 런타임 값 | Window & Display | UMG ViewportClient | Window & Display (유일) |

---

## G. Tuning Knobs

모든 튜닝 노브는 `assets/data/window_display_settings.ini` (또는 UE `UDeveloperSettings` 서브클래스)에 정의된다. 하드코딩 금지.

| 노브 | 기본값 | 안전 범위 | 카테고리 | 너무 높으면 | 너무 낮으면 | 참조 |
|---|---|---|---|---|---|---|
| `MinWindowWidth` | 800 px | `[640, 1280]` | Gate | 플레이어가 작게 줄일 수 없어 데스크톱 활용 방해 | UI 요소가 잘리거나 판독 불가 | Rule 2 |
| `MinWindowHeight` | 600 px | `[480, 960]` | Gate | 같은 이유 | 같은 이유 | Rule 2 |
| `DefaultWindowWidth` | 1024 px | `[800, 1920]` | Gate | 첫 실행 시 화면 너무 큼 | 기본이 너무 작아 이끼 아이가 작게 보임 | Rule 3 |
| `DefaultWindowHeight` | 768 px | `[600, 1080]` | Gate | 같은 이유 | 같은 이유 | Rule 3 |
| `bAlwaysOnTopDefault` | `false` | `{true, false}` | Feel | 다른 앱을 덮어 사용자 거부감 | (기본 false가 올바름 — "선택" 강조) | Rule 7 |
| `DPIScaleOverride` | `0.0` (자동) | `[0.0, 2.0]` | Feel | UI가 지나치게 커 창이 꽉 참 | UI가 너무 작아 판독 불가 | Formula 1 |
| `FocusEventDebounceMsec` | 50 ms | `[0, 200]` | Feel | 빠른 Alt-Tab 전환을 한 이벤트로 묶어 누락 | 진동성 포커스 이벤트 다수 발행 → Time System 과부하 | EC-03 |
| `bDisableRenderWhenMinimized` | `true` | `{true, false}` | Feel | — | 최소화 중에도 렌더로 CPU/GPU 소모 | §States |
| `SceneAspectRatioNumerator` | 4 | `[3, 16]` | Feel | 씬이 와이드해져 작은 창에서 씬이 찌그러짐 | 씬이 세로로 길어 공간 낭비 | Formula 2 |
| `SceneAspectRatioDenominator` | 3 | `[3, 9]` | Feel | 같은 이유 | 같은 이유 | Formula 2 |

**노브 카테고리**: Feel (순간 경험에 영향), Gate (창 크기 제한 — 직접 리사이즈 경험)

---

## H. Acceptance Criteria

> Given-When-Then 형식. QA 테스터가 GDD 없이 독립 검증 가능.

### AC-WD-01 — 최소 창 크기 강제
- **GIVEN** 게임 실행 중, 창 모드(`Windowed`)
- **WHEN** 창 리사이즈 핸들로 700×500 크기로 줄이려 시도
- **THEN** 창이 800×600 이하로 줄어들지 않음. 실제 창 크기 `Width >= 800 AND Height >= 600`
- **Evidence**: 리사이즈 시도 후 `FPlatformApplicationMisc::GetWorkArea()`로 창 크기 확인
- **Type**: MANUAL — **Source**: Rule 2, EC-08

### AC-WD-02 — 기본 창 크기
- **GIVEN** 세이브 데이터 없이 첫 실행
- **WHEN** 게임이 메인 루프에 진입
- **THEN** 창 크기 `1024 × 768` (허용 오차 ±1px)
- **Evidence**: 첫 프레임에서 `SWindow::GetPositionInScreen()` + `GetSizeInScreen()` 확인
- **Type**: AUTOMATED — **Source**: Rule 3

### AC-WD-03 — 창 위치·크기 영속화
- **GIVEN** 기본 창 크기 1024×768
- **WHEN** 창을 960×720으로 리사이즈하고 화면 좌상단으로 이동 후 게임 종료 → 재시작
- **THEN** 재시작 후 창 크기 960×720, 위치 복원
- **Evidence**: `FWindowState` 로드 후 창 속성 확인. 허용 오차 ±5px (OS 데코레이션 차이 고려)
- **Type**: INTEGRATION — **Source**: Rule 3, Rule 8, Save/Load #3

### AC-WD-04 — DPI 스케일 계산 (144 DPI)
- **GIVEN** `DPIScaleOverride = 0.0` (자동), 시스템 DPI = 144 (150% 스케일 환경)
- **WHEN** 게임 초기화 완료
- **THEN** `CurrentDPIScale == 1.5` (허용 오차 ±0.01)
- **Evidence**: `UGameViewportClient::GetDPIScaleFactor()` 또는 동등 API 반환값 확인
- **Type**: AUTOMATED — **Source**: Formula 1

### AC-WD-05 — DPI 오버라이드 적용
- **GIVEN** `DPIScaleOverride = 1.25`, 시스템 DPI = 144
- **WHEN** 게임 초기화 완료
- **THEN** `CurrentDPIScale == 1.25` (Formula 1 무시, 오버라이드 우선)
- **Evidence**: `CurrentDPIScale` 쿼리 == 1.25
- **Type**: AUTOMATED — **Source**: Formula 1

### AC-WD-06 — 포커스 상실 시 알림 없음
- **GIVEN** 게임 Active 상태, 카드 3장 표시 중
- **WHEN** Alt-Tab으로 다른 앱으로 포커스 이동
- **THEN** 알림·팝업·모달·태스크바 깜빡임 없음. 게임 일시정지 없음. 이끼 아이 씬 계속 렌더
- **Evidence**: 포커스 상실 전후 UI 스택 불변. `FMossNotificationSystem` 호출 0건
- **Type**: MANUAL — **Source**: Rule 6, Rule 10, Pillar 1

### AC-WD-07 — 포커스 상실 → Time System 콜백
- **GIVEN** Time & Session System 초기화됨, 게임 Active 상태
- **WHEN** 창이 포커스를 잃음 (Alt-Tab 또는 클릭)
- **THEN** `OnWindowFocusLost()` 콜백 1회 발행 (50ms 디바운스 후)
- **Evidence**: 콜백 수신 카운트 == 1; 발행 시각이 포커스 상실로부터 `FocusEventDebounceMsec` 이내
- **Type**: AUTOMATED — **Source**: Rule 6, EC-03

### AC-WD-08 — 포커스 획득 → 커서 가시성 복원 (Mouse 모드)
- **GIVEN** `EInputMode::Mouse`, 창이 Background 상태
- **WHEN** 창이 포커스를 획득
- **THEN** 커서가 표시됨 (`APlayerController::bShowMouseCursor == true`)
- **Evidence**: 포커스 획득 후 `bShowMouseCursor` 상태 확인
- **Type**: AUTOMATED — **Source**: Rule 6, §States

### AC-WD-09 — Always-on-top 기본값
- **GIVEN** 세이브 데이터 없이 첫 실행 (`bAlwaysOnTopDefault = false`)
- **WHEN** 게임이 메인 루프에 진입
- **THEN** `bAlwaysOnTop == false`. 창이 다른 앱 위에 강제 표시되지 않음
- **Evidence**: `FWindowState.bAlwaysOnTop == false`; Windows `GetWindowLong(GWL_EXSTYLE) & WS_EX_TOPMOST == 0`
- **Type**: MANUAL — **Source**: Rule 7

### AC-WD-10 — Always-on-top 토글 적용
- **GIVEN** 게임 실행 중, `bAlwaysOnTop == false`
- **WHEN** Always-on-top 토글을 활성화
- **THEN** 창이 즉시 다른 앱 위에 표시됨 (테스트: 메모장 위에 Moss Baby 창이 나타남)
- **Evidence**: Windows `GetWindowLong(GWL_EXSTYLE) & WS_EX_TOPMOST != 0`
- **Type**: MANUAL — **Source**: Rule 7

### AC-WD-11 — 씬 비율 유지 (와이드 뷰포트)
- **GIVEN** `SceneAspectRatioNumerator = 4, SceneAspectRatioDenominator = 3`, 창 크기 1280×720 (AR = 16:9)
- **WHEN** 렌더 프레임 캡처
- **THEN** 씬 렌더 영역 크기 960×720 (AR = 4:3 유지). 좌우 레터박스 각 160px. 씬 내 이끼 아이가 찌그러지지 않음
- **Evidence**: Formula 2 결과값 검증. 렌더 타겟 크기 확인
- **Type**: AUTOMATED — **Source**: Formula 2

### AC-WD-12 — 최소 창(800×600)에서 UI 판독 가능성
- **GIVEN** 창 크기 800×600 (최소값)
- **WHEN** 메인 게임 화면 (카드 3장 + 이끼 아이 씬 + 일기 버튼)
- **THEN** 모든 UI 요소(카드 텍스트, 버튼)가 잘리거나 겹치지 않음. 이끼 아이 씬이 표시됨
- **Evidence**: 스크린샷 캡처 + 리드 확인
- **Type**: MANUAL — **Source**: Rule 2

### AC-WD-13 — 모달 금지 (창 모드 전환)
- **GIVEN** `Windowed` 모드 실행 중
- **WHEN** `BorderlessWindowed` 모드로 변경 (설정 메뉴)
- **THEN** 확인 팝업·모달 없이 즉시 전환됨
- **Evidence**: 전환 전후 `UGameViewportClient` UI 스택에 모달 위젯 없음
- **Type**: MANUAL — **Source**: Rule 10, Pillar 1

### AC-WD-14 — 저장된 창 위치 화면 밖 복구
- **GIVEN** `FWindowState.Position = (-5000, -5000)` (화면 밖) 세이브 데이터 주입
- **WHEN** 게임 시작
- **THEN** 창이 기본 모니터 중앙으로 이동됨. 창의 적어도 50%가 화면 내에 있음
- **Evidence**: 시작 후 창 위치 쿼리가 유효 화면 범위 내
- **Type**: AUTOMATED — **Source**: EC-07

---

### Coverage 요약

| 분류 | Criterion 수 |
|---|---|
| 창 크기 제한 + 기본값 | 2 (AC-WD-01, 02) |
| 창 영속화 | 1 (AC-WD-03) |
| DPI 스케일링 | 2 (AC-WD-04, 05) |
| 포커스 관리 | 3 (AC-WD-06, 07, 08) |
| Always-on-top | 2 (AC-WD-09, 10) |
| 씬 비율 + 레이아웃 | 2 (AC-WD-11, 12) |
| 모달 금지 | 1 (AC-WD-13) |
| 이상 복구 (화면 밖) | 1 (AC-WD-14) |
| **Total** | **14** |
| AUTOMATED | 6 (42.9%) |
| INTEGRATION | 1 (7.1%) |
| MANUAL | 7 (50.0%) |

---

## Open Questions

| # | 질문 | 담당 | 해결 시점 |
|---|---|---|---|
| OQ-1 | `BorderlessWindowed` 모드에서 Always-on-top이 다른 `BorderlessWindowed` 앱과 경합할 때 OS 동작은? Windows DWM 정책 확인 필요 | `unreal-specialist` | MVP 구현 시 실기 확인 |
| OQ-2 | 최소화 중 렌더 중단(`bDisableWorldRendering`)이 이끼 아이의 머티리얼 애니메이션(시간 기반) 상태에 영향을 주는가? Moss Baby Character(#6)와 협의 필요 | `unreal-specialist` + `game-designer` | Moss Baby Character GDD 구현 시 |
| OQ-3 | Always-on-top 우클릭 컨텍스트 메뉴의 UX — 시스템 트레이 아이콘 방식 vs 창 제목표시줄 우클릭 방식 중 어느 쪽이 Pillar 1과 더 잘 어울리는가? | `ux-designer` | Title & Settings UI GDD (VS) 작성 시 |
| OQ-4 | Steam Deck 데스크톱 모드(Proton 환경)에서 `SetWindowPos(HWND_TOPMOST)` Windows API가 정상 동작하는가? Proton 호환성 조사 필요 | `unreal-specialist` | Steam Deck 실기 테스트 시 |

---

## Implementation Notes

> 설계 의도가 구현 단계에서 유실되지 않도록 프로그래머에게 전달하는 참고 사항.

1. **창 생성 시 최소 크기 설정**: `SWindow::FArguments`에서 `MinWidth(800).MinHeight(600)` 설정이 `SetSizingRule(ESizingRule::UserSized)`와 함께 사용되어야 OS 리사이즈 핸들이 최소값 아래로 내려가지 않는다.

2. **DPI 인식 등록**: Windows에서 Per-Monitor DPI Awareness V2를 사용하려면 `app.manifest`에 `<dpiAwareness>PerMonitorV2</dpiAwareness>` 선언이 필요하다. UE 5.6은 기본적으로 이를 처리하지만, 커스텀 창 생성 시 재확인할 것.

3. **Always-on-top 구현**: `FGenericWindow::SetNativeWindowButtonsVisibility` 대신 Windows 플랫폼에서 `::SetWindowPos(HWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE)`를 직접 호출. HWND 접근은 `FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle()`로 획득.

4. **포커스 이벤트 디바운스**: `FTimerHandle FocusDebounceTimer`를 `APlayerController` 또는 전용 `UGameInstanceSubsystem`에 두고, 포커스 변경마다 타이머를 리셋 후 `FocusEventDebounceMsec`(기본 50ms) 후 콜백 발행.

5. **최소화 감지**: `FSlateApplication`의 `OnWindowBeingDestroyed`나 `FCoreUObjectDelegates`보다 `UGameViewportClient::Activated` / `UGameViewportClient::Deactivated`가 최소화/복원에 더 신뢰성 있다. UE 5.6에서 `FGenericWindow::IsMinimized()` 폴링도 유효.

6. **레터박스 구현 접근 선택**: UMG 방식(창 스케일은 UMG가 처리하고 씬 뷰포트 영역을 `SConstraintCanvas`로 제한)이 커스텀 `UGameViewportClient` 서브클래스보다 유지보수 용이. 단, 씬 렌더 해상도가 UI 해상도와 분리되어야 한다면 커스텀 ViewportClient 접근이 필요.
