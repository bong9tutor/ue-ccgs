# Story 002: UMossSaveLoadSubsystem 뼈대 + 4-trigger lifecycle (T1/T2/T3/T4) + coalesce 정책

> **Epic**: Save/Load Persistence
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 0.7 days (~5 hours)
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/save-load-persistence.md`
**Requirement**: `TR-save-004` (4-trigger lifecycle — T1 CloseRequested / T2 FlushOnly / T3 OnExit / T4 GameInstance Shutdown)
**ADR Governing Implementation**: ADR-0009 (per-trigger atomicity + coalesce for compound events)
**ADR Decision Summary**: `UMossSaveLoadSubsystem : UGameInstanceSubsystem`. 5-state 머신 (Idle/Loading/Migrating/Saving/FreshStart). `SaveAsync(ESaveReason)` public API — 진행 중 재호출 시 coalesce (latest in-memory 승리). T1/T3/T4는 `FlushAndDeinit()`, T2는 `FlushOnly()` (R3 분리).
**Engine**: UE 5.6 | **Risk**: MEDIUM (Windows lifecycle delegates)
**Engine Notes**: `UGameViewportClient::CloseRequested` override, `FSlateApplication::OnApplicationActivationStateChanged`, `FCoreDelegates::OnExit`. `FThreadSafeBool` 사용 (R3: T3 비-게임 스레드 발화 가능). Dangling callback 방지 `RemoveAll(this)` 필수.

**Control Manifest Rules (Foundation layer)**:
- Required: "Save/Load는 per-trigger atomicity만 보장" (ADR-0009)
- Required: "in-memory mutation + SaveAsync(reason)는 동일 game-thread 함수"
- Forbidden: "sequence-level atomicity API in Save/Load layer" — Compound atomicity는 GSM boundary 책임 (ADR-0009)

---

## Acceptance Criteria

*From GDD §Core Rules 5 + §States:*

- [ ] `UMossSaveLoadSubsystem : UGameInstanceSubsystem` 클래스 뼈대 (Initialize/Deinitialize, state enum, FOnLoadComplete delegate)
- [ ] `ESaveLoadState` enum — `Idle`, `Loading`, `Migrating`, `Saving`, `FreshStart`
- [ ] `SaveAsync(ESaveReason)` public API — Idle 상태에서 thread-pool worker 큐잉
- [ ] `FlushOnly()` public API (T2 전용) — Alt+Tab 시 비동기 SaveAsync 1회 트리거, `bDeinitFlushInProgress` 설정 안 함
- [ ] `FlushAndDeinit()` private — T1/T3/T4 공통. `FThreadSafeBool bDeinitFlushInProgress` 첫 호출에서 set (이후 재진입 차단)
- [ ] T1 hook: `UGameViewportClient::CloseRequested(UWorld*)` override → `FlushAndDeinit()`
- [ ] T2 hook: `FSlateApplication::Get().OnApplicationActivationStateChanged().AddRaw(...)` → deactivation 시 `FlushOnly()`. Deinitialize에서 `RemoveAll(this)` 호출
- [ ] T3 hook: `FCoreDelegates::OnExit.AddRaw(...)` → `FlushAndDeinit()`
- [ ] T4 hook: `Deinitialize()` 내부 `FlushAndDeinit()` 호출 (아직 실행 안 된 경우)
- [ ] Coalesce 정책: 진행 중 `SaveAsync` + 추가 호출 시 pending 큐 (최대 1) — 진행 완료 후 단일 commit
- [ ] AC E19_LOADING_STATE_DROPS_SAVES: Loading 상태에서 `SaveAsync(ECardOffered)` silent drop + 진단 로그 "drop"
- [ ] AC E20_MIGRATING_STATE_COALESCE: Migrating 상태 + `SaveAsync` 3회 연속 → 큐에 coalesce → Migrating 완료 후 Idle 직후 1회 저장
- [ ] AC E22_100X_COALESCE: Idle 상태 + 1회 I/O 진행 중 + 50ms 내 100회 SaveAsync → 실제 I/O ≤ 2회
- [ ] AC E23_DEINIT_TIMEOUT: `DeinitFlushTimeoutSec = 3.0` + 4초 지연 mock → 3초 경과 후 abandon + 직전 정상 슬롯 무탈 + 로그 "timeout"

---

## Implementation Notes

*Derived from GDD §5 Atomicity + §States "강제 flush 트리거":*

```cpp
// Source/MadeByClaudeCode/SaveLoad/MossSaveLoadSubsystem.h
UENUM()
enum class ESaveLoadState : uint8 { Idle, Loading, Migrating, Saving, FreshStart };

