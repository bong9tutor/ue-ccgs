# Enhanced Input System — UE 5.6 Reference

*Last verified: 2026-04-17*
*Project pinned version: Unreal Engine 5.6*
*Relevant systems: Input Abstraction Layer (#4), Card Hand UI (#12), Window & Display (#14)*

> Enhanced Input은 UE 5.0에서 도입되어 5.1+에서 기본 활성화된 입력 프레임워크. 5.6에서 Input 도메인에 대한 breaking change는 발견되지 않음 (WebSearch 2026-04-17 확인).

---

## Core Architecture

### 4-Layer Model

```
[Hardware]  →  [UInputMappingContext]  →  [UInputAction]  →  [Handler]
 Key/Button     바인딩 묶음 (DataAsset)    행위 정의 (DataAsset)   C++ / BP 콜백
```

| Layer | Class | Lifetime | Description |
|---|---|---|---|
| **Action** | `UInputAction` | DataAsset (영속) | 개별 입력 행위 정의. `ValueType` (bool/float/Vector2D/Vector3D) 지정 |
| **Mapping Context** | `UInputMappingContext` | DataAsset (영속) | 키/버튼 → Action 매핑 묶음. Priority 기반 충돌 해소 |
| **Subsystem** | `UEnhancedInputLocalPlayerSubsystem` | PlayerController 귀속 | 매핑 컨텍스트 등록/제거. `AddMappingContext(Context, Priority)` |
| **Component** | `UEnhancedInputComponent` | Pawn/Controller 귀속 | Action을 핸들러 함수에 바인딩. `BindAction(Action, TriggerEvent, Object, Func)` |

### PlayerController 귀속 주의

Enhanced Input의 `UEnhancedInputLocalPlayerSubsystem`은 `ULocalPlayerSubsystem`이므로 **`APlayerController` 생존 시간에 귀속**된다. `UGameInstanceSubsystem`(Time, Dream 등)과 다른 수명.

- 이 프로젝트는 단일 PlayerController(레벨 전환 없음) → 수명 문제 없음
- 매핑 컨텍스트 등록은 `APlayerController::BeginPlay()` 또는 `SetupInputComponent()`에서

---

## Moss Baby 프로젝트 관련 판단

### 사용할 API

| API | 용도 | 비고 |
|---|---|---|
| `UInputAction` | 카드 선택, 건넴, 일기 열기, 메뉴 등 행위 정의 | DataAsset — 에디터에서 생성 |
| `UInputMappingContext` | Mouse 컨텍스트 + Gamepad 컨텍스트 분리 | Priority로 활성 컨텍스트 전환 |
| `UEnhancedInputComponent::BindAction()` | C++ 핸들러 바인딩 | `ETriggerEvent::Triggered`, `Started`, `Completed` 등 |
| `UInputModifier` | Dead zone, smoothing | Gamepad 스틱 입력 시 |
| `UInputTrigger` | Hold, tap, press-release 구분 | 카드 건넴 인터랙션용 |

### 사용하지 않을 API

| API | 이유 |
|---|---|
| **CommonUI** (`UCommonUIActionRouter`) | current-best-practices.md: "이 스코프에 과함". 단일 화면 + 최소 UI 전환 |
| **Legacy Input** (`APlayerController::InputComponent`) | UE 5.1+ deprecated path. Enhanced Input 전용 |
| **In-Game Remapping** (`UEnhancedInputUserSettings`, 5.3+) | MVP 범위 외. Vertical Slice 이후 검토 |

### Steam Deck / Gamepad 전략

- **Mouse 컨텍스트** (기본): `IA_SelectCard` → `LMB`, `IA_OfferCard` → `LMB Hold/Drag`, `IA_OpenJournal` → `J`
- **Gamepad 컨텍스트** (선택): `IA_SelectCard` → `A Button`, `IA_OfferCard` → `A Hold`, `IA_OpenJournal` → `Y Button`
- Steam Deck의 Steam Input은 **Gamepad Emulation** 모드에서 트랙패드를 마우스로 매핑 가능 → Mouse 컨텍스트로 작동
- **Hover-only 상호작용 금지** (technical-preferences.md) — Steam Deck 컨트롤러 모드에서 hover 불가능

### 입력 행위 예상 목록 (GDD에서 확정)

| Action 후보 | ValueType | 주 사용처 |
|---|---|---|
| `IA_PointerMove` | `Axis2D` | 마우스/트랙패드 커서 위치 (Card Hand UI) |
| `IA_Select` | `bool` | 카드 선택 / UI 확인 |
| `IA_Cancel` | `bool` | UI 닫기 / 뒤로 |
| `IA_OfferCard` | `bool` (Hold trigger) | 카드 건넴 (drag-to-offer는 Card Hand UI 소유) |
| `IA_NavigateUI` | `Axis2D` | Gamepad 방향키 UI 탐색 |
| `IA_OpenJournal` | `bool` | 꿈 일기 열기/닫기 |
| `IA_Pause` | `bool` | 메뉴/설정 |

---

## UE 5.6 Stability Notes

- Enhanced Input Plugin은 5.0 도입, 5.1 기본 활성화, 5.3 in-game remapping 추가
- 5.5 → 5.6 사이 Enhanced Input 관련 breaking change **없음** (공식 릴리스 노트 + 포럼 확인)
- `UEnhancedPlayerInput` API는 5.6에서 정상 유지 (공식 API 문서 확인)
- 5.5에서 일부 Enhanced Input 문제 보고 있었으나 5.6에서 수정된 것으로 판단

---

## Official References

- [Enhanced Input in Unreal Engine — Official Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/enhanced-input-in-unreal-engine)
- [UEnhancedPlayerInput — UE 5.6 API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/EnhancedInput/UEnhancedPlayerInput)
- [Enhanced Input C++ Tutorial](https://dev.epicgames.com/community/learning/tutorials/6dp3/unreal-engine-using-the-enhancedinput-system-in-c)
- [Steam Input Gamepad Emulation Best Practices](https://partner.steamgames.com/doc/features/steam_controller/steam_input_gamepad_emulation_bestpractices)
- [Gamepad UI Navigation with Enhanced Input](https://medium.com/@Jamesroha/dev-guide-gamepad-ui-navigation-in-unreal-engine-5-with-enhanced-input-3ab5403f8ab5)

---

## Maintenance

- GDD에서 새 Input Action을 추가할 때 이 문서의 "입력 행위 예상 목록" 갱신
- 엔진 업그레이드 시 `/setup-engine refresh`로 breaking-changes 재확인
- CommonUI 도입 결정 시 이 문서 + current-best-practices.md 동시 갱신
