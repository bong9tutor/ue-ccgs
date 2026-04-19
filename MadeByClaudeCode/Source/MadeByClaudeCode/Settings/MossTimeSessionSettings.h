// Copyright Moss Baby
//
// MossTimeSessionSettings.h — Time & Session System Tuning Knobs
//
// Story-003: UMossTimeSessionSubsystem 뼈대
// ADR-0011: Tuning Knob 노출 방식 — UDeveloperSettings 정식 채택
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// GDD: design/gdd/time-session-system.md §Tuning Knobs (7개 노브)
//
// ADR-0011 §Naming Convention:
//   U[SystemName]Settings : public UDeveloperSettings
//   GetCategoryName() → "Moss Baby"
//   GetSectionName()  → "Time & Session"
//
// ADR-0011 §ConfigRestartRequired convention:
//   - Subsystem Initialize()에서 캐싱하는 knob → ConfigRestartRequired="true"
//   - 매 호출 시점마다 read하는 knob → hot reload 자연 지원 (meta 불필요)
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.
//
// ini 저장 위치: Config/DefaultGame.ini
//   [/Script/MadeByClaudeCode.MossTimeSessionSettings]
//
// 사용 예 (런타임):
//   const auto* S = UMossTimeSessionSettings::Get();
//   int32 Cap = S->NarrativeCapPerPlaythrough;

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MossTimeSessionSettings.generated.h"