UCLASS()
class UMossSaveLoadSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void SaveAsync(ESaveReason Reason);
    void FlushOnly();  // T2 전용

    ESaveLoadState GetState() const { return State; }
    bool IsSaveDegraded() const { return SaveData ? SaveData->bSaveDegraded : false; }

    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLoadComplete, bool /*bFreshStart*/, bool /*bHadPreviousData*/);
    FOnLoadComplete OnLoadComplete;

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnSaveDegradedReached, FMossSlotMetadata);
    FOnSaveDegradedReached OnSaveDegradedReached;

private:
    void FlushAndDeinit();  // T1/T3/T4 공통
    void OnAppActivationChanged(bool bIsActive);  // T2 delegate handler
    void OnViewportCloseRequested(UWorld* World);  // T1
    void OnEngineExit();  // T3

    FThreadSafeBool bDeinitFlushInProgress = false;  // 재진입 차단
    ESaveLoadState State = ESaveLoadState::Idle;

    UPROPERTY() TObjectPtr<UMossSaveData> SaveData;

    TOptional<ESaveReason> PendingSaveReason;  // coalesce 큐 (최대 1)
    TFuture<void> InFlightSave;
};

// .cpp
void UMossSaveLoadSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    // 실제 로드는 Story 003에서 (Formula 1-3 + CRC)
    State = ESaveLoadState::Idle;

    // T2 — Alt+Tab deactivation
    FSlateApplication::Get().OnApplicationActivationStateChanged()
        .AddRaw(this, &UMossSaveLoadSubsystem::OnAppActivationChanged);
    // T3 — engine exit
    FCoreDelegates::OnExit.AddUObject(this, &UMossSaveLoadSubsystem::OnEngineExit);
    // T1 — viewport close (GEngine->GameViewport 초기화 후 override 또는 binding)
}

void UMossSaveLoadSubsystem::SaveAsync(ESaveReason Reason) {
    if (State == ESaveLoadState::Loading) {
        UE_LOG(LogMossSaveLoad, Verbose, TEXT("drop — Loading state"));  // AC E19
        return;
    }
    if (State == ESaveLoadState::Migrating || State == ESaveLoadState::Saving) {
        // AC E20/E22 — coalesce
        PendingSaveReason = Reason;  // 최신이 승리, 최대 1개
        return;
    }
    // Idle → Saving 전이 후 worker thread 큐잉
    State = ESaveLoadState::Saving;
    SaveData->LastSaveReason = UEnum::GetValueAsString(Reason);
    InFlightSave = Async(EAsyncExecution::ThreadPool, [this]() { /* Story 004 snapshot + write */ });
}

void UMossSaveLoadSubsystem::FlushOnly() {  // T2
    if (State == ESaveLoadState::Idle) {
        SaveAsync(ESaveReason::ECardOffered);  // last reason 또는 ECardOffered default
    }
    // bDeinitFlushInProgress 변경 안 함 — T1/T3/T4 정상 작동 보장
}

void UMossSaveLoadSubsystem::FlushAndDeinit() {
    if (bDeinitFlushInProgress.AtomicSet(true)) return;  // 재진입 차단
    const auto* Settings = UMossSaveLoadSettings::Get();
    const double Start = FPlatformTime::Seconds();
    // In-flight future 동기 대기 (timeout)
    if (InFlightSave.IsValid()) {
        const bool bDone = InFlightSave.WaitFor(FTimespan::FromSeconds(Settings->DeinitFlushTimeoutSec));
        if (!bDone) {
            UE_LOG(LogMossSaveLoad, Warning, TEXT("Deinit flush timeout %.2f s"), Settings->DeinitFlushTimeoutSec);
            // abandon in-flight — 직전 정상 슬롯 무탈
        }
    }
}

