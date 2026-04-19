// Copyright Moss Baby
//
// FSessionRecord — Time & Session 세션 요약 구조체.
//
// Story-002: FSessionRecord USTRUCT 정의
// ADR-0001: Forward Time Tamper 명시적 수용 정책 참조
// GDD: design/gdd/time-session-system.md §Dependencies #3, §Formula 2 precision note
//
// AC 커버리지:
//   AC-001 (SESSION_RECORD_DOUBLE_TYPE_DECLARATION):
//       LastMonotonicSec 필드 double 타입 선언 + static_assert 컴파일 통과 (CODE_REVIEW)
//   AC-002 (SESSION_RECORD_DOUBLE_PRECISION_RUNTIME):
//       FMemoryWriter/FMemoryReader round-trip 검증 (AUTOMATED — SessionRecordTests.cpp)
//
// IMPORTANT — DO NOT change `double` to `float` — see GDD Formula 2 precision note:
//   float(32-bit)은 1.8M초(21일) 범위에서 0.25초 오차 발생.
//   LastMonotonicSec은 반드시 double 유지.
//
// Forward Tamper 주의 (ADR-0001):
//   이 구조체 내에 tamper/cheat/bIsForward/bIsSuspicious/DetectClockSkew/IsSuspiciousAdvance
//   패턴을 사용하는 필드·메서드 추가 금지. CI grep이 자동으로 차단함.

#pragma once
#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include <type_traits>
#include "SessionRecord.generated.h"

/**
 * FSessionRecord — 세션 간 연속성을 유지하기 위한 Time & Session 세션 요약.
 *
 * Save/Load 서브시스템이 직렬화 소유권을 가진다 (foundation-save-load epic).
 * Time 서브시스템이 생산하고, Save/Load가 저장·복원한다.
 *
 * 필드별 clamp 범위:
 *   DayIndex         — [1, 21]
 *   NarrativeCount   — [0, NarrativeCap]
 *   SessionCountToday — DayIndex 변화 시 리셋
 *
 * LastSaveReason은 ESaveReason enum의 이름 문자열을 저장한다
 * (Save/Load GDD §ESaveReason enum 6 contract 참조).
 * ESaveReason 정의 자체는 foundation-save-load epic Story-001 소유 — 여기서는 FString으로 보존.
 */
USTRUCT(BlueprintType)
struct FSessionRecord
{
    GENERATED_BODY()

    /** 세션 고유 식별자. 앱 시작 시 FGuid::NewGuid()로 생성. */
    UPROPERTY()
    FGuid SessionUuid;

    /** 현재 날짜 인덱스. 범위: [1, 21]. FIRST_RUN 시 1로 초기화. */
    UPROPERTY()
    int32 DayIndex = 1;

    /**
     * 마지막 저장 시점의 UTC wall clock.
     * FDateTime 내부 int64 ticks 직렬화 — 精度 손실 없음.
     * BACKWARD_GAP 케이스에서만 갱신 거부 (ADR-0001 §Decision).
     */
    UPROPERTY()
    FDateTime LastWallUtc;

    /**
     * 마지막 저장 시점의 monotonic counter (초 단위).
     *
     * DO NOT change `double` to `float` — see GDD Formula 2 precision note.
     * float(32-bit)은 1.8M초(21일) 범위에서 0.25초 오차 발생.
     * double(64-bit)로만 소수 3자리 정밀도(≤0.001초 오차) 보장 가능.
     *
     * AC SESSION_RECORD_DOUBLE_TYPE_DECLARATION: 아래 static_assert 참조.
     */
    UPROPERTY()
    double LastMonotonicSec = 0.0;

    /** 현재 플레이스루에서 발화된 시스템 내러티브 수. 범위: [0, NarrativeCapPerPlaythrough]. */
    UPROPERTY()
    int32 NarrativeCount = 0;

    /** 오늘(DayIndex) 세션 수. DayIndex 변화 시 0으로 리셋. */
    UPROPERTY()
    int32 SessionCountToday = 0;

    /**
     * 마지막 저장 이유. ESaveReason enum 이름 문자열.
     * 예: "ECardOffered", "EDreamReceived", "ENarrativeEmitted", "EFarewellReached".
     * ESaveReason 정의는 foundation-save-load epic Story-001 소유.
     */
    UPROPERTY()
    FString LastSaveReason;
};

// ─────────────────────────────────────────────────────────────────────────────
// CODE_REVIEW AC 강제 — AC SESSION_RECORD_DOUBLE_TYPE_DECLARATION
//
// LastMonotonicSec이 double임을 컴파일 타임에 검증한다.
// 이 static_assert가 실패하면 빌드 자체가 깨진다 (런타임 테스트 불필요).
//
// GDD Formula 2 precision contract: "float(32-bit)은 1.8M초 범위에서 0.25초 오차"
// → AC 검증 타입: CODE_REVIEW (컴파일 실패 = CI BLOCKING)
// ─────────────────────────────────────────────────────────────────────────────
static_assert(
    std::is_same_v<decltype(FSessionRecord::LastMonotonicSec), double>,
    "monotonic field must be double — Formula 2 precision contract (GDD Time §F2)"
);
