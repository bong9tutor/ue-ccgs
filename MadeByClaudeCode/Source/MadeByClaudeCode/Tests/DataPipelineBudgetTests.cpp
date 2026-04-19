// Copyright Moss Baby
//
// DataPipelineBudgetTests.cpp — T_init Budget + Catalog Size 3-단계 Overflow 단위 테스트
//
// Story-1-19: T_init budget 3-단계 overflow threshold (AC-DP-09/10) + GetCatalogStats (AC-DP-08)
// ADR-0003: Sync 일괄 로드 채택 — T_init 예산 Normal ≤ 50ms 기준
// ADR-0011: UMossDataPipelineSettings CDO default 값 (MaxInitTimeBudgetMs=50, MaxCatalogSizeCards=200 등)
//
// 테스트 값 근거 (CDO default 기준):
//   T_init:
//     Budget = 50ms, WarnMul = 1.05, ErrorMul = 1.5, FatalMul = 2.0
//     Normal threshold:  ElapsedMs ≤ 52.5ms  → 아무 flag도 미설정
//     Warn threshold:    ElapsedMs > 52.5ms   → bInitWarningTriggered (예: 52.6ms)
//     Error threshold:   ElapsedMs > 75ms     → bInitErrorTriggered   (예: 76.0ms)
//     Fatal threshold:   ElapsedMs > 100ms    → bInitFatalTriggered   (예: 101.0ms)
//
//   CatalogSize (Card, Budget=200):
//     Warn threshold:  Count >= 210  (200 * 1.05 = 210.0)
//     Error threshold: Count >= 300  (200 * 1.5  = 300.0)
//     Fatal threshold: Count >= 400  (200 * 2.0  = 400.0)
//
// 테스트 전략:
//   - TestingEvaluateTInitBudget / TestingCheckCatalogSize 훅으로 Settings 내부 경로 직접 호출
//   - 각 테스트 시작 시 TestingResetBudgetFlags() 로 이전 flag 초기화
//   - else-if 분기 특성 확인: Fatal 진입 시 Error/Warning flag 미설정
//
// 헤드리스 실행:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.Data.Pipeline.Budget.; Quit" \
//     -nullrhi -nosound -log -unattended
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#include "Misc/AutomationTest.h"
#include "Data/DataPipelineSubsystem.h"

