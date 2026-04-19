// Copyright Moss Baby
//
// DataPipelineSubsystem.cpp — Data Pipeline 메인 서브시스템 구현
//
// Story-1-5: UDataPipelineSubsystem 뼈대 + 4-state machine + pull API 스텁
// Story-1-6: Initialize 4단계 실제 구현 + RegisterXxx helpers + DegradedFallback
// Story-1-19: T_init budget 3-단계 overflow + Catalog size 3-단계 체크 + GetCatalogStats
// ADR-0003: Sync 일괄 로드 채택 (Card → FinalForm → Dream → Stillness 순서 고정)
// ADR-0002: 컨테이너 선택
//
// 구현 단계:
//   - Initialize(): 4단계 RegisterXxx() 호출 → Ready / DegradedFallback 전이
//   - RegisterCardCatalog(): DataTable RowStruct 검증 (Story 1-15 ini 연동 전 skeleton)
//   - RegisterFinalFormCatalog(): UAssetManager PrimaryAsset type-bulk load
//   - RegisterDreamCatalog(): UAssetManager PrimaryAsset type-bulk load
//   - RegisterStillnessCatalog(): single DataAsset load
//   - EnterDegradedFallback(): 공통 DegradedFallback 진입 처리
//   - pull API: checkf 가드 + Fail-close 빈 반환 (NAME_None, 없는 ID)
//   - RefreshCatalog(): no-op (Story 1-7에서 hot-swap 로직 구현)
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#include "Data/DataPipelineSubsystem.h"
#include "Engine/DataTable.h"
#include "Engine/AssetManager.h"
#include "Settings/MossDataPipelineSettings.h"

// ─────────────────────────────────────────────────────────────────────────────
// Log Category
// ─────────────────────────────────────────────────────────────────────────────

DEFINE_LOG_CATEGORY_STATIC(LogDataPipeline, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UDataPipelineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 4-state machine: Uninitialized → Loading
    CurrentState = EDataPipelineState::Loading;
    FailedCatalogs.Empty();

    const double StartTime = FPlatformTime::Seconds();

    // Story 1-19: Settings 로컬 캐시 (null guard 포함)
    const UMossDataPipelineSettings* Settings = UMossDataPipelineSettings::Get();

    // ── Step 1: Card DataTable (ADR-0003 §구체 로드 순서 R2 deterministic) ────
    UE_LOG(LogDataPipeline, Log, TEXT("Registering Card catalog..."));
    if (!RegisterCardCatalog())
    {
        EnterDegradedFallback(TEXT("Card"), TEXT("Card DataTable registration failed"));
        return; // AC-DP-03: 이후 단계 시도하지 않음 (즉시 return)
    }
    // Story 1-19: Card 등록 수 3-단계 overflow 체크 (AC-DP-10)
    if (Settings && CardTable)
    {
        CheckCatalogSize(TEXT("Card"), CardTable->GetRowMap().Num(), Settings);
    }

    // ── Step 2: FinalForm DataAsset bucket ────────────────────────────────────
    UE_LOG(LogDataPipeline, Log, TEXT("Registering FinalForm catalog..."));
    if (!RegisterFinalFormCatalog())
    {
        EnterDegradedFallback(TEXT("FinalForm"), TEXT("FinalForm catalog registration failed"));
        return; // AC-DP-03: 즉시 return
    }
    // Story 1-19: FinalForm 등록 수 3-단계 overflow 체크 (AC-DP-10)
    if (Settings)
    {
        CheckCatalogSize(TEXT("FinalForm"), FormRegistry.Num(), Settings);
    }

    // ── Step 3: Dream DataAsset bucket ───────────────────────────────────────
    UE_LOG(LogDataPipeline, Log, TEXT("Registering Dream catalog..."));
    if (!RegisterDreamCatalog())
    {
        EnterDegradedFallback(TEXT("Dream"), TEXT("Dream catalog registration failed"));
        return; // AC-DP-03: 즉시 return
    }
    // Story 1-19: Dream 등록 수 3-단계 overflow 체크 (AC-DP-10)
    if (Settings)
    {
        CheckCatalogSize(TEXT("Dream"), DreamRegistry.Num(), Settings);
    }

    // ── Step 4: Stillness single DataAsset ───────────────────────────────────
    UE_LOG(LogDataPipeline, Log, TEXT("Registering Stillness catalog..."));
    if (!RegisterStillnessCatalog())
    {
        EnterDegradedFallback(TEXT("Stillness"), TEXT("Stillness catalog registration failed"));
        return; // AC-DP-03: 즉시 return
    }

    // ── Story 1-19: T_init 3-단계 budget 평가 (AC-DP-09) ──────────────────────
    // ADR-0001 비위반: 시간 로직 아닌 순수 로그/진단 목적
    const double ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0;
    if (Settings)
    {
        EvaluateTInitBudget(ElapsedMs, Settings);
    }
    else
    {
        UE_LOG(LogDataPipeline, Log, TEXT("Pipeline Ready — T_init = %.2f ms (Settings null)"), ElapsedMs);
    }

    CurrentState = EDataPipelineState::Ready;

    // OnLoadComplete 발행 (Ready 진입)
    // bFreshStart / bHadPreviousData: Story 1-7 Save/Load 연동 후 실제화.
    OnLoadComplete.Broadcast(/*bFreshStart*/ true, /*bHadPreviousData*/ false);
}

