# Story 007 — PSO Precaching Phase 2 (Runtime Async Bundle Load + `BatchMode::Fast`)

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **Engine Risk**: **HIGH** (UE 5.6 `FShaderPipelineCache::SetBatchMode` post-cutoff API — AC-LDS-11 [5.6-VERIFY] 실기 검증 대상)
- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 6 (PSO Precaching — Phase 2 Runtime Initialize)
- **TR-ID**: TR-lumen-001 (HIGH risk PSO Precaching)
- **Governing ADR**: **ADR-0013** §Decision §Phase 2 — Pipeline Ready 후 비동기 로드 + `FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast)` 백그라운드 컴파일. 게임 플레이 비차단 (Pillar 1). §Key Interfaces `StartPSOPrecaching()` + `OnPSOCacheLoaded()`.
- **Engine Notes**: **5.6-VERIFY AC**: AC-LDS-11 — `FShaderPipelineCache::SetBatchMode(BatchMode::Fast)` + `NumPrecompilesRemaining()` polling. `UE_LOG(LogLumenDusk, Warning, ...)` fallback 로그 1회 (silent DegradedFallback).
- **Control Manifest Rules**:
  - **Core Layer Rules §Required Patterns (PSO 3-Phase)**: Phase 2 = Pipeline Ready 후 비동기 로드.
  - **Core Layer Rules §Required Patterns (PSO graceful degradation)**: "PSO 번들 부재·손상 시 silent DegradedFallback — `UE_LOG(Warning)` 1회 + 점진 컴파일 정상 진행, 에러 다이얼로그 금지".

## Acceptance Criteria

- **AC-LDS-12** [`AUTOMATED`/BLOCKING] — PSO 번들 파일(`PSO_MossDusk.bin`) 없는 상태로 앱 실행 시 PSO Precaching 초기화 진행에서 크래시 없이 DegradedFallback 진행 + `UE_LOG(LogLumenDusk, Warning)` 1회 기록 (E5).

## Implementation Notes

1. **`ALumenDuskSceneActor::StartPSOPrecaching()`** (ADR-0013 §Key Interfaces):
   ```cpp
   void ALumenDuskSceneActor::StartPSOPrecaching() {
       PSOStartTime = GetWorld()->GetTimeSeconds();
       
       // Attempt to open pipeline cache
       const FString BundlePath = FPaths::ProjectContentDir() + TEXT("PipelineCaches/PSO_MossDusk.bin");
       if (!FPaths::FileExists(BundlePath)) {
           UE_LOG(LogLumenDusk, Warning, TEXT("PSO bundle missing at %s — DegradedFallback"), *BundlePath);
           bPSOReady = true;  // Proceed with degraded — 점진 컴파일 허용
           return;
       }
       
       // UE 5.6 API
       FShaderPipelineCache::OpenPipelineFileCache(TEXT("MossDusk"), SP_PCD3D_SM6);
       FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
       
       // Poll timer for NumPrecompilesRemaining()
       GetWorld()->GetTimerManager().SetTimer(PSOPollTimer, this, 
           &ALumenDuskSceneActor::PollPSOProgress, 0.1f, true);
   }
   ```
2. **`PollPSOProgress()`**:
   ```cpp
   void ALumenDuskSceneActor::PollPSOProgress() {
       const int32 Remaining = FShaderPipelineCache::NumPrecompilesRemaining();
       if (Remaining == 0) {
           OnPSOCacheLoaded();
       }
       
       // Timeout check
       const float Elapsed = GetWorld()->GetTimeSeconds() - PSOStartTime;
       if (Elapsed > Config->PSOWarmupTimeoutSec) {
           UE_LOG(LogLumenDusk, Warning, 
               TEXT("PSO precaching incomplete after %.1fs — accepting first-frame hitching"), 
               Config->PSOWarmupTimeoutSec);
           OnPSOCacheLoaded();  // Degraded — 타임아웃 후 진행
       }
   }
   ```
3. **`OnPSOCacheLoaded()`**:
   - `GetWorld()->GetTimerManager().ClearTimer(PSOPollTimer);`
   - `bPSOReady = true;`
   - `UE_LOG(LogLumenDusk, Log, TEXT("PSO ready after %.1fs"), GetWorld()->GetTimeSeconds() - PSOStartTime);`
4. **`BeginPlay` 통합** (story 003의 BeginPlay 확장):
   - `StartPSOPrecaching()` 호출. 게임 플레이는 차단하지 않음 — Dawn sequence 진행.
5. **DegradedFallback 시나리오** (ADR-0013 §DegradedFallback 시나리오):
   - PSO 번들 부재 → Warning 로그 1회 + `bPSOReady = true` 즉시.
   - Phase 2 타임아웃 → Warning 로그 1회 + `bPSOReady = true`.
   - PSO 번들 손상 (CRC 실패) → 부재와 동일 처리.

## Out of Scope

- Phase 3 Dawn Entry IsPSOReady check (story 008)
- 실기 GPU 프로파일링 AC-LDS-11 [5.6-VERIFY] 실측 — 별도 session (Implementation milestone)
- Build time bundle 생성 (story 006)
- GPU profiling은 별도 session (Implementation milestone)

## QA Test Cases

**Test Case 1 — AC-LDS-12 E5 Missing bundle**
- **Setup**: `rm Saved/Cooked/.../PSO_MossDusk.bin` (또는 Shipping package에서 파일 제외).
- **When**: 앱 실행 → `BeginPlay` → `StartPSOPrecaching` → fallback path.
- **Then**: 
  - Crash 없음.
  - `LogLumenDusk: Warning` 로그 1회 기록 (정확히 1회 — 중복 없음).
  - `bPSOReady = true` 즉시.
  - GSM Dawn sequence 정상 진행 (story 008 IsPSOReady 반환 true).

**Test Case 2 — Phase 2 timeout**
- **Setup**: Shipping 빌드 + PSO_MossDusk.bin 손상 (CRC 실패 시뮬) + `PSOWarmupTimeoutSec = 5.0` (테스트용 단축).
- **When**: `StartPSOPrecaching` 호출 → 5.0초 경과.
- **Then**: Warning 로그 1회 "PSO precaching incomplete after 5.0s". `bPSOReady = true`.

**Test Case 3 — Normal path (MANUAL, AC-LDS-11 [5.6-VERIFY])**
- **Setup**: Clean Shipping build + 정상 PSO bundle.
- **When**: 앱 실행 → Day 1 Dawn 진입 관찰.
- **Then**: 
  - Dawn 체류 3.0s 내에 `bPSOReady = true` 전환 (로그 확인).
  - 첫 상태 전환에서 히칭 < 100ms (frame time 측정).
- **Evidence**: `production/qa/evidence/pso-phase2-normal-[YYYY-MM-DD].mp4` (Implementation milestone).

## Test Evidence

- [ ] `tests/integration/lumen-dusk/pso_missing_bundle_test.cpp` — AC-LDS-12
- [ ] `tests/integration/lumen-dusk/pso_timeout_test.cpp` — Phase 2 timeout
- [ ] `production/qa/evidence/pso-phase2-normal-[YYYY-MM-DD].md` — AC-LDS-11 [5.6-VERIFY] MANUAL (별도 session)

## Dependencies

- **Depends on**: story-003 (LumenDuskSceneActor), story-006 (Build time bundle)
- **Unlocks**: story-008 (Phase 3 Dawn entry integration)
