# Story 012 — Visual Acceptance Manual Sign-off (AC-GSM-19)

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Visual/Feel
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2h (QA session)

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Acceptance Criteria Visual (AC-GSM-19) + §Visual/Audio Requirements
- **TR-ID**: TR-gsm-001 (MPC-Light Actor 동기화 결과 시각 검증)
- **Governing ADR**: ADR-0004 §Validation Criteria 6 상태 시각 구분 (QA 리드 sign-off).
- **Engine Risk**: MEDIUM (실기 검증 — Lumen HWRT GPU 예산 초과 시 SWRT 폴백 필요 여부 평가)
- **Engine Notes**: 전제조건: OQ-6 ADR-0004 완료 후 실행 가능. Lumen Dusk Scene first milestone 이후.
- **Control Manifest Rules**: Cross-Cutting — UI 알림·팝업·모달 금지 (QA 녹화 시 팝업 없음 확인).

## Acceptance Criteria

- **AC-GSM-19** [`MANUAL`/ADVISORY] — Waiting→Dream 전환(BlendDurationBias=0.5, D_blend=1.35s) 중 화면 녹화 시 색온도가 구리빛(2,800K)에서 달빛(4,200K)으로 끊김·점프 없이 부드럽게 전환. QA 리드 sign-off 필요.

## Implementation Notes

1. **QA 녹화 환경**:
   - Shipping 빌드 또는 Development build. 
   - 해상도 1920×1080, 60fps 고정. `r.OneFrameThreadLag 1`.
   - 녹화 툴: OBS Studio, 60fps constant.
2. **녹화 시나리오**:
   1. 앱 시작 → Menu → `FOnLoadComplete(fresh)` → Dawn.
   2. Dawn dwell 완료 → Offering.
   3. 카드 건넴 → Growth → Stage 2 → Waiting.
   4. Dream Trigger 강제 트리거 (개발 콘솔 command `moss.force_dream`) → Dream.
   5. Dream 완료 → Waiting.
   6. Withered 강제 트리거 (`moss.force_advance_silent 5`) → withered Waiting.
   7. 카드 건넴 → withered clear.
   8. Farewell 강제 트리거 → Farewell.
3. **sign-off 기준** (ADR-0004 §Validation Criteria):
   - 6 상태 스크린샷 (각 상태 정적 프레임) — Menu / Dawn / Offering / Waiting / Dream / Farewell.
   - Waiting→Dream 전환 녹화 1.35s — 프레임 단위 리뷰 시 끊김·점프 없음.
   - Lumen GI 반영 확인 — 이끼 아이 표면 색온도가 6 상태에서 명확히 다름.
   - Farewell Vignette 점진 증가 (Lumen Dusk story 015 완료 시 가능).
4. **증거 저장**:
   - `production/qa/evidence/gsm-6-state-[YYYY-MM-DD].mp4` — 녹화
   - `production/qa/evidence/gsm-6-state-screenshots-[YYYY-MM-DD]/` — 스크린샷 디렉토리
   - `production/qa/evidence/gsm-manual-signoff-[YYYY-MM-DD].md` — QA 리드 sign-off 문서

## Out of Scope

- Lumen GPU 5ms 프로파일링 (AC-LDS-04 [5.6-VERIFY] — Lumen Dusk epic 별도 session)
- Dream DoF / Farewell Vignette 개별 검증 (Lumen Dusk epic)
- Audio 레이어 (VS)

## QA Test Cases

**Test Case 1 — 6 상태 시각 구분**
- **Setup**: Shipping 빌드 + `moss.force_state [Menu|Dawn|Offering|Waiting|Dream|Farewell]` 콘솔 명령어 (debug-only) 순차 실행.
- **Verify**: 각 상태에서 1920×1080 스크린샷 캡처. 
  - Menu: 중성 라벤더 3,600K.
  - Dawn: 호박빛 2,800K + 낮은 광원 각도 15°.
  - Offering: 황금빛 3,200K + 대비 0.6.
  - Waiting: 구리빛 2,800K.
  - Dream: 차가운 달빛 4,200K + SSS boost 0.40 (이끼 아이 발광).
  - Farewell: 촛불빛 2,200K + (Lumen Dusk 완료 시) Vignette 증가.
- **Pass**: 6 스크린샷 모두 다른 무드 — QA 리드 리뷰 후 sign-off 문서 작성.

**Test Case 2 — Waiting→Dream 전환 smooth**
- **Setup**: Waiting 상태 → `moss.trigger_dream` → Dream → 전환 1.35s 녹화.
- **Verify**: 프레임 단위 리뷰 (81 frames @60fps) — 색온도가 단조 증가, 점프 없음. Pillar 4 앵커 모먼트 감정 감각 준수.
- **Pass**: QA 리드 "부드럽다" 평가 + sign-off.

## Test Evidence

- [ ] `production/qa/evidence/gsm-6-state-[YYYY-MM-DD].mp4` — 6 상태 녹화
- [ ] `production/qa/evidence/gsm-6-state-screenshots-[YYYY-MM-DD]/` — 스크린샷
- [ ] `production/qa/evidence/gsm-manual-signoff-[YYYY-MM-DD].md` — QA 리드 승인 문서

## Dependencies

- **Depends on**: story-005 (Light Actor integration), story-006 (Entry/Exit), story-007 (ReducedMotion), Lumen Dusk epic (scene assets — Post-process Vignette)
- **Unlocks**: AC-LDS-07 협력 검증 (Lumen Dusk epic)
