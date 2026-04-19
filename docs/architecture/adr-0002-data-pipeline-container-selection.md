# ADR-0002: Data Pipeline 컨테이너 선택

> **⚠ Cross-reference (C7 해결 2026-04-19)**: §Decision 표의 "FinalForm = UDataTable" 결정은 **ADR-0010에 의해 `UMossFinalFormAsset : UPrimaryDataAsset` per-form으로 격상**됨. 본 ADR의 §Alternatives 4 평가에서 도출. 독자는 본 ADR 단독으로 읽을 경우 §Decision 표의 FinalForm 행을 ADR-0010으로 대체하여 해석할 것. 본 ADR의 FinalForm-관련 DataTable 논의는 원본 proposal 보존용이며 실제 구현은 ADR-0010을 따른다.

## Status

**Accepted** (2026-04-19 — `/architecture-review` 통과, C7 해결 cross-ref 노트 추가)

## Date

2026-04-18

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | Core / Data / Asset Management |
| **Knowledge Risk** | MEDIUM — `UAssetManager::GetPrimaryAssetIdList` cooked-build 동작 검증 필요 (Data Pipeline R2 BLOCKER 4, AC-DP-18 [5.6-VERIFY]). `UDataRegistrySubsystem`은 5.6 production 검증 부족 (HIGH risk — 본 ADR이 의도적으로 회피) |
| **References Consulted** | `VERSION.md`, `current-best-practices.md §7 Data-driven`, `breaking-changes.md`, data-pipeline.md §Container Choices, systems-index.md R2 |
| **Post-Cutoff APIs Used** | None — `UDataTable` / `UPrimaryDataAsset` / `UAssetManager` 모두 4.x+ 안정 |
| **Verification Required** | AC-DP-18 (Cooked build PrimaryAssetTypesToScan `DreamData` 등록 동작) — Implementation 단계 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None |
| **Enables** | ADR-0003 (로딩 전략), ADR-0010 (FFinalFormRow 격상), 모든 Feature/Presentation 시스템의 Pipeline pull API 사용 |
| **Blocks** | Foundation sprint (Data Pipeline 구현) — 컨테이너 선택 없이 구현 불가 |
| **Ordering Note** | ADR-0003, ADR-0010과 **동시 작성 권장** — 3건 모두 Foundation sprint 시작 전 묶음 결정. ADR-0010은 본 ADR의 "FinalForm = DataTable" 결정을 **격상**하여 UPrimaryDataAsset으로 변경 |

## Context

### Problem Statement

