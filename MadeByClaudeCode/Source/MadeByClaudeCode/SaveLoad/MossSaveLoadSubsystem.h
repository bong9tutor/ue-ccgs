// Copyright Moss Baby
//
// MossSaveLoadSubsystem.h — Save/Load Persistence 메인 서브시스템 선언
//
// Story 1-8: UMossSaveLoadSubsystem 뼈대 + 4-trigger lifecycle (T1/T2/T3/T4) + coalesce 정책
// ADR-0009: per-trigger atomicity + coalesce (sequence-level API 금지 — GSM 책임)
// GDD: design/gdd/save-load-persistence.md §Core Rules 5 + §States
//
// 5-state 머신 (GDD §States):
//   Idle → Saving (SaveAsync 정상 진입)
//   Idle → Loading (Story 1-9 실제 로드 구현 시 전이)
//   Loading → Migrating (Story 1-9 마이그레이션 체인 시 전이)
//   Loading/Migrating → Idle (로드/마이그레이션 완료)
//   Saving → Idle (저장 완료, OnSaveTaskComplete)
//   FreshStart: 최초 실행 — Story 1-9에서 사용
//
// 4-trigger lifecycle:
//   T1: UGameViewportClient::CloseRequested(UWorld*) → FlushAndDeinit()
//       현재 뼈대: OnViewportCloseRequested() 스텁 — 실제 World* 바인딩은 Story 1-20
//   T2: FSlateApplication::OnApplicationActivationStateChanged → FlushOnly()
//       deactivation(Alt+Tab)에서만 발화, bDeinitFlushInProgress 변경 안 함
//   T3: FCoreDelegates::OnExit → FlushAndDeinit()
//   T4: Deinitialize() 내부 → FlushAndDeinit() (T1/T3 미실행 시)
//
// Coalesce 정책 (GDD §5 + ADR-0009):
//   Saving/Migrating 중 재호출 → PendingSaveReason(TOptional, 최대 1) 덮어쓰기
//   Loading 중 재호출 → silent drop + 진단 로그 "drop" (AC E19)
//   Idle 복귀 시 pending 즉시 단일 commit (AC E20/E22)
//
// ADR-0009 금지: sequence-level atomicity API in Save/Load layer
//   Compound event sequence atomicity = GSM BeginCompoundEvent/EndCompoundEvent 책임
//
// Story 1-8 Out of Scope:
//   - Header block + CRC32 (Story 1-9)
//   - Atomic write + dual-slot (Story 1-10)
//   - T1 GameViewport World* 실제 바인딩 (Story 1-20 또는 게임 코드)
//   - AC E23 TFuture 실제 대기 (Story 1-10)

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HAL/ThreadSafeBool.h"
#include "SaveLoad/MossSaveData.h"
#include "MossSaveLoadSubsystem.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// ESaveLoadState — 5-state 머신 (GDD §States)
//
// 전이 규칙 (Story 1-8 뼈대 레벨):
//   Idle      → Saving   : SaveAsync(Reason) 정상 진입
//   Saving    → Idle     : OnSaveTaskComplete() (뼈대 동기 시뮬레이션)
//   Idle      → Loading  : Story 1-9에서 실제 로드 경로 구현
//   Loading   → Migrating: Story 1-9에서 마이그레이션 체인 진입
//   FreshStart           : Story 1-9에서 최초 실행 경로 분기
// ─────────────────────────────────────────────────────────────────────────────

/**
 * ESaveLoadState — Save/Load Subsystem 5-state 머신.
 *
 * GDD §States: 각 상태에서 SaveAsync 호출 시 coalesce 또는 drop 정책이 달라진다.
 * Loading/Migrating/Saving 중 재호출 → PendingSaveReason coalesce (AC E20/E22).
 * Loading 중 재호출 → silent drop (AC E19).
 */
UENUM(BlueprintType)
enum class ESaveLoadState : uint8
{
    /** 정상 대기 상태. SaveAsync 신규 진입 가능. */
    Idle,

    /** 세이브 데이터 로드 중 (Story 1-9에서 전이 로직 구현). */
    Loading,

    /** 스키마 마이그레이션 중 (Story 1-9에서 전이 로직 구현). */
    Migrating,

    /** 저장 I/O 진행 중. 재호출 시 PendingSaveReason coalesce. */
    Saving,

    /** 최초 실행 — 이전 세이브 파일 없음 (Story 1-9에서 사용). */
    FreshStart
};

