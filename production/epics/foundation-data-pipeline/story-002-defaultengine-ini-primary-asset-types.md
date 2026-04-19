# Story 002: DefaultEngine.ini PrimaryAssetTypesToScan 등록 + AC-DP-18 Cooked build 검증

> **Epic**: Data Pipeline
> **Status**: Awaiting Cooked Verification
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: 0.5 days (~3 hours incl. Package)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/data-pipeline.md`
**Requirement**: `TR-dp-004` (AC-DP-18 [5.6-VERIFY] Cooked PrimaryAssetType 검증)
**ADR Governing Implementation**: ADR-0002 (DefaultEngine.ini 필수 등록), ADR-0010 (FinalForm 추가 등록)
**ADR Decision Summary**: `DefaultEngine.ini`의 `[/Script/Engine.AssetManagerSettings]` → `PrimaryAssetTypesToScan`에 3종 등록 (DreamData, FinalForm, StillnessBudget). **Cooked 빌드에서 미등록 시 빈 결과** → Dream 카탈로그 전체 DegradedFallback.
**Engine**: UE 5.6 | **Risk**: MEDIUM (Cooked PrimaryAssetType 5.6-VERIFY)
**Engine Notes**: `UAssetManager::GetPrimaryAssetIdList(FPrimaryAssetType, OutIds)` Cooked 빌드 동작은 [5.6-VERIFY] 대상. `AssetBaseClass` 경로가 프로젝트 Module 이름 (MadeByClaudeCode)과 일치해야 함.

**Control Manifest Rules (Foundation layer)**:
- Required: "DefaultEngine.ini `[/Script/Engine.AssetManagerSettings]`에 `PrimaryAssetTypesToScan` 3종 등록" (ADR-0002)
- Required: AC-DP-18 [5.6-VERIFY] 대상 명시

---

## Acceptance Criteria

*From GDD §Acceptance Criteria + ADR-0002 §DefaultEngine.ini 필수 등록:*

- [ ] `Config/DefaultEngine.ini`에 `[/Script/Engine.AssetManagerSettings]` 섹션 존재
- [ ] `+PrimaryAssetTypesToScan` 3건 추가: `DreamData` (`Content/Data/Dreams`), `FinalForm` (`Content/Data/FinalForms`), `StillnessBudget` (`Content/Data`)
- [ ] AC-DP-18 [5.6-VERIFY]: Cooked 빌드에서 `UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"), OutIds)` 호출 시 `OutIds`가 비어있지 않음 + 등록된 Dream DataAsset 수와 일치 (Integration — PIE + Cooked 환경 각 1회)
- [ ] 미등록 시뮬레이션: ini 주석처리 후 `GetPrimaryAssetIdList` → 빈 `OutIds` → Pipeline DegradedFallback 진입 확인 (반대 케이스 확인)

---

## Implementation Notes

*Derived from ADR-0002 §DefaultEngine.ini 필수 등록 + ADR-0010 §DefaultEngine.ini 등록:*

```ini
; Config/DefaultEngine.ini
[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="DreamData",AssetBaseClass=/Script/MadeByClaudeCode.DreamDataAsset,Directories=((Path="/Game/Data/Dreams")),bHasBlueprintClasses=False,bIsEditorOnly=False)
+PrimaryAssetTypesToScan=(PrimaryAssetType="FinalForm",AssetBaseClass=/Script/MadeByClaudeCode.MossFinalFormAsset,Directories=((Path="/Game/Data/FinalForms")),bHasBlueprintClasses=False,bIsEditorOnly=False)
+PrimaryAssetTypesToScan=(PrimaryAssetType="StillnessBudget",AssetBaseClass=/Script/MadeByClaudeCode.StillnessBudgetAsset,Directories=((Path="/Game/Data")),bHasBlueprintClasses=False,bIsEditorOnly=False)
```

- `AssetBaseClass` 경로 — `/Script/[ModuleName].[ClassName]` — MadeByClaudeCode 프로젝트 맞춤
- `Content/Data/Dreams/`, `Content/Data/FinalForms/`, `Content/Data/` 3 폴더 수동 생성 필요 (Story 004에서 샘플 자산 배치)
- Cooked 검증 절차:
  1. PIE에서 `UAssetManager::Get().GetPrimaryAssetIdList(...)` 호출 → 비어있지 않음
  2. Cooked 빌드 (`Package Project` Windows) → 동일 API 호출 → 동일 결과
  3. 결과를 `production/qa/evidence/cooked-primary-asset-type-evidence-[date].md`에 기록

