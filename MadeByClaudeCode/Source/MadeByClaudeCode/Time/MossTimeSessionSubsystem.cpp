// Copyright Moss Baby
//
// MossTimeSessionSubsystem.cpp — Time & Session System 메인 서브시스템 구현
//
// Story-003: UMossTimeSessionSubsystem 뼈대 (스텁 구현)
// Story 1-14: Between-session Classifier Rules 1-4 + Formula 4/5 실제 구현
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/time-session-system.md
//
// Story 005에서 In-session TickInSession 1Hz 구현 예정.
// Story 1-7에서 TriggerSaveForNarrative SaveSubsystem 연동 구현 예정.
//
// ADR-0001 준수:
//   FDateTime::UtcNow() / FPlatformTime::Seconds() 직접 호출 금지.
//   모든 시간 접근은 Clock->GetUtcNow() / Clock->GetMonotonicSec() 경유.

#include "Time/MossTimeSessionSubsystem.h"
#include "Settings/MossTimeSessionSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogMossTime, Log, All);

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
    // Clock null guard — SetClockSource 미호출 시 안전 fallback
    if (!Clock.IsValid())
    {
        UE_LOG(LogMossTime, Warning, TEXT("Clock source not injected — HOLD_LAST_TIME fallback"));
        return ETimeAction::HOLD_LAST_TIME;
    }

    // Rule 1 — FIRST_RUN
    if (!PrevRecord)
    {
        CurrentRecord.SessionUuid = FGuid::NewGuid();
        CurrentRecord.DayIndex = 1;
        CurrentRecord.LastWallUtc = Clock->GetUtcNow();
        CurrentRecord.LastMonotonicSec = Clock->GetMonotonicSec();
        OnTimeActionResolved.Broadcast(ETimeAction::START_DAY_ONE);
        return ETimeAction::START_DAY_ONE;
    }

    CurrentRecord = *PrevRecord;

    // E13 corruption clamp (sanity check before classification)
    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    CurrentRecord.DayIndex = FMath::Clamp(CurrentRecord.DayIndex, 1, Settings->GameDurationDays);

    const FDateTime Now = Clock->GetUtcNow();
    const FTimespan WallDelta = Now - PrevRecord->LastWallUtc;
    const double WallDeltaSec = WallDelta.GetTotalSeconds();

    // Rule 2 — BACKWARD_GAP_REJECT (유일하게 시각 거부)
    if (WallDeltaSec < 0.0)
    {
        OnTimeActionResolved.Broadcast(ETimeAction::HOLD_LAST_TIME);
        return ETimeAction::HOLD_LAST_TIME; // LastWallUtc 갱신 없음
    }

    // Rule 3 — LONG_GAP_SILENT (> 21일)
    const double GameDurationSec = Settings->GameDurationDays * 86400.0;
    if (WallDeltaSec > GameDurationSec)
    {
        CurrentRecord.DayIndex = Settings->GameDurationDays; // clamp 21
        OnTimeActionResolved.Broadcast(ETimeAction::LONG_GAP_SILENT);
        OnFarewellReached.Broadcast(EFarewellReason::LongGapAutoFarewell);
        return ETimeAction::LONG_GAP_SILENT;
    }

    // Rule 4 — ACCEPTED_GAP (ADR-0001: Forward 경과도 여기로 silent 수용)
    // Formula 4: DayIndex 전진
    const int32 DaysElapsed = FMath::FloorToInt32(WallDeltaSec / 86400.0);
    const int32 NewDayIndex = FMath::Clamp(PrevRecord->DayIndex + DaysElapsed, 1, Settings->GameDurationDays);
    CurrentRecord.DayIndex = NewDayIndex;
    CurrentRecord.LastWallUtc = Now;

    // Formula 5 — Narrative threshold (strict `>` + cap)
    const double NarrativeThresholdSec = Settings->NarrativeThresholdHours * 3600.0;
    const bool bCrossThreshold = WallDeltaSec > NarrativeThresholdSec;
    const bool bUnderCap = PrevRecord->NarrativeCount < Settings->NarrativeCapPerPlaythrough;

    if (bCrossThreshold && bUnderCap)
    {
        IncrementNarrativeCountAndSave();
        OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_WITH_NARRATIVE);
        return ETimeAction::ADVANCE_WITH_NARRATIVE;
    }
    if (bCrossThreshold && !bUnderCap)
    {
        UE_LOG(LogMossTime, Verbose, TEXT("cap reached, narrative suppressed"));
    }
    OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_SILENT);
    return ETimeAction::ADVANCE_SILENT;
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
