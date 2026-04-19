// Copyright Moss Baby
//
// MossTimeSessionSubsystem.h — Time & Session System 메인 서브시스템 선언
//
// Story-003: UMossTimeSessionSubsystem 뼈대 + 3 delegates
// Story 1-14: Between-session Classifier Rules 1-4 + Formula 4/5
// Story 1-18: In-session 1Hz FTSTicker + Rules 5-8
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/time-session-system.md §Core Rules / §Delegates / §Lifecycle
//
// AC 커버리지:
//   AC-1: UMossTimeSessionSubsystem 뼈대 (Initialize/Deinitialize + FSessionRecord 멤버
//          + clock source 의존성 주입 API) — CODE_REVIEW + 헤더 컴파일 성공
//   AC-6: 3 Delegate 선언
//          FOnTimeActionResolved(ETimeAction), FOnDayAdvanced(int32), FOnFarewellReached(EFarewellReason)
//          — CODE_REVIEW
//   Story 1-18: MONOTONIC_DELTA_CLAMP_ZERO, NORMAL_TICK_IN_SESSION, MINOR_SHIFT_NTP,
//          MINOR_SHIFT_DST, IN_SESSION_DISCREPANCY_LOG_ONLY, RULE_PRECEDENCE_FIRST_MATCH_WINS,
//          SESSION_COUNT_TODAY_RESET_CONTRACT — AUTOMATED
//
// IMossClockSource 의존성 주입:
//   production → FRealClockSource
//   테스트    → FMockClockSource
//   ADR-0001: 모든 시간 API(FDateTime::UtcNow / FPlatformTime::Seconds)는
//             FRealClockSource 내부에서만 허용. 이 클래스는 Clock->Get* 경유만 사용.
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지. CI grep이 자동 차단.
//
// FTSTicker 등록 (Story 1-18):
//   engine-level 1Hz tick — SetGamePaused(true) 및 focus loss에 영향 없음.
//   FTimerManager 사용 금지 (pause 시 중단됨).

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Time/MossClockSource.h"
#include "Time/SessionRecord.h"
#include "Time/MossTimeSessionTypes.h"
#include "MossTimeSessionSubsystem.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Delegate 선언 (GDD §Actions / §Dep #8 Card / §Dep #17 Farewell 참조)
//
// 1. FOnTimeActionResolved  — ETimeAction 신호 (GSM 소비)
// 2. FOnDayAdvanced         — 날짜 전진 알림 (Card / Dream Trigger 소비)
// 3. FOnFarewellReached     — Farewell 진입 알림 (Farewell Archive 소비)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Time & Session이 분류 완료 시 발행하는 Action 신호.
 * GSM(Game State Machine #5)이 구독하여 게임플레이 상태 전환에 사용.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTimeActionResolved, ETimeAction);

/**
 * DayIndex 전진 알림.
 * Card System(#8)이 구독하여 새 3장 덱 재생성.
 * Dream Trigger(#9)가 구독하여 희소성 재평가.
 * ADVANCE_SILENT / ADVANCE_WITH_NARRATIVE 완료 직후 발행.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnDayAdvanced, int32);

/**
 * Farewell 진입 알림.
 * Farewell Archive(#17)가 구독하여 연출·BGM·메시지 처리.
 * Time은 enum 의미만 발행 — 톤 차등은 Farewell Archive GDD 책임.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFarewellReached, EFarewellReason);

// ─────────────────────────────────────────────────────────────────────────────
// UMossTimeSessionSubsystem
// ─────────────────────────────────────────────────────────────────────────────

/**
 * UMossTimeSessionSubsystem — Moss Baby의 21일 실시간 인프라 계층.
 *
 * UGameInstanceSubsystem 기반. GameInstance 직접 수정 금지 — 별도 클래스.
 * 두 시계(wall clock + monotonic)를 결합해 8개 분류 → 6개 Action 신호로 변환.
 *
 * Clock source 의존성 주입(SetClockSource)으로 테스트에서 FMockClockSource 교체 가능.
 *
 * Story 003(이 파일): 뼈대 선언 + 3 delegates 정의.
 * Story 004+: ClassifyOnStartup 실제 8-Rule Classifier 구현 예정.
 */
UCLASS()
class MADEBYCLAUDECODE_API UMossTimeSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── Lifecycle (UGameInstanceSubsystem overrides) ──────────────────────────

    /**
     * Clock source 준비 확인. Super 호출 필수.
     * Clock은 외부 주입(SetClockSource) 대기 상태로 시작.
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * FTSTicker handle 해제 + Clock TSharedPtr 해제 + Super 호출.
     * Story 1-18: TickHandle 해제 추가.
     */
    virtual void Deinitialize() override;

    // ── Clock Injection (테스트 seam) ─────────────────────────────────────────

    /**
     * 시간 입력 seam 교체.
     * production → FRealClockSource, 테스트 → FMockClockSource.
     * ADR-0001: 모든 시간 API 호출은 InClock->Get* 경유만 허용.
     * @param InClock  교체할 clock source.
     */
    void SetClockSource(TSharedPtr<IMossClockSource> InClock);

    // ── Public API — Story 004+ 구현 예정 (현재 스텁) ─────────────────────────

    /**
     * Between-session 분류기 (Rule 1~4). 앱 시작 시 1회 실행.
     * Story 003 스텁: PrevRecord == nullptr → START_DAY_ONE,
     *                 유효 레코드 → HOLD_LAST_TIME (Story 004 실제 로직 전 안전한 스텁).
     * Story 004에서 실제 Classifier 로직 구현.
     * @param PrevRecord  이전 세션 레코드. 최초 실행 시 nullptr.
     * @return 분류 결과 ETimeAction.
     */
    ETimeAction ClassifyOnStartup(const FSessionRecord* PrevRecord);

    // ── Narrative Count (Save/Load 연동 단위 — Story 1-7 연동) ───────────────

    /**
     * NarrativeCount를 1 증가시키고 저장을 트리거하는 atomic 단위.
     * 내부적으로 IncrementNarrativeCount() + TriggerSaveForNarrative() 호출.
     * Story 1-7 SaveSubsystem 연동 완료 전까지 TriggerSaveForNarrative는 no-op.
     */
    void IncrementNarrativeCountAndSave();

    // ── State Accessors ───────────────────────────────────────────────────────

    /** 현재 세션 레코드 조회 (read-only). */
    const FSessionRecord& GetCurrentRecord() const { return CurrentRecord; }

#if WITH_AUTOMATION_TESTS
    /** [테스트 전용] 이전 세션 레코드를 CurrentRecord에 직접 주입한다. */
    void TestingInjectPrevRecord(const FSessionRecord& Record) { CurrentRecord = Record; }

    /** [테스트 전용] CurrentRecord를 const 참조로 반환한다. */
    const FSessionRecord& TestingGetCurrentRecord() const { return CurrentRecord; }
#endif

    // ── Delegates ────────────────────────────────────────────────────────────

    /**
     * Action 신호 delegate. GSM이 구독하여 게임플레이 상태 전환.
     * ETimeAction 값을 파라미터로 전달.
     */
    FOnTimeActionResolved OnTimeActionResolved;

    /**
     * 날짜 전진 알림. Card System(#8) + Dream Trigger(#9) 구독.
     * 전진한 DayIndex(int32)를 파라미터로 전달.
     */
    FOnDayAdvanced OnDayAdvanced;

    /**
     * Farewell 진입 알림. Farewell Archive(#17) 구독.
     * EFarewellReason을 파라미터로 전달.
     */
    FOnFarewellReached OnFarewellReached;

private:
    // ── Internal Helpers ──────────────────────────────────────────────────────

    /**
     * NarrativeCount를 1 증가. CurrentRecord.NarrativeCount += 1.
     * IncrementNarrativeCountAndSave()의 내부 단계.
     */
    void IncrementNarrativeCount();

    /**
     * 내러티브 발화 후 저장 트리거.
     * Story 1-7 SaveSubsystem 연동 후 구현 — 현재 no-op.
     */
    void TriggerSaveForNarrative();

    // ── Story 1-18: In-session 1Hz tick (FTSTicker-based, pause/focus 무관) ──

    /**
     * FTSTicker 등록 콜백. engine-level에서 1Hz 호출됨.
     * SetGamePaused(true) 및 focus loss에 영향 없음 (GDD §Window Focus).
     * Rule 5-8 분류 후 ETimeAction 신호 발행. true 반환으로 ticker 유지.
     * ADR-0001: 의심 분기 없음. 모든 경로는 정상 경과와 동일 처리.
     * @param DeltaTime  FTSTicker가 전달하는 elapsed time — 분류 로직 미사용 (직접 clock 조회).
     * @return true (ticker 계속 유지).
     */
    bool TickInSession(float DeltaTime);

    /**
     * SUSPEND_RESUME 분류(Rule 6) 또는 LONG_GAP 복귀 시 DayIndex 전진 처리.
     * Formula 3: DaysElapsed = floor(WallDeltaSec / 86400). DayIndex += DaysElapsed, clamp [1, GameDurationDays].
     * DayIndex 변화 시 SessionCountToday 0 리셋 + OnDayAdvanced 발행.
     * @param WallDeltaSec  wall clock 경과 초.
     */
    void AdvanceDayIfNeeded(double WallDeltaSec);

    // ── State ─────────────────────────────────────────────────────────────────

    /** 시간 입력 seam. production: FRealClockSource, 테스트: FMockClockSource. */
    TSharedPtr<IMossClockSource> Clock;

    /** 현재 세션 누적 레코드. Save/Load가 직렬화 소유권 보유. */
    FSessionRecord CurrentRecord;

    /**
     * FTSTicker 등록 핸들. Deinitialize()에서 해제.
     * Reset() 후 IsValid() == false 보장.
     */
    FTSTicker::FDelegateHandle TickHandle;

#if WITH_AUTOMATION_TESTS
    // ── 테스트 훅 (Shipping 빌드 미포함) ────────────────────────────────────

    /** [테스트 전용] TickInSession을 직접 호출한다. */
    bool TestingTickInSession(float DeltaTime = 1.0f) { return TickInSession(DeltaTime); }

    /** [테스트 전용] FTSTicker에 TickHandle이 등록되어 있는지 반환한다. */
    bool TestingIsTickRegistered() const { return TickHandle.IsValid(); }

    /** [테스트 전용] TickHandle을 강제 해제한다 (double-remove 방지 테스트용). */
    void TestingUnregisterTick()
    {
        if (TickHandle.IsValid())
        {
            FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
            TickHandle.Reset();
        }
    }
#endif
};
