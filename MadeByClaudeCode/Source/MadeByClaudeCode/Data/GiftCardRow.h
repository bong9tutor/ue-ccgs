// Copyright Moss Baby
//
// GiftCardRow.h — Card System DataTable row 타입
//
// Story-1-4: Data Pipeline Schema Types
// ADR-0002: Data Pipeline 컨테이너 선택 — Card = UDataTable + FGiftCardRow
//
// C1 Schema Gate (ADR-0002):
//   FGiftCardRow에는 수치 효과 필드(int32/float/double)를 절대 추가 금지.
//   허용 타입: FName, TArray<FName>, FText, FSoftObjectPath만.
//   Pillar 2 보호 — 수치 stat 필드가 없어야 카드가 직접 스탯을 변경하지 않음.
//   CI grep: grep -rE "(int32|float|double) [A-Z]" GiftCardRow.h → 0 매치 기대.
//
// ADR-0001 금지: tamper, cheat, bIsForward, bIsSuspicious,
//   DetectClockSkew, IsSuspiciousAdvance 패턴 금지.
//
// 사용 예:
//   UDataTable* CardTable = ...;
//   const FGiftCardRow* Row = CardTable->FindRow<FGiftCardRow>(CardId, TEXT("GiftCardRow lookup"));

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GiftCardRow.generated.h"

/**
 * FGiftCardRow — Card System의 DataTable row 타입.
 *
 * GDD: design/gdd/card-system.md
 * ADR-0002 §Key Interfaces — Card = UDataTable + FGiftCardRow
 *
 * C1 schema gate (ADR-0002): 수치 효과 필드 절대 금지.
 * 허용 타입: FName, TArray<FName>, FText, FSoftObjectPath만.
 * 카드는 태그 벡터를 누적할 뿐, 직접 스탯을 수치로 변경하지 않는다.
 *
 * NO int32/float/double fields — C1 schema gate (ADR-0002)
 */
USTRUCT(BlueprintType)
struct FGiftCardRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 카드 고유 식별자. DataTable FindRow 키로 사용. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card")
    FName CardId;

    /**
     * 카드 태그 목록. 성장 누적 엔진이 이 태그를 가중치 벡터로 변환한다.
     * GDD §Growth Accumulation Engine — TagVector 누적.
     * FGameplayTagContainer 전환 검토 예정 (Card Implementation Notes 참조).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card")
    TArray<FName> Tags;

    /** 카드 표시 이름 (UI 렌더링용). FText — 로컬라이제이션 지원. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card")
    FText DisplayName;

    /** 카드 설명 텍스트. FText — 로컬라이제이션 지원. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card")
    FText Description;

    /** 카드 아이콘 에셋 경로 (Soft Reference — 필요 시 로드). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Card", meta=(AllowedClasses="Texture2D"))
    FSoftObjectPath IconPath;

    // NO int32/float/double fields — C1 schema gate (ADR-0002)
    // 수치 효과 필드 추가 시 CI grep이 차단한다:
    //   grep -rE "(int32|float|double) [A-Z][A-Za-z]*;?" Source/MadeByClaudeCode/Data/GiftCardRow.h
};
