// Copyright Moss Baby
//
// MossSaveSnapshot.cpp — MakeSnapshotFromSaveData factory 구현
//
// Story 1-10: FMossSaveSnapshot factory
// GDD: design/gdd/save-load-persistence.md §2 저장 절차 Step 1
//
// 이 파일에서만 UMossSaveData 전체 include 허용.
// 헤더(MossSaveSnapshot.h)는 UMossSaveData를 forward declaration만 사용.
// → POD-only 헤더 원칙 유지 + factory 구현 분리.

#include "SaveLoad/MossSaveSnapshot.h"
#include "SaveLoad/MossSaveData.h"

/**
 * UMossSaveData로부터 FMossSaveSnapshot을 생성.
 *
 * game thread에서만 호출 가능. 반환값은 worker thread로 안전하게 전달 가능.
 * UMossSaveData의 각 필드를 value copy — GC 참조 없음.
 */
FMossSaveSnapshot MakeSnapshotFromSaveData(const UMossSaveData& Data)
{
    FMossSaveSnapshot Out;
    Out.SessionRecord      = Data.SessionRecord;
    Out.SaveSchemaVersion  = Data.SaveSchemaVersion;
    Out.LastSaveReason     = Data.LastSaveReason;
    return Out;
}
