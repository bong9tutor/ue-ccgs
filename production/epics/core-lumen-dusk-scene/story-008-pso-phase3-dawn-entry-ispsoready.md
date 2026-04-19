# Story 008 — PSO Precaching Phase 3 (Dawn Entry `IsPSOReady()` + 10s Timeout)

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2h

## Context

- **Engine Risk**: **HIGH** (AC-LDS-11 [5.6-VERIFY] 연관 — first-frame hitching 측정)
- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 6 (PSO Precaching — Phase 3 Dawn Entry) + §E4 (PSO 컴파일 미완료)
- **TR-ID**: TR-lumen-001 (PSO Precaching)
- **Governing ADR**: **ADR-0013** §Decision §Phase 3 — GSM Dawn 진입 시 `ALumenDuskSceneActor::IsPSOReady()` 확인. `PSOWarmupTimeoutSec` (기본 10.0초) 내 미완 시 DegradedFallback 허용. §Pillar 1 준수 검증 — silent DegradedFallback.
- **Engine Notes**: **5.6-VERIFY AC**: AC-LDS-11 수동 검증 — 첫 프레임 히칭 < 100ms stall. 전제조건: OQ-1 (MPC-Light Actor 동기화) ADR 완료.
- **Control Manifest Rules**:
  - **Core Layer Rules §Required Patterns (PSO 3-Phase)**: Phase 3 = Dawn Entry `IsPSOReady()` 확인, 10초 타임아웃 후 DegradedFallback.
  - **Core Layer Rules §Performance Guardrails**: `PSOWarmupTimeoutSec` 기본 10.0초 (Dawn 최소 체류 3초 내 대부분 완료 목표).

## Acceptance Criteria

- **AC-LDS-11** [`MANUAL`/ADVISORY] [**5.6-VERIFY**] — 클린 빌드 (PSO 캐시 삭제) 후 앱 최초 실행 시 Day 1 Dawn 상태 진입 관찰(화면 녹화)에서 명시적 히치(1초 이상 프레임 정지)가 Dawn 체류 시간(DawnMinDwellSec=3.0초) 내에 발생하지 않음. 히치가 있다면 최대 1-2프레임(33-66ms) 이하. PSO 번들 포함 배포 확인.

## Implementation Notes

1. **`IsPSOReady()` API** (ADR-0013 §Key Interfaces):
   ```cpp
   bool ALumenDuskSceneActor::IsPSOReady() const {
       return bPSOReady || (Config->PSOWarmupTimeoutSec > 0.0f &&
                            GetWorld()->GetTimeSeconds() - PSOStartTime > Config->PSOWarmupTimeoutSec);
   }
   ```
2. **GSM Dawn Entry 통합** (GSM epic story 006의 Dawn dwell timer 확장):
   - `UMossGameStateSubsystem::TryAdvanceToOffering()` (GSM story 006):
     ```cpp
     void UMossGameStateSubsystem::TryAdvanceToOffering() {
         auto* LumenScene = /* GetLumenDuskSceneActor() */;
         if (LumenScene && !LumenScene->IsPSOReady()) {
             // PSO 미준비 — 대기 (단, PSOWarmupTimeoutSec 만료 시 IsPSOReady() true 반환)
             return;
         }
         if (bBlendComplete && bCardSystemReady) {
             RequestStateTransition(EGameState::Offering, EGameStatePriority::P1);
         }
     }
     ```
   - `TryAdvanceToOffering`은 Dawn dwell timer + CardReady timer에 의해 재시도 — PSO ready 시 progression.
3. **Timeout 로그** (ADR-0013 §Phase 3):
   - `PSOWarmupTimeoutSec` 만료 시 Warning 로그 (story 007의 PollPSOProgress timeout path에서 처리).
   - GSM 로그: Dawn 체류 시간 기록 — PSO가 Dawn dwell 내에 ready되었는지 추적.
4. **Pillar 1 준수** (ADR-0013 §Pillar 1 준수 검증):
   - splash screen 없음 ✓ (Dawn sequence 자체가 visual presentation).
   - stall 체감 < 200ms 목표 ✓ (PSOWarmupTimeoutSec 여유).
   - silent DegradedFallback ✓ (story 007에서 Warning만 발행).

## Out of Scope

- Build time bundle (story 006)
- Runtime async load (story 007)
- GSM Dawn dwell timer (GSM epic story 006)
- 실기 GPU 프로파일링 AC-LDS-11 [5.6-VERIFY] 실측 — 별도 session (Implementation milestone)
- GPU profiling은 별도 session (Implementation milestone)

## QA Test Cases

**Test Case 1 — IsPSOReady before timeout**
- **Given**: `bPSOReady = false`, `PSOStartTime = 0`, current time = 5.0s, `PSOWarmupTimeoutSec = 10.0`.
- **When**: `IsPSOReady()` 호출.
- **Then**: Return `false` (5.0s < 10.0s, still waiting).

**Test Case 2 — IsPSOReady after timeout**
- **Given**: `bPSOReady = false`, `PSOStartTime = 0`, current time = 11.0s.
- **When**: `IsPSOReady()` 호출.
- **Then**: Return `true` (11.0s > 10.0s, DegradedFallback).

**Test Case 3 — IsPSOReady success path**
- **Given**: `bPSOReady = true`, current time = 5.0s.
- **When**: `IsPSOReady()` 호출.
- **Then**: Return `true`.

**Test Case 4 — AC-LDS-11 [5.6-VERIFY] MANUAL (Implementation milestone)**
- **Setup**: Clean Shipping build + 정상 PSO bundle. PSO 캐시 삭제.
- **When**: 앱 최초 실행 → Day 1 Dawn 진입 → 60fps 녹화.
- **Then**: 
  - Dawn 체류 3.0s 내 `IsPSOReady() == true` 전환.
  - 첫 상태 전환 (Dawn→Offering) 히칭 < 100ms (frame time 1-6 frames @60fps ≈ 17-100ms).
  - 이후 상태 전환은 warm — 히칭 < 17ms.
- **Evidence**: `production/qa/evidence/pso-phase3-dawn-[YYYY-MM-DD].mp4` (별도 session).

## Test Evidence

- [ ] `tests/unit/lumen-dusk/ispsoready_test.cpp` — 3 state machine check
- [ ] `tests/integration/lumen-dusk/dawn_pso_gate_test.cpp` — GSM ↔ Phase 3 integration
- [ ] `production/qa/evidence/pso-phase3-dawn-[YYYY-MM-DD].md` — AC-LDS-11 [5.6-VERIFY] MANUAL (별도 session)

## Dependencies

- **Depends on**: story-003 (LumenDuskSceneActor), story-007 (Phase 2 load), GSM epic story 006 (Dawn dwell timer)
- **Unlocks**: Full PSO 3-Phase strategy complete → Implementation milestone GPU profiling session
