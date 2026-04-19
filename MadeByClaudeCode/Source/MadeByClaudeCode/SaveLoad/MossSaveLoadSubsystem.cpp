// Copyright Moss Baby
//
// MossSaveLoadSubsystem.cpp — Save/Load Persistence 메인 서브시스템 구현
//
// Story 1-8: UMossSaveLoadSubsystem 뼈대 + 4-trigger lifecycle (T1/T2/T3/T4) + coalesce 정책
// Story 1-9: LoadInitial / ReadSlot / ComputeNextWSN / GetSlotPath 구현
// ADR-0009: per-trigger atomicity (sequence-level API 금지 — GSM 책임)
// GDD: design/gdd/save-load-persistence.md §Core Rules 5 + §States + §Formula 1-3
//
// 구현 범위:
//   - Initialize: SaveData NewObject, delegate 등록, LoadInitial() 호출
//   - Deinitialize: delegate Remove (정확한 핸들), T4 FlushAndDeinit
//   - SaveAsync: state-based coalesce/drop 로직 + IOCommitCount (동기 시뮬레이션)
//   - FlushOnly (T2): Idle 시 SaveAsync 1회
//   - FlushAndDeinit (T1/T3/T4): FThreadSafeBool 재진입 차단 + pending commit
//   - OnSaveTaskComplete: Idle 복귀 + pending coalesced save 처리
//   - LoadInitial: dual-slot 로드 + Formula 1 (argmax WSN ∩ Valid) + FreshStart 분기
//   - ReadSlot: Formula 3 6-condition short-circuit 유효성 검증
//   - ComputeNextWSN: Formula 2 max+1 + wrap detection
//   - GetSlotPath: 플랫폼 경로 생성
//
// Deferred (Story 1-10):
//   - 실제 worker thread TFuture 위임
//   - TFuture::WaitFor timeout (AC E23)
//   - Atomic write + dual-slot write (CRC 생성 포함)
//
// Deferred (Story 1-16):
//   - Migration chain (Formula 4)
//
// Deferred (Story 1-20 또는 게임 코드):
//   - T1 UGameViewportClient::CloseRequested UWorld* 실제 바인딩

#include "SaveLoad/MossSaveLoadSubsystem.h"
#include "SaveLoad/MossSaveSnapshot.h"
#include "SaveLoad/MossSaveHeader.h"
#include "Settings/MossSaveLoadSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Crc.h"
#include "Kismet/GameplayStatics.h"

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
    if (State == ESaveLoadState::Loading)
    {
        UE_LOG(LogMossSaveLoad, Verbose, TEXT("drop — Loading state (AC E19)"));
        return;
    }
    if (State == ESaveLoadState::Migrating || State == ESaveLoadState::Saving)
    {
        PendingSaveReason = Reason;
        UE_LOG(LogMossSaveLoad, Verbose, TEXT("coalesce during %s"), *UEnum::GetValueAsString(State));
        return;
    }

    State = ESaveLoadState::Saving;
    if (SaveData) { SaveData->LastSaveReason = UEnum::GetValueAsString(Reason); }

    // Step 1: Snapshot (game thread)
    const FMossSaveSnapshot Snapshot = SaveData
        ? MakeSnapshotFromSaveData(*SaveData)
        : FMossSaveSnapshot{};

    // Step 2: Target slot (ping-pong, FreshStart → A)
    const TCHAR TargetSlot = (ActiveSlot == TEXT('A')) ? TEXT('B') : TEXT('A');

    // Step 3: NextWSN
    const uint32 NextWSN = ComputeNextWSN();

    // Step 4-9: 동기 실행 (Story 1-20 Async 구현 defer)
    const bool bSuccess = WriteSlotAtomic(TargetSlot, Snapshot, NextWSN);

    if (bSuccess)
    {
        ActiveSlot = TargetSlot;
        IOCommitCount++;

        // Step 10: 반대 슬롯 .tmp cleanup (best-effort)
        const TCHAR OtherSlot = (TargetSlot == TEXT('A')) ? TEXT('B') : TEXT('A');
        const FString OtherTmp = GetSlotPath(OtherSlot) + TEXT(".tmp");
        IFileManager::Get().Delete(*OtherTmp, /*RequireExists=*/false);
    }
    else
    {
        UE_LOG(LogMossSaveLoad, Warning, TEXT("WriteSlotAtomic failed for slot %c"), TargetSlot);
    }

    State = ESaveLoadState::Idle;

    // Pending coalesced save 즉시 commit
    if (PendingSaveReason.IsSet())
    {
        const ESaveReason Next = PendingSaveReason.GetValue();
        PendingSaveReason.Reset();
        SaveAsync(Next);
    }
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

