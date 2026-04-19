# Story 003: UMossTimeSessionSubsystem 뼈대 + enum 타입 정의 + Developer Settings

> **Epic**: Time & Session System
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/time-session-system.md`
**Requirement**: `TR-time-002` (OQ-5 UDeveloperSettings knob exposure — NarrativeCap, DefaultEpsilonSec, WitheredThresholdDays 등)
**ADR Governing Implementation**: ADR-0011 (Tuning Knob UDeveloperSettings 정식 채택)
**ADR Decision Summary**: `UMossTimeSessionSettings : UDeveloperSettings` 클래스에 7개 knob 노출. `UCLASS(Config=Game, DefaultConfig)` 매크로 + `UPROPERTY(Config, EditAnywhere)` 필드.
**Engine**: UE 5.6 | **Risk**: LOW
**Engine Notes**: `UDeveloperSettings`는 UE 4.x+ 안정 API, 5.6 변경 없음. `GetDefault<>()` 정적 accessor로 런타임/에디터 접근.

**Control Manifest Rules (Foundation layer + Cross-Layer)**:
- Required: Cross-Layer "시스템 전역 상수 Tuning Knob은 `U[SystemName]Settings : UDeveloperSettings`" — `UMossTimeSessionSettings` (ADR-0011)
- Required: Cross-Layer "Hot reload 불가 knob에 `meta=(ConfigRestartRequired=true)` 명시"
- Forbidden: `const` / `static constexpr` for system-wide tuning values — 재빌드 필수 (ADR-0011)

---

## Acceptance Criteria

*From GDD §Tuning Knobs + §Acceptance Criteria:*

- [ ] `UMossTimeSessionSubsystem : UGameInstanceSubsystem` 클래스 뼈대 정의 (Initialize/Deinitialize override, `FSessionRecord` 멤버, clock source 의존성 주입 API)
- [ ] Enum 정의: `ETimeAction { START_DAY_ONE, ADVANCE_SILENT, ADVANCE_WITH_NARRATIVE, HOLD_LAST_TIME, LONG_GAP_SILENT, LOG_ONLY }`
- [ ] Enum 정의: `EFarewellReason { NormalCompletion, LongGapAutoFarewell }` (FAREWELL_LONG_GAP_SILENT AC가 요구하는 두 값 분리)
- [ ] Enum 정의: `EBetweenSessionClass`, `EInSessionClass` (Core Rules §Rule 명명 보존)
- [ ] `UMossTimeSessionSettings : UDeveloperSettings` 7 knobs: `GameDurationDays` (default 21), `DefaultEpsilonSec` (5.0), `InSessionToleranceMinutes` (90), `NarrativeThresholdHours` (24), `NarrativeCapPerPlaythrough` (3), `SuspendMonotonicThresholdSec` (5.0), `SuspendWallThresholdSec` (60.0)
- [ ] Delegate 선언: `FOnTimeActionResolved(ETimeAction)`, `FOnDayAdvanced(int32 NewDayIndex)`, `FOnFarewellReached(EFarewellReason)`

---

## Implementation Notes

*Derived from ADR-0011 §Key Interfaces + GDD §Tuning Knobs §Playtest Tuning Priorities:*

```cpp
// Source/MadeByClaudeCode/Settings/MossTimeSessionSettings.h
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Time & Session"))
class UMossTimeSessionSettings : public UDeveloperSettings {
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Day Structure",
              meta=(ClampMin="7", ClampMax="60", ConfigRestartRequired="true"))
    int32 GameDurationDays = 21;

    UPROPERTY(Config, EditAnywhere, Category="Classification",
              meta=(ClampMin="1.0", ClampMax="30.0", Units="Seconds"))
    float DefaultEpsilonSec = 5.0f;

    UPROPERTY(Config, EditAnywhere, Category="Classification",
              meta=(ClampMin="5.0", ClampMax="240.0", Units="Minutes"))
    int32 InSessionToleranceMinutes = 90;

    UPROPERTY(Config, EditAnywhere, Category="Narrative Cap",
              meta=(ClampMin="6", ClampMax="72", Units="Hours"))
    int32 NarrativeThresholdHours = 24;

    UPROPERTY(Config, EditAnywhere, Category="Narrative Cap",
              meta=(ClampMin="0", ClampMax="10", ConfigRestartRequired="true"))
    int32 NarrativeCapPerPlaythrough = 3;

    UPROPERTY(Config, EditAnywhere, Category="Suspend Detection",
              meta=(ClampMin="1.0", ClampMax="30.0", Units="Seconds"))
    float SuspendMonotonicThresholdSec = 5.0f;

    UPROPERTY(Config, EditAnywhere, Category="Suspend Detection",
              meta=(ClampMin="10.0", ClampMax="300.0", Units="Seconds"))
    float SuspendWallThresholdSec = 60.0f;

    virtual FName GetCategoryName() const override { return TEXT("Moss Baby"); }
    virtual FName GetSectionName() const override { return TEXT("Time & Session"); }
    static const UMossTimeSessionSettings* Get() { return GetDefault<UMossTimeSessionSettings>(); }
};

