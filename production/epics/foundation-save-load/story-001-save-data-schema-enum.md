# Story 001: UMossSaveData USaveGame 서브클래스 + ESaveReason enum + SaveSchemaVersion + Developer Settings

> **Epic**: Save/Load Persistence
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/save-load-persistence.md`
**Requirement**: `TR-save-005` (SaveSchemaVersion + migration chain), `ESaveReason` cross-system contract 정의
**ADR Governing Implementation**: ADR-0011 (UMossSaveLoadSettings knob 노출)
**ADR Decision Summary**: `UMossSaveData : USaveGame`에 `FSessionRecord`, `FGrowthState`, `FDreamState` 포함. `SaveSchemaVersion: uint8` 헤더 필드. `ESaveReason { ECardOffered, EDreamReceived, ENarrativeEmitted, EFarewellReached }` 4값 enum + stable integer.
**Engine**: UE 5.6 | **Risk**: LOW (USaveGame 표준 API)
**Engine Notes**: `USaveGame` subclass + `UPROPERTY()` 모든 필드. `FMossSaveSnapshot` POD-only USTRUCT은 worker thread 안전성 확보 (Story 004 이후 구현).

**Control Manifest Rules (Foundation layer + Global)**:
- Required: "USaveGame 필드는 전부 `UPROPERTY()` 표기" — GC 안전성 (Global)
- Required: "SaveSchemaVersion bump 정책: 필드 추가·삭제·재타입·의미 변경 시마다 +1"

---

## Acceptance Criteria

*From GDD §Core Rules + §Tuning Knobs:*

- [ ] `UMossSaveData : public USaveGame` 정의 — 필드: `FSessionRecord SessionRecord`, `FGrowthState GrowthState` (placeholder), `FDreamState DreamState` (placeholder), `uint8 SaveSchemaVersion`, `FString LastSaveReason`, `uint32 WriteSeqNumber`
- [ ] `ESaveReason` enum 정의 — 4개 값: `ECardOffered = 0`, `EDreamReceived = 1`, `ENarrativeEmitted = 2`, `EFarewellReached = 3` (stable integer, UMETA(DisplayName))
- [ ] `CURRENT_SCHEMA_VERSION: uint8` 빌드 시간 상수 = 1
- [ ] `UMossSaveLoadSettings : UDeveloperSettings` 정의 — knobs: `MinCompatibleSchemaVersion` (uint8, default 1), `MaxPayloadBytes` (default 10⁷ = 10MB), `SaveFailureRetryThreshold` (default 3), `DeinitFlushTimeoutSec` (default 2.0), `bDevDiagnosticsEnabled` (default false)
- [ ] `FGrowthState`, `FDreamState`는 forward declare + 빈 struct (실제 필드는 다른 epic 소관. 본 story는 컨테이너만)

---

## Implementation Notes

*Derived from GDD §Core Rules 1-6 + ADR-0011 §Naming Convention:*

```cpp
// Source/MadeByClaudeCode/SaveLoad/MossSaveData.h
UENUM(BlueprintType)
enum class ESaveReason : uint8 {
    ECardOffered      = 0 UMETA(DisplayName="Card Offered"),
    EDreamReceived    = 1 UMETA(DisplayName="Dream Received"),
    ENarrativeEmitted = 2 UMETA(DisplayName="Narrative Emitted"),
    EFarewellReached  = 3 UMETA(DisplayName="Farewell Reached"),
};

USTRUCT() struct FGrowthState { GENERATED_BODY() /* Growth epic 소관 */ };
USTRUCT() struct FDreamState  { GENERATED_BODY() /* Dream epic 소관 */ };

UCLASS()
class UMossSaveData : public USaveGame {
    GENERATED_BODY()
public:
    static constexpr uint8 CURRENT_SCHEMA_VERSION = 1;

    UPROPERTY() uint8 SaveSchemaVersion = CURRENT_SCHEMA_VERSION;
    UPROPERTY() uint32 WriteSeqNumber = 0;
    UPROPERTY() FString LastSaveReason;  // ESaveReason enum name string
    UPROPERTY() FSessionRecord SessionRecord;    // Time Story 002에서 정의
    UPROPERTY() FGrowthState GrowthState;
    UPROPERTY() FDreamState DreamState;
    UPROPERTY() bool bSaveDegraded = false;       // E14 (Story 007)
};

// Source/MadeByClaudeCode/Settings/MossSaveLoadSettings.h
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Save/Load Persistence"))
class UMossSaveLoadSettings : public UDeveloperSettings {
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Schema",
              meta=(ClampMin="1", ClampMax="255", ConfigRestartRequired="true"))
    uint8 MinCompatibleSchemaVersion = 1;

    UPROPERTY(Config, EditAnywhere, Category="Sanity Cap",
              meta=(ClampMin="100000", ClampMax="100000000"))
    int32 MaxPayloadBytes = 10000000;  // 10MB

    UPROPERTY(Config, EditAnywhere, Category="Resilience",
              meta=(ClampMin="1", ClampMax="10"))
    int32 SaveFailureRetryThreshold = 3;

    UPROPERTY(Config, EditAnywhere, Category="Lifecycle",
              meta=(ClampMin="1.0", ClampMax="10.0", Units="Seconds"))
    float DeinitFlushTimeoutSec = 2.0f;

    UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
    bool bDevDiagnosticsEnabled = false;

    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }
    virtual FName GetSectionName() const override { return TEXT("Save/Load Persistence"); }
    static const UMossSaveLoadSettings* Get() { return GetDefault<UMossSaveLoadSettings>(); }
};
```

- `FSessionRecord`는 Time epic Story 002에서 정의 — include
- `ESaveReason` stable integer 약속: 미래 값 append only, 중간 삽입 금지 (GDD §6)

---

## Out of Scope

- Story 002: USaveLoadSubsystem 뼈대 + 4-state machine
- Story 003: Header block struct + CRC32 + Formula 1-3
- Story 004: FMossSaveSnapshot POD + worker thread 분리

---

## QA Test Cases

**For Logic story:**
- **UMossSaveData schema**
  - Given: UMossSaveData CDO
  - When: `NewObject<UMossSaveData>()` 생성
  - Then: `SaveSchemaVersion == 1`, `WriteSeqNumber == 0`, `LastSaveReason.IsEmpty()`
- **ESaveReason stable integers**
  - Given: enum 정의
  - When: `(uint8)ESaveReason::ECardOffered`, `(uint8)ESaveReason::EFarewellReached`
  - Then: 0, 3
  - Edge cases: `UEnum::GetValueAsString(ESaveReason::EDreamReceived)` == "EDreamReceived"
- **UMossSaveLoadSettings**
  - Given: CDO
  - When: `UMossSaveLoadSettings::Get()->MaxPayloadBytes`
  - Then: 10000000 (10MB default)
  - Edge cases: Project Settings → Moss Baby → Save/Load Persistence 렌더링 수동 확인

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/save-load/save_data_schema_test.cpp` (USaveGame subclass 생성 + enum stable int)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: **foundation-time-session Story 002** (FSessionRecord 타입 — 크로스-에픽 의존)
- Unlocks: Story 002, 003
