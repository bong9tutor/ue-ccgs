// Copyright Moss Baby
//
// MossSaveLoadSubsystemTests.cpp — UMossSaveLoadSubsystem 자동화 테스트
//
// Story 1-8: 뼈대 + 4-trigger lifecycle + coalesce 정책
// ADR-0009: per-trigger atomicity (sequence-level API 금지 확인)
// GDD: design/gdd/save-load-persistence.md §Core Rules 5 + §States
//
// AC 커버리지:
//   ESaveLoadState enum + 뼈대 선언  → FMossSaveLoadInitialStateTest
//   SaveAsync Idle→Saving→Idle 전이  → FMossSaveLoadSaveAsyncIdleToSavingTest
//   AC E19 (Loading drop)             → FMossSaveLoadLoadingStateDropTest
//   AC E20 (Migrating coalesce 3회)   → FMossSaveLoadMigratingCoalesceTest
//   AC E22 (100X coalesce 큐 최대 1)  → FMossSaveLoadHundredXCoalesceTest
//   Coalesce 큐 소비                  → FMossSaveLoadPendingConsumeOnIdleTest
//   Deinit 재진입 차단                → FMossSaveLoadDeinitFlushReentryTest
//
// ─── Initialize/Deinitialize 직접 호출 테스트 분류 ────────────────────────────
//
// UGameInstanceSubsystem::Initialize는 FSubsystemCollectionBase& 인자를 받으며,
// FSubsystemCollectionBase 직접 생성 불가 (UE 내부 구조 귀속).
// 따라서 생명주기 통합 테스트는 integration test 또는 GameInstance functional test 범위.
// 본 파일: NewObject<> 직접 생성 후 Initialize 미호출(기본값 검증) 또는
//          TestingSetState/TestingTriggerPending 훅을 통한 상태 강제 설정 경로만 테스트.
//
// ─── AC E22 100X coalesce 테스트 주의사항 ────────────────────────────────────
//
// AC E22 원문: "Idle 상태 + 1회 I/O 진행 중 + 50ms 내 100회 SaveAsync → 실제 I/O ≤ 2회"
// 뼈대 레벨 한계: SaveAsync가 동기 완료 시뮬레이션(OnSaveTaskComplete 즉시 호출)이므로
//   Saving 상태를 유지하는 "진행 중" 시뮬레이션이 불가.
// 본 테스트는 coalesce 큐 최대 1 증명에 집중:
//   TestingSetState(Saving) 강제 → 100회 SaveAsync → PendingSaveReason 1개만 유지.
// 실제 I/O ≤ 2회 증명은 Story 1-10 TFuture 실제 구현 이후.
//
// ─── 헤드리스 실행 ─────────────────────────────────────────────────────────────
//
// UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//   -ExecCmds="Automation RunTests MossBaby.SaveLoad.Subsystem.; Quit" \
//   -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "SaveLoad/MossSaveLoadSubsystem.h"
#include "SaveLoad/MossSaveData.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 1 — ESaveLoadState 초기 상태 + 뼈대 선언 검증
//
// Initialize 미호출 시 기본값 확인:
//   GetState() == Idle (멤버 초기값)
//   GetSaveData() == nullptr (Initialize 미호출)
//   HasPendingSave() == false
//   IsSaveDegraded() == false (SaveData nullptr 가드)
//
// NOTE: Initialize 호출 후 State == Idle 확인은 Integration test 범위
//       (FSubsystemCollectionBase 직접 생성 불가 — UE 내부 귀속).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveLoadInitialStateTest,
    "MossBaby.SaveLoad.Subsystem.InitialState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveLoadInitialStateTest::RunTest(const FString& Parameters)
{
    // Arrange: Subsystem 인스턴스 직접 생성 (Initialize 미호출 — 기본값 상태)
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    TestNotNull(TEXT("UMossSaveLoadSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // Act: 초기 상태 조회 (Initialize 미호출)

    // Assert: GetState() == Idle (멤버 초기값 ESaveLoadState::Idle)
    TestEqual(
        TEXT("초기 GetState() == Idle"),
        Sub->GetState(),
        ESaveLoadState::Idle);

    // Assert: GetSaveData() == nullptr (Initialize 미호출 시 NewObject 미실행)
    TestNull(
        TEXT("초기 GetSaveData() == nullptr (Initialize 미호출)"),
        Sub->GetSaveData());

    // Assert: HasPendingSave() == false (coalesce 큐 비어있음)
    TestFalse(
        TEXT("초기 HasPendingSave() == false"),
        Sub->HasPendingSave());

    // Assert: IsSaveDegraded() == false (SaveData nullptr 가드 — false 반환)
    TestFalse(
        TEXT("초기 IsSaveDegraded() == false (SaveData nullptr 가드)"),
        Sub->IsSaveDegraded());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 2 — SaveAsync: Idle → Saving → Idle 전이 + IOCommitCount 증가
//
// TestingSetState(Idle) 후 SaveAsync(ECardOffered):
//   - IOCommitCount +1 (뼈대 commit 카운터)
//   - 뼈대 동기 완료 후 State == Idle 복귀 (OnSaveTaskComplete 즉시 호출)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveLoadSaveAsyncIdleToSavingTest,
    "MossBaby.SaveLoad.Subsystem.SaveAsyncIdleToSaving",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveLoadSaveAsyncIdleToSavingTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    TestNotNull(TEXT("인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    Sub->TestingSetState(ESaveLoadState::Idle);
    Sub->TestingResetIOCommitCount();

    // Act
    Sub->SaveAsync(ESaveReason::ECardOffered);

    // Assert: IOCommitCount == 1 (Idle→Saving 전이 1회)
    TestEqual(
        TEXT("SaveAsync(ECardOffered) 후 IOCommitCount == 1"),
        Sub->TestingGetIOCommitCount(),
        1);

    // Assert: 뼈대 동기 완료 후 State == Idle 복귀
    TestEqual(
        TEXT("SaveAsync 완료 후 State == Idle (뼈대 동기 시뮬레이션)"),
        Sub->GetState(),
        ESaveLoadState::Idle);

    // Assert: pending coalesced save 없음 (단순 1회 호출)
    TestFalse(
        TEXT("SaveAsync 완료 후 HasPendingSave() == false"),
        Sub->HasPendingSave());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 3 — AC E19: Loading 상태에서 SaveAsync silent drop
//
// TestingSetState(Loading) 후 SaveAsync(ECardOffered):
//   - IOCommitCount 증가 없음 (silent drop)
//   - State == Loading 유지
//   - HasPendingSave() == false (coalesce 큐에도 추가 안 됨)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveLoadLoadingStateDropTest,
    "MossBaby.SaveLoad.Subsystem.LoadingStateDrop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveLoadLoadingStateDropTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    TestNotNull(TEXT("인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    Sub->TestingSetState(ESaveLoadState::Loading);
    Sub->TestingResetIOCommitCount();

    // Act: Loading 상태에서 SaveAsync 호출
    Sub->SaveAsync(ESaveReason::ECardOffered);

    // Assert: IOCommitCount 증가 없음 (AC E19 silent drop)
    TestEqual(
        TEXT("Loading 상태 SaveAsync → IOCommitCount == 0 (AC E19 drop)"),
        Sub->TestingGetIOCommitCount(),
        0);

    // Assert: State == Loading 유지 (drop 시 상태 변경 없음)
    TestEqual(
        TEXT("Loading 상태 SaveAsync 후 State == Loading 유지"),
        Sub->GetState(),
        ESaveLoadState::Loading);

    // Assert: HasPendingSave() == false (Loading drop은 coalesce 큐에도 추가 안 됨)
    TestFalse(
        TEXT("Loading 상태 SaveAsync 후 HasPendingSave() == false"),
        Sub->HasPendingSave());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 4 — AC E20: Migrating 상태에서 3회 SaveAsync coalesce
//
// TestingSetState(Migrating) + 3회 SaveAsync(다른 reason):
//   - IOCommitCount 증가 없음 (Migrating 중 실제 commit 없음)
//   - HasPendingSave() == true (coalesce 큐에 최신 1개 유지)
//
// 이후 TestingSetState(Idle) + TestingTriggerPending():
//   - pending coalesced save 1회 commit (IOCommitCount == 1)
//   - HasPendingSave() == false (소비 완료)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveLoadMigratingCoalesceTest,
    "MossBaby.SaveLoad.Subsystem.MigratingCoalesce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveLoadMigratingCoalesceTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    TestNotNull(TEXT("인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    Sub->TestingSetState(ESaveLoadState::Migrating);
    Sub->TestingResetIOCommitCount();

    // Act: Migrating 상태에서 3회 연속 SaveAsync (서로 다른 reason)
    Sub->SaveAsync(ESaveReason::ECardOffered);
    Sub->SaveAsync(ESaveReason::EDreamReceived);
    Sub->SaveAsync(ESaveReason::ENarrativeEmitted); // 최신 reason — 이것이 pending에 남아야 함

    // Assert: IOCommitCount == 0 (Migrating 중 실제 commit 없음, AC E20)
    TestEqual(
        TEXT("Migrating 중 3회 SaveAsync → IOCommitCount == 0 (AC E20)"),
        Sub->TestingGetIOCommitCount(),
        0);

    // Assert: State == Migrating 유지 (coalesce는 상태 변경 없음)
    TestEqual(
        TEXT("3회 SaveAsync 후 State == Migrating 유지"),
        Sub->GetState(),
        ESaveLoadState::Migrating);

    // Assert: HasPendingSave() == true (큐 최대 1 — 최신 ENarrativeEmitted 유지)
    TestTrue(
        TEXT("Migrating coalesce 후 HasPendingSave() == true (최신 1개 큐)"),
        Sub->HasPendingSave());

    // Act: Idle 복귀 후 pending 소비
    Sub->TestingSetState(ESaveLoadState::Idle);
    Sub->TestingTriggerPending();

    // Assert: IOCommitCount == 1 (pending 1회 commit)
    TestEqual(
        TEXT("TriggerPending 후 IOCommitCount == 1 (coalesced 단일 commit)"),
        Sub->TestingGetIOCommitCount(),
        1);

    // Assert: HasPendingSave() == false (소비 완료)
    TestFalse(
        TEXT("TriggerPending 후 HasPendingSave() == false (큐 소비 완료)"),
        Sub->HasPendingSave());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 5 — AC E22: 100회 연속 SaveAsync coalesce 큐 최대 1 증명
//
// 뼈대 레벨 한계 주의:
//   AC E22 원문 "실제 I/O ≤ 2회" 증명은 Story 1-10 TFuture 구현 이후.
//   본 테스트: coalesce 큐 최대 1 (PendingSaveReason 덮어쓰기) 증명.
//
// TestingSetState(Saving) 강제 후 100회 SaveAsync:
//   - IOCommitCount == 0 (Saving 중 모두 coalesce)
//   - HasPendingSave() == true (큐에 최신 reason 1개만 유지)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveLoadHundredXCoalesceTest,
    "MossBaby.SaveLoad.Subsystem.HundredXCoalesce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveLoadHundredXCoalesceTest::RunTest(const FString& Parameters)
{
    // Arrange
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    TestNotNull(TEXT("인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    // Saving 상태 강제: 뼈대에서 동기 완료를 막고 "진행 중" 시뮬레이션
    Sub->TestingSetState(ESaveLoadState::Saving);
    Sub->TestingResetIOCommitCount();

    // Act: Saving 상태에서 100회 연속 SaveAsync
    // reason을 번갈아 사용해 최신 승리 정책 검증
    for (int32 i = 0; i < 100; ++i)
    {
        const ESaveReason Reason = (i % 2 == 0)
            ? ESaveReason::ECardOffered
            : ESaveReason::EDreamReceived;
        Sub->SaveAsync(Reason);
    }

    // Assert: IOCommitCount == 0 (Saving 중 100회 모두 coalesce, 실제 commit 없음)
    TestEqual(
        TEXT("Saving 중 100회 SaveAsync → IOCommitCount == 0 (AC E22 coalesce)"),
        Sub->TestingGetIOCommitCount(),
        0);

    // Assert: HasPendingSave() == true (큐에 최신 reason 1개만 유지)
    TestTrue(
        TEXT("100회 coalesce 후 HasPendingSave() == true (큐 최대 1)"),
        Sub->HasPendingSave());

    // Assert: State == Saving 유지 (coalesce는 상태 변경 없음)
    TestEqual(
        TEXT("100회 SaveAsync 후 State == Saving 유지"),
        Sub->GetState(),
        ESaveLoadState::Saving);

    // Act: Idle 복귀 후 pending 소비 — IOCommitCount 상한 확인
    // 뼈대에서는 동기 완료이므로 1회 commit만 발생
    Sub->TestingSetState(ESaveLoadState::Idle);
    Sub->TestingTriggerPending();

    // Assert: TriggerPending 후 IOCommitCount == 1 (coalesced 단일 commit)
    TestEqual(
        TEXT("TriggerPending 후 IOCommitCount == 1 (100개 coalesced → 단일 commit)"),
        Sub->TestingGetIOCommitCount(),
        1);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 6 — Coalesce 큐 소비: HasPendingSave 후 TestingTriggerPending
//
// PendingSaveReason 수동 설정 (TestingSetState로 Saving 후 SaveAsync 호출) +
// TestingSetState(Idle) + TestingTriggerPending():
//   - HasPendingSave() == false (소비 완료)
//   - IOCommitCount +1 (소비 시 1회 commit)
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveLoadPendingConsumeOnIdleTest,
    "MossBaby.SaveLoad.Subsystem.PendingConsumeOnIdle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveLoadPendingConsumeOnIdleTest::RunTest(const FString& Parameters)
{
    // Arrange: Saving 상태에서 SaveAsync 호출로 PendingSaveReason 설정
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    TestNotNull(TEXT("인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    Sub->TestingSetState(ESaveLoadState::Saving);
    Sub->TestingResetIOCommitCount();

    // Saving 중 SaveAsync 호출 → PendingSaveReason 설정
    Sub->SaveAsync(ESaveReason::EFarewellReached);

    // Arrange 검증: pending이 설정됐는지 확인
    TestTrue(
        TEXT("Saving 중 SaveAsync 후 HasPendingSave() == true"),
        Sub->HasPendingSave());
    TestEqual(
        TEXT("Saving 중 SaveAsync는 IOCommitCount 증가 없음"),
        Sub->TestingGetIOCommitCount(),
        0);

    // Act: Idle 복귀 후 TestingTriggerPending
    Sub->TestingSetState(ESaveLoadState::Idle);
    Sub->TestingTriggerPending();

    // Assert: HasPendingSave() == false (소비 완료)
    TestFalse(
        TEXT("TestingTriggerPending 후 HasPendingSave() == false"),
        Sub->HasPendingSave());

    // Assert: IOCommitCount == 1 (pending 소비 시 1회 commit)
    TestEqual(
        TEXT("TestingTriggerPending 후 IOCommitCount == 1"),
        Sub->TestingGetIOCommitCount(),
        1);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 7 — Deinitialize 재진입 차단
//
// Deinitialize() 2회 호출 시:
//   - bDeinitFlushInProgress가 첫 호출에서만 FlushAndDeinit 실행
//   - 두 번째 호출은 재진입 차단으로 FlushAndDeinit 스킵
//
// IOCommitCount로 FlushAndDeinit 내 SaveAsync 호출 횟수 확인.
// 첫 호출: pending 없으면 commit 0. 두 번째 호출: commit 추가 없음.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossSaveLoadDeinitFlushReentryTest,
    "MossBaby.SaveLoad.Subsystem.DeinitFlushReentry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossSaveLoadDeinitFlushReentryTest::RunTest(const FString& Parameters)
{
    // Arrange: Saving 상태에서 pending coalesced save 설정
    // (FlushAndDeinit 첫 호출에서 pending이 있는 경우 1회 commit 발생)
    UMossSaveLoadSubsystem* Sub = NewObject<UMossSaveLoadSubsystem>();
    TestNotNull(TEXT("인스턴스 생성 성공"), Sub);
    if (!Sub)
    {
        return false;
    }

    Sub->TestingSetState(ESaveLoadState::Saving);
    Sub->TestingResetIOCommitCount();

    // pending coalesced save 설정
    Sub->SaveAsync(ESaveReason::ECardOffered);
    TestTrue(TEXT("Arrange: HasPendingSave() == true"), Sub->HasPendingSave());

    // Idle로 복귀 (FlushAndDeinit은 Idle 상태에서 pending을 처리)
    Sub->TestingSetState(ESaveLoadState::Idle);

    // Act: 첫 번째 Deinitialize 호출
    // FlushAndDeinit → pending 1회 commit → IOCommitCount == 1
    Sub->Deinitialize();

    const int32 CommitAfterFirst = Sub->TestingGetIOCommitCount();

    // Act: 두 번째 Deinitialize 호출 (재진입 시도)
    // bDeinitFlushInProgress가 set이므로 FlushAndDeinit 스킵 → IOCommitCount 변화 없음
    Sub->Deinitialize();

    const int32 CommitAfterSecond = Sub->TestingGetIOCommitCount();

    // Assert: 두 번째 Deinitialize 후 IOCommitCount 변화 없음 (재진입 차단)
    TestEqual(
        TEXT("두 번째 Deinitialize 후 IOCommitCount 변화 없음 (재진입 차단)"),
        CommitAfterSecond,
        CommitAfterFirst);

    return true;
}

#endif // WITH_AUTOMATION_TESTS
