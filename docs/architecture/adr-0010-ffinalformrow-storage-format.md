# ADR-0010: FFinalFormRow 저장 형식 — `UPrimaryDataAsset` 격상

## Status

**Accepted** (2026-04-19 — `/architecture-review` 통과, ADR-0002 Accepted 의존 충족)

## Date

2026-04-18

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | Core / Data / Asset Management |
| **Knowledge Risk** | LOW — `UPrimaryDataAsset` 4.x+ 안정. `UAssetManager::GetPrimaryAssetIdList`는 ADR-0002에서 MEDIUM (cooked 검증) — 본 ADR이 동일 위험 상속 |
| **References Consulted** | growth-accumulation-engine.md §CR-8 (DataTable 편집 제한), data-pipeline.md §Container Choices, ADR-0002 §Decision §Alternatives 4 |
| **Post-Cutoff APIs Used** | None — `UPrimaryDataAsset` + `UAssetManager` 4.x 안정 |
| **Verification Required** | AC-DP-18 [5.6-VERIFY] — `FinalForm` PrimaryAssetType cooked 빌드 동작 (ADR-0002와 공유) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0002 (Data Pipeline 컨테이너 선택 — 본 ADR이 ADR-0002의 "FinalForm = DataTable" 결정을 격상) |
| **Enables** | Growth Engine CR-8 해소 (TMap 편집 제한 회피). Data Pipeline FinalForm 카탈로그 등록 패턴 (per-form DataAsset bucket) |
| **Blocks** | Growth sprint (`UMossFinalFormAsset` 정의 미작성 시 Growth `EvaluateFinalForm()` 구현 불가) |
| **Ordering Note** | ADR-0002 / ADR-0003과 묶음 결정. 본 ADR은 ADR-0002의 §Alternatives 4 평가에서 도출 — DataTable 유지가 아닌 격상 채택 |

## Context

### Problem Statement

