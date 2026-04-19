// Copyright Moss Baby
//
// MossSaveLoadSubsystem.cpp — Save/Load Persistence 메인 서브시스템 구현
//
// Story 1-8: UMossSaveLoadSubsystem 뼈대 + 4-trigger lifecycle (T1/T2/T3/T4) + coalesce 정책
// ADR-0009: per-trigger atomicity (sequence-level API 금지 — GSM 책임)
// GDD: design/gdd/save-load-persistence.md §Core Rules 5 + §States
//
// 뼈대 구현 범위:
//   - Initialize: SaveData NewObject, delegate 등록, OnLoadComplete Broadcast (bFreshStart=true)
//   - Deinitialize: delegate Remove (정확한 핸들), T4 FlushAndDeinit
//   - SaveAsync: state-based coalesce/drop 로직 + IOCommitCount (동기 시뮬레이션)
//   - FlushOnly (T2): Idle 시 SaveAsync 1회
//   - FlushAndDeinit (T1/T3/T4): FThreadSafeBool 재진입 차단 + pending commit
//   - OnSaveTaskComplete: Idle 복귀 + pending coalesced save 처리
//
// Deferred (Story 1-10):
//   - 실제 worker thread TFuture 위임
//   - TFuture::WaitFor timeout (AC E23)
//   - Atomic write + dual-slot (AC Header CRC)
//
// Deferred (Story 1-9):
//   - 실제 디스크 로드 + 마이그레이션 체인
//   - FreshStart 분기
//
// Deferred (Story 1-20 또는 게임 코드):
//   - T1 UGameViewportClient::CloseRequested UWorld* 실제 바인딩

#include "SaveLoad/MossSaveLoadSubsystem.h"
#include "Settings/MossSaveLoadSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY_STATIC(LogMossSaveLoad, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Initialize
// ─────────────────────────────────────────────────────────────────────────────

void UMossSaveLoadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // UMossSaveData 인스턴스 생성 (실제 디스크 로드는 Story 1-9)
    SaveData = NewObject<UMossSaveData>(this);
    State = ESaveLoadState::Idle;

    // T2: Alt+Tab deactivation 감지
    // FSlateApplication::IsInitialized() 가드: -nullrhi / headless CI 환경에서는
    // SlateApplication이 초기화되지 않아 직접 호출 시 crash.
    if (FSlateApplication::IsInitialized())
    {
        ActivationChangedHandle = FSlateApplication::Get()
            .OnApplicationActivationStateChanged()
            .AddUObject(this, &UMossSaveLoadSubsystem::OnAppActivationChanged);

        UE_LOG(LogMossSaveLoad, Verbose, TEXT("T2 activation delegate 등록 완료"));
    }
    else
    {
        UE_LOG(LogMossSaveLoad, Verbose,
            TEXT("SlateApplication 미초기화 (headless/nullrhi) — T2 delegate 등록 생략"));
    }

    // T3: Engine exit (비-게임 스레드 발화 가능 — FThreadSafeBool 사용 이유)
    EngineExitHandle = FCoreDelegates::OnExit.AddUObject(
        this, &UMossSaveLoadSubsystem::OnEngineExit);

    UE_LOG(LogMossSaveLoad, Verbose, TEXT("T3 CoreDelegates::OnExit 등록 완료"));

    // T1 viewport close: GameViewport 초기화 후 바인딩 필요.
    // 실제 UGameViewportClient::CloseRequested Override 또는 delegate 바인딩은
    // Story 1-20 또는 게임 코드(예: UMossBabyGameInstance::Init)에서 처리.
    // 현재 뼈대: OnViewportCloseRequested() 스텁만 준비.

    // 뼈대: Initialize 완료 직후 FreshStart Broadcast
    // Story 1-9에서 실제 디스크 로드 결과(bFreshStart, bHadPreviousData)로 대체 예정.
    OnLoadComplete.Broadcast(/*bFreshStart*/ true, /*bHadPreviousData*/ false);

    UE_LOG(LogMossSaveLoad, Log,
        TEXT("UMossSaveLoadSubsystem 초기화 완료 (State=Idle, 뼈대 FreshStart Broadcast)"));
}

// ─────────────────────────────────────────────────────────────────────────────
// Deinitialize
// ─────────────────────────────────────────────────────────────────────────────

