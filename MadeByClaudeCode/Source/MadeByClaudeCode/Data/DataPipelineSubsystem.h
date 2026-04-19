// Copyright Moss Baby
//
// DataPipelineSubsystem.h — Data Pipeline 메인 서브시스템 선언
//
// Story-1-5: UDataPipelineSubsystem 뼈대 + 4-state machine + pull API 스텁
// Story-1-6: Initialize 4단계 실제 구현 + RegisterXxx helpers + DegradedFallback
// Story-1-19: T_init budget 3-단계 overflow threshold + Catalog size 체크 + GetCatalogStats
// ADR-0003: Sync 일괄 로드 채택 (4-state machine, sync return contract)
// ADR-0002: 컨테이너 선택 (Card=DT, FinalForm=DataAsset, Dream=DataAsset, Stillness=DataAsset)
// ADR-0010: FMossFinalFormData read-only view struct (UObject 직접 노출 회피)
//
// 4-state machine (ADR-0003 §State Machine):
//   Uninitialized → Loading → Ready
//   Loading → DegradedFallback (카탈로그 등록 실패 시)
//
// Pull API R3 contract (ADR-0003):
//   모든 조회 API는 sync return (TOptional<FRow> / UObject* / TArray<T>).
//   Ready 상태 확인 후에만 조회 호출 — checkf(IsReady()) AC-DP-13.
//
// AC-DP-13: Uninitialized/Loading 상태에서 pull API 호출 시 checkf assertion
//           (Debug/Development 빌드). CI 전략: Debug/Dev 빌드 런타임 검증,
//           Shipping은 소스 grep으로 checkf 존재 확인.
//
// GC 안전성 (R2 BLOCKER 6):
//   모든 UObject* 멤버는 UPROPERTY() 매크로 필수.
//   TObjectPtr<> 사용으로 UE 5.0+ Access Tracking 지원.
//
// Downstream 의존 선언 (R2 BLOCKER 5):
//   이 Subsystem에 의존하는 Subsystem은 Initialize에서 반드시
//   Collection.InitializeDependency<UDataPipelineSubsystem>() 호출.
//
// Story 1-7에서 Save/Load 연동 + OnLoadComplete 파라미터 실제화 예정.
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Data/GiftCardRow.h"
#include "Data/DreamDataAsset.h"
#include "Data/StillnessBudgetAsset.h"
#include "Data/MossFinalFormAsset.h"
#include "Data/MossFinalFormData.h"
#include "DataPipelineSubsystem.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// EDataPipelineState — 4-state machine (ADR-0003 §State Machine)
//
// 전이 규칙:
//   Uninitialized → Loading   (Initialize 진입 시)
//   Loading       → Ready     (4개 카탈로그 모두 정상 등록)
//   Loading       → DegradedFallback (카탈로그 등록 실패 — Step 1~4 중 하나라도 실패)
//
// DegradedFallback: pull API는 여전히 호출 가능하나, GetFailedCatalogs()로 실패 목록 확인 필수.
// Ready / DegradedFallback: Initialize() 반환 전 확정 (AC-DP-01).
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EDataPipelineState — Data Pipeline 로딩 상태 4-state machine.
 *
 * ADR-0003 §State Machine: Initialize() 호출 이후 Ready 또는 DegradedFallback으로 확정.
 * pull API 호출 전 IsReady() 또는 GetState() 확인 필수 (AC-DP-13).
 */
UENUM(BlueprintType)
enum class EDataPipelineState : uint8
{
    /** 초기 상태. Initialize() 호출 전. pull API 호출 불가 (AC-DP-13 checkf). */
    Uninitialized,

    /** 카탈로그 로딩 중. Initialize() 내부 동기 로드 진행 중. pull API 호출 불가 (AC-DP-13 checkf). */
    Loading,

    /** 4개 카탈로그 모두 정상 등록. pull API 호출 가능. */
    Ready,

    /**
     * 하나 이상의 카탈로그 등록 실패.
     * GetFailedCatalogs()로 실패 목록 확인 후 처리 필요.
     * Story 1-6에서 DegradedFallback 진입 조건 + 처리 로직 구현 예정.
     */
    DegradedFallback
};

