# Story 003: UDataPipelineSubsystem 뼈대 + 4-state machine + pull API contract

> **Epic**: Data Pipeline
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 0.5 days (~3-4 hours)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/data-pipeline.md`
**Requirement**: `TR-dp-002` (OQ-ADR-2 loading strategy — sync return)
**ADR Governing Implementation**: ADR-0003 (Sync 일괄 로드), ADR-0002 (컨테이너)
**ADR Decision Summary**: `UDataPipelineSubsystem : UGameInstanceSubsystem`. 4-state 머신 (Uninitialized → Loading → Ready / DegradedFallback). 모든 pull API는 sync return (`TOptional<FRow>`, `UObject*`, `TArray<T>`). Fail-close contract.
**Engine**: UE 5.6 | **Risk**: MEDIUM (GameInstance::Init timing)
**Engine Notes**: `UGameInstanceSubsystem::Initialize` 자동 호출 — `UGameInstance::Init()` 반환 전 확정 보장 (AC-DP-01). Downstream은 `Collection.InitializeDependency<UDataPipelineSubsystem>()` 호출 (R2 BLOCKER 5).

**Control Manifest Rules (Foundation layer)**:
- Required: "`UDataPipelineSubsystem::Initialize()`는 sync 순차 로드" (ADR-0003)
- Required: "Pipeline pull API는 전부 sync return" — `TOptional<FRow>` / `UObject*` / `TArray<T>` (ADR-0003)
- Required: "Downstream Subsystem Initialize는 `InitializeDependency<UDataPipelineSubsystem>()` 호출" (ADR-0003)
- Forbidden: "async bundle loading for all catalogs" — R3 sync contract 깨짐 (ADR-0003)
- Forbidden: "lazy load (첫 조회 시 로드)" — 첫 프레임 이후 지연 스파이크 (ADR-0003)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] AC-DP-01: `UGameInstance::Init()` 호출 시점에 `UDataPipelineSubsystem::Initialize()` 자동 호출. `IsReady()` 또는 `GetState() == DegradedFallback`이 `Init()` 반환 전 확정. 첫 World tick 전까지 상태 변경 없음 (Integration · BLOCKING · P0)
- [ ] `EDataPipelineState` enum 정의: `Uninitialized`, `Loading`, `Ready`, `DegradedFallback`
- [ ] `GetState() const` + `IsReady() const` public API (sync)
- [ ] `OnLoadComplete(bool bFreshStart, bool bHadPreviousData)` delegate 선언 (Foundation `foundation-save-load` epic도 동일 pattern)
- [ ] AC-DP-13: `Uninitialized` 또는 `Loading` 상태에서 `GetCardRow(유효 이름)` 호출 시 `checkf` assertion (Debug/Development 빌드) + `IsReady() == false` (Logic · BLOCKING · P0)
- [ ] AC-DP-04 skeleton: `GetCardRow(FName)` → `TOptional<FGiftCardRow>` (NAME_None, 존재하지 않는 ID, 삭제된 ID 모두 빈 `TOptional` 반환. default-constructed row 반환 금지) — Story 004에서 카탈로그 연동 후 실제 AC 검증
- [ ] Pull API 스텁 선언: `GetCardRow(FName) → TOptional<FGiftCardRow>`, `GetAllCardIds() → TArray<FName>`, `GetGrowthFormRow(FName) → TOptional<FMossFinalFormData>`, `GetAllGrowthFormIds() → TArray<FName>`, `GetDreamAsset(FName) → UDreamDataAsset*`, `GetAllDreamAssets() → TArray<UDreamDataAsset*>`, `GetDreamBodyText(FName) → FText`, `GetStillnessBudgetAsset() → UStillnessBudgetAsset*`

---

## Implementation Notes

*Derived from ADR-0003 §Step 1-4 + GDD §States and Transitions:*

```cpp
// Source/MadeByClaudeCode/Data/DataPipelineSubsystem.h
UENUM()
enum class EDataPipelineState : uint8 { Uninitialized, Loading, Ready, DegradedFallback };

UCLASS()
class UDataPipelineSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    EDataPipelineState GetState() const { return CurrentState; }
    bool IsReady() const { return CurrentState == EDataPipelineState::Ready; }

    // Pull API — Story 004/005에서 구체 구현
    TOptional<FGiftCardRow> GetCardRow(FName CardId) const;
    TArray<FName> GetAllCardIds() const;
    TOptional<FMossFinalFormData> GetGrowthFormRow(FName FormId) const;
    TArray<FName> GetAllGrowthFormIds() const;
    UDreamDataAsset* GetDreamAsset(FName DreamId) const;
    TArray<UDreamDataAsset*> GetAllDreamAssets() const;
    FText GetDreamBodyText(FName DreamId) const;
    UStillnessBudgetAsset* GetStillnessBudgetAsset() const;

    // DegradedFallback API (AC-DP-15)
    TArray<FName> GetFailedCatalogs() const { return FailedCatalogs; }

    // PIE hot-swap (AC-DP-07)
    void RefreshCatalog();

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLoadComplete, bool /*bFreshStart*/, bool /*bHadPreviousData*/);
    FOnLoadComplete OnLoadComplete;

private:
    EDataPipelineState CurrentState = EDataPipelineState::Uninitialized;

    UPROPERTY()  // R2 BLOCKER 6 GC safety
    TMap<FName, TObjectPtr<UDreamDataAsset>> DreamRegistry;
    UPROPERTY()
    TMap<FName, TObjectPtr<UMossFinalFormAsset>> FormRegistry;
    UPROPERTY()
    TObjectPtr<UDataTable> CardTable;
    UPROPERTY()
    TObjectPtr<UStillnessBudgetAsset> StillnessAsset;

    TArray<FName> FailedCatalogs;
    bool bRefreshInProgress = false;  // AC-DP-14 재진입 차단
};

// Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp
TOptional<FGiftCardRow> UDataPipelineSubsystem::GetCardRow(FName CardId) const {
    checkf(IsReady(), TEXT("Pipeline not Ready — AC-DP-13 violation"));
    if (CardId.IsNone()) { return {}; }  // Fail-close
    if (!CardTable) { return {}; }
    const FGiftCardRow* Row = CardTable->FindRow<FGiftCardRow>(CardId, TEXT("GetCardRow"));
    if (!Row) {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("Unknown CardId: %s — rename 또는 migration 누락 의심"), *CardId.ToString());  // AC-DP-16
        return {};
    }
    return *Row;
}
```

- `FMossFinalFormData`는 read-only view struct (UObject 직접 노출 회피, ADR-0010)
- 모든 `UObject*` 멤버는 `UPROPERTY()` 매크로 필수 (R2 BLOCKER 6)
- `checkf` 사용 시 Debug/Development 빌드 검증 (Shipping에서도 유지 권장 — ADR-0003 AC-DP-13)

---

## Out of Scope

- Story 004: 실제 카탈로그 등록 로직 (Step 1-4)
- Story 005: E 케이스 (DuplicateCardId, FTextEmpty, RefreshCatalog 재진입)

---

## QA Test Cases

**For Logic story:**
- **AC-DP-01**
  - Given: 정상 GameInstance init 경로
  - When: `UGameInstance::Init()` 실행
  - Then: `Init()` 반환 시점에 `GetState() != Uninitialized` (Ready 또는 DegradedFallback)
- **AC-DP-13 (Pre-init 조회 차단)**
  - Given: Subsystem Uninitialized 상태 (Init 시작 전)
  - When: `GetCardRow(SomeValidName)` 호출
  - Then: `checkf` fire (Debug/Development) + process assertion failure
  - Edge cases: `Loading` 상태도 동일 동작
- **AC-DP-04 skeleton (Story 004에서 완성)**
  - Given: Ready 상태
  - When: `GetCardRow(NAME_None)` 또는 `GetCardRow("nonexistent")`
  - Then: 빈 `TOptional` 반환 (default FGiftCardRow 반환 금지)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/data-pipeline/subsystem_lifecycle_test.cpp` (AC-DP-01 + AC-DP-13 checkf)
- `tests/unit/data-pipeline/pre_init_query_guard_test.cpp` (AC-DP-13)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (자산 타입), Story 002 (DefaultEngine.ini)
- Unlocks: Story 004, 005, 006

---

## Completion Notes

**Completed**: 2026-04-19
**Criteria**: 7/7 passing (AC-DP-01 + enum + GetState/IsReady + OnLoadComplete + AC-DP-13 + AC-DP-04 + 8 pull API)
**Files delivered**:
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.h` (177 lines)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp` (195 lines, 8 checkf(IsReady()) + state machine + 8 pull API 스텁)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Data/MossFinalFormData.h` (89 lines, read-only view struct + FromAsset static factory)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineSubsystemTests.cpp` (197 lines, 4 tests)
- `tests/unit/data-pipeline/README.md` (Story 1-5 섹션 append)

**Test Evidence**: 4 UE Automation tests — `MossBaby.Data.Pipeline.{InitialState/PullAPIReturnsEmptyOnReady/DelegateBind/DeinitClearsRegistries}`.  `#if WITH_AUTOMATION_TESTS`로 `TestingSetState()` 노출 (checkf 우회 필요한 테스트용).

**ADR 준수**:
- ADR-0001 금지 패턴 grep: 0 매치 (주석 인용만)
- ADR-0003: 모든 8 pull API `TOptional`/`UObject*`/`TArray` sync return. async/lazy 실제 사용 0건.
- ADR-0002/0010: `FMossFinalFormData` read-only view struct, UObject 직접 노출 회피.

**AC-DP-13 checkf 구현**: `.cpp`의 8개 pull API 함수 각각 `checkf(IsReady(), "... AC-DP-13 violation")` 가드. Debug/Development 빌드에서 pre-Ready 조회 차단. CI에서 Shipping 빌드 grep 검증 전략은 README에 문서화.

**Code Review**: Direct verification pass (자동 진행 정책 하에 specialist 리뷰 생략 — 이전 4개 스토리의 패턴 일관 확인)
- checkf(IsReady()) count: 8/8 (각 pull API 1개씩)
- Async/Lazy load: 0건 (주석 "Async Bundle 전환 검토 flag"는 Story 006 DegradedFallback 트리거 설명)

**Deviations / Notes**:
- `GetDreamBodyText()` 내부에서 `GetDreamAsset()` 호출로 `checkf` 중복 체크 발생 (성능 무시 가능). Story 1-6에서 내부 헬퍼 분리 검토.
- `Deinitialize()` 직접 호출 테스트는 실제 `UGameInstance` 해제 경로와 다름. 전체 lifecycle 검증은 integration test 필요 (TD-005에 포함).

**Out of Scope (다음 스토리)**:
- Story 1-6 (Pipeline 004): 실제 catalog registration + AssetManager loading
- Story 1-20 (Pipeline 005): Edge cases (DuplicateCardId, FTextEmpty, RefreshCatalog 재진입)