void UMossSaveLoadSubsystem::Deinitialize()
{
    // Dangling callback 방지 — RemoveAll(this) 대신 정확한 FDelegateHandle 사용.
    // RemoveAll은 동일 오브젝트의 모든 바인딩을 제거하므로 의도치 않은 제거 위험.
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get()
            .OnApplicationActivationStateChanged()
            .Remove(ActivationChangedHandle);
        ActivationChangedHandle.Reset();
    }

    FCoreDelegates::OnExit.Remove(EngineExitHandle);
    EngineExitHandle.Reset();

    // T4: Deinit flush — T1/T3 미실행 시 최종 보장
    // bDeinitFlushInProgress가 이미 set이면 T1 또는 T3이 FlushAndDeinit를 완료한 것
    if (!bDeinitFlushInProgress)
    {
        FlushAndDeinit();
    }

    SaveData = nullptr;

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveAsync
// ─────────────────────────────────────────────────────────────────────────────

void UMossSaveLoadSubsystem::SaveAsync(ESaveReason Reason)
{
    // AC E19: Loading 상태 — silent drop + 진단 로그 "drop"
    if (State == ESaveLoadState::Loading)
    {
        UE_LOG(LogMossSaveLoad, Verbose,
            TEXT("drop — Loading state (AC E19): reason=%s"),
            *UEnum::GetValueAsString(Reason));
        return;
    }

    // AC E20/E22: Saving 또는 Migrating 상태 — coalesce (최신 reason 승리)
    if (State == ESaveLoadState::Saving || State == ESaveLoadState::Migrating)
    {
        const FString PrevReason = PendingSaveReason.IsSet()
            ? UEnum::GetValueAsString(PendingSaveReason.GetValue())
            : TEXT("(없음)");

        PendingSaveReason = Reason;

        UE_LOG(LogMossSaveLoad, Verbose,
            TEXT("coalesce — %s 상태 중 reason=%s (이전 pending=%s, 최신 승리)"),
            *UEnum::GetValueAsString(State),
            *UEnum::GetValueAsString(Reason),
            *PrevReason);
        return;
    }

    // Idle → Saving 전이
    State = ESaveLoadState::Saving;

    if (SaveData)
    {
        SaveData->LastSaveReason = UEnum::GetValueAsString(Reason);
    }

    // 뼈대: I/O commit 카운트 증가 (실제 atomic write는 Story 1-10에서 구현)
    IOCommitCount++;

    UE_LOG(LogMossSaveLoad, Verbose,
        TEXT("SaveAsync 진입 — reason=%s, IOCommitCount=%d (뼈대 동기 시뮬레이션)"),
        *UEnum::GetValueAsString(Reason), IOCommitCount);

    // 뼈대: 동기 완료 시뮬레이션 — 실제 worker thread TFuture 위임은 Story 1-10
    // Story 1-10: Async(EAsyncExecution::ThreadPool, [this]() { snapshot + atomic write })
    //             완료 후 game thread에서 OnSaveTaskComplete() 콜백
    OnSaveTaskComplete();
}

// ─────────────────────────────────────────────────────────────────────────────
// OnSaveTaskComplete
// ─────────────────────────────────────────────────────────────────────────────

