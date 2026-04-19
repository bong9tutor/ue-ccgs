// Copyright Moss Baby
//
// GrowthStage.h — Moss Baby 성장 단계 공용 열거형 (임시 위치)
//
// Story-1-4: Data Pipeline Schema Types
// ADR-0002: Data Pipeline 컨테이너 선택
//
// 임시 위치 안내:
//   이 enum은 core-moss-baby-character epic이 요구하는 EGrowthStage를
//   DreamDataAsset::RequiredStage 필드가 참조할 수 있도록 Data epic에서 임시 정의한다.
//   TODO(Sprint 1+ core-moss-baby-character): 이 enum을 Characters/ 폴더로 이동.
//   이동 시 DreamDataAsset.h의 include 경로 업데이트 필요.
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.

#pragma once
#include "CoreMinimal.h"
#include "GrowthStage.generated.h"

/**
 * EGrowthStage — Moss Baby 성장 단계.
 *
 * GDD: design/gdd/growth-accumulation-engine.md §Growth Stage
 * 4단계 성장을 표현한다. FinalForm 결정 이전까지 Sprout → Growing → Mature 순으로 진행.
 *
 * 소유권:
 *   현재 소유: Data Pipeline epic (임시)
 *   최종 소유 예정: core-moss-baby-character epic (Characters/ 폴더)
 *   이동 전까지 이 헤더를 include하여 참조할 것.
 */
UENUM(BlueprintType)
enum class EGrowthStage : uint8
{
    /** 새싹 단계 — 성장 시작. DayIndex 초반. */
    Sprout      UMETA(DisplayName = "Sprout"),

    /** 성장 중 단계 — 중반 카드 반응 누적. */
    Growing     UMETA(DisplayName = "Growing"),

    /** 성숙 단계 — FinalForm 결정 임박. */
    Mature      UMETA(DisplayName = "Mature"),

    /** 최종 형태 — FinalForm 결정 완료. */
    FinalForm   UMETA(DisplayName = "Final Form"),
};
