# Cooked PrimaryAssetType [5.6-VERIFY] Evidence — PLACEHOLDER

**Story**: `production/epics/foundation-data-pipeline/story-002-defaultengine-ini-primary-asset-types.md`
**Status**: ⏳ Awaiting Cooked Build Verification (UE Editor Package Project 필요)
**Created**: 2026-04-19
**AC**: `AC-DP-18 [5.6-VERIFY]`

---

## 완료된 에이전트 작업

`MadeByClaudeCode/Config/DefaultEngine.ini` 수정 — ADR-0002/0010 스펙 그대로 `+PrimaryAssetTypesToScan` 3건 추가:

```ini
[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="DreamData", ... Path="/Game/Data/Dreams" ...)
+PrimaryAssetTypesToScan=(PrimaryAssetType="FinalForm", ... Path="/Game/Data/FinalForms" ...)
+PrimaryAssetTypesToScan=(PrimaryAssetType="StillnessBudget", ... Path="/Game/Data" ...)
```

---

## 사용자 수동 검증 절차 (Cooked [5.6-VERIFY])

### Prerequisites

1. Story 1-11 에셋 (TD-008) 생성 완료
2. `Content/Data/Dreams/` 폴더 + 최소 3개 `UDreamDataAsset` 인스턴스 생성 (예: `DA_Dream_Rain`, `DA_Dream_Sprout`, `DA_Dream_Farewell`)
3. `Content/Data/FinalForms/` 폴더 + 최소 1개 `UMossFinalFormAsset` 생성
4. `Content/Data/` 에 1개 `UStillnessBudgetAsset` 인스턴스 생성

### PIE (Play In Editor) 검증

1. UE Editor에서 프로젝트 열기
2. Content Browser → 에셋 존재 확인
3. PIE 시작 → Output Log에서 다음 명령 수동 실행 가능 (콘솔 또는 임시 C++ debug code)
4. `UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"), OutIds)` 반환 결과:
   - `OutIds.Num() == N` (생성한 DreamData 개수)
   - 각 ID의 PrimaryAssetType == "DreamData"

**임시 로그 명령 예시** (C++ debug snippet):
```cpp
// 임시 PIE 테스트 code — Story 1-20에서 정식 integration test로 이전
TArray<FPrimaryAssetId> OutIds;
UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"), OutIds);
UE_LOG(LogTemp, Log, TEXT("PIE DreamData count: %d"), OutIds.Num());
for (const auto& Id : OutIds)
{
    UE_LOG(LogTemp, Log, TEXT("  %s"), *Id.ToString());
}
```

### Cooked 검증 [5.6-VERIFY]

1. UE Editor → File → Package Project → Windows → (Shipping 또는 Development)
2. 출력 디렉토리에서 `MadeByClaudeCode.exe` 실행
3. 인게임 로그 확인 (Saved/Logs/ 또는 Output Log 백업)
4. 동일 API 호출 결과 `OutIds.Num() == N` (PIE와 일치)

### 반대 케이스 (미등록 시뮬레이션)

1. `DefaultEngine.ini`의 3 `+PrimaryAssetTypesToScan` 중 1개를 주석 처리 (예: DreamData)
2. Editor 재시작 + PIE 재실행
3. 결과: DreamData `OutIds.Num() == 0` → `UDataPipelineSubsystem::RegisterDreamCatalog`에서 Warning 로그 발행
4. 복구: 주석 해제 + Editor 재시작

---

## 검증 체크리스트

- [ ] PIE에서 DreamData `OutIds.Num() >= 3`
- [ ] PIE에서 FinalForm `OutIds.Num() >= 1`
- [ ] PIE에서 StillnessBudget `OutIds.Num() >= 1`
- [ ] Cooked 빌드에서 동일 결과 (PIE ↔ Cooked 일치)
- [ ] 미등록 시뮬레이션에서 빈 배열 반환 + DegradedFallback 진입 로그
- [ ] 스크린샷 or 로그 캡처 첨부 (Output Log "DreamData count: N")

---

## 완료 후

1. ✅ 체크리스트 모두 체크
2. ✅ 로그 결과 이 문서에 첨부 (하단 Results 섹션)
3. ✅ Story 1-15 Status: Awaiting Cooked → Complete 전환
4. ✅ tech-debt TD-009 close
5. ✅ Story 1-15 Completion Notes의 Evidence Results 채우기

---

## Results (사용자 작성)

**Date**: [사용자 기입]
**UE Version**: 5.6.x (hotfix 번호)
**Build Type**: [Development / Shipping]

| Asset Type | PIE Count | Cooked Count | Match | Notes |
|---|---:|---:|---|---|
| DreamData | ? | ? | ⏳ | |
| FinalForm | ? | ? | ⏳ | |
| StillnessBudget | ? | ? | ⏳ | |

**Verdict**: [PASS / FAIL / BLOCKED]

---

## 참고 문서

- `docs/architecture/adr-0002-data-pipeline-container-selection.md` §DefaultEngine.ini 필수 등록
- `docs/architecture/adr-0010-ffinalformrow-storage-format.md` §Pipeline 통합
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp` RegisterDreamCatalog / RegisterFinalFormCatalog / RegisterStillnessCatalog 실구현
- `docs/engine-reference/unreal/VERSION.md` 5.6-VERIFY 정책