// ─────────────────────────────────────────────────────────────────────────────
// FOnLoadComplete — 로드 완료 Multicast Delegate (global scope — UE idiom)
//
// 발행 시점:
//   - Story 1-8 뼈대: Initialize() 완료 직후 (bFreshStart=true, bHadPreviousData=false)
//   - Story 1-9 실제 로드 구현 후: 실제 디스크 로드 결과에 따라 파라미터 실제화
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Save/Load 로드 완료 시 발행하는 Multicast Delegate.
 *
 * @param bFreshStart      true = 최초 실행 / 이전 세이브 없음
 * @param bHadPreviousData true = 이전 세이브 파일 존재
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLoadComplete, bool /*bFreshStart*/, bool /*bHadPreviousData*/);

// ─────────────────────────────────────────────────────────────────────────────
// UMossSaveLoadSubsystem
// ─────────────────────────────────────────────────────────────────────────────

/**
 * UMossSaveLoadSubsystem — Moss Baby Save/Load Persistence 메인 Subsystem.
 *
 * UGameInstanceSubsystem 기반. GameInstance 생명주기에 귀속.
 * 4-trigger lifecycle (T1 CloseRequested / T2 FlushOnly / T3 OnExit / T4 Deinit).
 * Coalesce 정책: 진행 중 재호출 시 PendingSaveReason 덮어쓰기 (최신 in-memory 승리).
 *
 * ADR-0009 준수: sequence-level atomicity API 없음.
 *   Compound event sequence atomicity = GSM BeginCompoundEvent/EndCompoundEvent 책임.
 */
UCLASS()
class MADEBYCLAUDECODE_API UMossSaveLoadSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── Lifecycle (UGameInstanceSubsystem overrides) ──────────────────────────

    /**
     * 서브시스템 초기화.
     *   - UMossSaveData 인스턴스 생성 (실제 디스크 로드는 Story 1-9)
     *   - T2 Slate activation delegate 등록 (nullrhi 가드 포함)
     *   - T3 CoreDelegates::OnExit 등록
     *   - State = Idle 설정
     *   - OnLoadComplete.Broadcast(bFreshStart=true, bHadPreviousData=false) (뼈대)
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * 서브시스템 정리.
     *   - T2/T3 delegate 핸들 Remove (dangling 방지)
     *   - T4: FlushAndDeinit() 호출 (T1/T3 미실행 시 최종 플러시)
     *   - SaveData = nullptr 후 Super::Deinitialize()
     */
    virtual void Deinitialize() override;

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * 비동기 저장 요청.
     *
     * - Loading 상태: silent drop + 진단 로그 "drop" (AC E19)
     * - Saving/Migrating 상태: PendingSaveReason coalesce (최신 reason 승리, AC E20/E22)
     * - Idle 상태: Saving 전이 + IOCommitCount 증가 + OnSaveTaskComplete 호출 (뼈대 동기)
     *
     * 실제 worker thread 위임 + atomic write는 Story 1-10에서 구현.
     *
     * @param Reason  저장 트리거 이유 (ESaveReason enum)
     */
    void SaveAsync(ESaveReason Reason);

    /**
     * T2 전용 Alt+Tab 플러시.
     *
     * deactivation 시 Idle 상태이면 SaveAsync 1회 트리거.
     * bDeinitFlushInProgress 변경 안 함 — T1/T3/T4 정상 동작 보장.
     */
    void FlushOnly();

    // ── State Inspection API ──────────────────────────────────────────────────

    /** 현재 5-state machine 상태 반환. */
    ESaveLoadState GetState() const { return State; }

    /**
     * 저장 데이터 열화 여부.
     * SaveData가 nullptr이면 false 반환.
     */
    bool IsSaveDegraded() const { return SaveData ? SaveData->bSaveDegraded : false; }

    /** 현재 UMossSaveData 포인터 반환 (읽기 전용). */
    UMossSaveData* GetSaveData() const { return SaveData; }

    // ── Delegates ─────────────────────────────────────────────────────────────

    /**
     * 로드 완료 Multicast Delegate.
     * 바인딩: OnLoadComplete.AddUObject(this, &UMyClass::OnLoadCompleteHandler)
     */
    FOnLoadComplete OnLoadComplete;

    // ── Coalesce 확인용 (테스트 + 진단) ──────────────────────────────────────

    /**
     * pending coalesced save 존재 여부.
     * Saving/Migrating 중 재호출된 요청이 대기 중이면 true.
     */
    bool HasPendingSave() const { return PendingSaveReason.IsSet(); }

    // ── 테스트 전용 훅 ────────────────────────────────────────────────────────

