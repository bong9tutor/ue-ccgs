# Story 006 — PSO Precaching Phase 1 (Build Time Bundle 생성)

> **Epic**: [core-lumen-dusk-scene](EPIC.md)
> **Layer**: Core
> **Type**: Config/Data
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **Engine Risk**: **HIGH** (PSO 점진 컴파일 — UE 5.6 런타임 동작. AC-LDS-11 [5.6-VERIFY] Implementation 단계 검증)
- **GDD**: [design/gdd/lumen-dusk-scene.md](../../../design/gdd/lumen-dusk-scene.md) §Rule 6 (PSO Precaching — Phase 1 Build Time) + §E5 PSO 번들 파일 없음
- **TR-ID**: TR-lumen-001 (HIGH risk PSO Precaching)
- **Governing ADR**: **ADR-0013** §Decision §3-Phase 전략 §Phase 1 — `r.ShaderPipelineCache.Enabled 1` + `PSO_MossDusk.bin` 번들 생성. §Migration Plan §1-2 (Project Settings + Cook 설정).
- **Engine Notes**: **5.6-VERIFY AC**: AC-LDS-11 — `FShaderPipelineCache::SetBatchMode(BatchMode::Fast)`, `NumPrecompilesRemaining()` polling. UE 5.6 PSO는 런타임 점진 컴파일 default. `PSO_MossDusk.bin`은 빌드 파이프라인이 사전 생성. Shipping/Cooked 빌드만 적용 — Editor/PIE에서는 점진 컴파일 정상.
- **Control Manifest Rules**:
  - **Core Layer Rules §Required Patterns (PSO Precaching 3-Phase 전략)**: (1) Build: `r.ShaderPipelineCache.Enabled 1` + `PSO_MossDusk.bin` 번들 생성. (2) Runtime: story 007. (3) Dawn Entry: story 008.
  - **Core Layer Rules §Forbidden**: "Never use splash screen to precompile all PSOs" (Pillar 1 위반). "Never enable PSO precaching in Editor/PIE".

## Acceptance Criteria

- **PSO Bundle Build Verification** (본 story 신규 AC — ADR-0013 §Validation Criteria §1) [`CODE_REVIEW`/BLOCKING] — Cook 단계 완료 후 `Saved/Cooked/Windows/MadeByClaudeCode/Content/PipelineCaches/PSO_MossDusk.bin` 파일 존재 + 크기 > 0. CI 파이프라인이 매 릴리스 빌드에 PSO 번들 포함 확인.

## Implementation Notes

1. **`DefaultEngine.ini` 설정** (ADR-0013 §Phase 1):
   ```
   [SystemSettings]
   r.ShaderPipelineCache.Enabled=1
   r.ShaderPipelineCache.SaveAfterPSOsLogged=0
   r.ShaderPipelineCache.AutoSaveTime=30
   r.ShaderPipelineCache.AutoSaveTimeBoundPSO=15
   ```
2. **Project Settings → Packaging**:
   - "Include in Base Package" → `Content/PipelineCaches/` 디렉토리 포함.
   - Cook options에 PSO 번들 생성 commandlet 추가 (`-precookcache` 또는 유사).
3. **Cook commandlet workflow** (ADR-0013 §Migration Plan §2):
   - Development build 플레이 세션 → PSO usage 기록 (`.upipelinecache` 파일 생성).
   - Shipping cook → Cook Commandlet이 기록된 PSO를 `PSO_MossDusk.bin` 번들로 직렬화.
   - Shipping package에 번들 포함.
4. **PSO 포함 범위** (GDD §Rule 6):
   - 6개 GSM 상태의 MPC 극단값 조합 (6 states × 8 params = 48 permutations).
   - Lumen Surface Cache 초기화 셰이더.
   - 포스트 프로세싱 조합 (Contrast 최소·최대, Vignette 0~0.6, DoF 1.4~2.0).
   - 앰비언트 파티클 Niagara 셰이더 (story 009).
5. **빌드 파이프라인 CI 검증** (AC):
   - Shipping cook 후 파일 존재 체크: `test -s Saved/Cooked/Windows/MadeByClaudeCode/Content/PipelineCaches/PSO_MossDusk.bin`
   - `/release-checklist` 스킬에 체크 항목 추가 권장 (미래).

## Out of Scope

- Runtime async bundle load (story 007 Phase 2)
- Dawn entry IsPSOReady check (story 008 Phase 3)
- 실기 GPU 프로파일링 AC-LDS-11 [5.6-VERIFY] 실측 — 별도 session (Implementation milestone)
- GPU profiling은 별도 session (Implementation milestone)

## QA Test Cases

**Test Case 1 — PSO Bundle file exists**
- **Setup**: Shipping cook 실행: `UAT.bat BuildCookRun -Project=MadeByClaudeCode -Platform=Win64 -Cook -Pak -Shipping`.
- **Verify**: 
  - `ls Saved/Cooked/Windows/MadeByClaudeCode/Content/PipelineCaches/PSO_MossDusk.bin` 존재.
  - `file size > 0`.
- **Pass**: 두 조건 충족.

**Test Case 2 — Editor/PIE에서 PSO precaching 비활성**
- **Setup**: Editor 실행 → PIE 시작.
- **Verify**: `DefaultEditor.ini` 또는 `#if WITH_EDITOR` 가드로 PIE에서 `r.ShaderPipelineCache.Enabled=0` (Editor 점진 컴파일 정상).
- **Pass**: Editor log에서 PSO precaching 관련 메시지 없음.

**Test Case 3 — `DefaultEngine.ini` grep**
- **Setup**: `grep -E "r\.ShaderPipelineCache\.Enabled\s*=\s*1" Config/DefaultEngine.ini`.
- **Verify**: 매치 1건.
- **Pass**: ini 설정 확인.

## Test Evidence

- [ ] `tests/unit/lumen-dusk/pso_ini_grep.md` — CI grep 문서화
- [ ] `production/qa/evidence/pso-bundle-build-[YYYY-MM-DD].md` — Cook 결과 + 파일 존재 스크린샷
- [ ] `docs/release-checklist-pso.md` — `/release-checklist` 미래 체크 항목

## Dependencies

- **Depends on**: story-001 (scene layout), story-002 (Lumen settings + Material permutations)
- **Unlocks**: story-007 (Phase 2 runtime load), story-008 (Phase 3 Dawn entry)
