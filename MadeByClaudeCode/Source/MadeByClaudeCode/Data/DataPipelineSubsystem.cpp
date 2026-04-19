// Copyright Moss Baby
//
// DataPipelineSubsystem.cpp — Data Pipeline 메인 서브시스템 구현
//
// Story-1-5: UDataPipelineSubsystem 뼈대 + 4-state machine + pull API 스텁
// ADR-0003: Sync 일괄 로드 채택
// ADR-0002: 컨테이너 선택
//
// 현재 뼈대 단계:
//   - Initialize(): Uninitialized → Loading → Ready 전이 (빈 레지스트리)
//   - 실제 카탈로그 등록 로직은 Story 1-6에서 구현 (RegisterCard/FinalForm/Dream/StillnessCatalog)
//   - pull API: checkf 가드 + Fail-close 빈 반환 (NAME_None, 없는 ID)
//   - RefreshCatalog(): no-op (Story 1-7에서 hot-swap 로직 구현)
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#include "Data/DataPipelineSubsystem.h"
#include "Engine/DataTable.h"

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
    UE_LOG(LogDataPipeline, Log, TEXT("DataPipelineSubsystem: Loading 상태 진입"));

    // ── Story 1-6에서 구현 예정 ────────────────────────────────────────────────
    //
    // ADR-0003 §구체 로드 순서 (R2 deterministic):
    //   Step 1: Card DataTable  → RegisterCardCatalog()
    //   Step 2: FinalForm bucket → RegisterFinalFormCatalog()
    //   Step 3: Dream bucket    → RegisterDreamCatalog()
    //   Step 4: Stillness       → RegisterStillnessCatalog()
    //
    // 각 Step 실패 시 → DegradedFallback 전이 + FailedCatalogs 추가 + 즉시 return.
    //
    // 3단계 T_init 임계 로그 (ADR-0003 §T_init 성능 예산):
    //   Normal   ≤ 50ms     : 정보 로그
    //   Warning  ≤ 52.5ms   : UE_LOG Warning
    //   Error    ≤ 75ms     : UE_LOG Error + Async Bundle 전환 검토 flag
    //   Fatal    > 100ms    : UE_LOG Fatal (Shipping 포함)
    //
    // 현재 뼈대: empty registries 상태로 Ready 전이 (AC-DP-01 skeleton 충족).
    // ──────────────────────────────────────────────────────────────────────────

    // Loading → Ready 전이 (뼈대 단계 — 빈 레지스트리)
    CurrentState = EDataPipelineState::Ready;
    UE_LOG(LogDataPipeline, Log,
        TEXT("DataPipelineSubsystem: Ready 상태 확정 (뼈대 — 실제 카탈로그 Story 1-6 구현 예정)"));

    // OnLoadComplete 발행
    // bFreshStart: Story 1-7 Save/Load 연동 후 실제 값 결정.
    // bHadPreviousData: Story 1-7 Save/Load 연동 후 실제 값 결정.
    OnLoadComplete.Broadcast(/*bFreshStart*/ true, /*bHadPreviousData*/ false);
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