// ─────────────────────────────────────────────────────────────────────────────
// FOnLoadComplete — Pipeline 로드 완료 Delegate (global scope — UE Multicast idiom)
//
// 발행 시점: Initialize() 완료 직후 (Ready 또는 DegradedFallback 진입 시).
// bFreshStart:      true = 이전 세션 데이터 없음 (최초 실행 또는 세이브 없음).
// bHadPreviousData: true = 이전 세션 세이브 파일 존재.
// Story 1-7 Save/Load 연동 후 실제 파라미터 값 결정.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Pipeline 로드 완료 시 발행하는 Multicast Delegate.
 *
 * @param bFreshStart      true = 최초 실행 / 세이브 없음.
 * @param bHadPreviousData true = 이전 세이브 파일 존재.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLoadComplete, bool /*bFreshStart*/, bool /*bHadPreviousData*/);

// ─────────────────────────────────────────────────────────────────────────────
// UDataPipelineSubsystem
// ─────────────────────────────────────────────────────────────────────────────

/**
 * UDataPipelineSubsystem — Moss Baby Data Pipeline 메인 Subsystem.
 *
 * UGameInstanceSubsystem 기반. GameInstance 생명주기에 귀속.
 * Initialize() 자동 호출 — UGameInstance::Init() 반환 전 확정 (AC-DP-01).
 *
 * 4개 카탈로그 동기 순차 로드 (ADR-0003 §구체 로드 순서):
 *   Step 1: Card DataTable (UDataTable)
 *   Step 2: FinalForm DataAsset bucket (UMossFinalFormAsset per-form)
 *   Step 3: Dream DataAsset bucket (UDreamDataAsset per-dream)
 *   Step 4: Stillness single DataAsset (UStillnessBudgetAsset)
 *
 * Pull API R3 contract: 모든 조회 sync return.
 * AC-DP-13: Ready 아닌 상태에서 호출 시 checkf assertion.
 *
 * Story 1-6에서 실제 카탈로그 등록 로직 구현.
 */
