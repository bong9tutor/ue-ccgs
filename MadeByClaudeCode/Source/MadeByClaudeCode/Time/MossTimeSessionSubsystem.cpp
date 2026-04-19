// Copyright Moss Baby
//
// MossTimeSessionSubsystem.cpp — Time & Session System 메인 서브시스템 구현
//
// Story-003: UMossTimeSessionSubsystem 뼈대 (스텁 구현)
// Story 1-14: Between-session Classifier Rules 1-4 + Formula 4/5 실제 구현
// Story 1-18: In-session 1Hz FTSTicker + Rules 5-8 + Formula 2/3
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/time-session-system.md
//
// Story 1-7에서 TriggerSaveForNarrative SaveSubsystem 연동 구현 예정.
//
// ADR-0001 준수:
//   FDateTime::UtcNow() / FPlatformTime::Seconds() 직접 호출 금지.
//   모든 시간 접근은 Clock->GetUtcNow() / Clock->GetMonotonicSec() 경유.
//
// FTSTicker 선택 근거 (Story 1-18):
//   FTimerManager는 SetGamePaused(true) 시 중단됨 — 21일 실시간 추적에 부적합.
//   FTSTicker::GetCoreTicker()는 engine-level — pause/focus loss 무관.
//   ADR-0001 TR-time-004 요구사항 충족.

#include "Time/MossTimeSessionSubsystem.h"
#include "Settings/MossTimeSessionSettings.h"
#include "SaveLoad/MossSaveLoadSubsystem.h"
#include "SaveLoad/MossSaveData.h"

DEFINE_LOG_CATEGORY_STATIC(LogMossTime, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UMossTimeSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Clock은 외부 SetClockSource() 주입 대기.
    // Story 004: OnPostWorldInitialization 구독 추가 예정.

    // Story 1-18: 1Hz FTSTicker 등록 (engine-level — pause/focus loss 무관).
    // FTimerManager 금지: pause 시 중단됨 (ADR 요구사항 TR-time-004).
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UMossTimeSessionSubsystem::TickInSession),
        1.0f); // 1Hz
}

void UMossTimeSessionSubsystem::Deinitialize()
{
    // Story 1-18: FTSTicker handle 해제. TickHandle 유효 여부 확인 후 제거.
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }

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
    // Story 1-20: SaveSubsystem 연동 구현. no-op 스텁 교체.
    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogMossTime, Warning,
            TEXT("TriggerSaveForNarrative — GameInstance null, save skipped"));
        return;
    }

    UMossSaveLoadSubsystem* Save = GI->GetSubsystem<UMossSaveLoadSubsystem>();
    if (!Save)
    {
        UE_LOG(LogMossTime, Warning,
            TEXT("TriggerSaveForNarrative — SaveLoadSubsystem null, save skipped"));
        return;
    }

    // Sync in-memory SessionRecord into SaveData + async write.
    // ADR-0009: UpdateSessionRecord은 in-memory 동기화만. disk write는 SaveAsync가 담당.
    Save->UpdateSessionRecord(CurrentRecord);
    Save->SaveAsync(ESaveReason::ENarrativeEmitted);
}

// ─────────────────────────────────────────────────────────────────────────────
// Story 1-18: In-session 1Hz Tick (Rules 5-8) + AdvanceDayIfNeeded
// ─────────────────────────────────────────────────────────────────────────────

