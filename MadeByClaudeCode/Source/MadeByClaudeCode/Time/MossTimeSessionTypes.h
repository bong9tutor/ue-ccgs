// Copyright Moss Baby
//
// MossTimeSessionTypes.h — Time & Session System 공용 열거형 정의
//
// Story-003: UMossTimeSessionSubsystem 뼈대
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// ADR-0011: Tuning Knob UDeveloperSettings 채택 참조
// GDD: design/gdd/time-session-system.md §Core Rules / §States / §Actions
//
// AC 커버리지:
//   AC-2: ETimeAction 정의 (CODE_REVIEW — 헤더 컴파일 성공)
//   AC-3: EFarewellReason 정의 (CODE_REVIEW — 헤더 컴파일 성공)
//   AC-4: EBetweenSessionClass + EInSessionClass 정의 (CODE_REVIEW — 헤더 컴파일 성공)
//
// ADR-0001 금지 패턴:
//   변수·함수·enum 이름에 tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 사용 금지.
//   CI grep이 자동으로 차단함.

#pragma once
#include "CoreMinimal.h"
#include "MossTimeSessionTypes.generated.h"

/**
 * ETimeAction — Classifier가 8-Rule을 적용한 결과 행동.
 *
 * Story 004에서 Between-session Classifier가 이 값을 Subsystem에 전달하고,
 * Subsystem이 해당 행동(날짜 전진, 내러티브 트리거, 홀드 등)을 실행한다.
 * 각 Rule의 세부 매핑은 GDD §Core Rules §Rule 1~8 참조.
 */
UENUM(BlueprintType)
enum class ETimeAction : uint8
{
    /** Rule 1 (FirstLaunch) — DayIndex=1 초기화, 내러티브 없이 세션 시작. */
    START_DAY_ONE     UMETA(DisplayName = "Start Day One"),

    /** Rule 2/4 — 날짜는 전진하지만 내러티브 이벤트를 발화하지 않는다. */
    ADVANCE_SILENT    UMETA(DisplayName = "Advance Silent"),

    /**
     * Rule 3 (AcceptedGap) — NarrativeCap 내에서 날짜를 전진하고 내러티브 이벤트를 발화.
     * NarrativeCapPerPlaythrough 강제. Dream Trigger(#9)와 무관한 시스템 보이스.
     */
    ADVANCE_WITH_NARRATIVE  UMETA(DisplayName = "Advance With Narrative"),

    /** Rule 5 (BackwardGap) — 날짜 전진 없이 마지막 저장 시각을 유지한다. */
    HOLD_LAST_TIME    UMETA(DisplayName = "Hold Last Time"),

    /**
     * Rule 6 (LongGap) — DayIndex를 GameDurationDays로 이동하되 내러티브는 발화하지 않는다.
     * OnFarewellReached(LongGapAutoFarewell)와 동시 발행.
     */
    LONG_GAP_SILENT   UMETA(DisplayName = "Long Gap Silent"),

    /** Rule 8 (InSessionDiscrepancy) — 분류 불가. 로그만 남기고 아무 행동도 취하지 않는다. */
    LOG_ONLY          UMETA(DisplayName = "Log Only"),
};

/**
 * EFarewellReason — Farewell(작별) 이벤트 발생 원인.
 *
 * GDD AC FAREWELL_LONG_GAP_SILENT 요구사항에 따라 두 원인이 분리된다.
 * FOnFarewellReached 델리게이트의 파라미터로 사용된다.
 * Time은 enum 의미만 규정 — 연출·BGM·메시지·감정 톤 차등은 Farewell Archive GDD(#17) 책임.
 */
UENUM(BlueprintType)
enum class EFarewellReason : uint8
{
    /** DayIndex가 GameDurationDays(기본 21일)에 도달하여 정상 완료된 경우. */
    NormalCompletion      UMETA(DisplayName = "Normal Completion"),

    /**
     * WallDelta > GameDurationDays일로 LONG_GAP_SILENT 분류. 자동 작별.
     * 플레이어 선택 없음. 톤 차등은 Farewell Archive GDD 책임.
     */
    LongGapAutoFarewell   UMETA(DisplayName = "Long Gap Auto Farewell"),
};

/**
 * EBetweenSessionClass — 이전 세션 종료와 현재 세션 시작 사이의 간격 분류.
 *
 * Story 004에서 Between-session Classifier(Rule 1~4)가 이 분류를 계산하여 ETimeAction을 결정.
 * GDD §Core Rules §Rule naming 원문 보존.
 */
UENUM(BlueprintType)
enum class EBetweenSessionClass : uint8
{
    /** Rule 1 — 저장된 이전 세션 레코드가 없는 최초 실행. */
    FirstLaunch     UMETA(DisplayName = "First Launch"),

    /**
     * 이전 세션과 동일한 달력 날짜 내에서의 재접속.
     * WallDelta가 매우 작아 날짜 전진이 불필요한 경우.
     */
    SameDaySession  UMETA(DisplayName = "Same Day Session"),

    /** 허용 범위 내의 정상 간격. NarrativeCap + WallDelta 조건으로 ADVANCE 분기. */
    AcceptedGap     UMETA(DisplayName = "Accepted Gap"),

    /**
     * WallDelta > GameDurationDays — 초장기 미실행.
     * 즉시 FarewellExit 전환 (LONG_GAP_SILENT 발행).
     */
    LongGap         UMETA(DisplayName = "Long Gap"),

    /** Rule 2 — Wall clock이 이전 저장 시점보다 과거인 역행 간격. 행동 보류(HOLD_LAST_TIME). */
    BackwardGap     UMETA(DisplayName = "Backward Gap"),
};

/**
 * EInSessionClass — 단일 세션 내에서의 시각 변화 분류.
 *
 * Story 005에서 In-session Classifier(Rule 5~8)가 이 분류를 계산하여 행동을 결정한다.
 * GDD §Core Rules §Rule naming 원문 보존.
 */
UENUM(BlueprintType)
enum class EInSessionClass : uint8
{
    /** Rule 5 — |WallΔ − MonoΔ| ≤ DefaultEpsilonSec. 정상 동기. */
    Normal       UMETA(DisplayName = "Normal"),

    /** Rule 7 — DefaultEpsilonSec < |WallΔ − MonoΔ| ≤ InSessionToleranceMinutes. NTP/DST 흡수. */
    MinorShift   UMETA(DisplayName = "Minor Shift"),

    /** Rule 8 — |WallΔ − MonoΔ| > InSessionToleranceMinutes. 진단 로그만. */
    Discrepancy  UMETA(DisplayName = "Discrepancy"),

    /** Rule 6 — MonoDelta < SuspendMonotonicThresholdSec AND WallDelta > SuspendWallThresholdSec. OS 절전 재개. */
    Suspended    UMETA(DisplayName = "Suspended"),
};