---

## Out of Scope

- Story 003: UDataPipelineSubsystem 뼈대
- Story 001: 자산 타입 정의 (선행)

---

## QA Test Cases

**For Integration story:**
- **AC-DP-18 PIE 환경**
  - Given: Story 001 자산 타입 정의 완료 + DefaultEngine.ini 등록 + Content/Data/Dreams에 test DreamDataAsset 3개 생성
  - When: PIE 진입 → `TArray<FPrimaryAssetId> Ids; UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"), Ids)` 호출
  - Then: `Ids.Num() == 3` + 각 ID의 PrimaryAssetType == "DreamData"
- **AC-DP-18 Cooked 환경 [5.6-VERIFY]**
  - Setup: Package Project Windows → 생성된 `MadeByClaudeCode.exe` 실행 (in-game logging enabled)
  - Verify: 동일 API 호출 로그 확인 → `Ids.Num() == 3` (PIE와 일치)
  - Pass: Cooked 빌드에서도 빈 배열 아님 + 로그에 "3 DreamData assets found"
- **미등록 시뮬레이션 (반대 케이스)**
  - Given: DefaultEngine.ini `+PrimaryAssetTypesToScan` 주석처리 (DreamData만)
  - When: PIE 진입 → API 호출
  - Then: `Ids.Num() == 0` → downstream Pipeline (Story 004)이 DegradedFallback 진입
  - Edge cases: 3개 중 1개만 등록되어 있으면 해당 type만 성공

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- `tests/integration/data-pipeline/cooked_primary_asset_type_test.cpp` (PIE 환경 AUTOMATED 부분 가능)
- `production/qa/evidence/cooked-primary-asset-type-evidence-[date].md` (MANUAL Cooked 환경, [5.6-VERIFY])

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (자산 타입 정의)
- Unlocks: Story 003 (Subsystem이 이 등록에 의존)

---

## Completion Notes (Partial — Awaiting Cooked Verification)

**Partially Completed**: 2026-04-19 (에이전트 완료 부분)
**Status**: **Awaiting Cooked Verification** — DefaultEngine.ini 등록 완료, [5.6-VERIFY] 실측은 사용자 Package Project 필요.

**에이전트 완료 작업**:
- `MadeByClaudeCode/Config/DefaultEngine.ini` 수정 — `[/Script/Engine.AssetManagerSettings]` + 3 `+PrimaryAssetTypesToScan`:
  - DreamData → `/Game/Data/Dreams`
  - FinalForm → `/Game/Data/FinalForms`
  - StillnessBudget → `/Game/Data`
- `production/qa/evidence/cooked-primary-asset-type-placeholder-2026-04-19.md` 신규 — PIE/Cooked 검증 가이드 + 체크리스트
- tech-debt TD-009 등록 (S2, Sprint 1 완료 게이트)

**ADR 준수**:
- ADR-0002 §DefaultEngine.ini 필수 등록 — 3 type 모두 등록
- ADR-0010 §Pipeline 통합 — FinalForm 등록 포함
- `AssetBaseClass` 경로 `/Script/MadeByClaudeCode.{ClassName}` 형식 엄수

**사용자 수동 검증 필요** (TD-008 해결 후):
1. Story 1-11 에셋 (TD-008) 생성 선행
2. `Content/Data/Dreams` + `Content/Data/FinalForms` + `Content/Data` 폴더에 샘플 자산 배치
3. PIE 실행 → `UAssetManager::GetPrimaryAssetIdList` 호출 로그 확인
4. Package Project (Windows Development) → Cooked .exe 실행 → 동일 결과
5. 반대 케이스 (주석처리 후 빈 배열 확인)
6. Results를 placeholder evidence에 기입

**Sprint 1 완료 게이트**: Cooked [5.6-VERIFY] 실측 + placeholder Results 채우기 완료 시 Status → Complete.
