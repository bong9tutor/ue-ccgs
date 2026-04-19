# Input Abstraction — Unit Test Evidence

**Story**: Story-1-12 (UMossInputAbstractionSubsystem + UMossInputAbstractionSettings + IMC 등록)
**Epic**: Foundation Input Abstraction
**ADR**: ADR-0011 (Tuning Knob UDeveloperSettings 채택)

## 실제 테스트 파일 위치

UE Automation 테스트는 UE 모듈 빌드 시스템 상 UE 모듈 내부에 위치해야 한다.
이 README는 CCGS evidence 인덱스 역할을 한다.

실제 파일:
`MadeByClaudeCode/Source/MadeByClaudeCode/Tests/InputAbstractionSubsystemTests.cpp`

구현 파일:
- `MadeByClaudeCode/Source/MadeByClaudeCode/Input/MossInputAbstractionSubsystem.h`
- `MadeByClaudeCode/Source/MadeByClaudeCode/Input/MossInputAbstractionSubsystem.cpp`
- `MadeByClaudeCode/Source/MadeByClaudeCode/Settings/MossInputAbstractionSettings.h`

---

## Story-1-12 AC 매핑

| AC | 내용 | 검증 유형 | 테스트 함수 |
|---|---|---|---|
| AC-1 Settings 3 knobs | CDO 기본값 (InputModeMouseThreshold=3.0, OfferDragThresholdSec=0.15, OfferHoldDurationSec=0.5) | AUTOMATED | `FInputSubsystemSettingsCDOTest` |
| AC-2 Subsystem 뼈대 | 초기 CurrentMode==Mouse, bMappingContextRegistered==false | AUTOMATED | `FInputSubsystemInitialStateTest` |
| AC-3 IMC 등록 | null 허용 정책 (Story-1-11 에셋 미생성 상태 시뮬레이션) | AUTOMATED | `FInputSubsystemNullAssetsAllowedTest` |
| AC-4 EInputMode enum | Mouse==0, Gamepad==1, uint8 안정성 | AUTOMATED | `FInputSubsystemModeEnumTest` |
| AC-5 6 Action + 2 IMC | Action injection seam + 카운트 검증 | AUTOMATED | `FInputSubsystemMockActionInjectionTest` |

---

## Story-1-11 에셋 미생성 상태 — null 허용 정책

**TD-008 상태**: Story-1-11 Input Action / IMC 에셋은 UE Editor 수동 생성 대기 중.

이 서브시스템은 에셋이 없어도 crash 없이 동작하도록 설계되었다:

- `LoadObject<UInputAction>()` 반환값 null → 경고 로그 출력 + 정상 진행
- `bMappingContextRegistered == false` 유지 (에셋 생성 후 PIE 재실행 시 자동으로 true)
- 테스트 환경(-nullrhi CI 포함)에서 에셋 없이도 5개 테스트 전부 통과 가능

**에셋 생성 후 예상 동작**:
1. Story-1-11 에셋을 `/Game/Input/Actions/` + `/Game/Input/Contexts/` 에 생성
2. PIE 재실행 → `LoadInputAssets()` 8/8 성공
3. PlayerController 있을 경우 `bMappingContextRegistered == true` 전환

---

## 테스트 함수 목록 (5개)

| 함수 | 카테고리 | 검증 내용 |
|---|---|---|
| `FInputSubsystemInitialStateTest` | `MossBaby.Input.Subsystem.InitialState` | NewObject 후 CurrentMode==Mouse, bMappingContextRegistered==false |
| `FInputSubsystemSettingsCDOTest` | `MossBaby.Input.Subsystem.SettingsCDODefaults` | 3 knob CDO 기본값 (InputModeMouseThreshold, OfferDragThresholdSec, OfferHoldDurationSec) |
| `FInputSubsystemNullAssetsAllowedTest` | `MossBaby.Input.Subsystem.NullAssetsAllowed` | Initialize 미호출 시 8개 포인터 모두 null + CountLoadedActions==0 |
| `FInputSubsystemMockActionInjectionTest` | `MossBaby.Input.Subsystem.MockActionInjection` | TestingInjectAction("IA_Select") → IA_Select non-null, Count==1 |
| `FInputSubsystemModeEnumTest` | `MossBaby.Input.Subsystem.ModeEnumStability` | EInputMode::Mouse==0, Gamepad==1 (uint8 회귀 방지) |

---

## 실행 방법

### 에디터 (개발 중)

1. UnrealEditor 실행 → `Tools` → `Session Frontend` → `Automation` 탭
2. `MossBaby.Input.Subsystem` 카테고리 expand
3. `Start Tests` → 결과 확인

### 헤드리스 (-nullrhi)

