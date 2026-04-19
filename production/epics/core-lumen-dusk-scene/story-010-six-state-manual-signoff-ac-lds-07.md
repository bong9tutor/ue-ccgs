# Story 010 — 6 상태 시각 수동 Sign-off (AC-LDS-07)

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Visual/Feel
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3h (QA session)

## Context

- **Engine Risk**: MEDIUM (실기 시각 검증 — Lumen HWRT 결과 확인)
- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §States (Per-GSM-State Atmosphere) + §Acceptance Criteria
- **TR-ID**: TR-lumen-001 (HIGH risk 시각 검증 결과)
- **Governing ADR**: ADR-0004 §Validation Criteria "6 상태 시각 구분" (QA 리드 sign-off).
- **Engine Notes**: GSM epic story 012와 협력 — 동일 6 상태 시각 검증이나 본 story는 Lumen Dusk Scene 관점 (Post-process Vignette/DoF, 파티클 동작 포함).
- **Control Manifest Rules**: Cross-Cutting — 에러 표시 금지. UI 알림·팝업 금지.

## Acceptance Criteria

- **AC-LDS-07** [`MANUAL`/BLOCKING] — 6개 GSM 상태를 수동으로 순환하면서 각 상태 진입 후 씬 캡처 시 (1) Menu: 중성 라벤더. (2) Dawn: 호박빛 낮은 광원. (3) Offering: 밝은 황금빛. (4) Waiting: 구리빛 황혼. (5) Dream: 차가운 달빛. (6) Farewell: 촛불빛 + 비녜트. 6개 상태 모두 시각적으로 구분 가능. QA 리드 sign-off 필요.
- **AC-LDS-18** [`INTEGRATION`/ADVISORY] — Moss Baby GrowthTransition 발생 (MBC GrowthTransition 상태) 시 씬 Lumen 업데이트 관찰 (2-3프레임)에서 Lumen Surface Cache 재빌드 발생 가능. 씬 렌더링 크래시 없음. 1-2프레임 GI 흐려짐 허용 범위 (Rule 1 메모).

## Implementation Notes

1. **QA 환경**:
   - Shipping 또는 Development build.
   - 1920×1080 60fps 고정. Full Lumen HWRT enabled.
   - 녹화: OBS Studio 60fps constant.
2. **6 상태 순환 프로토콜**:
   - Editor/PIE에서는 불가 — Shipping build 필수 (Lumen HWRT + PSO Precaching 활성).
   - Debug-only console command: `moss.force_state [Menu|Dawn|Offering|Waiting|Dream|Farewell]` (별도 debug hook 구현 필요).
   - 각 상태 진입 후 5초 안정화 → 1920×1080 스크린샷 캡처.
3. **시각 기준** (GDD §States 표):
   - Menu: ColorTemp 3,600K, 라벤더 계열 보조광.
   - Dawn: ColorTemp 2,800K, LightAngle 15°, SSS boost 0.10.
   - Offering: ColorTemp 3,200K, Contrast 0.6 (선명한 황금빛).
   - Waiting: ColorTemp 2,800K, Contrast 0.3 (구리빛).
   - Dream: ColorTemp **4,200K** (차가운), SSS boost 0.40, DoF 강화 (ApertureFStop 1.5), 파티클 감소.
   - Farewell: ColorTemp 2,200K, Vignette 0.6 (점진 증가 후).
4. **AC-LDS-18 GrowthTransition + Lumen**:
   - MBC story 004 (GrowthTransition) 트리거: `moss.force_growth_stage [Growing|Mature|Final]`.
   - 메시 교체 시 2-3프레임 Lumen Surface Cache 재빌드 — GI 흐려짐 관찰.
   - Crash 없음 확인 (stderr/log 모니터).
5. **Evidence 저장**:
   - `production/qa/evidence/lumen-6-state-[YYYY-MM-DD]/` — 6 스크린샷.
   - `production/qa/evidence/lumen-growth-transition-lumen-[YYYY-MM-DD].mp4` — AC-LDS-18 녹화.
   - `production/qa/evidence/lumen-dusk-signoff-[YYYY-MM-DD].md` — QA 리드 sign-off 문서.

## Out of Scope

- GPU profiling AC-LDS-04 [5.6-VERIFY] — 별도 story 002 + Implementation milestone session
- PSO hitching 수동 검증 AC-LDS-11 — story 008 수동 검증 파트
- Light Actor Mobility 확인 — story 002 Editor config

## QA Test Cases

**Test Case 1 — AC-LDS-07 6 상태 시각 구분**
- **Setup**: Shipping build + `moss.force_state` debug commands.
- **When**: 6 상태 순환 — 각 5초 안정 후 캡처.
- **Verify**:
  - Menu 스크린샷 — 라벤더 무드 hex ≈ #8A7BA8 (중성).
  - Dawn — 호박빛 hex ≈ #C78B5A (낮은 광원 각도 15° 그림자).
  - Offering — 선명한 황금빛 hex ≈ #D4A855 (대비 높음).
  - Waiting — 구리빛 hex ≈ #B07A4C (낮은 대비, 기본 상태).
  - Dream — **차가운 달빛** hex ≈ #7A8CB0 (4,200K 냉색 — 이 게임 유일 냉색).
  - Farewell — 촛불빛 hex ≈ #C07040 + 화면 가장자리 Vignette 0.6.
  - 6 상태 모두 명확히 다른 시각 무드.
- **Pass**: QA 리드 리뷰 후 sign-off 문서 작성. 6 스크린샷 파일 저장.

**Test Case 2 — AC-LDS-18 GrowthTransition + Lumen**
- **Setup**: `moss.force_state Waiting` → `moss.force_growth_stage Mature`.
- **When**: 메시 교체 수행 + 2-3프레임 관찰.
- **Verify**:
  - Crash 없음 (stderr clean).
  - Lumen Surface Cache 재빌드 시각적 흐려짐 1-2프레임 (허용 범위).
  - 3프레임 후 GI 정상화.
- **Pass**: 녹화 + QA 리드 확인.

## Test Evidence

- [ ] `production/qa/evidence/lumen-6-state-[YYYY-MM-DD]/` — 6 1920×1080 스크린샷
- [ ] `production/qa/evidence/lumen-growth-transition-lumen-[YYYY-MM-DD].mp4` — AC-LDS-18
- [ ] `production/qa/evidence/lumen-dusk-signoff-[YYYY-MM-DD].md` — QA 리드 sign-off

## Dependencies

- **Depends on**: story-002 (Lumen settings), story-003 (Light Actors), story-004 (Post-process), story-005 (Dream DoF), story-009 (particles), GSM epic story 005 (Light Actor 실제 구동)
- **Unlocks**: Implementation milestone gate (GPU profiling session precondition)
