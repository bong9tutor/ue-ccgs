// Copyright Moss Baby — Example Automation Tests
//
// Purpose: UE Automation Framework scaffold — 14 GDD Formula 검증 예시.
// Framework가 정상 작동함을 확인하는 최소한의 test 3건.
// 실제 구현이 진행되면 각 시스템별 테스트 파일로 분리 (Moss[System]Tests.cpp).
//
// Run headless:
//   UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
//     -ExecCmds="Automation RunTests MossBaby.; Quit" \
//     -nullrhi -nosound -log -unattended

#include "Misc/AutomationTest.h"
#include "Containers/Map.h"

// ─────────────────────────────────────────────────────────────────────────────
// Time & Session — Formula 2: MonoDelta = max(0, M1 - M0)
// AC: MONOTONIC_DELTA_CLAMP_ZERO
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossTimeMonoDeltaTest,
    "MossBaby.Time.Formula2MonoDelta",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossTimeMonoDeltaTest::RunTest(const FString& Parameters)
{
    // 정상 경우: M1 > M0
    {
        const double M1 = 720.5;
        const double M0 = 600.0;
        const double MonoDelta = FMath::Max(0.0, M1 - M0);
        TestEqual(TEXT("정상 경우 120.5s"), MonoDelta, 120.5);
    }

    // 앱 재시작 경우: M1 < M0 (부팅 리셋)
    {
        const double M1 = 0.8;
        const double M0 = 600.0;
        const double MonoDelta = FMath::Max(0.0, M1 - M0);
        TestEqual(TEXT("음수 클램프 0"), MonoDelta, 0.0);
    }

    // 동일 시각 (세션 간격 0)
    {
        const double M1 = 500.0;
        const double M0 = 500.0;
        const double MonoDelta = FMath::Max(0.0, M1 - M0);
        TestEqual(TEXT("동일 시각 0"), MonoDelta, 0.0);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Growth Accumulation Engine — Formula 2: VectorNorm
// AC: AC-GAE-13
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossGrowthVectorNormTest,
    "MossBaby.Growth.Formula2VectorNorm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossGrowthVectorNormTest::RunTest(const FString& Parameters)
{
    // Given: TagVector = {Season.Spring: 3.0, Emotion.Calm: 2.0, Element.Water: 1.0}
    TMap<FName, float> TagVector;
    TagVector.Add(TEXT("Season.Spring"), 3.0f);
    TagVector.Add(TEXT("Emotion.Calm"), 2.0f);
    TagVector.Add(TEXT("Element.Water"), 1.0f);

    // When: F2 VectorNorm 계산
    float TotalWeight = 0.0f;
    for (const auto& Pair : TagVector) { TotalWeight += Pair.Value; }

    TestEqual(TEXT("TotalWeight"), TotalWeight, 6.0f);

    TMap<FName, float> VNorm;
    if (TotalWeight > KINDA_SMALL_NUMBER)
    {
        for (const auto& Pair : TagVector)
        {
            VNorm.Add(Pair.Key, Pair.Value / TotalWeight);
        }
    }

    // Then: V_norm 값 검증 (±1e-5)
    TestEqual(TEXT("Season.Spring V_norm"), VNorm[TEXT("Season.Spring")], 0.500f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("Emotion.Calm V_norm"), VNorm[TEXT("Emotion.Calm")], 0.333333f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("Element.Water V_norm"), VNorm[TEXT("Element.Water")], 0.166666f, KINDA_SMALL_NUMBER);

    // 합계 = 1.0 검증
    float Sum = 0.0f;
    for (const auto& Pair : VNorm) { Sum += Pair.Value; }
    TestEqual(TEXT("VNorm 합계 1.0"), Sum, 1.0f, KINDA_SMALL_NUMBER);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Game State Machine — Formula 1: SmoothStep Lerp
// AC: AC-GSM-07
// ─────────────────────────────────────────────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossGSMSmoothStepTest,
    "MossBaby.GSM.Formula1SmoothStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossGSMSmoothStepTest::RunTest(const FString& Parameters)
{
    // SmoothStep: ease(x) = x * x * (3 - 2x)

    // Boundary values
    TestEqual(TEXT("ease(0.0) = 0.0"), FMath::SmoothStep(0.0f, 1.0f, 0.0f), 0.0f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("ease(0.5) = 0.5"), FMath::SmoothStep(0.0f, 1.0f, 0.5f), 0.5f, KINDA_SMALL_NUMBER);
    TestEqual(TEXT("ease(1.0) = 1.0"), FMath::SmoothStep(0.0f, 1.0f, 1.0f), 1.0f, KINDA_SMALL_NUMBER);

    // Overshoot 방어 — [0, 1] 범위 보장
    const float Ease025 = FMath::SmoothStep(0.0f, 1.0f, 0.25f);
    TestTrue(TEXT("ease(0.25) ∈ [0, 1]"), Ease025 >= 0.0f && Ease025 <= 1.0f);

    const float Ease075 = FMath::SmoothStep(0.0f, 1.0f, 0.75f);
    TestTrue(TEXT("ease(0.75) ∈ [0, 1]"), Ease075 >= 0.0f && Ease075 <= 1.0f);

    // Monotonic 증가 (단조성 — Lerp의 기본 요구사항)
    TestTrue(TEXT("ease(0.25) < ease(0.75)"), Ease025 < Ease075);

    // Waiting→Dream 전환 예시: V₀=2800K, V₁=4200K, t=0.675s, D=1.35s
    const float T_over_D = 0.675f / 1.35f;  // 0.5
    const float Lerp_mid = 2800.0f + (4200.0f - 2800.0f) * FMath::SmoothStep(0.0f, 1.0f, T_over_D);
    TestEqual(TEXT("Waiting→Dream 중간값 3500K"), Lerp_mid, 3500.0f, 1.0f);

    return true;
}