#if WITH_AUTOMATION_TESTS
    /**
     * [테스트 전용] 상태 강제 설정.
     * Initialize() 미호출 시 AC E19/E20/E22 테스트에서 상태 시뮬레이션용.
     * 프로덕션 코드에서 절대 호출 금지.
     * @param NewState  강제 설정할 상태
     */
    void TestingSetState(ESaveLoadState NewState) { State = NewState; }

    /**
     * [테스트 전용] pending coalesced save 즉시 실행.
     * Idle 상태이고 PendingSaveReason이 설정되어 있으면 SaveAsync 1회 호출.
     * 프로덕션 코드에서 절대 호출 금지.
     */
    void TestingTriggerPending()
    {
        if (PendingSaveReason.IsSet() && State == ESaveLoadState::Idle)
        {
            const ESaveReason Reason = PendingSaveReason.GetValue();
            PendingSaveReason.Reset();
            SaveAsync(Reason);
        }
    }

    /**
     * [테스트 전용] 실제 I/O commit 횟수 반환.
     * 뼈대 레벨 카운터 — 실제 atomic write는 Story 1-10에서 구현.
     * 프로덕션 코드에서 절대 호출 금지.
     */
    int32 TestingGetIOCommitCount() const { return IOCommitCount; }

    /**
     * [테스트 전용] I/O commit 카운터 초기화.
     * 테스트 격리를 위해 테스트 시작 전 호출.
     * 프로덕션 코드에서 절대 호출 금지.
     */
    void TestingResetIOCommitCount() { IOCommitCount = 0; }
#endif

private:
    // ── Trigger Handlers ──────────────────────────────────────────────────────

    /**
     * T1/T3/T4 공통 flush + deinit.
     * FThreadSafeBool bDeinitFlushInProgress 첫 호출에서 set — 재진입 차단.
     * pending coalesced save 1회 즉시 commit 후 deinit 완료.
     * 실제 TFuture 대기 + timeout 처리는 Story 1-10에서 구현.
     */
    void FlushAndDeinit();

    /**
     * T2: FSlateApplication::OnApplicationActivationStateChanged 핸들러.
     * deactivation(bIsActive=false) 시 FlushOnly() 호출.
     * @param bIsActive  true = 앱 활성, false = 비활성(Alt+Tab 등)
     */
    void OnAppActivationChanged(bool bIsActive);

    /**
     * T1 스텁: UGameViewportClient::CloseRequested 바인딩용.
     * 실제 UWorld* 파라미터 바인딩은 Story 1-20 또는 게임 코드에서 처리.
     * 현재 뼈대: FlushAndDeinit() 직접 호출.
     */
    void OnViewportCloseRequested();

    /**
     * T3: FCoreDelegates::OnExit 핸들러.
     * FlushAndDeinit() 호출 — T4와 재진입 차단 공유.
     */
    void OnEngineExit();

    /**
     * SaveAsync 완료 후 game thread 콜백.
     * State = Idle 복귀 + pending coalesced save 즉시 처리.
     * 뼈대: SaveAsync 내에서 동기 직접 호출.
     * 실제 worker thread 완료 콜백은 Story 1-10에서 구현.
     */
    void OnSaveTaskComplete();

    // ── State ─────────────────────────────────────────────────────────────────

    /**
     * Deinitialize 재진입 차단 플래그.
     * T1/T3/T4 중 첫 FlushAndDeinit 호출에서 AtomicSet(true).
     * thread-safe: T3은 비-게임 스레드에서 발화 가능.
     */
    FThreadSafeBool bDeinitFlushInProgress = false;

    /** 현재 5-state machine 상태. 초기값 Idle. */
    ESaveLoadState State = ESaveLoadState::Idle;

    /**
     * 게임 전체 영속 데이터 컨테이너.
     * UPROPERTY 필수 — GC 안전성. Initialize에서 NewObject<> 생성.
     * 실제 디스크 로드는 Story 1-9에서 구현.
     */
    UPROPERTY()
    TObjectPtr<UMossSaveData> SaveData;

    /**
     * Coalesce 큐 (최대 1).
     * Saving/Migrating 중 SaveAsync 재호출 시 최신 reason 덮어쓰기 (latest 승리).
     * OnSaveTaskComplete()/FlushAndDeinit()에서 소비.
     */
    TOptional<ESaveReason> PendingSaveReason;

    /**
     * I/O commit 횟수 카운터.
     * 뼈대 레벨: SaveAsync에서 Idle→Saving 전이마다 +1.
     * 실제 atomic write 횟수 추적은 Story 1-10에서 구현.
     * 테스트 가시성: TestingGetIOCommitCount() / TestingResetIOCommitCount().
     */
    int32 IOCommitCount = 0;

    // ── Delegate Handles (dangling 방지) ─────────────────────────────────────

    /**
     * T2 Slate activation delegate 핸들.
     * Deinitialize에서 Remove(ActivationChangedHandle) 호출 — RemoveAll 대신 정확한 핸들 사용.
     */
    FDelegateHandle ActivationChangedHandle;

    /**
     * T3 CoreDelegates::OnExit 핸들.
     * Deinitialize에서 Remove(EngineExitHandle) 호출 — RemoveAll 대신 정확한 핸들 사용.
     */
    FDelegateHandle EngineExitHandle;
};
