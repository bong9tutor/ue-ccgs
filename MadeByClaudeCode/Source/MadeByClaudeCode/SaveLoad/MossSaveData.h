// Copyright Moss Baby
//
// MossSaveData.h — 게임 전체 영속 데이터 컨테이너 + ESaveReason enum + placeholder USTRUCT
//
// Story-001: UMossSaveData USaveGame 서브클래스 + ESaveReason enum + SaveSchemaVersion
// ADR-0011: UMossSaveLoadSettings knob 노출 방식
// GDD: design/gdd/save-load-persistence.md §Core Rules + §ESaveReason enum contract
//
// AC 커버리지:
//   AC-1 (UCLASS_USAVEGAME):
//       UMossSaveData : public USaveGame, 모든 필드 UPROPERTY()
//   AC-2 (ESAVEREASON_STABLE_INTEGERS):
//       ESaveReason 4값 — ECardOffered=0, EDreamReceived=1, ENarrativeEmitted=2, EFarewellReached=3
//       stable integer 약속: 미래 값 append only, 중간 삽입 금지 (GDD §6)
//   AC-3 (CURRENT_SCHEMA_VERSION_CONSTANT):
//       static constexpr uint8 CURRENT_SCHEMA_VERSION = 1
//   AC-5 (PLACEHOLDER_STRUCTS):
//       FGrowthState, FDreamState — 빈 USTRUCT (Growth/Dream epic 소관, 본 story는 컨테이너만)
//
// 의존성:
//   - Time/SessionRecord.h (foundation-time-session Story-002, FSessionRecord USTRUCT)
//   - UMossSaveLoadSettings (Settings/MossSaveLoadSettings.h — Story-001 AC-4)
//
// Story-001 Out of Scope:
//   - Save/Load Subsystem (Story 1-8)
//   - Header block + CRC32 (Story 1-9)
//   - Atomic write + dual-slot (Story 1-10)
//   - FGrowthState 실제 필드 (Growth epic 소관)
//   - FDreamState 실제 필드 (Dream epic 소관)

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Time/SessionRecord.h"
#include "MossSaveData.generated.h"

/**
 * ESaveReason — 세이브 트리거 이유를 나타내는 4종 enum.
 *
 * stable integer 약속 (GDD §6):
 *   미래 값은 append only로 추가. 기존 값의 정수 재배치·중간 삽입 금지.
 *   직렬화된 파일에서 정수 값으로 역직렬화되므로 변경 시 하위 호환성 파괴.
 */
UENUM(BlueprintType)
enum class ESaveReason : uint8
{
    ECardOffered      = 0 UMETA(DisplayName="Card Offered"),
    EDreamReceived    = 1 UMETA(DisplayName="Dream Received"),
    ENarrativeEmitted = 2 UMETA(DisplayName="Narrative Emitted"),
    EFarewellReached  = 3 UMETA(DisplayName="Farewell Reached")
};

/**
 * FGrowthState placeholder — Growth epic 소관.
 *
 * 이끼 아이의 성장 단계 데이터를 보관할 컨테이너.
 * 실제 필드는 foundation-growth epic에서 추가 예정.
 * 본 Story-001은 UMossSaveData 컨테이너로의 포함만 담당.
 */
USTRUCT()
struct FGrowthState
{
    GENERATED_BODY()
    // Growth epic에서 필드 추가 예정
};

/**
 * FDreamState placeholder — Dream epic 소관.
 *
 * 꿈 시퀀스·수신 기록 데이터를 보관할 컨테이너.
 * 실제 필드는 foundation-dream epic에서 추가 예정.
 * 본 Story-001은 UMossSaveData 컨테이너로의 포함만 담당.
 */
USTRUCT()
struct FDreamState
{
    GENERATED_BODY()
    // Dream epic에서 필드 추가 예정
};

/**
 * UMossSaveData — 게임 전체 영속 데이터 컨테이너.
 *
 * USaveGame 상속. 모든 필드 UPROPERTY() 필수 (GC 안전성).
 * SaveSchemaVersion 기반 마이그레이션 체인 지원 (TR-save-005).
 *
 * 직렬화 소유권: Save/Load 서브시스템 (Story-002 이후 구현).
 * FSessionRecord: Time 서브시스템이 생산, Save/Load가 저장·복원.
 *
 * SaveSchemaVersion bump 정책:
 *   필드 추가·삭제·재타입·의미 변경 시마다 +1 (Control Manifest Rule).
 */
UCLASS()
class MADEBYCLAUDECODE_API UMossSaveData : public USaveGame
{
    GENERATED_BODY()

public:
    /** 현재 스키마 버전 빌드 시간 상수. SaveSchemaVersion 기본값의 정의 소스. */
    static constexpr uint8 CURRENT_SCHEMA_VERSION = 1;

    /**
     * 저장 파일 스키마 버전.
     * 로드 시 UMossSaveLoadSettings::MinCompatibleSchemaVersion 과 비교하여 마이그레이션 여부 판단.
     * CURRENT_SCHEMA_VERSION 과 동기화하여 초기화.
     */
    UPROPERTY()
    uint8 SaveSchemaVersion = CURRENT_SCHEMA_VERSION;

    /**
     * 단조 증가 쓰기 시퀀스 번호.
     * 저장마다 +1. dual-slot 원자 쓰기 (Story 1-10)에서 최신본 판별에 사용.
     */
    UPROPERTY()
    uint32 WriteSeqNumber = 0;

    /**
     * 마지막 저장 이유를 나타내는 ESaveReason enum 이름 문자열.
     * 예: "ECardOffered", "EDreamReceived", "ENarrativeEmitted", "EFarewellReached".
     * enum 직접 직렬화 대신 이름 문자열 보관 — 하위 호환성 우선.
     */
    UPROPERTY()
    FString LastSaveReason;

    /**
     * 세션 연속성 데이터.
     * Time 서브시스템이 생산, Save/Load 서브시스템이 저장·복원.
     * 정의: Time/SessionRecord.h (foundation-time-session Story-002).
     */
    UPROPERTY()
    FSessionRecord SessionRecord;

    /**
     * 이끼 아이 성장 단계 데이터.
     * FGrowthState 실제 필드는 Growth epic 소관 — 현재 placeholder.
     */
    UPROPERTY()
    FGrowthState GrowthState;

    /**
     * 꿈 시퀀스·수신 기록 데이터.
     * FDreamState 실제 필드는 Dream epic 소관 — 현재 placeholder.
     */
    UPROPERTY()
    FDreamState DreamState;

    /**
     * 저장 데이터 열화(degraded) 플래그.
     * E14 (Story 1-10 Atomic Write + dual-slot) 이후 사용.
     * true 시: 읽기 가능하나 최신 슬롯 CRC 검증 실패 또는 부분 복구 상태.
     */
    UPROPERTY()
    bool bSaveDegraded = false;
};