#if WITH_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 1 — T_init Normal (30ms → 어떤 flag도 설정 안 됨)
//
// Budget 50ms, WarnMul 1.05 → Warn threshold = 52.5ms.
// 30ms ≤ 52.5ms이므로 Normal 분기. 3개 flag 모두 false 확인.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBudgetTInitNormalTest,
    "MossBaby.Data.Pipeline.Budget.TInitNormal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBudgetTInitNormalTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetBudgetFlags();

    // Act
    Sub->TestingEvaluateTInitBudget(30.0);

    // Assert: Normal 분기 — flag 전체 false
    TestFalse(TEXT("Normal(30ms): Fatal flag 미설정"), Sub->TestingWasFatalTriggered());
    TestFalse(TEXT("Normal(30ms): Error flag 미설정"), Sub->TestingWasErrorTriggered());
    TestFalse(TEXT("Normal(30ms): Warning flag 미설정"), Sub->TestingWasWarningTriggered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 2 — T_init Warning (52.6ms → Warning flag만 설정)
//
// Budget 50ms × WarnMul 1.05 = 52.5ms. 52.6 > 52.5 → Warning 분기.
// Error/Fatal flag 미설정 확인 (else-if 분기).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBudgetTInitWarningTest,
    "MossBaby.Data.Pipeline.Budget.TInitWarning",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBudgetTInitWarningTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetBudgetFlags();

    // Act: 50ms × 1.05 = 52.5ms, 52.6 > 52.5 → Warning
    Sub->TestingEvaluateTInitBudget(52.6);

    // Assert
    TestTrue (TEXT("Warning(52.6ms): Warning flag 설정"),    Sub->TestingWasWarningTriggered());
    TestFalse(TEXT("Warning(52.6ms): Error flag 미설정"),    Sub->TestingWasErrorTriggered());
    TestFalse(TEXT("Warning(52.6ms): Fatal flag 미설정"),    Sub->TestingWasFatalTriggered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 3 — T_init Error (76ms → Error flag 설정, Warning flag 미설정)
//
// Budget 50ms × ErrorMul 1.5 = 75ms. 76 > 75 → Error 분기 (else-if).
// Warning flag는 else-if 분기 특성상 설정 안 됨.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBudgetTInitErrorTest,
    "MossBaby.Data.Pipeline.Budget.TInitError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBudgetTInitErrorTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetBudgetFlags();

    // Act: 50ms × 1.5 = 75ms, 76 > 75 → Error
    Sub->TestingEvaluateTInitBudget(76.0);

    // Assert
    TestTrue (TEXT("Error(76ms): Error flag 설정"),          Sub->TestingWasErrorTriggered());
    TestFalse(TEXT("Error(76ms): Warning flag 미설정"),      Sub->TestingWasWarningTriggered());
    TestFalse(TEXT("Error(76ms): Fatal flag 미설정"),        Sub->TestingWasFatalTriggered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 4 — T_init Fatal threshold (101ms → Fatal flag 설정)
//
// Budget 50ms × FatalMul 2.0 = 100ms. 101 > 100 → Fatal 분기.
// Error/Warning flag 미설정 (else-if — Fatal이 먼저 진입).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBudgetTInitFatalTest,
    "MossBaby.Data.Pipeline.Budget.TInitFatal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBudgetTInitFatalTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetBudgetFlags();

    // Act: 50ms × 2.0 = 100ms, 101 > 100 → Fatal threshold
    Sub->TestingEvaluateTInitBudget(101.0);

    // Assert
    TestTrue (TEXT("Fatal(101ms): Fatal flag 설정"),         Sub->TestingWasFatalTriggered());
    TestFalse(TEXT("Fatal(101ms): Error flag 미설정"),       Sub->TestingWasErrorTriggered());
    TestFalse(TEXT("Fatal(101ms): Warning flag 미설정"),     Sub->TestingWasWarningTriggered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 5 — Catalog Card Warn (210 → Warning flag 설정)
//
// MaxCatalogSizeCards=200, WarnMul=1.05 → WarnThresh = 200 * 1.05 = 210.0.
// Count 210 >= 210 → Warning 분기.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBudgetCatalogCardWarnTest,
    "MossBaby.Data.Pipeline.Budget.CatalogCardWarn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBudgetCatalogCardWarnTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetBudgetFlags();

    // Act: 200 * 1.05 = 210, Count 210 >= 210 → Warning
    Sub->TestingCheckCatalogSize(TEXT("Card"), 210);

    // Assert
    TestTrue (TEXT("CardWarn(210): Warning flag 설정"),      Sub->TestingWasWarningTriggered());
    TestFalse(TEXT("CardWarn(210): Error flag 미설정"),      Sub->TestingWasErrorTriggered());
    TestFalse(TEXT("CardWarn(210): Fatal flag 미설정"),      Sub->TestingWasFatalTriggered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 6 — Catalog Card Error (301 → Error flag 설정)
//
// MaxCatalogSizeCards=200, ErrorMul=1.5 → ErrorThresh = 200 * 1.5 = 300.0.
// Count 301 >= 300 → Error 분기.
// Warning flag도 Error 분기에서 출력하지만 bInitWarningTriggered 미설정
// (else-if 분기 — Error가 먼저 진입, Warning 개별 set 로직 없음).
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBudgetCatalogCardErrorTest,
    "MossBaby.Data.Pipeline.Budget.CatalogCardError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBudgetCatalogCardErrorTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetBudgetFlags();

    // Act: 200 * 1.5 = 300, 301 >= 300 → Error
    Sub->TestingCheckCatalogSize(TEXT("Card"), 301);

    // Assert
    TestTrue (TEXT("CardError(301): Error flag 설정"),       Sub->TestingWasErrorTriggered());
    TestFalse(TEXT("CardError(301): Fatal flag 미설정"),     Sub->TestingWasFatalTriggered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 7 — Catalog Card Fatal threshold (401 → Fatal flag 설정)
//
// MaxCatalogSizeCards=200, FatalMul=2.0 → FatalThresh = 200 * 2.0 = 400.0.
// Count 401 >= 400 → Fatal 분기.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBudgetCatalogCardFatalTest,
    "MossBaby.Data.Pipeline.Budget.CatalogCardFatal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBudgetCatalogCardFatalTest::RunTest(const FString& Parameters)
{
    // Arrange
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    Sub->TestingResetBudgetFlags();

    // Act: 200 * 2.0 = 400, 401 >= 400 → Fatal threshold
    Sub->TestingCheckCatalogSize(TEXT("Card"), 401);

    // Assert
    TestTrue (TEXT("CardFatal(401): Fatal flag 설정"),       Sub->TestingWasFatalTriggered());

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// 테스트 8 — GetCatalogStats 포맷 (빈 카탈로그 → "Pipeline CPU-side" + "Card=0" 등)
//
// 카탈로그 주입 없이(CardTable=nullptr, DreamRegistry 비어 있음, FormRegistry 비어 있음)
// GetCatalogStats 호출 시 포맷 문자열에 필수 키워드 포함 여부 확인.
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBudgetGetCatalogStatsFormatTest,
    "MossBaby.Data.Pipeline.Budget.GetCatalogStatsFormat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBudgetGetCatalogStatsFormatTest::RunTest(const FString& Parameters)
{
    // Arrange: 빈 카탈로그 상태 (CardTable=nullptr, registries 비어 있음)
    UDataPipelineSubsystem* Sub = NewObject<UDataPipelineSubsystem>();
    TestNotNull(TEXT("DataPipelineSubsystem 인스턴스 생성 성공"), Sub);
    if (!Sub) { return false; }

    // Act
    const FString Stats = Sub->GetCatalogStats();

    // Assert: 필수 키워드 포함 확인
    TestTrue(TEXT("Stats에 'Pipeline CPU-side' 포함"), Stats.Contains(TEXT("Pipeline CPU-side")));
    TestTrue(TEXT("Stats에 'Card=0' 포함"),             Stats.Contains(TEXT("Card=0")));
    TestTrue(TEXT("Stats에 'Dream=0' 포함"),            Stats.Contains(TEXT("Dream=0")));
    TestTrue(TEXT("Stats에 'Form=0' 포함"),             Stats.Contains(TEXT("Form=0")));

    return true;
}

#endif // WITH_AUTOMATION_TESTS