// Source/MadeByClaudeCode/Time/MossTimeSessionSubsystem.h
UCLASS()
class UMossTimeSessionSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Clock source 주입 (production = FRealClockSource, test = FMockClockSource)
    void SetClockSource(TSharedPtr<IMossClockSource> InClock) { Clock = InClock; }

    // Delegates
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnTimeActionResolved, ETimeAction);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnDayAdvanced, int32);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnFarewellReached, EFarewellReason);
    FOnTimeActionResolved OnTimeActionResolved;
    FOnDayAdvanced OnDayAdvanced;
    FOnFarewellReached OnFarewellReached;

    // Public API (다음 story에서 구현)
    ETimeAction ClassifyOnStartup(const FSessionRecord* PrevRecord);

    // 단일 atomicity contract 강제 (Save/Load §5 R2 NarrativeCount)
    void IncrementNarrativeCountAndSave();
private:
    void IncrementNarrativeCount();
    void TriggerSaveForNarrative();

    TSharedPtr<IMossClockSource> Clock;
    FSessionRecord CurrentRecord;
};
```

- `ETimeAction` 등 enum은 `Source/MadeByClaudeCode/Time/MossTimeSessionTypes.h` 별도 헤더
- `Settings` 디렉터리: `Source/MadeByClaudeCode/Settings/` (ADR-0011 §Migration Plan Phase A)
- Subsystem이 `FDateTime::UtcNow()` 직접 호출하지 않음 — `Clock->GetUtcNow()`만

---

## Out of Scope

- Story 004: 8-Rule Classifier 로직 (Between-session Rules 1-4)
- Story 005: In-session Rules 5-8 + tick
- Story 006: Farewell + Idempotency
- Story 007: Forward Tamper CI grep hook + Windows suspend manual test

---

## QA Test Cases

**For Logic story:**
- **Subsystem lifecycle**
  - Given: Empty game instance with clock source injected
  - When: `Initialize()` 호출
  - Then: Subsystem registered, delegates accessible, no crash
  - Edge cases: `Deinitialize()` 후 재호출 시 exception 없음
- **UDeveloperSettings CDO access**
  - Given: `UMossTimeSessionSettings` 정의
  - When: `UMossTimeSessionSettings::Get()` 호출
  - Then: CDO 포인터 반환, default 값 일치 (`GameDurationDays == 21` 등)
  - Edge cases: Project Settings → Game → Moss Baby → "Time & Session" 카테고리 렌더링 수동 확인

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/time-session/subsystem_skeleton_test.cpp` (Subsystem initialize/deinitialize + settings CDO access)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (IMossClockSource), Story 002 (FSessionRecord)
- Unlocks: Story 004, 005, 006