void UMossSaveLoadSubsystem::OnSaveTaskComplete()
{
    // Saving → Idle 복귀
    State = ESaveLoadState::Idle;

    // Coalesced pending save 즉시 처리 (AC E20/E22)
    // Idle 복귀 직후 pending이 있으면 재귀가 아닌 단일 SaveAsync 호출로 처리.
    // SaveAsync 내부에서 Idle → Saving 전이 후 OnSaveTaskComplete 재호출 (뼈대 동기 체인).
    if (PendingSaveReason.IsSet())
    {
        const ESaveReason Pending = PendingSaveReason.GetValue();
        PendingSaveReason.Reset();

        UE_LOG(LogMossSaveLoad, Verbose,
            TEXT("pending coalesced save 처리 — reason=%s"),
            *UEnum::GetValueAsString(Pending));

        SaveAsync(Pending);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// FlushOnly (T2 전용)
// ─────────────────────────────────────────────────────────────────────────────

void UMossSaveLoadSubsystem::FlushOnly()
{
    // T2: bDeinitFlushInProgress 변경 안 함 — T1/T3/T4 정상 동작 보장
    // Idle 상태에서만 SaveAsync 호출 (Saving/Migrating 중이면 coalesce가 이미 처리)
    if (State == ESaveLoadState::Idle)
    {
        // Last save reason parse는 Story 1-10 — 현재 뼈대 기본값 ECardOffered
        SaveAsync(ESaveReason::ECardOffered);

        UE_LOG(LogMossSaveLoad, Verbose, TEXT("T2 FlushOnly 완료 (Idle → SaveAsync)"));
    }
    else
    {
        UE_LOG(LogMossSaveLoad, Verbose,
            TEXT("T2 FlushOnly 스킵 — 현재 상태 %s (coalesce 처리됨)"),
            *UEnum::GetValueAsString(State));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// FlushAndDeinit (T1/T3/T4 공통)
// ─────────────────────────────────────────────────────────────────────────────

void UMossSaveLoadSubsystem::FlushAndDeinit()
{
    // FThreadSafeBool::AtomicSet(true): 기존 값이 true이면 true 반환 (재진입 차단)
    // 첫 번째 호출만 통과, 이후 재진입은 즉시 return
    if (bDeinitFlushInProgress.AtomicSet(true))
    {
        UE_LOG(LogMossSaveLoad, Verbose,
            TEXT("FlushAndDeinit 재진입 차단 — 이미 진행 중"));
        return;
    }

    const auto* Settings = UMossSaveLoadSettings::Get();
    const double Start = FPlatformTime::Seconds();
    const double TimeoutSec = Settings ? static_cast<double>(Settings->DeinitFlushTimeoutSec) : 2.0;

    UE_LOG(LogMossSaveLoad, Log,
        TEXT("FlushAndDeinit 시작 (timeout %.2f s)"), TimeoutSec);

    // pending coalesced save 즉시 commit (Idle 상태인 경우)
    // 실제 in-flight TFuture 대기는 Story 1-10에서 구현:
    //   if (InFlightSave.IsValid()) {
    //       const bool bDone = InFlightSave.WaitFor(FTimespan::FromSeconds(TimeoutSec));
    //       if (!bDone) { UE_LOG(... "timeout" ...) }
    //   }
    if (State == ESaveLoadState::Idle && PendingSaveReason.IsSet())
    {
        const ESaveReason Pending = PendingSaveReason.GetValue();
        PendingSaveReason.Reset();

        UE_LOG(LogMossSaveLoad, Verbose,
            TEXT("FlushAndDeinit: pending coalesced save 1회 commit — reason=%s"),
            *UEnum::GetValueAsString(Pending));

        SaveAsync(Pending);
    }

    // AC E23 뼈대: timeout 경과 시 로그 (실제 TFuture 대기 후 판단은 Story 1-10)
    const double Elapsed = FPlatformTime::Seconds() - Start;
    if (Elapsed > TimeoutSec)
    {
        UE_LOG(LogMossSaveLoad, Warning,
            TEXT("Deinit flush timeout %.2f s (elapsed=%.2f s) — in-flight 포기 (AC E23)"),
            TimeoutSec, Elapsed);
    }
    else
    {
        UE_LOG(LogMossSaveLoad, Log,
            TEXT("FlushAndDeinit 완료 (elapsed=%.4f s)"), Elapsed);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Trigger Handlers
// ─────────────────────────────────────────────────────────────────────────────

void UMossSaveLoadSubsystem::OnAppActivationChanged(bool bIsActive)
{
    // T2: deactivation(Alt+Tab 등)에서만 FlushOnly
    // activation(복귀)은 무시
    if (!bIsActive)
    {
        UE_LOG(LogMossSaveLoad, Verbose, TEXT("T2 App deactivation — FlushOnly 호출"));
        FlushOnly();
    }
}

void UMossSaveLoadSubsystem::OnViewportCloseRequested()
{
    // T1: 실제 UWorld* 파라미터 바인딩은 Story 1-20 또는 게임 코드에서 처리.
    // 현재 스텁: FlushAndDeinit() 직접 호출
    UE_LOG(LogMossSaveLoad, Log, TEXT("T1 ViewportCloseRequested — FlushAndDeinit 호출"));
    FlushAndDeinit();
}

void UMossSaveLoadSubsystem::OnEngineExit()
{
    // T3: FCoreDelegates::OnExit — 비-게임 스레드 발화 가능
    // FThreadSafeBool bDeinitFlushInProgress가 thread-safe 재진입 차단 보장
    UE_LOG(LogMossSaveLoad, Log, TEXT("T3 EngineExit — FlushAndDeinit 호출"));
    FlushAndDeinit();
}
