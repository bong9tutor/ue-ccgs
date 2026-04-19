// Copyright Moss Baby
//
// MossSaveSnapshot.h — worker thread 전달용 POD 복사 구조체 선언
//
// Story 1-10: FMossSaveSnapshot POD USTRUCT + MakeSnapshotFromSaveData free function
// GDD: design/gdd/save-load-persistence.md §2 저장 절차 Step 1
//
// POD-only 컴파일 타임 가드:
//   이 헤더에 "UObject/UObject.h" 또는 "GameFramework/SaveGame.h" include 금지.
//   UObject*, TWeakObjectPtr, TSharedPtr 등 GC 경합 타입 필드 추가 시
//   → UObject.h 없으므로 빌드 실패 → CODE_REVIEW AC 자동 강제.
//
// FMossSaveSnapshot 용도:
//   SaveAsync에서 game thread에서 복사본을 만들어 worker thread로 전달 (Step 1).
//   UMossSaveData(UObject) 직접 참조 없이 POD만 복사 → GC 안전성 보장.
//
// AC 커버리지:
//   POD-only guard (CODE_REVIEW): UObject.h 미include → UObject* 필드 추가 시 빌드 실패
//   FMossSaveSnapshot 필드: SessionRecord, SaveSchemaVersion, LastSaveReason
//   MakeSnapshotFromSaveData: free function (헤더에 선언, .cpp에서 구현)
//
// IMPORTANT: "UObject/UObject.h" 또는 "GameFramework/SaveGame.h" include 금지 — POD-only 컴파일 타임 가드.

#pragma once
#include "CoreMinimal.h"
#include "Time/SessionRecord.h"
#include "MossSaveSnapshot.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// FMossSaveSnapshot — worker thread 전달용 POD-only 복사 구조체
//
// GC 경합 방지 설계:
//   - UObject*, TWeakObjectPtr, TSharedPtr 등 GC 경합 타입 필드 금지
//   - 오직 value type만 허용: FSessionRecord, uint8, FString
//   - UObject.h 미include → UObject* 필드 추가 시 빌드 실패로 POD-only 강제
//
// Growth/Dream 필드 확장 계획:
//   foundation-growth/dream epic 완료 후 FName ID 방식으로 flatten 추가 예정.
//   UObject* 방식은 영구 금지 (POD-only 컴파일 타임 가드).
// ─────────────────────────────────────────────────────────────────────────────

/**
 * FMossSaveSnapshot — worker thread 전달용 POD-only 세이브 데이터 복사 구조체.
 *
 * SaveAsync Step 1에서 game thread가 UMossSaveData로부터 값 복사.
 * worker thread는 이 구조체만 참조 — GC 경합 없음.
 *
 * POD-only 컴파일 타임 가드: 이 헤더에 UObject.h 미include.
 * UObject* 또는 TWeakObjectPtr 필드 추가 시 빌드 실패로 자동 차단.
 */
USTRUCT()
struct FMossSaveSnapshot
{
    GENERATED_BODY()

    /**
     * 세션 연속성 데이터 복사본.
     * FSessionRecord: FGuid, int32, FDateTime, double, FString — 모두 value type.
     */
    UPROPERTY()
    FSessionRecord SessionRecord;

    /**
     * 저장 시점의 스키마 버전.
     * UMossSaveData::CURRENT_SCHEMA_VERSION 값을 복사.
     */
    UPROPERTY()
    uint8 SaveSchemaVersion = 0;

    /**
     * 저장 이유 문자열.
     * ESaveReason enum 이름 (예: "ECardOffered", "EDreamReceived").
     */
    UPROPERTY()
    FString LastSaveReason;

    // Growth/Dream 필드는 future flatten:
    //   foundation-growth epic 완료 후 FName GrowthStageId 추가 예정.
    //   foundation-dream epic 완료 후 FName ActiveDreamId 추가 예정.
    //   UObject* 방식은 영구 금지.
};

// ─────────────────────────────────────────────────────────────────────────────
// MakeSnapshotFromSaveData — free function factory
//
// 헤더에 선언만 두고 .cpp에서 구현.
// UMossSaveData 전체 include를 헤더에서 회피하여 POD-only 보장.
// ─────────────────────────────────────────────────────────────────────────────

// Forward declaration — .cpp에서 UMossSaveData 전체 include
class UMossSaveData;

/**
 * UMossSaveData로부터 FMossSaveSnapshot을 생성하는 factory free function.
 *
 * game thread에서만 호출 가능 (UMossSaveData는 UObject — game thread 전용).
 * 반환된 FMossSaveSnapshot은 worker thread로 안전하게 전달 가능.
 *
 * @param Data  복사 원본 UMossSaveData 참조 (const — 수정 없음)
 * @return      값 복사된 FMossSaveSnapshot (GC 안전, worker thread 전달 가능)
 */
FMossSaveSnapshot MakeSnapshotFromSaveData(const UMossSaveData& Data);