// ─────────────────────────────────────────────────────────────────────────────
// DegradedFallback 공통 처리 (AC-DP-03)
// ─────────────────────────────────────────────────────────────────────────────

void UDataPipelineSubsystem::EnterDegradedFallback(const FName& FailedCatalog, const FString& Reason)
{
    CurrentState = EDataPipelineState::DegradedFallback;
    FailedCatalogs.Add(FailedCatalog);
    UE_LOG(LogDataPipeline, Error, TEXT("%s — %s"), *Reason, *FailedCatalog.ToString());

    // Broadcast — DegradedFallback 진입 시에도 발행 (downstream이 상태 인지 가능)
    OnLoadComplete.Broadcast(/*bFreshStart*/ true, /*bHadPreviousData*/ false);
}

// ─────────────────────────────────────────────────────────────────────────────
// RegisterCardCatalog — Step 1
// ─────────────────────────────────────────────────────────────────────────────

bool UDataPipelineSubsystem::RegisterCardCatalog()
{
    // Card DataTable 경로는 DefaultGame.ini / Build configuration에서 지정 예정 (Story 1-15).
    // 뼈대 단계: CardTable 미주입 시 Warning만 출력 후 return true (empty OK).
    if (!CardTable)
    {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("Card DataTable not assigned — Story 1-15 ini 설정 후 자동 주입 예정"));
        return true; // empty OK — CardTable 없어도 Ready 허용
    }

    // DataTable RowStruct 타입 검증 (C1 schema gate — ADR-0002)
    if (CardTable->RowStruct != FGiftCardRow::StaticStruct())
    {
        UE_LOG(LogDataPipeline, Error,
            TEXT("Card DataTable RowStruct mismatch — expected FGiftCardRow, got '%s'"),
            CardTable->RowStruct ? *CardTable->RowStruct->GetName() : TEXT("null"));
        return false; // 치명적 실패 → DegradedFallback
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RegisterFinalFormCatalog — Step 2
// ─────────────────────────────────────────────────────────────────────────────

bool UDataPipelineSubsystem::RegisterFinalFormCatalog()
{
    UAssetManager& AssetMgr = UAssetManager::Get();

    TArray<FPrimaryAssetId> FormIds;
    AssetMgr.GetPrimaryAssetIdList(FPrimaryAssetType("FinalForm"), FormIds);

    if (FormIds.Num() == 0)
    {
        // DefaultEngine.ini PrimaryAssetTypesToScan 미등록 — Story 1-15 대기
        UE_LOG(LogDataPipeline, Warning,
            TEXT("FinalForm PrimaryAssetType empty — Story 1-15 ini 등록 대기"));
        return true; // empty OK
    }

    // sync 로드 (ADR-0003: UAssetManager::LoadPrimaryAssets sync variant)
    TArray<FName> Bundles;
    AssetMgr.LoadPrimaryAssets(FormIds, Bundles);

    for (const FPrimaryAssetId& Id : FormIds)
    {
        UMossFinalFormAsset* Asset = Cast<UMossFinalFormAsset>(AssetMgr.GetPrimaryAssetObject(Id));
        if (!Asset) { continue; }
        FormRegistry.Add(Asset->FormId, Asset);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RegisterDreamCatalog — Step 3
// ─────────────────────────────────────────────────────────────────────────────

bool UDataPipelineSubsystem::RegisterDreamCatalog()
{
    UAssetManager& AssetMgr = UAssetManager::Get();

    TArray<FPrimaryAssetId> DreamIds;
    AssetMgr.GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"), DreamIds);

    if (DreamIds.Num() == 0)
    {
        // DefaultEngine.ini PrimaryAssetTypesToScan 미등록 — Story 1-15 대기.
        // 빈 카탈로그는 실패가 아닌 정상 (사용자가 꿈 미정의 상태 허용).
        UE_LOG(LogDataPipeline, Warning,
            TEXT("DreamData PrimaryAssetType empty — DefaultEngine.ini PrimaryAssetTypesToScan 미등록 (Story 1-15)"));
        return true; // empty OK
    }

    // sync 로드 (ADR-0003: UAssetManager::LoadPrimaryAssets sync variant)
    TArray<FName> Bundles;
    AssetMgr.LoadPrimaryAssets(DreamIds, Bundles);

    for (const FPrimaryAssetId& Id : DreamIds)
    {
        UDreamDataAsset* Asset = Cast<UDreamDataAsset>(AssetMgr.GetPrimaryAssetObject(Id));
        if (!Asset) { continue; }
        DreamRegistry.Add(Asset->DreamId, Asset);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// RegisterStillnessCatalog — Step 4
// ─────────────────────────────────────────────────────────────────────────────

bool UDataPipelineSubsystem::RegisterStillnessCatalog()
{
    UAssetManager& AssetMgr = UAssetManager::Get();

    TArray<FPrimaryAssetId> BudgetIds;
    AssetMgr.GetPrimaryAssetIdList(FPrimaryAssetType("StillnessBudget"), BudgetIds);

    if (BudgetIds.Num() == 0)
    {
        // StillnessBudget 자산 없음 — consumer가 default 값 사용
        UE_LOG(LogDataPipeline, Warning,
            TEXT("StillnessBudget asset not found — default values will be used by consumers"));
        return true; // empty OK — StillnessAsset == nullptr은 consumer가 처리
    }

    // ADR-0002: single instance 정책 — 첫 번째만 사용
    TArray<FName> Bundles;
    AssetMgr.LoadPrimaryAssets({BudgetIds[0]}, Bundles);

    StillnessAsset = Cast<UStillnessBudgetAsset>(AssetMgr.GetPrimaryAssetObject(BudgetIds[0]));

    if (!StillnessAsset)
    {
        UE_LOG(LogDataPipeline, Error,
            TEXT("StillnessBudget asset load failed — cast mismatch (expected UStillnessBudgetAsset)"));
        return false; // 치명적 실패 → DegradedFallback
    }

    return true;
}

void UDataPipelineSubsystem::Deinitialize()
{
    // 모든 레지스트리 클리어 → GC에 참조 해제
    DreamRegistry.Empty();
    FormRegistry.Empty();
    CardTable = nullptr;
    StillnessAsset = nullptr;
    FailedCatalogs.Empty();

    // 상태 Uninitialized 복귀
    CurrentState = EDataPipelineState::Uninitialized;
    UE_LOG(LogDataPipeline, Log, TEXT("DataPipelineSubsystem: Deinitialize 완료"));

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pull API — Card
// ─────────────────────────────────────────────────────────────────────────────

TOptional<FGiftCardRow> UDataPipelineSubsystem::GetCardRow(FName CardId) const
{
    // AC-DP-13: Ready 아닌 상태 조회 차단 (Debug/Development checkf)
    checkf(IsReady(),
        TEXT("UDataPipelineSubsystem::GetCardRow — Ready 아닌 상태에서 호출 (AC-DP-13 위반). "
             "IsReady() 확인 후 호출할 것."));

    // Fail-close: NAME_None → 즉시 빈 반환
    if (CardId.IsNone()) { return {}; }

    // CardTable 미등록 (뼈대 단계) → 빈 반환
    if (!CardTable) { return {}; }

    const FGiftCardRow* Row = CardTable->FindRow<FGiftCardRow>(CardId, TEXT("GetCardRow"));
    if (!Row)
    {
        // AC-DP-16: 알 수 없는 ID → Warning (rename 또는 migration 누락 의심)
        UE_LOG(LogDataPipeline, Warning,
            TEXT("GetCardRow: 알 수 없는 CardId '%s' — rename 또는 migration 누락 의심 (AC-DP-16)"),
            *CardId.ToString());
        return {};
    }
    return *Row;
}

TArray<FName> UDataPipelineSubsystem::GetAllCardIds() const
{
    // AC-DP-13: Ready 아닌 상태 조회 차단
    checkf(IsReady(),
        TEXT("UDataPipelineSubsystem::GetAllCardIds — Ready 아닌 상태에서 호출 (AC-DP-13 위반)."));

    if (!CardTable) { return {}; }

    TArray<FName> Ids;
    CardTable->GetRowMap().GetKeys(Ids);
    return Ids;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pull API — FinalForm
// ─────────────────────────────────────────────────────────────────────────────

TOptional<FMossFinalFormData> UDataPipelineSubsystem::GetGrowthFormRow(FName FormId) const
{
    // AC-DP-13
    checkf(IsReady(),
        TEXT("UDataPipelineSubsystem::GetGrowthFormRow — Ready 아닌 상태에서 호출 (AC-DP-13 위반)."));

    if (FormId.IsNone()) { return {}; }

    const TObjectPtr<UMossFinalFormAsset>* Found = FormRegistry.Find(FormId);
    if (!Found || !(*Found))
    {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("GetGrowthFormRow: 알 수 없는 FormId '%s' — rename 또는 migration 누락 의심 (AC-DP-16)"),
            *FormId.ToString());
        return {};
    }

    // ADR-0010: UObject 직접 노출 회피 — FMossFinalFormData read-only view로 변환
    // Story 1-6에서 FromAsset() 변환 완성 검증 예정
    return FMossFinalFormData::FromAsset(Found->Get());
}

TArray<FName> UDataPipelineSubsystem::GetAllGrowthFormIds() const
{
    // AC-DP-13
    checkf(IsReady(),
        TEXT("UDataPipelineSubsystem::GetAllGrowthFormIds — Ready 아닌 상태에서 호출 (AC-DP-13 위반)."));

    TArray<FName> Ids;
    FormRegistry.GetKeys(Ids);
    return Ids;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pull API — Dream
// ─────────────────────────────────────────────────────────────────────────────

UDreamDataAsset* UDataPipelineSubsystem::GetDreamAsset(FName DreamId) const
{
    // AC-DP-13
    checkf(IsReady(),
        TEXT("UDataPipelineSubsystem::GetDreamAsset — Ready 아닌 상태에서 호출 (AC-DP-13 위반)."));

    if (DreamId.IsNone()) { return nullptr; }

    const TObjectPtr<UDreamDataAsset>* Found = DreamRegistry.Find(DreamId);
    if (!Found || !(*Found))
    {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("GetDreamAsset: 알 수 없는 DreamId '%s' — rename 또는 migration 누락 의심 (AC-DP-16)"),
            *DreamId.ToString());
        return nullptr;
    }
    return Found->Get();
}

TArray<UDreamDataAsset*> UDataPipelineSubsystem::GetAllDreamAssets() const
{
    // AC-DP-13
    checkf(IsReady(),
        TEXT("UDataPipelineSubsystem::GetAllDreamAssets — Ready 아닌 상태에서 호출 (AC-DP-13 위반)."));

    TArray<UDreamDataAsset*> Assets;
    Assets.Reserve(DreamRegistry.Num());
    for (const auto& Pair : DreamRegistry)
    {
        if (Pair.Value)
        {
            Assets.Add(Pair.Value.Get());
        }
    }
    return Assets;
}

FText UDataPipelineSubsystem::GetDreamBodyText(FName DreamId) const
{
    // AC-DP-13: checkf는 GetDreamAsset 내부에서도 발동하지 않도록 이 메서드에서 직접 가드
    checkf(IsReady(),
        TEXT("UDataPipelineSubsystem::GetDreamBodyText — Ready 아닌 상태에서 호출 (AC-DP-13 위반)."));

    // GetDreamAsset 재사용 (중복 checkf 방지 — 이미 위에서 통과)
    // 주의: GetDreamAsset도 checkf를 포함하므로 Ready 확정 후 호출 안전
    UDreamDataAsset* Asset = GetDreamAsset(DreamId);
    return Asset ? Asset->BodyText : FText::GetEmpty();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pull API — Stillness
// ─────────────────────────────────────────────────────────────────────────────

UStillnessBudgetAsset* UDataPipelineSubsystem::GetStillnessBudgetAsset() const
{
    // AC-DP-13
    checkf(IsReady(),
        TEXT("UDataPipelineSubsystem::GetStillnessBudgetAsset — Ready 아닌 상태에서 호출 (AC-DP-13 위반)."));

    return StillnessAsset.Get();
}

// ─────────────────────────────────────────────────────────────────────────────
// PIE Hot-swap
// ─────────────────────────────────────────────────────────────────────────────

void UDataPipelineSubsystem::RefreshCatalog()
{
    // AC-DP-14: 재진입 차단
    if (bRefreshInProgress)
    {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("RefreshCatalog: 이미 refresh 진행 중 — 재진입 차단 (AC-DP-14 위반 시도)"));
        return;
    }
    bRefreshInProgress = true;

    UE_LOG(LogDataPipeline, Log, TEXT("RefreshCatalog: PIE hot-swap 진입 (뼈대 — no-op, Story 1-7 구현 예정)"));

    // Story 1-7에서 실제 hot-swap 로직 구현:
    //   1. CurrentState = Loading 재진입
    //   2. 레지스트리 클리어
    //   3. 카탈로그 재등록 (RegisterCard/FinalForm/Dream/StillnessCatalog)
    //   4. Ready / DegradedFallback 전이
    //   5. OnLoadComplete.Broadcast(...)
    // Cooked 빌드에서는 이 메서드 호출 자체를 막는 로직 추가 예정

    bRefreshInProgress = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Story 1-19: EvaluateTInitBudget — T_init 3-단계 overflow 평가 (AC-DP-09)
// ─────────────────────────────────────────────────────────────────────────────
//
// Fatal threshold 처리 방침:
//   UE_LOG Fatal은 process 종료 유발 → test harness crash 방지를 위해
//   Error 로그 + [FATAL_THRESHOLD] 접두어 + bInitFatalTriggered = true 대체.
//   Story 1-20에서 checkf 강제 강화 가능 (TD-013).
//
// ADR-0001 비위반: 시간 측정은 로그/진단 전용, 게임 로직 분기 없음.

void UDataPipelineSubsystem::EvaluateTInitBudget(double ElapsedMs,
                                                   const UMossDataPipelineSettings* Settings)
{
    if (!Settings) { return; }

    const float Budget = Settings->MaxInitTimeBudgetMs;

    if (ElapsedMs > Budget * Settings->CatalogOverflowFatalMultiplier)
    {
        UE_LOG(LogDataPipeline, Error,
            TEXT("[FATAL_THRESHOLD] T_init %.2f ms > %.2f ms * %.2f — Pipeline exceeds fatal budget (MaxInitTimeBudgetMs)"),
            ElapsedMs, Budget, Settings->CatalogOverflowFatalMultiplier);
        bInitFatalTriggered = true;
    }
    else if (ElapsedMs > Budget * Settings->CatalogOverflowErrorMultiplier)
    {
        UE_LOG(LogDataPipeline, Error,
            TEXT("T_init %.2f ms > %.2f ms * %.2f (Error) — Async Bundle 전환 검토 flag (MaxInitTimeBudgetMs)"),
            ElapsedMs, Budget, Settings->CatalogOverflowErrorMultiplier);
        bInitErrorTriggered = true;
    }
    else if (ElapsedMs > Budget * Settings->CatalogOverflowWarnMultiplier)
    {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("T_init %.2f ms > %.2f ms * %.2f (Warning) MaxInitTimeBudgetMs=%.2f"),
            ElapsedMs, Budget, Settings->CatalogOverflowWarnMultiplier, Budget);
        bInitWarningTriggered = true;
    }
    else
    {
        UE_LOG(LogDataPipeline, Log,
            TEXT("T_init %.2f ms (Normal, budget %.2f)"), ElapsedMs, Budget);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Story 1-19: CheckCatalogSize — Catalog 등록 수 3-단계 overflow 체크 (AC-DP-10)
// ─────────────────────────────────────────────────────────────────────────────

void UDataPipelineSubsystem::CheckCatalogSize(const FString& CatalogName, int32 Count,
                                               const UMossDataPipelineSettings* Settings)
{
    if (!Settings) { return; }

    // CatalogName에 따라 대응 MaxCatalogSize* 선택
    int32 Budget = 200; // 알 수 없는 카탈로그 이름에 대한 기본값
    if (CatalogName == TEXT("Card"))          { Budget = Settings->MaxCatalogSizeCards; }
    else if (CatalogName == TEXT("Dream"))    { Budget = Settings->MaxCatalogSizeDreams; }
    else if (CatalogName == TEXT("FinalForm")){ Budget = Settings->MaxCatalogSizeForms; }

    const float WarnThresh  = Budget * Settings->CatalogOverflowWarnMultiplier;
    const float ErrorThresh = Budget * Settings->CatalogOverflowErrorMultiplier;
    const float FatalThresh = Budget * Settings->CatalogOverflowFatalMultiplier;

    if (Count >= FatalThresh)
    {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("%s count %d >= Warn threshold %.0f"), *CatalogName, Count, WarnThresh);
        UE_LOG(LogDataPipeline, Error,
            TEXT("%s count %d >= Error threshold %.0f"), *CatalogName, Count, ErrorThresh);
        UE_LOG(LogDataPipeline, Error,
            TEXT("[FATAL_THRESHOLD] %s count %d >= Fatal threshold %.0f"), *CatalogName, Count, FatalThresh);
        bInitFatalTriggered = true;
    }
    else if (Count >= ErrorThresh)
    {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("%s count %d >= Warn threshold %.0f"), *CatalogName, Count, WarnThresh);
        UE_LOG(LogDataPipeline, Error,
            TEXT("%s count %d >= Error threshold %.0f"), *CatalogName, Count, ErrorThresh);
        bInitErrorTriggered = true;
    }
    else if (Count >= WarnThresh)
    {
        UE_LOG(LogDataPipeline, Warning,
            TEXT("%s count %d >= Warn threshold %.0f"), *CatalogName, Count, WarnThresh);
        bInitWarningTriggered = true;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Story 1-19: GetCatalogStats — CPU-side 메모리 발자국 근사치 (AC-DP-08)
// ─────────────────────────────────────────────────────────────────────────────
//
// 실제 동적 할당 측정이 아닌 항목 수 × 고정 바이트 근사치.
//   Card: 160 bytes/row (FGiftCardRow 필드 추정 — FName×2, TArray<FName>, FText×2, FSoftObjectPath)
//   Dream: 1280 bytes/asset (UDreamDataAsset — FText BodyText + metadata)
//   Form: 320 bytes/asset (UMossFinalFormAsset — FName FormId + metadata)

FString UDataPipelineSubsystem::GetCatalogStats() const
{
    constexpr int64 CardBytesPerRow    = 160;
    constexpr int64 DreamBytesPerAsset = 1280;
    constexpr int64 FormBytesPerAsset  = 320;

    const int64 CardBytes  = CardTable ? int64(CardTable->GetRowMap().Num()) * CardBytesPerRow : 0;
    const int64 DreamBytes = int64(DreamRegistry.Num()) * DreamBytesPerAsset;
    const int64 FormBytes  = int64(FormRegistry.Num()) * FormBytesPerAsset;
    const int64 Total      = CardBytes + DreamBytes + FormBytes;

    return FString::Printf(
        TEXT("Pipeline CPU-side: %lld bytes (Card=%lld, Dream=%lld, Form=%lld)"),
        Total, CardBytes, DreamBytes, FormBytes);
}

// ─────────────────────────────────────────────────────────────────────────────
// Story 1-19: 테스트 전용 래퍼 (WITH_AUTOMATION_TESTS)
// ─────────────────────────────────────────────────────────────────────────────

#if WITH_AUTOMATION_TESTS

void UDataPipelineSubsystem::TestingCheckCatalogSize(const FString& CatalogName, int32 Count)
{
    const UMossDataPipelineSettings* S = UMossDataPipelineSettings::Get();
    if (S) { CheckCatalogSize(CatalogName, Count, S); }
}

void UDataPipelineSubsystem::TestingEvaluateTInitBudget(double ElapsedMs)
{
    const UMossDataPipelineSettings* S = UMossDataPipelineSettings::Get();
    if (S) { EvaluateTInitBudget(ElapsedMs, S); }
}

#endif // WITH_AUTOMATION_TESTS