UCLASS()
class MADEBYCLAUDECODE_API UDataPipelineSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── Lifecycle (UGameInstanceSubsystem overrides) ──────────────────────────

    /**
     * 4-state machine 진입 (Uninitialized → Loading → Ready/DegradedFallback).
     * ADR-0003 §구체 로드 순서: Card → FinalForm → Dream → Stillness 순서 보장.
     * UGameInstance::Init() 반환 전 상태 확정 (AC-DP-01).
     * Story 1-6에서 RegisterCard/FinalForm/Dream/Stillness Catalog() 구현 예정.
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * 모든 레지스트리 클리어 + 상태 Uninitialized 복귀.
     * Super::Deinitialize() 마지막 호출 필수.
     */
    virtual void Deinitialize() override;

    // ── State Inspection API (sync) ───────────────────────────────────────────

    /** 현재 4-state machine 상태 반환. */
    EDataPipelineState GetState() const { return CurrentState; }

    /** Ready 상태 여부. pull API 호출 전 확인 권장. */
    bool IsReady() const { return CurrentState == EDataPipelineState::Ready; }

    // ── Pull API — R3 sync return contract (ADR-0003) ─────────────────────────
    //
    // 모든 pull API:
    //   - checkf(IsReady()) — AC-DP-13: Ready 아닌 상태에서 호출 시 assert
    //   - Fail-close: 없는 ID → 빈 TOptional / nullptr / 빈 TArray (default row 반환 금지)
    //   - NAME_None → 즉시 빈 반환 (checkf 통과 후)
    //   - Story 1-6에서 실제 레지스트리 연동 구현. 현재 뼈대 단계에서는 빈 반환.
    //
    // AC-DP-16: 알 수 없는 ID → UE_LOG Warning (rename 또는 migration 누락 의심)

    /**
     * CardId로 카드 row 조회.
     * @param CardId  DataTable row 이름. NAME_None → 빈 TOptional.
     * @return 카드 row. 없으면 빈 TOptional (default-constructed row 반환 금지).
     */
    TOptional<FGiftCardRow> GetCardRow(FName CardId) const;

    /**
     * 전체 카드 ID 목록 반환.
     * @return CardTable row 이름 목록. 빈 등록 시 빈 TArray.
     */
    TArray<FName> GetAllCardIds() const;

    /**
     * FormId로 최종 형태 read-only view 조회.
     * @param FormId  FormRegistry 키. NAME_None → 빈 TOptional.
     * @return FMossFinalFormData view. 없으면 빈 TOptional.
     */
    TOptional<FMossFinalFormData> GetGrowthFormRow(FName FormId) const;

    /**
     * 전체 최종 형태 ID 목록 반환.
     * @return FormRegistry 키 목록. 빈 등록 시 빈 TArray.
     */
    TArray<FName> GetAllGrowthFormIds() const;

    /**
     * DreamId로 꿈 자산 조회.
     * @param DreamId  DreamRegistry 키. NAME_None → nullptr.
     * @return UDreamDataAsset 포인터. 없으면 nullptr.
     */
    UDreamDataAsset* GetDreamAsset(FName DreamId) const;

    /**
     * 전체 꿈 자산 목록 반환.
     * @return DreamRegistry 값 목록. nullptr 항목 제외. 빈 등록 시 빈 TArray.
     */
    TArray<UDreamDataAsset*> GetAllDreamAssets() const;

    /**
     * DreamId로 꿈 본문 텍스트 조회.
     * @param DreamId  DreamRegistry 키. 없으면 FText::GetEmpty().
     * @return 꿈 BodyText. 없으면 empty FText.
     */
    FText GetDreamBodyText(FName DreamId) const;

    /**
     * Stillness Budget 자산 조회.
     * @return UStillnessBudgetAsset 포인터. 등록 없으면 nullptr.
     */
    UStillnessBudgetAsset* GetStillnessBudgetAsset() const;

    // ── DegradedFallback API (AC-DP-15) ──────────────────────────────────────

    /**
     * 등록 실패한 카탈로그 이름 목록 반환 (AC-DP-15).
     * DegradedFallback 상태에서 진단용.
     * @return 실패한 카탈로그 이름 배열.
     */
    TArray<FName> GetFailedCatalogs() const { return FailedCatalogs; }

    // ── PIE Hot-swap (AC-DP-07) ───────────────────────────────────────────────

    /**
     * 카탈로그 hot-swap (PIE 전용).
     * Cooked 빌드에서는 런타임 변경 없음.
     * AC-DP-14 재진입 차단: Loading 상태에서 재호출 시 즉시 return + Warning.
     * Story 1-7에서 실제 hot-swap 로직 구현 예정. 현재 뼈대는 no-op.
     */
    void RefreshCatalog();

    // ── Load Complete Delegate ────────────────────────────────────────────────

    /**
     * Pipeline 로드 완료 시 발행. Ready 또는 DegradedFallback 진입 시 1회 Broadcast.
     * Story 1-7 Save/Load 연동 후 bFreshStart / bHadPreviousData 실제화.
     */
    FOnLoadComplete OnLoadComplete;

    // ── Story 1-19: Memory footprint 통계 (AC-DP-08) ──────────────────────────

    /**
     * CPU-side 메모리 발자국 추정 문자열 반환 (AC-DP-08).
     * 실제 동적 할당 측정이 아닌 항목 수 × 고정 바이트 근사치.
     * 포맷: "Pipeline CPU-side: N bytes (Card=A, Dream=B, Form=C)"
     */
    FString GetCatalogStats() const;

    // ── 테스트 전용 훅 ────────────────────────────────────────────────────────