```bash
"C:/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "MadeByClaudeCode/MadeByClaudeCode.uproject" \
  -ExecCmds="Automation RunTests MossBaby.Input.Subsystem.; Quit" \
  -nullrhi -nosound -log -unattended
```

---

## Out of Scope (이 Story 범위 밖)

- **Story-1-17**: Offer Hold Formula 2 실제 임계 로직 + OfferHoldDurationSec 연동
- **Story-1-20**: PlayerSpawn 이벤트 구독으로 지연된 IMC 등록 보완
- **Story-1-21**: NO_RAW_KEY_BINDING CI grep + Mouse-only MANUAL 테스트
- **IMC 등록 integration test**: FSubsystemCollectionBase 직접 생성 불가 — Game Instance가 살아있는 functional test 또는 PIE 수동 검증으로 대체

---

## Story-1-13 — Input Mode Auto-Detect + Hysteresis

- 실제 cpp: `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/InputModeDetectionTests.cpp`
- 카테고리: `MossBaby.Input.ModeDetection.*`
- AC 매핑:
  - INPUT_MODE_AUTO_DETECT: `FInputModeAutoDetectMouseToGamepadTest`
  - INPUT_MODE_HYSTERESIS: `FInputModeHysteresisBelowThresholdTest`
  - Boundary strict `>` (3.0 정확 — 전환 미발생): `FInputModeBoundaryExactThresholdTest`
  - Boundary 3.1 초과 (전환 발생): `FInputModeBoundaryAboveThresholdTest`
  - Same mode no double broadcast: `FInputModeSameModeNoDelegateTest`
  - Multi-subscriber: `FInputModeDelegateSubscriptionCountTest`
- Formula 1: `ShouldSwitchToMouse = (MouseDelta > InputModeMouseThreshold) AND (CurrentMode == Gamepad)` — strict `>` 엄수
- PlayerController SetShowMouseCursor: 단위 테스트에서 PC nullptr guard OK (TransitionToMode null chain guard), integration test 분리
- Pillar 1 준수: 알림·모달 없음 (코드에 관련 API 호출 0건)

### 테스트 함수 목록 (6개)

| 함수 | 카테고리 | 검증 내용 |
|---|---|---|
| `FInputModeAutoDetectMouseToGamepadTest` | `MossBaby.Input.ModeDetection.AutoDetectMouseToGamepad` | Mouse → OnGamepadInputReceived() → Gamepad 전환 + delegate 1회 + 인자 Gamepad |
| `FInputModeHysteresisBelowThresholdTest` | `MossBaby.Input.ModeDetection.HysteresisBelowThreshold` | Gamepad + delta 2.0 < 3.0 → 전환 미발생 + delegate 미발행 |
| `FInputModeBoundaryExactThresholdTest` | `MossBaby.Input.ModeDetection.BoundaryExactThreshold` | Gamepad + delta == 3.0 (경계 정확) → strict `>` 미충족 → 전환 미발생 |
| `FInputModeBoundaryAboveThresholdTest` | `MossBaby.Input.ModeDetection.BoundaryAboveThreshold` | Gamepad + delta 3.1 > 3.0 → Mouse 전환 + delegate 1회 |
| `FInputModeSameModeNoDelegateTest` | `MossBaby.Input.ModeDetection.SameModeNoDelegate` | 동일 모드 재진입 시 TransitionToMode return early → delegate 추가 발행 없음 |
| `FInputModeDelegateSubscriptionCountTest` | `MossBaby.Input.ModeDetection.DelegateSubscriptionCount` | 3개 구독자 등록 → Broadcast 시 모두 1회 호출 |

### 실행 방법

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.Input.ModeDetection.; Quit" \
  -nullrhi -nosound -log -unattended
```

---

## Story-1-17 — Offer Hold Formula 2 + ApplySettingsToMappingContext

- 실제 cpp: `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/InputOfferHoldTests.cpp`
- 카테고리: `MossBaby.Input.OfferHold.*`
- AC 매핑:
  - ApplySettingsToMappingContext null-safe: `FInputOfferHoldApplyNoOpOnNullAssetTest`
  - Mock IMC HoldTimeThreshold 적용: `FInputOfferHoldApplyToMockIMCTest`
  - 다중 Hold trigger 카운트 (Pressed 제외): `FInputOfferHoldApplyMultipleHoldTriggersTest`
  - 다른 Action mapping 무시: `FInputOfferHoldApplyIgnoresWrongActionTest`
- Formula 2: `OfferTriggered = HoldElapsed >= HoldThreshold` (inclusive `>=`, Formula 1 strict `>`과 대비)
- IA_Select/IA_OfferCard 공유 키 해소: UE Enhanced Input implicit trigger priority (엔진 내장)
- Deferred (TD-011): boundary 실제 런타임 검증 (Mouse 0.15s, Gamepad 0.5s) — Story 1-11 에셋 생성 후 PIE integration test