Data Pipeline (#2)이 4개 카탈로그 (Card / FinalForm / Dream / Stillness Budget)를 어떤 UE 컨테이너로 저장할 것인가? 선택은 디자이너 편집 워크플로우 + 로딩 성능 + 5.6 production 검증도에 직접 영향.

### Constraints

- **C1 schema gate**: FGiftCardRow에 수치 stat 필드 정의 금지 (Pillar 2 보호)
- **C2 schema gate**: Dream 임계는 per-asset `UPROPERTY(EditAnywhere)` 필드로 외부 노출 필수 (Pillar 3 보호)
- **5.6 production 검증**: `UDataRegistrySubsystem`은 5.6 cutoff 직후 API로 production 사용 사례 부족 (MEDIUM-HIGH risk)
- **콘텐츠 규모**: MVP 10카드/5꿈/1형태/1Stillness → Full 40카드/50꿈/8-12형태/1Stillness. 100행 이하 수준
- **디자이너 편집 워크플로우**: 카드는 행 기반 편집 (CSV 임포트 가능), 꿈은 자산별 편집 (텍스트 작가 + 트리거 조건 병행)

### Requirements

- 각 컨테이너는 디자이너 편집 워크플로우에 자연스러움
- 5.6에서 안정적 동작 (LOW-MEDIUM risk API만 사용)
- Pipeline R3 fail-close contract 호환 (`TOptional<>` / `nullptr` 반환 가능)
- `UAssetManager` 통합 (Dream의 type-bulk 로드)
- Blueprint 직접 접근 가능 (프로토타이핑 지원)

## Decision

**4개 카탈로그 컨테이너 채택 (current-best-practices.md §7 사전 가이드 준수, 단 FinalForm은 ADR-0010에서 격상)**:

| 콘텐츠 | 컨테이너 | 근거 |
|---|---|---|
| **Card (MVP 10 / Full 40)** | `UDataTable` + `FGiftCardRow` USTRUCT | 행 기반 편집이 디자이너 워크플로우에 자연스러움. CSV 임포트로 대량 추가 가능. Blueprint 직접 접근. 단일 `.uasset` 패키징 |
| **FinalForm (MVP 1 / Full 8-12)** | **`UPrimaryDataAsset` per form** (**ADR-0010 격상**) | 각 형태별 Mesh/Material 경로 포함 → DataAsset이 자연. `TMap<FName, float> RequiredTagWeights` 편집 가능 (DataTable은 TMap 인라인 편집 미지원). 본 ADR은 current-best-practices.md의 DataTable 제안을 **ADR-0010에서 격상** |
| **Dream (MVP 5 / Full 50)** | `UPrimaryDataAsset` per dream | 트리거 조건 + FText 본문이 자산별 응집. 텍스트 작가가 자산 단위로 편집. `UAssetManager::GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"))`로 type-bulk 로드 — DefaultEngine.ini `PrimaryAssetTypesToScan` 등록 필수 (AC-DP-18 [5.6-VERIFY]) |
| **Stillness Budget (단일)** | `UPrimaryDataAsset` (single instance) | 외부 노출 필요한 모든 limit·threshold가 한 자산에 응집. Pipeline Initialize 시 1회 pull + 캐싱. multiple instance 금지 |

**의도적 미사용**: `UDataRegistrySubsystem` — 5.6 production 검증 부족 (MEDIUM-HIGH risk). 총 100행 이하 규모에서 런타임 스왑·스트리밍 이점 불필요. 미래 규모 확대 시 재검토 가능 (별도 ADR로 superseded).

### Key Interfaces

```cpp
// Card — DataTable + USTRUCT
USTRUCT(BlueprintType)
struct FGiftCardRow : public FTableRowBase {
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) FName CardId;
    UPROPERTY(EditAnywhere) TArray<FName> Tags;            // FGameplayTagContainer 검토 (Card Implementation Notes)
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere) FText Description;
    UPROPERTY(EditAnywhere) FSoftObjectPath IconPath;
    // C1 schema gate: int32/float/double 수치 필드 금지
};

// FinalForm — UPrimaryDataAsset (ADR-0010 격상)
UCLASS()
class UMossFinalFormAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) FName FormId;
    UPROPERTY(EditAnywhere) TMap<FName, float> RequiredTagWeights;  // DataTable 인라인 편집 불가 → DataAsset
    UPROPERTY(EditAnywhere) FText DisplayName;
    UPROPERTY(EditAnywhere) FSoftObjectPath MeshPath;
    UPROPERTY(EditAnywhere) FSoftObjectPath MaterialPresetPath;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override {
        return FPrimaryAssetId("FinalForm", FormId);
    }
};

// Dream — UPrimaryDataAsset per dream
UCLASS()
class UDreamDataAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere) FName DreamId;
    UPROPERTY(EditAnywhere) FText BodyText;
    UPROPERTY(EditAnywhere) TMap<FName, float> TriggerTagWeights;   // C2 schema gate
    UPROPERTY(EditAnywhere, meta=(ClampMin="0.0", ClampMax="1.0")) float TriggerThreshold;
    UPROPERTY(EditAnywhere) EGrowthStage RequiredStage;
    UPROPERTY(EditAnywhere) int32 EarliestDay = 2;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override {
        return FPrimaryAssetId("DreamData", DreamId);
    }
};

// Stillness Budget — UPrimaryDataAsset single instance
UCLASS()
class UStillnessBudgetAsset : public UPrimaryDataAsset {
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, meta=(ClampMin="1", ClampMax="6")) int32 MotionSlots = 2;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1", ClampMax="6")) int32 ParticleSlots = 2;
    UPROPERTY(EditAnywhere, meta=(ClampMin="1", ClampMax="8")) int32 SoundSlots = 3;
    UPROPERTY(EditAnywhere) bool bReducedMotionEnabled = false;
};
```

### DefaultEngine.ini 필수 등록

```ini
[/Script/Engine.AssetManagerSettings]
+PrimaryAssetTypesToScan=(PrimaryAssetType="DreamData", AssetBaseClass=/Script/CoreUObject.Class, Directories=((Path="/Game/Data/Dreams")), bHasBlueprintClasses=False, bIsEditorOnly=False)
+PrimaryAssetTypesToScan=(PrimaryAssetType="FinalForm", AssetBaseClass=/Script/CoreUObject.Class, Directories=((Path="/Game/Data/FinalForms")), bHasBlueprintClasses=False, bIsEditorOnly=False)
+PrimaryAssetTypesToScan=(PrimaryAssetType="StillnessBudget", AssetBaseClass=/Script/CoreUObject.Class, Directories=((Path="/Game/Data")), bHasBlueprintClasses=False, bIsEditorOnly=False)
```

## Alternatives Considered

### Alternative 1: 모든 콘텐츠를 `UDataTable`로 통일

- **Description**: Card + FinalForm + Dream + Stillness 모두 DataTable + USTRUCT.
- **Pros**: 단일 컨테이너 형태. CSV 임포트/익스포트로 대량 편집 가능
- **Cons**: Dream의 긴 FText 본문 + 자산별 에디팅 워크플로우에 부자연 (텍스트 작가가 행 기반 편집 어려움). **TMap 인라인 편집 미지원** (FinalForm `RequiredTagWeights`, Dream `TriggerTagWeights` 편집 불가) → CR-8 문제 발생
- **Rejection Reason**: TMap 편집 제약이 결정적. Dream UX도 DataAsset이 더 자연스러움

### Alternative 2: 모든 콘텐츠를 `UPrimaryDataAsset`으로 통일

- **Description**: Card 각 카드도 자산 단위로 편집.
- **Pros**: 단일 메커니즘. Card 이미지 미리보기 인라인 가능
- **Cons**: 40장 카드 각각 `.uasset` 생성 → 파일 시스템 비대. CSV 임포트 불가 → 디자이너 대량 편집 비효율. DataTable 단일 `.uasset`보다 메모리·로드 오버헤드 증가
- **Rejection Reason**: 행 기반 편집이 카드 콘텐츠 워크플로우에 최적. 40개 자산 관리 비용 > 이점

### Alternative 3: `UDataRegistrySubsystem` 채택

- **Description**: UE 4.27+에서 추가된 공식 콘텐츠 레지스트리 시스템. 런타임 스왑·스트리밍·태그 기반 쿼리 지원.
- **Pros**: 공식 API, 미래 대규모 확장 대비
- **Cons**: **5.6 production 검증 부족** (MEDIUM-HIGH risk). 100행 이하 규모에서 런타임 스왑·스트리밍 이점 불필요. 학습·디버깅 비용 + 5.6 API drift 가능성
- **Rejection Reason**: 검증되지 않은 복잡성 추가 vs 입증된 단순성. 미래 규모 확대 시 재검토 (별도 ADR로 superseded 가능)

### Alternative 4: FinalForm을 DataTable 유지 + `TArray<FTagWeightEntry>` struct 대체

- **Description**: TMap을 TArray로 변환, Pair struct로 저장 (DataTable 편집 가능 회피)
- **Pros**: DataTable 일관성 유지
- **Cons**: 런타임에 TMap 변환 비용 (무시 가능). **FinalForm은 Mesh/Material 경로 포함** → DataAsset이 자연스러움 (Content Browser에서 형태별 미리보기). 8-12개 형태 관리에 DataAsset이 더 명료
- **Rejection Reason**: FinalForm의 본질적 성격 (per-form unique asset references) 고려 시 DataAsset이 우월. → **ADR-0010으로 격상**

## Consequences

### Positive

- **워크플로우 최적 매칭**: 각 콘텐츠 성격에 맞는 컨테이너 (행 기반 vs 자산 기반). 디자이너 학습 비용 최소
- **5.6 안정성**: 모든 컨테이너가 4.x+ 안정 API — MEDIUM risk 1건 (AssetManager cooked 검증)만 명시적 flag
- **Pillar 보호 강화**: C1/C2 schema gate가 타입 수준에서 강제 (USTRUCT 필드 omission + UPROPERTY 노출 의무)
- **`UAssetManager` type-bulk 로드**: Dream은 50개 자산을 단일 API 호출로 로드 → 초기화 시간 O(1) scaling
- **Blueprint 접근**: UDataTable은 Blueprint 직접 쿼리 가능 (프로토타이핑 지원)

### Negative

- **이중 메커니즘 학습**: 디자이너가 "카드는 DataTable, 꿈/형태는 DataAsset" 규칙 학습. **Mitigation**: 본 ADR §Decision 표를 onboarding 문서에 인용
- **DefaultEngine.ini 등록 의무**: `PrimaryAssetTypesToScan` 미등록 시 cooked 빌드에서 Dream/FinalForm 조회 실패 — **AC-DP-18 검증 의무**
- **미래 확장 시 재평가 필요**: Full Vision 이후 100+ 카드/꿈 규모면 `UDataRegistrySubsystem` 재검토 — 별도 ADR로 superseded 가능

### Risks

- **Cooked build PrimaryAssetType 미동작**: DefaultEngine.ini 등록 누락 또는 5.6 cooked pipeline 차이로 Dream/FinalForm 빈 결과 반환 가능. **Mitigation**: AC-DP-18 [5.6-VERIFY] — Cooked build 테스트 환경에서 `GetPrimaryAssetIdList` 결과 검증 의무. 미등록 시 `UE_LOG Error` + DegradedFallback (기능 차단 없이 silent fallback)
- **DataTable CSV 임포트 비 ASCII**: 한국어 FText 필드 CSV import 시 인코딩 이슈 가능. **Mitigation**: UE 5.6 기본 UTF-8 CSV 지원 확인 필요 (Implementation 검증)
- **FinalForm 격상 (ADR-0010) 구현 복잡도**: Pipeline R2 등록 순서에서 "FinalForm DataTable" → "FinalForm DataAsset bucket"으로 변경. **Mitigation**: ADR-0010에서 구체 구현 명시

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `data-pipeline.md` | OQ-ADR-1 "데이터 컨테이너 선택 — `UDataTable` vs `UPrimaryDataAsset` vs `UDataRegistrySubsystem`" | **본 ADR이 OQ-ADR-1의 정식 결정** |
| `data-pipeline.md` | §Container Choices 표 — Card=DT / Dream=DataAsset / FinalForm=DT / Stillness=DataAsset | 본 ADR이 표를 채택 (FinalForm은 ADR-0010에서 격상) |
| `data-pipeline.md` | R4 C1 schema 가드 (카드 수치 필드 금지) | `FGiftCardRow` USTRUCT 정의에서 `int32`/`float` 필드 omission — 컴파일 타임 강제 |
| `data-pipeline.md` | R5 C2 schema 가드 (Dream 임계 외부 노출) | `UDreamDataAsset` 의 `UPROPERTY(EditAnywhere)` 필드로 TriggerThreshold/TriggerTagWeights 강제 노출 |
| `data-pipeline.md` | R2 BLOCKER 4 — AssetManager PrimaryAssetType 등록 | 본 ADR §DefaultEngine.ini 필수 등록 섹션 |
| `card-system.md` | §Dependencies — Pipeline.GetCardRow(FName) | Card = UDataTable 결정으로 GetCardRow 구현 자연스러움 |
| `growth-accumulation-engine.md` | CR-8 DataTable TMap 편집 제한 | **본 ADR이 ADR-0010에서 격상** (UPrimaryDataAsset per form) |
| `dream-trigger-system.md` | CR-7 UDreamDataAsset field 확장 (Pipeline forward-decl 인수) | Dream = UPrimaryDataAsset 결정으로 자산별 필드 확장 자연스러움 |

## Performance Implications

- **CPU 초기화**: SSD warm-load 기준 MVP ≈ 2.7ms, Full ≈ 8.2ms (Pipeline D2 Formula). 50ms budget 내 여유 (ADR-0003 채택 Sync 로드)
- **Memory (CPU-side catalog)**: MVP ≈ 12KB, Full ≈ 120KB (Pipeline D1 Formula). 1.5GB ceiling의 0.01% — 무시 가능
- **Memory (런타임 `TWeakObjectPtr` 캐시)**: Pipeline `ContentRegistry: TMap<FName, TWeakObjectPtr<UObject>>` — ~8 bytes per entry × 100 entries ≈ 800 bytes
- **디스크**: Card `.uasset` 1개 + FinalForm `.uasset` 8-12개 + Dream `.uasset` 50개 + Stillness 1개 = ~60 파일 (Full Vision). 무시 가능
- **Load Time**: `UAssetManager::GetPrimaryAssetIdList` type-bulk + 동기 sync load = 50ms budget 내 (ADR-0003 참조)

## Migration Plan

Foundation sprint 진입 시:

1. **USTRUCT + UCLASS 정의**: `FGiftCardRow`, `UMossFinalFormAsset` (ADR-0010에서 상세), `UDreamDataAsset`, `UStillnessBudgetAsset`
2. **콘텐츠 디렉터리 생성**: `Content/Data/Cards/DT_Cards.uasset` (DataTable), `Content/Data/FinalForms/` (DataAsset 폴더), `Content/Data/Dreams/` (DataAsset 폴더), `Content/Data/StillnessBudget.uasset`
3. **DefaultEngine.ini 갱신**: `[/Script/Engine.AssetManagerSettings]` 섹션에 PrimaryAssetTypesToScan 3건 추가
4. **UDataPipelineSubsystem 구현**: R2 등록 순서 (Card DataTable → FinalForm DataAsset bucket → Dream DataAsset bucket → Stillness single DataAsset)
5. **AC-DP-18 [5.6-VERIFY] 실행**: Cooked build에서 `GetPrimaryAssetIdList` 결과 검증

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| Card = DataTable | `Content/Data/Cards/DT_Cards.uasset` 존재 + FGiftCardRow USTRUCT 정의 | Content Browser 확인 |
| FinalForm = DataAsset (ADR-0010) | `Content/Data/FinalForms/` 폴더에 UMossFinalFormAsset 자산들 | Content Browser |
| Dream = DataAsset | `Content/Data/Dreams/` 폴더에 UDreamDataAsset 자산들 | Content Browser |
| Stillness = DataAsset | `Content/Data/StillnessBudget.uasset` 존재 | Content Browser |
| C1 schema gate | `FGiftCardRow` 정의 grep `int32\|float\|double` — 수치 효과 필드 0건 | AC-DP-06 |
| C2 schema gate | Dream 코드 리터럴 임계 grep 0건 | Code review |
| AssetManager 등록 | Cooked build에서 `GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"))` 결과 비어있지 않음 | AC-DP-18 [5.6-VERIFY] |
| UDataRegistrySubsystem 미사용 | 코드베이스 grep `UDataRegistry` = 0 매치 | Code review |

## Related Decisions

- **data-pipeline.md** §Container Choices — 본 ADR의 직접 출처
- **ADR-0003** (Data Pipeline 로딩 전략) — 본 ADR Accepted 후 로딩 방식 결정
- **ADR-0010** (FFinalFormRow 저장 형식) — 본 ADR의 FinalForm 결정을 DataTable → UPrimaryDataAsset으로 격상
- **ADR-0011** (Tuning Knob — `UDeveloperSettings`) — Data Pipeline의 Settings는 UDeveloperSettings로, 콘텐츠 자산은 본 ADR 결정으로 분리
- **architecture.md** §Architecture Principles §Principle 2 (Schema as Pillar Guard) — 본 ADR이 C1/C2 schema gate를 컨테이너 형태로 강제