#if WITH_AUTOMATION_TESTS
    /**
     * [테스트 전용] 상태 강제 설정.
     * checkf(IsReady()) 우회 테스트에서 사용. 프로덕션 코드에서 절대 호출 금지.
     * @param NewState  강제 설정할 상태.
     */
    void TestingSetState(EDataPipelineState NewState) { CurrentState = NewState; }

    /**
     * [테스트 전용] CardTable 직접 주입.
     * RegisterCardCatalog() 경로 우회 — 단위 테스트용. 프로덕션 코드에서 절대 호출 금지.
     * @param InTable  주입할 UDataTable. nullptr 허용.
     */
    void TestingSetCardTable(UDataTable* InTable) { CardTable = InTable; }

    /**
     * [테스트 전용] StillnessAsset 직접 주입.
     * RegisterStillnessCatalog() 경로 우회 — 단위 테스트용. 프로덕션 코드에서 절대 호출 금지.
     * @param InAsset  주입할 UStillnessBudgetAsset. nullptr 허용.
     */
    void TestingSetStillnessAsset(UStillnessBudgetAsset* InAsset) { StillnessAsset = InAsset; }

    /**
     * [테스트 전용] DreamRegistry에 자산 추가.
     * RegisterDreamCatalog() 경로 우회 — 단위 테스트용. 프로덕션 코드에서 절대 호출 금지.
     * @param Id     레지스트리 키.
     * @param Asset  추가할 UDreamDataAsset. nullptr 시 무시.
     */
    void TestingAddDreamAsset(FName Id, UDreamDataAsset* Asset) { if (Asset) DreamRegistry.Add(Id, Asset); }

    /**
     * [테스트 전용] FormRegistry에 자산 추가.
     * RegisterFinalFormCatalog() 경로 우회 — 단위 테스트용. 프로덕션 코드에서 절대 호출 금지.
     * @param Id     레지스트리 키.
     * @param Asset  추가할 UMossFinalFormAsset. nullptr 시 무시.
     */
    void TestingAddFormAsset(FName Id, UMossFinalFormAsset* Asset) { if (Asset) FormRegistry.Add(Id, Asset); }

    /**
     * [테스트 전용] 모든 레지스트리 + FailedCatalogs 클리어.
     * 테스트 정리 단계에서 사용. 프로덕션 코드에서 절대 호출 금지.
     */
    void TestingClearAll()
    {
        CardTable = nullptr;
        StillnessAsset = nullptr;
        DreamRegistry.Empty();
        FormRegistry.Empty();
        FailedCatalogs.Empty();
    }

    // ── Story 1-19: Budget flag 관찰 훅 ─────────────────────────────────────

    /** [테스트 전용] CheckCatalogSize 직접 호출. */
    void TestingCheckCatalogSize(const FString& CatalogName, int32 Count);

    /** [테스트 전용] EvaluateTInitBudget 직접 호출. */
    void TestingEvaluateTInitBudget(double ElapsedMs);

    /** [테스트 전용] Fatal threshold 플래그 반환. */
    bool TestingWasFatalTriggered() const { return bInitFatalTriggered; }

    /** [테스트 전용] Error threshold 플래그 반환. */
    bool TestingWasErrorTriggered() const { return bInitErrorTriggered; }

    /** [테스트 전용] Warning threshold 플래그 반환. */
    bool TestingWasWarningTriggered() const { return bInitWarningTriggered; }

    /** [테스트 전용] Budget 플래그 전체 리셋. */
    void TestingResetBudgetFlags()
    {
        bInitFatalTriggered = false;
        bInitErrorTriggered = false;
        bInitWarningTriggered = false;
    }
#endif