// ─────────────────────────────────────────────────────────────────────────────
// WriteSlotAtomic (Story 1-10, Step 4-9)
// ─────────────────────────────────────────────────────────────────────────────

bool UMossSaveLoadSubsystem::WriteSlotAtomic(TCHAR TargetSlot, const FMossSaveSnapshot& Snapshot, uint32 NextWSN)
{
    // Step 4: Serialize snapshot → Payload bytes
    TArray<uint8> Payload;
    {
        FMemoryWriter Writer(Payload, /*bIsPersistent=*/true);
        FMossSaveSnapshot MutableCopy = Snapshot;
        UScriptStruct* Struct = FMossSaveSnapshot::StaticStruct();
        Struct->SerializeItem(Writer, &MutableCopy, nullptr);
        if (Writer.IsError())
        {
            UE_LOG(LogMossSaveLoad, Warning, TEXT("Snapshot serialize error"));
            return false;
        }
    }

    // MaxPayloadBytes 체크
    const auto* Settings = UMossSaveLoadSettings::Get();
    if (Settings && Payload.Num() > Settings->MaxPayloadBytes)
    {
        UE_LOG(LogMossSaveLoad, Error, TEXT("Payload size %d exceeds MaxPayloadBytes %d"),
            Payload.Num(), Settings->MaxPayloadBytes);
        return false;
    }

    // Step 5: CRC32 seed=0
    const uint32 Crc = FCrc::MemCrc32(Payload.GetData(), Payload.Num(), 0);

    // Step 6: Header
    FMossSaveHeader Header;
    Header.SaveSchemaVersion = Snapshot.SaveSchemaVersion > 0
        ? Snapshot.SaveSchemaVersion
        : UMossSaveData::CURRENT_SCHEMA_VERSION;
    Header.WriteSeqNumber    = NextWSN;
    Header.PayloadByteLength = static_cast<uint32>(Payload.Num());
    Header.PayloadCrc32      = Crc;

    // Step 7: Full buffer = Header + Payload
    TArray<uint8> FullBuffer;
    Header.SerializeToBuffer(FullBuffer);
    FullBuffer.Append(Payload);

    // Step 8: Temp write (directory 보장)
    const FString FinalPath = GetSlotPath(TargetSlot);
    const FString TempPath  = FinalPath + TEXT(".tmp");
    const FString DirPath   = FPaths::GetPath(FinalPath);
    IFileManager::Get().MakeDirectory(*DirPath, /*Tree=*/true);

    if (!FFileHelper::SaveArrayToFile(FullBuffer, *TempPath))
    {
        UE_LOG(LogMossSaveLoad, Warning, TEXT("SaveArrayToFile failed: %s"), *TempPath);
        return false;
    }

    // Step 9: Atomic rename (Windows NTFS)
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (PF.FileExists(*FinalPath))
    {
        PF.DeleteFile(*FinalPath); // 기존 파일 삭제 후 MoveFile
    }
    if (!PF.MoveFile(*FinalPath, *TempPath))
    {
        UE_LOG(LogMossSaveLoad, Warning, TEXT("MoveFile failed: %s -> %s"), *TempPath, *FinalPath);
        return false;
    }

    return true;
}