/**
 * UMossTimeSessionSettings — Time & Session System 전역 Tuning Knobs
 *
 * Project Settings → Game → Moss Baby → Time & Session 에서 편집.
 * 7개 노브는 GDD §Tuning Knobs 표와 1:1 대응.
 * ADR-0011: 시스템 전역 상수는 UDeveloperSettings, per-instance 데이터는 UPrimaryDataAsset.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Time & Session"))
class MADEBYCLAUDECODE_API UMossTimeSessionSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // ── Day Structure ─────────────────────────────────────────────────────────

    /**
     * 게임 전체 진행 일수.
     * Formula 4(DayIndex clamp 상한), Rule 3(LongGap 판정), Rule 4(AcceptedGap) 기준.
     *
     * 안전 범위: [7, 60]
     *   너무 높으면 → 플레이어 이탈률 증가, 콘텐츠 압박 증가
     *   너무 낮으면 → 감정 아크 부족, Pillar 4 약화
     *
     * ADR-0011: Subsystem Initialize()에서 캐싱 → ConfigRestartRequired
     * MVP: 7 (축약 아크)  /  VS: 21 (기본값)
     */
    UPROPERTY(Config, EditAnywhere, Category="Day Structure",
              meta=(ClampMin="7", ClampMax="60", ConfigRestartRequired="true",
                    ToolTip="게임 전체 진행 일수. Formula 4 DayIndex 상한 + Rule 3 LongGap 판정 기준. MVP=7, VS=21."))
    int32 GameDurationDays = 21;

    // ── Classification Thresholds ─────────────────────────────────────────────

    /**
     * In-session 정상 시계 동기 허용 오차(초). Rule 5 NORMAL_TICK 판정.
     * |WallΔ − MonoΔ| ≤ DefaultEpsilonSec → NORMAL_TICK.
     *
     * 안전 범위: [1, 30] 초
     *   너무 높으면 → 실제 이상 탐지 못함
     *   너무 낮으면 → 정상 시계 흔들림이 MINOR_SHIFT로 오분류, 로그 노이즈
     *
     * ADR-0011: 1Hz tick 시점 read → hot reload 자연 지원 (ConfigRestartRequired 불필요)
     * 상호작용: 항상 DefaultEpsilonSec < InSessionToleranceMinutes×60 조건 유지
     */
    UPROPERTY(Config, EditAnywhere, Category="Classification",
              meta=(ClampMin="1.0", ClampMax="30.0", Units="Seconds",
                    ToolTip="In-session 정상 시계 허용 오차(초). Rule 5 NORMAL_TICK 판정 기준."))
    float DefaultEpsilonSec = 5.0f;

    /**
     * In-session 허용 시계 불일치 상한(분). Rule 7 MINOR_SHIFT 판정.
     * DefaultEpsilonSec < |WallΔ − MonoΔ| ≤ InSessionToleranceMinutes → MINOR_SHIFT.
     * 초과 시 Rule 8 IN_SESSION_DISCREPANCY(LOG_ONLY).
     *
     * 안전 범위: [30, 240] 분
     *   너무 높으면 → 2시간짜리 큰 불일치도 흡수됨
     *   너무 낮으면 → DST·NTP 흡수 실패, IN_SESSION_DISCREPANCY 오탐
     *
     * ADR-0011: 1Hz tick 시점 read → hot reload 자연 지원
     * 상호작용: 항상 DefaultEpsilonSec(초) < InSessionToleranceMinutes×60(초) 조건 유지
     */
    UPROPERTY(Config, EditAnywhere, Category="Classification",
              meta=(ClampMin="30", ClampMax="240", Units="Minutes",
                    ToolTip="In-session 허용 시계 불일치 상한(분). Rule 7 MINOR_SHIFT / Rule 8 경계."))
    int32 InSessionToleranceMinutes = 90;

    // ── Narrative Cap ─────────────────────────────────────────────────────────

    /**
     * 시스템 내러티브 발화 임계 경과 시간(시). Formula 5 UseNarrative 조건.
     * WallDelta > NarrativeThresholdHours → UseNarrative 후보.
     *
     * 안전 범위: [6, 72] 시간
     *   너무 높으면 → 긴 공백에도 조용, Pillar 4 감지 힌트 약함
     *   너무 낮으면 → 시스템 내러티브 남발, 희소성 상실 (Pillar 3 위반)
     *
     * ADR-0011: ClassifyOnStartup() 시점 read → hot reload 자연 지원
     * 상호작용: GameDurationDays=7(MVP) → NarrativeThresholdHours=8h 권장
     */
    UPROPERTY(Config, EditAnywhere, Category="Narrative Cap",
              meta=(ClampMin="6", ClampMax="72", Units="Hours",
                    ToolTip="시스템 내러티브 발화 임계(시간). Formula 5 UseNarrative WallDelta 조건."))
    int32 NarrativeThresholdHours = 24;

    /**
     * 플레이스루 전체 시스템 내러티브 최대 발화 횟수. Formula 5 UseNarrative 조건.
     * Prev.NarrativeCount < NarrativeCapPerPlaythrough → UseNarrative 후보.
     * cap=0은 영구 침묵 (Stoic Mode A/B 테스트 옵션).
     *
     * 안전 범위: [0, 10]
     *   너무 높으면 → 주말 플레이어 10–11회 발화 → Pillar 3 위반, Dream과 혼동
     *   너무 낮으면 → 영구 침묵, 긴 공백 인정 부재 (Pillar 4 약화)
     *
     * ADR-0011: ClassifyOnStartup() 시점 read → hot reload 자연 지원
     * 상호작용: GameDurationDays=7(MVP) → NarrativeCapPerPlaythrough=1 권장
     * Playtest 1순위 튜닝 노브 (희소성 보호의 구조적 핵심)
     */
    UPROPERTY(Config, EditAnywhere, Category="Narrative Cap",
              meta=(ClampMin="0", ClampMax="10",
                    ToolTip="플레이스루 전체 시스템 내러티브 최대 횟수. 0=영구 침묵(Stoic Mode). MVP=1, VS=3."))
    int32 NarrativeCapPerPlaythrough = 3;

    // ── Suspend Detection ─────────────────────────────────────────────────────

    /**
     * SUSPEND_RESUME 판정 monotonic 상한(초). Rule 6.
     * MonoDelta < SuspendMonotonicThresholdSec AND WallDelta > SuspendWallThresholdSec → SUSPEND_RESUME.
     *
     * 안전 범위: [1, 30] 초
     *   너무 높으면 → 짧은 렌더 스톨까지 suspend로 오분류
     *   너무 낮으면 → 진짜 suspend를 IN_SESSION_DISCREPANCY로 오분류
     *
     * ADR-0011: 1Hz tick 시점 read → hot reload 자연 지원
     * OQ-1: Windows FPlatformTime::Seconds() suspend 동작 드라이버별 차이 — 실기 검증 필수
     * 상호작용: SuspendMonotonicThresholdSec + SuspendWallThresholdSec 함께 조정 필요
     */
    UPROPERTY(Config, EditAnywhere, Category="Suspend Detection",
              meta=(ClampMin="1.0", ClampMax="30.0", Units="Seconds",
                    ToolTip="SUSPEND_RESUME 판정 monotonic 상한(초). Rule 6. OQ-1 실기 검증 참조."))
    float SuspendMonotonicThresholdSec = 5.0f;

    /**
     * SUSPEND_RESUME 판정 wall clock 하한(초). Rule 6.
     * WallDelta > SuspendWallThresholdSec AND MonoDelta < SuspendMonotonicThresholdSec → SUSPEND_RESUME.
     *
     * 안전 범위: [30, 300] 초
     *   너무 높으면 → 1분짜리 백그라운드 지연을 suspend로 오분류
     *   너무 낮으면 → 짧은 절전(수분)을 놓침
     *
     * ADR-0011: 1Hz tick 시점 read → hot reload 자연 지원
     * OQ-1: Playtest 4순위 튜닝 — Windows 실기 검증 결과에 따라
     * 상호작용: SuspendMonotonicThresholdSec과 함께 조정
     */
    UPROPERTY(Config, EditAnywhere, Category="Suspend Detection",
              meta=(ClampMin="30.0", ClampMax="300.0", Units="Seconds",
                    ToolTip="SUSPEND_RESUME 판정 wall clock 하한(초). Rule 6. OQ-1 실기 검증 참조."))
    float SuspendWallThresholdSec = 60.0f;

    // ── Static Accessor ───────────────────────────────────────────────────────

    /** 런타임 + 에디터 어디서나 사용 가능한 CDO accessor. */
    static const UMossTimeSessionSettings* Get()
    {
        return GetDefault<UMossTimeSessionSettings>();
    }

    // ── UDeveloperSettings overrides ─────────────────────────────────────────

    /** Project Settings 카테고리 — "Moss Baby" */
    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }

    /** Project Settings 섹션 — "Time & Session" */
    virtual FName GetSectionName() const override { return TEXT("Time & Session"); }
};