bool UMossTimeSessionSubsystem::TickInSession(float /*DeltaTime*/)
{
    // Clock null guard — SetClockSource 미호출 시 production 주입 대기.
    if (!Clock.IsValid())
    {
        return true; // ticker 유지
    }

    const FDateTime Now = Clock->GetUtcNow();
    const double MonoNow = Clock->GetMonotonicSec();
    const double WallDeltaSec = (Now - CurrentRecord.LastWallUtc).GetTotalSeconds();

    // Formula 2: MonoDelta = max(0, M₁ − M₀) — 부팅 리셋(MonoNow < LastMonotonicSec) 시 clamp 0
    const double MonoDelta = FMath::Max(0.0, MonoNow - CurrentRecord.LastMonotonicSec);

    const double DiscrepancyAbs = FMath::Abs(WallDeltaSec - MonoDelta);

    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    if (!Settings) { return true; }

    // Rule 6 — SUSPEND_RESUME (first-match-wins: Rule 8보다 앞에 위치)
    // MonoDelta < SuspendMonotonicThresholdSec AND WallDelta > SuspendWallThresholdSec
    // ADR-0001: 의심 분기 없음. OS suspend 기간의 wall clock 경과를 정상 경과로 수용.
    if (MonoDelta < static_cast<double>(Settings->SuspendMonotonicThresholdSec)
        && WallDeltaSec > static_cast<double>(Settings->SuspendWallThresholdSec))
    {
        UE_LOG(LogMossTime, Verbose, TEXT("classification: SUSPEND_RESUME"));
        AdvanceDayIfNeeded(WallDeltaSec);
        OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_SILENT);
        CurrentRecord.LastWallUtc = Now;
        CurrentRecord.LastMonotonicSec = MonoNow;
        return true;
    }

    // Rule 5 — NORMAL_TICK
    // |WallΔ − MonoΔ| ≤ DefaultEpsilonSec → 정상 시계 동기
    if (DiscrepancyAbs <= static_cast<double>(Settings->DefaultEpsilonSec))
    {
        OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_SILENT);
        CurrentRecord.LastWallUtc = Now;
        CurrentRecord.LastMonotonicSec = MonoNow;
        return true;
    }

    // Rule 7 — MINOR_SHIFT (NTP 재동기 / DST 흡수)
    // DefaultEpsilonSec < |Δ| ≤ InSessionToleranceMinutes×60
    const double ToleranceSec = static_cast<double>(Settings->InSessionToleranceMinutes) * 60.0;
    if (DiscrepancyAbs <= ToleranceSec)
    {
        UE_LOG(LogMossTime, Verbose, TEXT("classification: MINOR_SHIFT"));
        OnTimeActionResolved.Broadcast(ETimeAction::ADVANCE_SILENT);
        CurrentRecord.LastWallUtc = Now;
        CurrentRecord.LastMonotonicSec = MonoNow;
        return true;
    }

    // Rule 8 — IN_SESSION_DISCREPANCY (LOG_ONLY)
    // |Δ| > InSessionToleranceMinutes×60 — 진단 로그만 남김.
    // DayIndex 변경 없음, LastWallUtc / LastMonotonicSec 갱신 없음 (다음 tick 재평가).
    // ADR-0001: 의심 분기 없음. forward delta도 동일 처리.
    UE_LOG(LogMossTime, Warning, TEXT("classification: IN_SESSION_DISCREPANCY"));
    OnTimeActionResolved.Broadcast(ETimeAction::LOG_ONLY);
    return true;
}

void UMossTimeSessionSubsystem::AdvanceDayIfNeeded(double WallDeltaSec)
{
    const UMossTimeSessionSettings* Settings = UMossTimeSessionSettings::Get();
    if (!Settings) { return; }

    // Formula 3: DaysElapsed = floor(WallDeltaSec / 86400)
    const int32 DaysElapsed = FMath::FloorToInt32(WallDeltaSec / 86400.0);
    if (DaysElapsed > 0)
    {
        const int32 NewDay = FMath::Clamp(
            CurrentRecord.DayIndex + DaysElapsed, 1, Settings->GameDurationDays);
        if (NewDay != CurrentRecord.DayIndex)
        {
            CurrentRecord.DayIndex = NewDay;
            CurrentRecord.SessionCountToday = 0; // SESSION_COUNT_TODAY_RESET_CONTRACT
            OnDayAdvanced.Broadcast(NewDay);
        }
    }
}