Growth Accumulation Engine (#7)의 `FFinalFormRow.RequiredTagWeights` 필드는 `TMap<FName, float>` (각 태그의 이상적 가중치). UE DataTable 에디터는 USTRUCT 내 `TMap` 인라인 셀 편집을 지원하지 않으며, CSV 임포트도 중첩 맵 구조를 처리하지 못한다 (Growth GDD §CR-8 명시). 이 한계로 인해 디자이너가 형태별 가중치를 편집할 수 없다.

### Constraints

- **인터페이스 contract 불변**: GDD가 정의한 `RequiredTagWeights: TMap<FName, float>` 의미적 구조는 *저장 형식과 독립*하게 유지되어야 함. Growth Score Formula F3는 Map 구조를 전제 (내적 계산: `Σ V_norm[tag] × R[tag]`)
- **5.6 production 검증**: `UDataRegistrySubsystem` 회피 (ADR-0002 §Alternatives 3 — MEDIUM-HIGH risk)
- **콘텐츠 규모**: MVP 1형태 → Full 8-12형태. 자산 수는 작음
- **per-form 자원**: 각 형태가 고유한 Mesh + Material 경로 (`MeshPath`, `MaterialPresetPath` SoftPath) 보유 → Content Browser 미리보기 + 자산별 편집이 자연스러움
- **`UAssetManager` PrimaryAssetType 등록**: cooked 빌드에서 type-bulk 로드 의무 (DefaultEngine.ini)

### Requirements

- 디자이너가 각 형태의 `RequiredTagWeights` (TMap)을 에디터 UI에서 직접 편집 가능
- `MeshPath`, `MaterialPresetPath` SoftPath 프로퍼티 인라인 편집 가능
- Pipeline R3 동기 조회 contract 호환 (sync return 가능)
- ADR-0002의 컨테이너 결정과 일관 (`UPrimaryDataAsset` per asset 패턴 — Dream과 동일)
- Growth `EvaluateFinalForm` 구현이 자연스러움 (`GetAllGrowthFormRows()` 순회 + Score 내적)

## Decision

**`FFinalFormRow` USTRUCT를 `UMossFinalFormAsset : UPrimaryDataAsset`로 격상한다.** 각 최종 형태는 별도 `.uasset` 파일로 `Content/Data/FinalForms/` 폴더에 저장. `UAssetManager`가 `FPrimaryAssetType("FinalForm")` type-bulk 로드.

**ADR-0002에서 명시한 "FinalForm = DataTable" 결정은 본 ADR로 격상되어 무효화** — DataTable 한계 회피 + DataAsset의 자연스러운 적합성 (per-form 자원).

### Class 정의

```cpp
UCLASS(BlueprintType)
class UMossFinalFormAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    // Primary key — PrimaryAssetId 생성 시 사용
    UPROPERTY(EditAnywhere, AssetRegistrySearchable)
    FName FormId;

    // 형태의 이상적 태그 분포 — DataAsset이라 TMap 인라인 편집 가능
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="1.0"))
    TMap<FName, float> RequiredTagWeights;  // 합계 1.0 권고 (UEditorValidator 경고)

    // 형태 표시 이름 (String Table 경유 권장)
    UPROPERTY(EditAnywhere)
    FText DisplayName;

    // 형태별 고유 자산 경로 (per-form unique)
    UPROPERTY(EditAnywhere, meta=(AllowedClasses="StaticMesh"))
    FSoftObjectPath MeshPath;

    UPROPERTY(EditAnywhere, meta=(AllowedClasses="MaterialInstance"))
    FSoftObjectPath MaterialPresetPath;

    // PrimaryAssetId 생성
    virtual FPrimaryAssetId GetPrimaryAssetId() const override {
        return FPrimaryAssetId("FinalForm", FormId);
    }
};
```

### DefaultEngine.ini 등록 (ADR-0002와 통합)

```ini
[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="FinalForm",
    AssetBaseClass=/Script/MossBaby.MossFinalFormAsset,
    Directories=((Path="/Game/Data/FinalForms")),
    bHasBlueprintClasses=False,
    bIsEditorOnly=False)
```

### Pipeline 통합

```cpp
// UDataPipelineSubsystem 내 — ADR-0003 §Step 2
bool UDataPipelineSubsystem::RegisterFinalFormCatalog() {
    UAssetManager& AssetMgr = UAssetManager::Get();
    TArray<FPrimaryAssetId> FormIds;
    AssetMgr.GetPrimaryAssetIdList(FPrimaryAssetType("FinalForm"), FormIds);

    if (FormIds.Num() == 0) {
        UE_LOG(LogDataPipeline, Error,
            TEXT("FinalForm PrimaryAssetType empty — DefaultEngine.ini 등록 누락 의심"));
        return false;  // DegradedFallback 진입
    }

    // 동기 sync load (ADR-0003 채택)
    TArray<FName> Bundles;  // 빈 — 모든 자산 로드
    FStreamableHandle* Handle = AssetMgr.LoadPrimaryAssets(FormIds, Bundles).Get();
    if (!Handle || !Handle->HasLoadCompleted()) {
        // sync 변종 사용 — HasLoadCompleted 보장
    }

    for (const FPrimaryAssetId& Id : FormIds) {
        UMossFinalFormAsset* Asset = Cast<UMossFinalFormAsset>(AssetMgr.GetPrimaryAssetObject(Id));
        if (!Asset) continue;
        ContentRegistry.Add(Asset->FormId, MakeWeakObjectPtr(Asset));
    }
    return true;
}

// 조회 API — Growth가 호출
TOptional<FMossFinalFormData> UDataPipelineSubsystem::GetGrowthFormRow(FName FormId) const {
    if (auto* WeakObj = ContentRegistry.Find(FormId)) {
        if (auto* Asset = Cast<UMossFinalFormAsset>(WeakObj->Get())) {
            // FMossFinalFormData는 read-only view struct — UObject 직접 노출 회피
            return FMossFinalFormData::FromAsset(*Asset);
        }
    }
    return {};  // fail-close
}

TArray<FMossFinalFormData> UDataPipelineSubsystem::GetAllGrowthFormRows() const {
    TArray<FMossFinalFormData> Out;
    Out.Reserve(ContentRegistry.Num());
    for (const auto& Pair : ContentRegistry) {
        if (auto* Asset = Cast<UMossFinalFormAsset>(Pair.Value.Get())) {
            Out.Emplace(FMossFinalFormData::FromAsset(*Asset));
        }
    }
    return Out;
}
```

### `FMossFinalFormData` (read-only view struct)

```cpp
// Growth가 소비하는 read-only data struct — UObject 직접 노출 회피
USTRUCT(BlueprintType)
struct FMossFinalFormData {
    GENERATED_BODY()
    FName FormId;
    TMap<FName, float> RequiredTagWeights;
    FText DisplayName;
    FSoftObjectPath MeshPath;
    FSoftObjectPath MaterialPresetPath;

    static FMossFinalFormData FromAsset(const UMossFinalFormAsset& Asset) {
        return {
            Asset.FormId,
            Asset.RequiredTagWeights,
            Asset.DisplayName,
            Asset.MeshPath,
            Asset.MaterialPresetPath
        };
    }
};
```

## Alternatives Considered

### Alternative 1: DataTable 유지 + `TArray<FTagWeightEntry>` struct 대체

- **Description**: TMap을 TArray로 변환, Pair struct로 저장. DataTable 인라인 편집 가능.
- **Pros**: ADR-0002와 일관 (FinalForm = DataTable 유지). CSV 임포트 가능
- **Cons**:
  1. **DataTable이 SoftObjectPath 미리보기 약함** — Mesh/Material 경로를 형태별로 편집할 때 Content Browser 통합이 DataAsset 대비 부족
  2. 런타임에 `TArray<FTagWeightEntry>` → `TMap<FName, float>` 변환 비용 (무시 가능하지만 코드 추가)
  3. **per-form 자원 (Mesh/Material) 응집성 낮음** — DataTable은 행 기반 편집이 강점이지만 FinalForm은 자산 단위 편집이 자연스러움
  4. CSV 임포트 시 `TArray<FTagWeightEntry>` 직렬화 형식이 비표준 (`Tag1:0.7,Tag2:0.2` 등 커스텀 파서 필요)
- **Rejection Reason**: FinalForm의 본질적 성격 (per-form unique mesh/material + tag weights)이 DataAsset에 더 부합. CSV 임포트는 Card에는 가치 있으나 8-12개 형태에는 불필요

### Alternative 2: 외부 JSON/YAML + 런타임 파싱

- **Description**: `Content/Data/FinalForms.json` 외부 파일에 모든 형태 정의, 런타임에 파싱
- **Pros**: 디자이너가 텍스트 에디터로 편집 가능. version control diff 친화
- **Cons**: 타입 안전성 부재 (JSON 스키마 별도 검증 필요). UE 표준 워크플로우 우회. 디자이너가 Content Browser 대신 외부 에디터 사용 — 학습 비용 ↑
- **Rejection Reason**: UE Native 메커니즘 (`UPrimaryDataAsset` + `UAssetManager`) 우회 이유 없음. Pipeline R3 contract와 통합 어려움

### Alternative 3: 단일 `UMossFinalFormCatalogAsset` (모든 형태를 하나의 자산에)

- **Description**: 단일 `UPrimaryDataAsset`이 `TArray<FFinalFormEntry>` 보유. Stillness Budget 패턴.
- **Pros**: 자산 1개만 관리. PrimaryAssetType 등록 단순화
- **Cons**:
  1. **Content Browser 형태별 미리보기 부재** — 디자이너가 8-12개 형태를 한 자산에서 스크롤 편집
  2. Mesh/Material SoftPath 인라인 편집 시 자산 미리보기 표시 약함
  3. 형태 단위 version control diff 어려움 (한 자산에 모든 변경 누적)
  4. 콘텐츠 작가 + 아트 디렉터 협업 시 conflict 가능성 ↑
- **Rejection Reason**: 형태별 협업 워크플로우에 부적합. per-form 자산이 DataAsset 패턴의 본질

### Alternative 4: ADR-0002 채택 그대로 — `FFinalFormRow` USTRUCT 유지

- **Description**: TMap 편집 제한을 받아들이고, 형태별 가중치는 *코드 const 또는 별도 ini로 외부화*
- **Pros**: ADR-0002와 충돌 없음
- **Cons**: 디자이너 편집 워크플로우 분리 — 형태 메시는 DataTable, 가중치는 ini → 디자이너 컨텍스트 스위칭. ADR-0011 (`UDeveloperSettings`)에 가중치 노출도 부적합 (per-form 데이터)
- **Rejection Reason**: 디자이너가 한 곳에서 형태 편집 불가. 본 ADR이 정확히 이 분리를 회피

## Consequences

### Positive

- **TMap 편집 가능**: `RequiredTagWeights: TMap<FName, float>`을 DataAsset 에디터에서 인라인 편집 — Growth GDD §CR-8 ADR 사안 직접 해소
- **per-form 자원 응집**: Mesh/Material/Tag weights를 한 자산에서 편집 — 디자이너 워크플로우 자연
- **Content Browser 통합**: 각 형태별 자산 미리보기 + thumbnail + version control diff 단위
- **ADR-0002 컨테이너 일관성**: Dream과 동일한 `UPrimaryDataAsset` 패턴 — 학습 비용 1회
- **`FMossFinalFormData` view struct**: Growth가 UObject 직접 노출 받지 않음 — read-only POD struct로 처리 (코드 안전)

### Negative

- **8-12개 자산 파일 관리**: Card DataTable 1개 vs FinalForm DataAsset 8-12개 — 파일 수 ↑ (단, Dream 50개 대비 미미)
- **CSV 임포트 부재**: 디자이너가 대량 형태 추가 시 자산별 생성 (Dream과 동일 패턴 — 8-12개 규모는 수동 생성 충분)
- **DefaultEngine.ini 등록 의무**: ADR-0002 Dream과 함께 PrimaryAssetTypesToScan 추가 (3건 → 본 ADR로 1건 추가)
- **ADR-0002 §Decision 표 갱신 필요**: "FinalForm = `UDataTable`" → "FinalForm = `UPrimaryDataAsset`" — 본 ADR Accepted 후 ADR-0002 명시적 amend (또는 ADR-0002 status를 superseded by ADR-0010 부분 명시)

### Risks

- **PrimaryAssetType 등록 누락 → cooked 빈 결과**: ADR-0002와 동일 위험 (AC-DP-18 [5.6-VERIFY] 공유). **Mitigation**: AC-DP-18 검증 + `GetFailedCatalogs()` 진단
- **`UMossFinalFormAsset` GC 안전성**: `ContentRegistry`의 `TWeakObjectPtr` GC 후 nullptr 가능. **Mitigation**: ADR-0002 §Risks와 동일 — `UPROPERTY()` 캐시 + WeakPtr Get null check
- **`RequiredTagWeights` 합계 ≠ 1.0**: 디자이너 실수로 Score 계산 결과가 비교 불가능 (Growth F3 — 단, argmax 상대 비교이므로 절대 Score 무의미). **Mitigation**: `UEditorValidatorBase` 서브클래스가 저장 시점에 합계 검증 + Warning (Growth AC-GAE-11b ADVISORY)

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `growth-accumulation-engine.md` | §CR-8 ADR 대상 — "DataTable 편집 제한 (TMap 인라인 편집 미지원)" | **본 ADR이 CR-8 ADR 사안의 정식 결정** (Alternative 1 거부 + UPrimaryDataAsset 격상) |
| `growth-accumulation-engine.md` | §CR-8 필드 정의 (FormId, RequiredTagWeights, DisplayName, MeshPath, MaterialPresetPath) | `UMossFinalFormAsset` UCLASS가 모든 필드 보유 |
| `growth-accumulation-engine.md` | F3 Form Score Formula (`Σ V_norm[tag] × R[tag]`) | TMap 구조 유지로 내적 계산 자연스러움 |
| `growth-accumulation-engine.md` | AC-GAE-11a — `UEditorValidatorBase` 검증 (RequiredTagWeights 비어있거나 합 = 0인 행 차단) | UPrimaryDataAsset에도 동일 Validator 적용 가능 |
| `growth-accumulation-engine.md` | AC-GAE-11b ADVISORY — 합계 ≠ 1.0 경고 | DataAsset 저장 시점 Validator |
| `data-pipeline.md` | §Container Choices 표 (FinalForm = DataTable) | **본 ADR이 표를 amend** — FinalForm = DataAsset |
| `data-pipeline.md` | R2 BLOCKER 4 PrimaryAssetType 등록 | 본 ADR §DefaultEngine.ini 등록 섹션 |
| `data-pipeline.md` | AC-DP-18 [5.6-VERIFY] | FinalForm PrimaryAssetType도 동일 검증 적용 |
| `moss-baby-character-system.md` | CR-1 FinalReveal — `SM_MossBaby_Final_[ID]` mesh 교체 | `UMossFinalFormAsset.MeshPath` 직접 참조 |

## Performance Implications

- **CPU (load)**: ADR-0003과 동일 — 8-12개 자산 sync load = ~1ms 추가 (D2 Formula에 이미 포함)
- **Memory (catalog)**: ADR-0002 D1 Formula 대비 변화 없음 — `FFinalFormRow` ~256 bytes vs `UMossFinalFormAsset` ~256 bytes (UObject overhead 약간 ↑, 무시 가능)
- **Memory (per-form 자원)**: Mesh + Material은 SoftPath이므로 lazy load — 본 ADR과 무관
- **Disk**: DataTable 1 파일 → DataAsset 8-12 파일 — 파일 수 ↑이나 디스크 사용량 무시 가능
- **Network**: N/A

## Migration Plan

ADR-0002 / ADR-0003과 묶음 작성 → Foundation sprint:

1. **`UMossFinalFormAsset` UCLASS 정의**: `Source/MadeByClaudeCode/Data/MossFinalFormAsset.h`
2. **`FMossFinalFormData` view struct 정의**: 동일 헤더 또는 별도
3. **콘텐츠 폴더 생성**: `Content/Data/FinalForms/`
4. **MVP 1형태 자산 작성**: `FA_DefaultForm.uasset` (디자이너 작업)
5. **DefaultEngine.ini 갱신**: PrimaryAssetTypesToScan에 FinalForm 추가 (ADR-0002 통합)
6. **`UDataPipelineSubsystem::RegisterFinalFormCatalog` 구현**: 상기 코드
7. **Growth `EvaluateFinalForm` 갱신**: `GetAllGrowthFormRows() → TArray<FMossFinalFormData>` 사용
8. **`UEditorValidatorBase` 서브클래스 작성**: RequiredTagWeights 합 검증 (Growth AC-GAE-11)
9. **ADR-0002 §Decision 표 amend**: "FinalForm = UPrimaryDataAsset (ADR-0010 격상)" 명시 (이미 ADR-0002 §Decision 표에 그렇게 작성되어 있음 — 본 ADR이 그 명시의 정식 결정)

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| `UMossFinalFormAsset` 정의 + UPrimaryDataAsset 상속 | grep `class UMossFinalFormAsset.*UPrimaryDataAsset` | 매치 1건 |
| `RequiredTagWeights: TMap<FName, float>` 필드 존재 | grep | 매치 1건 |
| 에디터 인라인 편집 가능 | 수동 — 자산 더블클릭 → RequiredTagWeights 항목 추가/제거 | 동작 확인 |
| Pipeline 카탈로그 등록 | AC-DP-02 등록 순서 + Step 2 통과 | 정상 |
| Growth F3 Score 계산 정확 | Growth AC-GAE-06 (FormA Score 0.417, FormB Score 0.201) | 정확 |
| Cooked PrimaryAssetType | AC-DP-18 [5.6-VERIFY] — `GetPrimaryAssetIdList("FinalForm")` 비어있지 않음 | 결과 ≥ 1 |
| RequiredTagWeights 합 검증 | UEditorValidatorBase — 합이 0이면 Invalid | AC-GAE-11a 통과 |
| MVP 1형태 동작 | MVP 7일 플레이스루 — Day 7에 Default Form 결정 | 정상 |
| ADR-0002 amend | ADR-0002 §Decision 표에 "FinalForm = UPrimaryDataAsset (ADR-0010)" 명시 확인 | ADR-0002 검토 |

## Related Decisions

- **growth-accumulation-engine.md** §CR-8 ADR 사안 — 본 ADR의 직접 출처
- **ADR-0002** (Data Pipeline 컨테이너 선택) — 본 ADR이 §Decision 표의 "FinalForm = DataTable" 결정을 amend (UPrimaryDataAsset 격상)
- **ADR-0003** (Pipeline 로딩 전략) — 본 ADR의 자산은 ADR-0003 채택 sync 로드로 처리
- **ADR-0011** (`UDeveloperSettings`) — `UMossFinalFormAsset`은 per-content 데이터 → `UDeveloperSettings` 적용 대상 아님 (ADR-0011 예외 목록)
- **architecture.md** §Module Ownership §Growth Engine — `FFinalFormRow` field semantics는 Growth 소유, 본 ADR은 저장 형식만 결정
- **MBC GDD** CR-1 FinalReveal `SM_MossBaby_Final_[ID]` — `UMossFinalFormAsset.MeshPath` 참조