void UMossSaveLoadSubsystem::Deinitialize() {
    FSlateApplication::Get().OnApplicationActivationStateChanged().RemoveAll(this);  // dangling 방지
    FCoreDelegates::OnExit.RemoveAll(this);
    if (!bDeinitFlushInProgress) { FlushAndDeinit(); }  // T4
    Super::Deinitialize();
}
```

- 실제 슬롯 I/O는 Story 004에서 (atomic write 단계)
- `SaveAsync` 호출 시 worker thread로 위임 — 게임 스레드는 즉시 return (GDD §2 Step 1)

---

## Out of Scope

- Story 003: Header struct + CRC32 + Formula 1-3 + dual-slot read
- Story 004: FMossSaveSnapshot + atomic write (Step 2-10)
- Story 005: Migration chain
- Story 007: E14 disk full + E15 non-ASCII

---

## QA Test Cases

**For Logic story:**
- **Subsystem lifecycle**
  - Given: GameInstance init
  - When: `Initialize()`
  - Then: State = Idle, delegates registered
  - Edge cases: `Deinitialize()` → `FlushAndDeinit` 호출 + `RemoveAll(this)` 실행 + 재진입 시 first-call wins
- **E19 LOADING_STATE_DROPS_SAVES**
  - Given: Loading 상태 중
  - When: `SaveAsync(ECardOffered)`
  - Then: silent drop + 진단 로그 "drop"
- **E20 MIGRATING_STATE_COALESCE**
  - Given: Migrating state
  - When: 3회 `SaveAsync` 연속
  - Then: 3요청 coalesce → Migrating→Idle 후 1회 commit
- **E22 100X_COALESCE**
  - Given: Idle, I/O 진행 중
  - When: 50ms 내 100회 `SaveAsync`
  - Then: 실제 I/O ≤ 2회 (진행 1 + coalesce 1)
- **E23 DEINIT_TIMEOUT**
  - Given: `DeinitFlushTimeoutSec = 3.0`, mock 4초 지연
  - When: `Deinitialize()` 호출
  - Then: 3 ± 0.5초 후 반환 + abandon + 로그 "timeout" + 직전 슬롯 mtime 불변

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/save-load/subsystem_lifecycle_test.cpp` (trigger hookup + coalesce)
- `tests/unit/save-load/coalesce_policy_test.cpp` (E19, E20, E22)
- `tests/unit/save-load/deinit_timeout_test.cpp` (E23)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (UMossSaveData + Settings)
- Unlocks: Story 003, 004, 005

---

## Completion Notes

**Completed**: 2026-04-19
**Criteria**: 10/13 passing, 2 partial, 1 AC 스펙 범위 밖 (AC E19/E20/E22 뼈대 레벨 coalesce 증명)

**Files delivered**:
- `SaveLoad/MossSaveLoadSubsystem.h` (320 lines, 뼈대 선언 + Testing 훅)
- `SaveLoad/MossSaveLoadSubsystem.cpp` (300 lines, 4-trigger + coalesce)
- `Tests/MossSaveLoadSubsystemTests.cpp` (450 lines, 7 tests)
- `tests/unit/save-load/README.md` (Story 1-8 섹션 append)

**Test Evidence**: 7 UE Automation — `MossBaby.SaveLoad.Subsystem.*` (InitialState/SaveAsyncIdleToSaving/LoadingStateDrop/MigratingCoalesce/HundredXCoalesce/PendingConsumeOnIdle/DeinitFlushReentry)

**ADR 준수**:
- ADR-0001 grep: 0 매치
- ADR-0009: per-trigger atomicity (sequence-level BeginTransaction/Commit API 없음)
- Delegate dangling 방지: FDelegateHandle 정확한 Remove
- FThreadSafeBool 재진입 차단 (T3 비-게임 스레드 대비)
- FSlateApplication::IsInitialized() 가드 (nullrhi/headless)

**Deferred (Story 1-10)**:
- AC E22 실제 I/O ≤ 2 (TFuture 구현 이후 증명 가능)
- AC E23 DeinitFlushTimeoutSec TFuture WaitFor 타임아웃

**Deferred (Story 1-20 또는 게임 코드)**:
- T1 GameViewport `CloseRequested(UWorld*)` 실제 바인딩

**Integration test**: TD-005 lifecycle story에 포함 (UGameInstance 실제 초기화 경로)
