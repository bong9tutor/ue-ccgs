# Story 001 — 씬 레이아웃 (정적 메시·고정 카메라·FOV 35°)

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Config/Data
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 1 (씬 레이아웃) + §Rule 2 (고정 카메라)
- **TR-ID**: N/A (GDD-derived scene configuration)
- **Governing ADR**: N/A (scene layout)
- **Engine Risk**: LOW (정적 메시만 — Lumen GPU 영향은 후속 story 002)
- **Engine Notes**: 모든 정적 메시 `NaniteEnabled = false` (소형 씬 오버헤드). 카메라 고정 — 런타임 이동 금지. 800×600 판독성 요건 — 이끼 아이 화면 높이의 12% 이상.
- **Control Manifest Rules**: Cross-Cutting — Gameplay Camera System auto-spawn 경로 금지 (UE 5.6 제거됨). 명시적 Camera Component 배치 + Sequencer 사용. Raw pointer 대신 `TObjectPtr<>`.

## Acceptance Criteria

- **AC-LDS-01** [`CODE_REVIEW`/BLOCKING] — Lumen Dusk Scene 레벨 파일 및 카메라 Actor 설정에서 CameraActor 이동 코드 경로 grep + 런타임 SetActorLocation/SetActorRotation 호출 확인 시 카메라 이동 코드 경로 0건. Blueprint 또는 Sequencer에 의한 카메라 이동도 없음 (Rule 2).
- **AC-LDS-02** [`CODE_REVIEW`/BLOCKING] — 씬 정적 메시 에셋 목록 (`SM_GlassJar`, `SM_Desk`, 배경 소품)에서 NaniteEnabled 플래그 grep 시 모든 정적 메시에서 `bNaniteEnabled = false` (Rule 1).
- **AC-LDS-03** [`MANUAL`/ADVISORY] — 카메라 기본 배치(FOV=35°, Position=(0,−50,15)cm, Pitch=−10°) 상태에서 Viewport에서 씬 확인 시 이끼 아이가 화면 중앙에 위치하고 유리병이 명확히 보임. 800×600 해상도에서 이끼 아이가 화면 높이의 12% 이상 차지.

## Implementation Notes

1. **레벨 파일 생성** (`Content/Maps/MossBabyMain.umap` — MVP main level):
   - `SM_GlassJar` (이끼 아이 배치 기준점), `SM_Desk` (책상 표면).
   - Foreground/Surface layer 명시 — Outliner 폴더 구조로 분류.
   - 배경 소품 (`SM_Background_Objects`) — Full Vision에서 추가. MVP에서 비워둠 허용 (OQ-3 "MVP 0-1개").
2. **Nanite 비활성화** (AC-LDS-02):
   - 각 정적 메시 에셋: Editor → Mesh Properties → Nanite Settings → `Enabled = false`.
   - Cook 후 `.uasset` 메타에 `bNaniteEnabled = false` 확인 grep 가능.
3. **`ACameraActor` 배치** (AC-LDS-01):
   - 레벨에 단일 `ACameraActor` 배치:
     - `CameraComponent->FieldOfView = 35.0f;`
     - Position `(0, -50, 15)` cm.
     - Rotation `Pitch=-10, Yaw=0, Roll=0`.
     - `CameraComponent->bConstrainAspectRatio = false;` (창 크기 변경 FOV 보정 없음, GDD §E2).
   - PlayerController BeginPlay에서 `PossessCamera(CameraActor)` — 이후 이동 금지.
4. **Camera 이동 금지 grep 설계** (AC-LDS-01):
   - CI 검증 스크립트: `grep -rn "SetActorLocation\|SetActorRotation\|AddMovementInput" Source/MadeByClaudeCode/Core/LumenDusk/` = 0건.
   - Blueprint 레벨에서 Camera 노드 호출 금지 — Content 레벨의 Blueprint graph 검사 (manual review).
5. **DoF 설정** (Formula 1 기본값):
   - `CameraComponent->bEnableDepthOfField = true;` (VisualEffects area? UE5 — via `PostProcessSettings`).
   - `BaseFStop = 2.0`, `FocusDistance = 50.0cm` — `UMossLumenDuskSettings` 경유 (story 002 에서 구현).

## Out of Scope

- Lumen 설정 (story 002)
- Post-process Volume (story 004)
- PSO 번들 (story 006-008)
- Ambient particle (story 009)

## QA Test Cases

**Test Case 1 — AC-LDS-02 Nanite grep**
- **Setup**: Cook 후 또는 `.uasset` 텍스트 표현으로 `grep -l "bNaniteEnabled=True\|bEnableNanite=True" Content/Meshes/` 실행.
- **Verify**: 매치 0건.
- **Pass**: grep empty.

**Test Case 2 — AC-LDS-01 Camera 이동 grep**
- **Setup**: `grep -rn "SetActorLocation\|SetActorRotation\|AddControllerPitchInput\|AddControllerYawInput" Source/MadeByClaudeCode/Core/LumenDusk/`
- **Verify**: 매치 0건. Camera-관련 API 호출 부재.
- **Pass**: grep empty.

**Test Case 3 — AC-LDS-03 MANUAL**
- **Setup**: Editor → PIE with resolution 800×600.
- **Verify**: 이끼 아이 Static Mesh 높이 측정 (screen-space) ≥ 72px (800×600의 12%).
- **Pass**: 스크린샷 + QA 리드 sign-off.

## Test Evidence

- [ ] `tests/unit/lumen-dusk/nanite_disabled_grep.md` — AC-LDS-02 문서화
- [ ] `tests/unit/lumen-dusk/camera_static_grep.md` — AC-LDS-01 문서화
- [ ] `production/qa/evidence/lumen-dusk-camera-800x600-[YYYY-MM-DD].md` — AC-LDS-03 MANUAL

## Dependencies

- **Depends on**: None (foundational scene setup)
- **Unlocks**: story-002 (Lumen settings), story-003 (Light Actors for GSM), story-004 (Post-process)
