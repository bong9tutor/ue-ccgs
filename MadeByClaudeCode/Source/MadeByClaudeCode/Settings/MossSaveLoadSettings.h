// Copyright Moss Baby
//
// MossSaveLoadSettings.h — Save/Load Persistence System Tuning Knobs
//
// Story-001: UMossSaveData + ESaveReason + SaveSchemaVersion + Developer Settings
// ADR-0011: Tuning Knob 노출 방식 — UDeveloperSettings 정식 채택
// GDD: design/gdd/save-load-persistence.md §Tuning Knobs (5개 노브)
//
// ADR-0011 §Naming Convention:
//   U[SystemName]Settings : public UDeveloperSettings
//   GetCategoryName() → "Moss Baby"
//   GetSectionName()  → "Save/Load Persistence"
//
// ADR-0011 §ConfigRestartRequired convention:
//   - Subsystem Initialize()에서 캐싱하는 knob → ConfigRestartRequired="true"
//   - 매 호출 시점마다 read하는 knob → hot reload 자연 지원 (meta 불필요)
//
// ini 저장 위치: Config/DefaultGame.ini
//   [/Script/MadeByClaudeCode.MossSaveLoadSettings]
//
// 사용 예 (런타임):
//   const auto* S = UMossSaveLoadSettings::Get();
//   int32 Cap = S->MaxPayloadBytes;

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MossSaveLoadSettings.generated.h"

/**
 * UMossSaveLoadSettings — Save/Load Persistence System 전역 Tuning Knobs
 *
 * Project Settings → Game → Moss Baby → Save/Load Persistence 에서 편집.
 * 5개 노브는 GDD §Tuning Knobs 표와 1:1 대응.
 * ADR-0011: 시스템 전역 상수는 UDeveloperSettings, per-instance 데이터는 UPrimaryDataAsset.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Save/Load Persistence"))
class MADEBYCLAUDECODE_API UMossSaveLoadSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // ── Schema ────────────────────────────────────────────────────────────────

    /**
     * 로드 시 허용하는 최소 스키마 버전.
     * 저장 파일의 SaveSchemaVersion < MinCompatibleSchemaVersion 이면 로드 거부.
     * 이전 세이브 데이터 마이그레이션 체인 진입점 (TR-save-005).
     *
     * 안전 범위: [1, 255]
     *   값 내리면 → 오래된 세이브 파일 허용 (의도적 하위 호환)
     *   값 올리면 → 이전 세이브 파일 거부 (의도적 단절)
     *
     * ADR-0011: Subsystem Initialize()에서 캐싱 → ConfigRestartRequired
     */
    UPROPERTY(Config, EditAnywhere, Category="Schema",
              meta=(ClampMin="1", ClampMax="255", ConfigRestartRequired="true",
                    ToolTip="로드 시 허용 최소 스키마 버전. SaveSchemaVersion < 이 값이면 로드 거부. TR-save-005 마이그레이션 진입점."))
    uint8 MinCompatibleSchemaVersion = 1;

    // ── Sanity Cap ────────────────────────────────────────────────────────────

    /**
     * 세이브 페이로드 최대 허용 바이트 수.
     * 직렬화 후 바이트 배열 크기가 이 값을 초과하면 저장 거부 (sanity check).
     * 기본값 10MB — 데스크톱 상주형 오브제 특성상 여유 있음.
     *
     * 안전 범위: [100,000 (100KB), 100,000,000 (100MB)]
     *   너무 낮으면 → 정상 세이브도 거부될 위험
     *   너무 높으면 → 비정상 비대 세이브 탐지 실패
     *
     * ADR-0011: 저장 호출 시점 read → hot reload 자연 지원
     */
    UPROPERTY(Config, EditAnywhere, Category="Sanity Cap",
              meta=(ClampMin="100000", ClampMax="100000000",
                    ToolTip="세이브 페이로드 최대 바이트 수 (기본 10MB). 초과 시 저장 거부."))
    int32 MaxPayloadBytes = 10000000;

    // ── Resilience ────────────────────────────────────────────────────────────

    /**
     * 저장 실패 시 재시도 허용 횟수.
     * 저장 호출이 이 횟수만큼 연속 실패하면 오류 상태로 전환.
     *
     * 안전 범위: [1, 10]
     *   너무 낮으면 → 일시적 I/O 오류에 과민 반응
     *   너무 높으면 → 영구 장애 탐지 지연
     *
     * ADR-0011: 저장 호출 시점 read → hot reload 자연 지원
     */
    UPROPERTY(Config, EditAnywhere, Category="Resilience",
              meta=(ClampMin="1", ClampMax="10",
                    ToolTip="저장 실패 재시도 허용 횟수. 연속 실패 시 오류 상태 전환."))
    int32 SaveFailureRetryThreshold = 3;

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * 서브시스템 Deinitialize() 시 진행 중인 저장 작업 flush 대기 제한(초).
     * 이 시간 초과 시 저장 포기 후 Deinit 진행.
     *
     * 안전 범위: [1.0, 10.0] 초
     *   너무 짧으면 → 앱 종료 시 세이브 유실 위험
     *   너무 길면 → 앱 종료 지연 (UX 악화)
     *
     * ADR-0011: Subsystem Initialize()에서 캐싱 → ConfigRestartRequired 불필요
     *   (Deinit 시점에 read하므로 hot reload 자연 지원)
     */
    UPROPERTY(Config, EditAnywhere, Category="Lifecycle",
              meta=(ClampMin="1.0", ClampMax="10.0", Units="Seconds",
                    ToolTip="Deinitialize flush 대기 제한(초). 초과 시 저장 포기 후 Deinit 진행."))
    float DeinitFlushTimeoutSec = 2.0f;

    // ── Diagnostics ───────────────────────────────────────────────────────────

    /**
     * 개발용 진단 로그 활성화 여부.
     * true 시: 저장/로드 경로에 상세 로그 출력. 출시 빌드에서 false 유지.
     *
     * ADR-0011: 매 호출 시점 read → hot reload 자연 지원
     */
    UPROPERTY(Config, EditAnywhere, Category="Diagnostics",
              meta=(ToolTip="개발용 저장/로드 상세 진단 로그 활성화. 출시 빌드에서 false 유지."))
    bool bDevDiagnosticsEnabled = false;

    // ── Static Accessor ───────────────────────────────────────────────────────

    /** 런타임 + 에디터 어디서나 사용 가능한 CDO accessor. */
    static const UMossSaveLoadSettings* Get()
    {
        return GetDefault<UMossSaveLoadSettings>();
    }

    // ── UDeveloperSettings overrides ─────────────────────────────────────────

    /** Project Settings 카테고리 — "Moss Baby" */
    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }

    /** Project Settings 섹션 — "Save/Load Persistence" */
    virtual FName GetSectionName() const override { return TEXT("Save/Load Persistence"); }
};