private:
    // ── Story 1-19: T_init 3-단계 budget 평가 (AC-DP-09) ─────────────────────

    /**
     * T_init 3-단계 overflow 분기 처리 (AC-DP-09).
     * Warn: ElapsedMs > Budget × WarnMultiplier
     * Error: ElapsedMs > Budget × ErrorMultiplier
     * Fatal threshold: Error 로그 + [FATAL_THRESHOLD] 접두어 + bInitFatalTriggered = true
     *   (UE_LOG Fatal은 process 종료 유발 → test harness crash 방지를 위해 Error 로그 대체.
     *    Story 1-20에서 checkf 강제 강화 가능.)
     */
    void EvaluateTInitBudget(double ElapsedMs, const class UMossDataPipelineSettings* Settings);

    /**
     * 카탈로그 등록 수 3-단계 overflow 체크 (AC-DP-10).
     * CatalogName: "Card" / "Dream" / "FinalForm" → 대응 MaxCatalogSize* 선택.
     * 같은 Fatal 처리 방침: Error 로그 + [FATAL_THRESHOLD] 접두어 + bInitFatalTriggered.
     */
    void CheckCatalogSize(const FString& CatalogName, int32 Count,
                          const class UMossDataPipelineSettings* Settings);

    // ── Initialize 단계별 등록 helpers (ADR-0003 §구체 로드 순서) ──────────────

    /**
     * Step 1: Card DataTable 등록.
     * @return true = 성공 또는 empty-OK. false = 치명적 실패 → DegradedFallback.
     */
    bool RegisterCardCatalog();

    /**
     * Step 2: FinalForm DataAsset bucket 등록 (UAssetManager PrimaryAsset type-bulk).
     * @return true = 성공 또는 empty-OK. false = 치명적 실패 → DegradedFallback.
     */
    bool RegisterFinalFormCatalog();

    /**
     * Step 3: Dream DataAsset bucket 등록 (UAssetManager PrimaryAsset type-bulk).
     * @return true = 성공 또는 empty-OK. false = 치명적 실패 → DegradedFallback.
     */
    bool RegisterDreamCatalog();

    /**
     * Step 4: Stillness single DataAsset 등록.
     * @return true = 성공 또는 empty-OK. false = cast 실패 → DegradedFallback.
     */
    bool RegisterStillnessCatalog();

    /**
     * DegradedFallback 진입 공통 처리 (AC-DP-03).
     * CurrentState = DegradedFallback + FailedCatalogs 추가 + Error 로그 + Broadcast.
     * @param FailedCatalog  실패한 카탈로그 이름 (FailedCatalogs 배열에 추가).
     * @param Reason         Error 로그에 출력할 원인 설명.
     */
    void EnterDegradedFallback(const FName& FailedCatalog, const FString& Reason);

    // ── State ──────────────────────────────────────────────────────────────────

    /** 현재 4-state machine 상태. 초기값 Uninitialized. */
    EDataPipelineState CurrentState = EDataPipelineState::Uninitialized;

    // ── Registries (UPROPERTY — R2 BLOCKER 6 GC 안전성) ──────────────────────
    //
    // 모든 UObject* 멤버는 UPROPERTY() 매크로로 GC에 등록.
    // TObjectPtr<>: UE 5.0+ Access Tracking 지원 (raw pointer 대체).

    /** 꿈 자산 레지스트리. DreamId → UDreamDataAsset. Story 1-6 등록 예정. */
    UPROPERTY()
    TMap<FName, TObjectPtr<UDreamDataAsset>> DreamRegistry;

    /** 최종 형태 자산 레지스트리. FormId → UMossFinalFormAsset. Story 1-6 등록 예정. */
    UPROPERTY()
    TMap<FName, TObjectPtr<UMossFinalFormAsset>> FormRegistry;

    /** 카드 DataTable. Story 1-6 로드 예정. */
    UPROPERTY()
    TObjectPtr<UDataTable> CardTable;

    /** Stillness Budget 단일 자산. Story 1-6 로드 예정. */
    UPROPERTY()
    TObjectPtr<UStillnessBudgetAsset> StillnessAsset;

    // ── Internal State ────────────────────────────────────────────────────────

    /** 등록 실패 카탈로그 이름 목록 (AC-DP-15). DegradedFallback 진입 시 추가. */
    TArray<FName> FailedCatalogs;

    /** RefreshCatalog 재진입 차단 플래그 (AC-DP-14). */
    bool bRefreshInProgress = false;

    // ── Story 1-19: Budget overflow 관찰 플래그 (AC-DP-09/10) ────────────────
    //
    // UE_LOG Fatal 대신 Error 로그 + flag 조합으로 test harness crash 방지.
    // 각 Initialize() 호출 시 EvaluateTInitBudget / CheckCatalogSize 에서 set.
    // 테스트 사이 TestingResetBudgetFlags() 호출로 초기화.

    /** Fatal threshold 초과 플래그 (T_init 또는 CatalogSize). */
    bool bInitFatalTriggered = false;

    /** Error threshold 초과 플래그 (T_init 또는 CatalogSize). */
    bool bInitErrorTriggered = false;

    /** Warning threshold 초과 플래그 (T_init 또는 CatalogSize). */
    bool bInitWarningTriggered = false;
};
