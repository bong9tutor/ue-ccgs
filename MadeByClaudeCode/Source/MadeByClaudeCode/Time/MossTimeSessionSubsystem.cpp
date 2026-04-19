// Copyright Moss Baby
//
// MossTimeSessionSubsystem.cpp — Time & Session System 메인 서브시스템 최소 구현
//
// Story-003: UMossTimeSessionSubsystem 뼈대 (스텁 구현)
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/time-session-system.md
//
// Story 004에서 ClassifyOnStartup 실제 8-Rule Classifier 로직 구현 예정.
// Story 005에서 In-session TickInSession 1Hz 구현 예정.
// Story 1-7에서 TriggerSaveForNarrative SaveSubsystem 연동 구현 예정.
//
// ADR-0001 준수:
//   FDateTime::UtcNow() / FPlatformTime::Seconds() 직접 호출 금지.
//   모든 시간 접근은 Clock->GetUtcNow() / Clock->GetMonotonicSec() 경유.

#include "Time/MossTimeSessionSubsystem.h"

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UMossTimeSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Clock은 외부 SetClockSource() 주입 대기.
    // Story 004: OnPostWorldInitialization 구독 추가 예정.
    // Story 003: no-op (뼈대 단계).
}

void UMossTimeSessionSubsystem::Deinitialize()
{
    // Story 004: FTSTicker handle 해제 + FWorldDelegates 구독 해제 추가 예정.
    // Story 003: Clock TSharedPtr 해제.
    Clock.Reset();

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Clock Injection
// ─────────────────────────────────────────────────────────────────────────────

void UMossTimeSessionSubsystem::SetClockSource(TSharedPtr<IMossClockSource> InClock)
{
    Clock = InClock;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API (스텁)
// ─────────────────────────────────────────────────────────────────────────────

ETimeAction UMossTimeSessionSubsystem::ClassifyOnStartup(const FSessionRecord* PrevRecord)
{
    // Story 004에서 실제 8-Rule Classifier 로직 구현.
    // Story 003 안전 스텁:
    //   - PrevRecord == nullptr → 최초 실행 → START_DAY_ONE
    //   - 유효 레코드 존재 → 분류 보류 → HOLD_LAST_TIME
    if (PrevRecord == nullptr)
    {
        return ETimeAction::START_DAY_ONE;
    }

    // Story 004 이전 임시 반환. 실제 Rule 2~4 분류 미적용.
    return ETimeAction::HOLD_LAST_TIME;
}

// ─────────────────────────────────────────────────────────────────────────────
// Narrative Count (Save/Load 연동 단위)
// ─────────────────────────────────────────────────────────────────────────────

void UMossTimeSessionSubsystem::IncrementNarrativeCountAndSave()
{
    IncrementNarrativeCount();
    TriggerSaveForNarrative();
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal Helpers
// ─────────────────────────────────────────────────────────────────────────────

void UMossTimeSessionSubsystem::IncrementNarrativeCount()
{
    CurrentRecord.NarrativeCount += 1;
}

void UMossTimeSessionSubsystem::TriggerSaveForNarrative()
{
    // Story 1-7 SaveSubsystem 연동 후 구현.
    // 현재 no-op.
}
