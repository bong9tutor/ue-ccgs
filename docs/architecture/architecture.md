# Moss Baby — Master Architecture

## Document Status

- **Version**: 1.0 (Master Architecture — incremental authoring 완료 2026-04-18)
- **Last Updated**: 2026-04-18
- **Engine**: Unreal Engine 5.6 (hotfix 5.6.1 권장)
- **Review Mode**: lean (production/review-mode.txt) — Phase 7b LP-FEASIBILITY 자동 스킵, TD-ARCHITECTURE는 self-review 수행
- **GDDs Covered**: 14 MVP GDDs (전부 APPROVED 2026-04-18 기준)
  - Foundation: time-session-system.md (R3), data-pipeline.md (R3), save-load-persistence.md (R3+C1), input-abstraction-layer.md (R2)
  - Core: game-state-machine.md (R3+C2), moss-baby-character-system.md (R3), lumen-dusk-scene.md (L1)
  - Feature: growth-accumulation-engine.md (R4), card-system.md (R3), dream-trigger-system.md (L1), stillness-budget.md (R1)
  - Presentation: card-hand-ui.md (L1), dream-journal-ui.md (L1)
  - Meta: window-display-management.md (L1)
- **ADRs Referenced**: **13 Accepted** (2026-04-19 — ADR-0001~0013 모두 Accepted. Phase 6 참조)
- **Cross-review status**: 2026-04-18 CONCERNS verdict의 4건 blocking 모두 해소 (C-1, C-2, D-1 동기화 완료. **D-2 해소 by ADR-0005 Accepted 2026-04-19**)
- **Technical Director Sign-Off**: **APPROVED WITH CONCERNS** (2026-04-18, self-review — lean mode). 상세는 §Document Status (Phase 7b) 참조
- **Lead Programmer Feasibility**: SKIPPED (lean mode auto-skip)
- **다음 게이트**: `/gate-check pre-production` (모든 Required ADR 작성 + sign-off 완료 후)

## Engine Knowledge Gap Summary

UE 5.6은 2025년 6월 출시로 LLM 학습 컷오프(May 2025) 직후 버전. **HIGH RISK 1개 + MEDIUM RISK 5개** 도메인 식별. 모든 위험 도메인은 GDD 단계에서 5.6-VERIFY AC 또는 검증 task로 명시 적재되어 있어 Implementation 단계에서 처리 가능.

### HIGH RISK Domains

| Domain | 영향 시스템 | Mitigation |
|---|---|---|
| **Lumen HWRT GPU 비용** | Lumen Dusk Scene (#11) | 첫 구현 마일스톤에 GPU 프로파일링 게이트 (AC-LDS-04 [5.6-VERIFY]); 5ms budget 초과 시 SWRT 자동 폴백 분기 |
| **PSO Precaching 점진 컴파일** | Lumen Dusk Scene (#11 — 소유) | 첫 실행 시 PSO 번들 비동기 로드 + DegradedFallback 허용 (Rule 6, AC-LDS-11 ADVISORY) |

### MEDIUM RISK Domains

| Domain | 영향 시스템 | Mitigation |
|---|---|---|
| **AssetManager `PrimaryAssetTypesToScan` cooked-build 동작** | Data Pipeline (#2), Dream Trigger (#9) | DefaultEngine.ini 등록 + AC-DP-18 (R3 NEW, [5.6-VERIFY]) 검증 |
| **`FCoreDelegates` Windows lifecycle** (모바일 ApplicationWillEnterBackground는 Windows 비발화) | Save/Load (#3), Time (#1) | R2 4-trigger 패턴 (T1 CloseRequested / T2 ActivationStateChanged / T3 OnExit / T4 GameInstance Shutdown), R3 T2 `FlushOnly` 분리 |
| **`ensure*` Shipping strip** | Data Pipeline (#2 G.7 monotonicity guard) | R2: `checkf` 또는 명시적 `if+UE_LOG Fatal` 패턴 |
| **DataTable USTRUCT TMap 인라인 편집 미지원** | Growth Accumulation (#7 — `FFinalFormRow.RequiredTagWeights`) | CR-8 ADR 후보: `TArray<FTagWeightEntry>` 대체 vs UPrimaryDataAsset 격상 |
| **`FPlatformTime::Seconds` Windows suspend 동작** | Time & Session (#1) | OQ-1 Implementation 단계 실기 검증 (Win10/11 2종); fallback = `ACCEPTED_GAP + ADVANCE_SILENT` (플레이어 유리) |

### LOW RISK Domains (LLM training 범위 내)

Enhanced Input · UGameInstanceSubsystem · USaveGame + FFileHelper + IPlatformFile · UMG · MaterialParameterCollection · FTSTicker · UPrimaryDataAsset · Niagara (소형 씬 한정). 모두 5.6 변경 없거나 미미.

---

## System Layer Map (Phase 1)

systems-index R2의 5-layer 분류 채택. 14개 MVP 시스템 전부 매핑됨. 의존 방향은 항상 위 → 아래 (상위 layer가 하위 layer 소비). 순환 의존 0건 (systems-index §Circular Dependencies 확인됨).

```text
┌─────────────────────────────────────────────────────────────┐
│  PRESENTATION LAYER                                         │
│  • Card Hand UI (#12)         — drag-to-offer 인터랙션       │
│  • Dream Journal UI (#13)     — 읽기 전용 일기              │
└─────────────────────────────────────────────────────────────┘
                          ↓ depends on
┌─────────────────────────────────────────────────────────────┐
│  FEATURE LAYER                                              │
│  • Growth Accumulation Engine (#7)  — 태그 벡터 → 형태 매핑  │
│  • Card System (#8)                 — 일일 카드 풀 + 건넴    │
│  • Dream Trigger System (#9)        — 희소성 관리 (3-5회/21일)│
│  • Stillness Budget (#10)           — 동시 이벤트 상한 + RM  │
└─────────────────────────────────────────────────────────────┘
                          ↓ depends on
┌─────────────────────────────────────────────────────────────┐
│  CORE LAYER                                                 │
│  • Game State Machine (#5) (+ Visual Director 흡수 R2)       │
│  • Moss Baby Character (#6)                                 │
│  • Lumen Dusk Scene (#11) (re-layered R2; PSO 소유)          │
└─────────────────────────────────────────────────────────────┘
                          ↓ depends on
┌─────────────────────────────────────────────────────────────┐
│  FOUNDATION LAYER (병렬 설계 OK — 4개 시스템 간 의존 없음)    │
│  • Time & Session System (#1)  — 시간 분류기 + 8 rules       │
│  • Data Pipeline (#2)           — 카탈로그 + C1/C2 schema    │
│  • Save/Load Persistence (#3)  — atomic dual-slot ping-pong │
│  • Input Abstraction Layer (#4) — Enhanced Input wrapping    │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  META LAYER (cross-cutting)                                 │
│  • Window & Display Management (#14)  — depends on Input(#4) │
└─────────────────────────────────────────────────────────────┘
```

### Layer 별 핵심 책임 요약

| Layer | 시스템 수 | 핵심 책임 |
|---|---|---|
| **Foundation** | 4 | 병렬 설계 가능. 게임 시스템에 의존하지 않음. UE 엔진 API + OS 위에 직접 놓임. 모든 시스템의 기반 contract (Time delegates, Save/Load atomicity, Input actions, Pipeline pull APIs) 발행 |
| **Core** | 3 | Foundation 의존. 게임의 중앙 상태 + 시각 정체성 (GSM = 6-state + MPC orchestration, Moss Baby = 캐릭터 표현, Lumen Dusk = 환경 씬) |
| **Feature** | 4 | Foundation + Core 의존. 카드 의식 + 성장 누적 + 꿈 희소성 + 동시 이벤트 통제. Pillar 1·2·3의 직접 구현 |
| **Presentation** | 2 | Feature + Core 의존. 플레이어 대면 UI (Card Hand 드래그, Dream Journal 일기). UMG widgets |
| **Meta** | 1 | Cross-cutting. Window & Display는 모든 시스템의 시각 출력 컨테이너 (창 자체) + Save/Load에 FWindowState 위임 |

### Layer Boundary Rules (강제 사항)

1. **단방향 의존만 허용**: 상위 layer → 하위 layer. 역방향 의존 금지. 순환 의존 금지 (systems-index §Circular Dependencies 확인됨)
2. **Foundation 시스템 간 의존 0건**: Time / Data Pipeline / Save/Load / Input Abstraction은 서로 의존하지 않음 — 병렬 구현 가능
3. **Same-layer 의존 허용**: Feature 내 Card → Growth, GSM → Save/Load 같은 양방향 의존 가능 (단, 인터페이스 contract로 결합도 최소화)
4. **Meta는 cross-cutting**: Window & Display는 Foundation의 Input Abstraction에 의존하면서도 Save/Load에 FWindowState write — Meta layer의 특성상 허용
5. **Engine API 직접 호출은 Foundation/Meta에 기본 한정** — 예외: 해당 Core 시스템의 본질적 책임이 특정 엔진 서브시스템 통합이면 직접 호출 허용 (ADR에 명시 필수). **게임 로직** 수준의 엔진 API 사용은 Foundation 추상화를 거친다. 예: Card System은 `FDateTime` 직접 호출 금지 — Time System의 DayIndex 폴링만 허용. 예외: **GSM의 Light Actor 구동 (ADR-0004 hybrid)** + **Lumen Dusk Scene의 Lumen HWRT 설정 (ADR-0004)** — 렌더링 인프라 통합이 이들 시스템의 본질적 책임이므로 허용. Rule 5의 정신은 "업무 로직은 엔진에 직접 커플링되지 않는다"이며, 렌더링/씬 시스템은 본질적으로 엔진-adjacent

### HIGH/MEDIUM Risk Engine Domain Mapping

Foundation/Core 시스템의 위험 도메인 inline flag:

| System | Layer | Risk Domain | Reference |
|---|---|---|---|
| Lumen Dusk Scene | Core | **HIGH** Lumen HWRT, PSO Precaching | engine-reference: current-best-practices.md §1; Lumen Dusk GDD §C Rule 3, OQ-2 |
| Time & Session | Foundation | **MEDIUM** FCoreDelegates Windows lifecycle, FPlatformTime suspend | breaking-changes.md (모바일 vs Windows); Time GDD §States, OQ-1 |
| Data Pipeline | Foundation | **MEDIUM** AssetManager Cooked, ensure* Shipping strip | Data Pipeline GDD §R1 BLOCKER 4, §G.7 |
| Save/Load | Foundation | **MEDIUM** FCoreDelegates Windows lifecycle | Save/Load GDD §States §lifecycle 트리거 (T1-T4), R3 T2 분리 |
| GSM | Core | LOW (MPC scalar 자동 Light Actor 미반영은 ADR 사안 — 엔진 risk 아님) | GSM GDD OQ-6 |
| Dream Trigger | Feature | **MEDIUM** AssetManager PrimaryAssetType 등록 | Dream Trigger GDD Implementation Notes |

---

## Module Ownership (Phase 2)

각 모듈의 **Owns** (단독 소유 데이터/상태/책임) / **Exposes** (외부 인터페이스) / **Consumes** (의존 대상) / **Engine APIs** (직접 호출, risk noted) 매핑. 5 layer × 14 시스템 = 56 cells.

### Foundation Layer

| System | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Time & Session (#1)** | `FSessionRecord` content (DayIndex/NarrativeCount/SessionUuid/PlaythroughSeed/LastSaveReason 의미적 소유), `IMossClockSource` interface, `ETimeAction` + `EFarewellReason` enums, 8-rule classifier, NarrativeCap 강제 | `FOnTimeActionResolved`, `FOnDayAdvanced(int32)`, `FOnFarewellReached(EFarewellReason)`, `IncrementNarrativeCountAndSave()` (single public method), `DayIndex`/`SessionCountToday` polling | Save/Load `Get<FSessionRecord>()` + `SaveAsync(ENarrativeEmitted)`, `IMossClockSource` injection (FRealClockSource production / FMockClockSource test) | `UGameInstanceSubsystem` (LOW), `FDateTime::UtcNow()` (LOW), `FPlatformTime::Seconds()` **MEDIUM** (Windows suspend 동작), `FTSTicker::GetCoreTicker().AddTicker` (LOW), `FCoreDelegates::ApplicationWillEnter*` (모바일 전용 — Windows 미발화 명시), `FWorldDelegates::OnPostWorldInitialization` (LOW), `UDeveloperSettings` for Tuning (OQ-5 RESOLVED) |
| **Data Pipeline (#2)** | `UDataPipelineSubsystem` 4-state machine (`EDataPipelineState`), 4 catalog 등록 순서 (Card→FinalForm→Dream→Stillness), C1 schema gate (수치 stat 필드 금지), C2 schema gate (Dream 임계 외부 노출), `MaxCatalogSize*` constants, fail-close 정책, DegradedFallback | `Pipeline.GetCardRow(FName)`, `GetAllCardIds()`, `GetGrowthFormRow(FName)`, `GetAllGrowthFormRows()`, `GetAllDreamAssets()`, `GetCardIconPath(FName)`, `GetStillnessBudgetAsset()`, `GetDreamBodyText(FName)`, `GetFailedCatalogs()`, `IsReady()`, `RefreshCatalog()` | UE Asset Manager (forwarded calls) | `UGameInstanceSubsystem` (LOW), `UDataTable` (LOW), `UPrimaryDataAsset` (LOW), `UAssetManager::GetPrimaryAssetIdList(FPrimaryAssetType)` **MEDIUM** (PrimaryAssetTypesToScan cooked 검증), `FSoftObjectPath` / `TSoftObjectPtr<>` (LOW), `UEditorValidatorBase` (LOW, Editor only), `checkf` / `UE_LOG Fatal` for monotonicity guard (Shipping-safe — `ensureMsgf` strip 회피) **MEDIUM**, `Collection.InitializeDependency<>()` for Subsystem ordering (LOW) |
| **Save/Load Persistence (#3)** | `UMossSaveData : USaveGame` container (sole writer), dual-slot ping-pong + atomic rename, `ESaveReason` enum (4 values, ordinal stable), `SaveSchemaVersion` const, `MinCompatibleSchemaVersion` knob, `WriteSeqNumber` overflow reset 절차, `FMossSaveSnapshot` POD-only contract, `bSaveDegraded` flag, atomicity contracts (per-trigger) | `SaveAsync(ESaveReason)` (coalesced), typed slice accessors (`Get<FSessionRecord>`, `Get<FGrowthState>`, etc.), `FOnLoadComplete(bool bFreshStart, bool bHadPreviousData)` (one-shot), `FOnSaveDegradedReached(FMossSlotMetadata)`, `GetLastValidSlotMetadata() → TOptional<>`, `IsDegraded() → bool` | (none — Foundation 최하단; Time/Growth/Dream/Farewell이 양방향) | `USaveGame` (LOW), `UGameplayStatics::SaveGameToMemory/LoadGameFromMemory` (LOW), `FFileHelper::SaveArrayToFile` (LOW), `IPlatformFile::MoveFile` (NTFS atomic — Windows 한정 LOW), `FCrc::MemCrc32` (seed=0 고정 — R3 CRITICAL-4) (LOW), `FPlatformProcess::UserSettingsDir` (LOW, 비-ASCII 한국어 사용자명 E15 검증 필요), `Async(EAsyncExecution::ThreadPool)` + `TFuture<>` (LOW), `UGameViewportClient::CloseRequested` (T1) **MEDIUM**, `FSlateApplication::OnApplicationActivationStateChanged` (T2 FlushOnly) **MEDIUM**, `FCoreDelegates::OnExit` (T3) (LOW), `UGameInstance::Shutdown` (T4) (LOW), `FThreadSafeBool` for re-entry guard (LOW) |
| **Input Abstraction Layer (#4)** | 6개 `UInputAction` 자산 (`IA_PointerMove`/`IA_Select`/`IA_OfferCard`/`IA_NavigateUI`/`IA_OpenJournal`/`IA_Back`), 2개 `UInputMappingContext` (`IMC_MouseKeyboard`/`IMC_Gamepad`), `EInputMode` enum, hysteresis (`InputModeMouseThreshold` 기본 3px), Hover-only 금지 contract, `OfferDragThresholdSec`/`OfferHoldDurationSec` Tuning Knobs | `OnInputModeChanged(EInputMode)` delegate, action 바인딩 인터페이스 (`UEnhancedInputComponent::BindAction` 경유), `EInputMode` getter | `APlayerController` lifecycle (단일, 레벨 전환 없음) | UE 5.6 Enhanced Input plugin **LOW** (5.6 변경 없음 — 2026-04-17 WebSearch 확인), `UInputAction` (DataAsset, LOW), `UInputMappingContext` (LOW), `UEnhancedInputLocalPlayerSubsystem::AddMappingContext` (LOW), `UEnhancedInputComponent::BindAction(Action, ETriggerEvent, ...)` (LOW), `UInputTriggerHold` (HoldTimeThreshold 활용 — LOW), `APlayerController::SetShowMouseCursor` (LOW), `UEnhancedPlayerInput::GetLastInputDeviceType` (LOW) |

### Core Layer

| System | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Game State Machine (#5)** | `UMossGameStateSubsystem`, `EGameState` 6-state (Menu/Dawn/Offering/Waiting/Dream/Farewell), 4-tier 우선순위 (P0-P3) 전환 시스템, `bWithered` 서브컨디션, MPC blend orchestration (Formula 1 SmoothStep), `UGameStateMPCAsset` 8 parameter set, Visual State Director (R2 흡수 — MPC mapping), Dawn min dwell, CardSystem Ready timeout, Dream defer logic | `FOnGameStateChanged(EGameState OldState, EGameState NewState)` (BlueprintAssignable), `OnPrepareRequested()` (Card에 직접 호출), MPC 파라미터 갱신 (downstream 머티리얼이 자동 참조) | Time `FOnTimeActionResolved`, Save/Load `FOnLoadComplete` + `LastPersistedState` field, Stillness Budget `Request(Narrative)` for Dream/Farewell, Card `OnCardSystemReady` + `FOnCardOffered` | `UGameInstanceSubsystem` (LOW), `UMaterialParameterCollection` + `UMaterialParameterCollectionInstance::SetScalarParameterValue/SetVectorParameterValue` (LOW), `FTSTicker` for MPC Lerp (LOW), `GetWorld()` + Subsystem lazy init pattern with `TWeakObjectPtr` (LOW). **MEDIUM (ADR 사안)**: MPC scalar는 Light Actor의 Temperature/Intensity 자동 반영 안 함 — OQ-6 BLOCKING ADR (Light Actor 직접 제어 vs Post-process LUT vs Blueprint 바인딩) |
| **Moss Baby Character (#6)** | `AMossBaby : AActor`, 5-state machine (`ECharacterState`: Idle/Reacting/GrowthTransition/Drying/FinalReveal — 우선순위 FinalReveal > GrowthTransition > Reacting > Drying/Idle), 6 머티리얼 파라미터 (SSS_Intensity/HueShift/EmissionStrength/Roughness/DryingFactor/AmbientMoodBlend), `UMossBabyPresetAsset` (성장 단계별 프리셋), Formula 4 (지수 감쇠 τ_E=0.1, τ_S=0.33), Formula 5 (DryingOverlay 선형 보간), MaterialHints clamp 책임 | (no delegates exposed — pure consumer) Visual representation (rendering output) | Growth `FOnGrowthStageChanged` + `FOnFinalFormReached` + `GetMaterialHints()` + `GetLastCardDay()`, Card `FOnCardOffered`, GSM `FOnGameStateChanged` + MPC 파라미터 read-only, Stillness Budget `Request(Motion, Standard/Narrative)`, Save/Load DayIndex (간접 경유) | Static Mesh `SetStaticMesh()` (LOW), `UMaterialInstanceDynamic::SetScalarParameterValue` (LOW), Subsurface Scattering material features (LOW). **MVP**: 블렌드 셰이프·스켈레탈 리깅 미사용 (Static Mesh + MID만). VS에서 Niagara 검토 |
| **Lumen Dusk Scene (#11)** | Scene composition (`SM_GlassJar`/`SM_Desk`/`SM_Background_Objects`), 고정 카메라 (FOV 35°, Pitch -10°, position (0,-50,15)cm — 절대 이동 금지), DoF Formula 1 (`BaseFStop` + `DreamFStopDelta × DreamBlend`), `PP_MossDusk` Post Process Volume (Vignette Farewell increase, MotionBlur 강제 비활성), `ULumenDuskAsset` Tuning Knobs (12개), **PSO Precaching pipeline** (R6 — `PSO_MossDusk.bin` 비동기 로드 + DegradedFallback) | Visual environment (rendering output), MPC 소비자 — 자동 따라가기 | GSM `FOnGameStateChanged` + MPC 8 파라미터 read-only, Stillness Budget `Request(Particle/Motion, Background)` for ambient particles · light blend, Data Pipeline (PSO 번들 경로 조회), Window&Display 해상도/DPI 변경 이벤트 | **HIGH RISK**: `r.Lumen.HardwareRayTracing 1`, `r.Lumen.MaxTraceDistance 300`, `r.Lumen.SurfaceCacheResolution 0.5`, Lumen Surface Cache (~75MB), HWRT BVH (~20-30MB) — first milestone GPU 프로파일링 게이트 의무. **HIGH RISK**: `FShaderPipelineCache::SetBatchMode(BatchMode::Fast)` + `NumPrecompilesRemaining()` polling (PSO Precaching). DirectionalLight + SkyLight Mobility 설정 (Stationary/Movable), `UPostProcessVolume` (LOW), Niagara `BP_AmbientParticle` (LOW MVP) |

### Feature Layer

| System | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Growth Accumulation Engine (#7)** | `FGrowthState` (semantic) — TagVector + CurrentStage + LastCardDay + TotalCardsOffered + FinalFormId, `FFinalFormRow` field semantics (Pipeline forward-decl 인수), `FGrowthMaterialHints` 반환 struct (raw 오프셋 — clamp는 MBC 책임), CR-1 태그 가산 + atomic `SaveAsync(ECardOffered)`, F1-F5 공식 (Tag accumulation / Vector norm / Form Score 내적 / MaterialHints / MVP DayScaling), CR-3 빈 Tags Schema Guard, `UGrowthConfigAsset` Tuning Knobs | `FOnGrowthStageChanged(EGrowthStage)`, `FOnFinalFormReached(FName)`, `GetTagVector() → const TMap<FName, float>&`, `GetCurrentStage() → EGrowthStage`, `GetMaterialHints() → FGrowthMaterialHints`, `GetLastCardDay() → int32` | Card `FOnCardOffered`, Time `FOnDayAdvanced`, Data Pipeline `GetCardRow` + `GetAllGrowthFormRows`, Save/Load `Get<FGrowthState>` + `SaveAsync(ECardOffered)` | `TMap<FName, float>` (LOW), `TMap<FName, FFinalFormRow>` for catalog (LOW). **CR-8 ADR 사안**: `FFinalFormRow.RequiredTagWeights: TMap<FName, float>` UE DataTable 인라인 편집 미지원 — `TArray<FTagWeightEntry>` 대체 vs UPrimaryDataAsset 격상 결정 필요 |
| **Card System (#8)** | 4-state machine (`Uninitialized`/`Preparing`/`Ready`/`Offered` — 모두 비영속), CR-1 일일 카드 풀 구성 (deterministic seed `HashCombine(PlaythroughSeed, DayIndex)`), CR-3 Prepare/Ready 프로토콜 + 5초 timeout, CR-4 ConfirmOffer 검증 게이트 (3 가드: state/Contains/bHandOffered), CR-5 비영속화 설계 (PlaythroughSeed만 Save/Load 영속), CR-6 카드 필드 의미, F-CS-1/2/3 (Season + CardWeight + SelectionProb), `UCardSystemConfigAsset` | `GetDailyHand() → TArray<FName>`, `ConfirmOffer(FName CardId) → bool`, `OnCardSystemReady(bool bDegraded)` (GSM 직접 호출), `FOnCardOffered(FName CardId)` (BlueprintAssignable, 하루 최대 1회 보장) | Time `FOnDayAdvanced(int32)`, GSM `OnPrepareRequested()`, Data Pipeline `GetAllCardIds` + `GetCardRow`, Save/Load `FSessionRecord.PlaythroughSeed` read, Growth `GetLastCardDay()` for bHandOffered 복원 (OQ-CS-2 ADR) | `FRandomStream` (LOW — 결정론적 시드 + 전역 RNG 비오염), `UPrimaryDataAsset` for ConfigAsset (LOW), `Collection.InitializeDependency<UDataPipelineSubsystem>()` (LOW), `FTSTicker` for 5s timeout (LOW). **ADR 후보**: `FGameplayTagContainer` vs `TArray<FName>` for Tags (Implementation Notes), DYNAMIC vs non-dynamic delegate 선택 (Implementation Notes) |
| **Dream Trigger System (#9)** | `FDreamState` semantic (UnlockedDreamIds + LastDreamDay + DreamsUnlockedCount + PendingDreamId), `UDreamDataAsset` field 소유권 (Pipeline forward-decl 인수 — DreamId/BodyText/TriggerTagWeights/TriggerThreshold/RequiredStage/EarliestDay), 4-state machine (Dormant/Evaluating/Triggered/Cooldown), CR-2 dual rarity gates (cooldown + cap), F1 TagScore 내적, F2 DaysSinceLastDream, F3 frequency target 검증, EC-1 Day 21 강제 트리거 (MinimumGuaranteedDreams), `UDreamConfigAsset` (MinDreamIntervalDays/MaxDreamsPerPlaythrough/MinimumGuaranteedDreams) | `FOnDreamTriggered(FName DreamId)` (BlueprintAssignable), `GetUnlockedDreamIds() → TArray<FName>` (Dream Journal UI 소비) | Card `FOnCardOffered`, Growth `GetTagVector()` + `GetCurrentStage()`, Data Pipeline `GetAllDreamAssets()` + `GetDreamBodyText`, Save/Load `Get<FDreamState>` + `SaveAsync(EDreamReceived)`, GSM `FOnGameStateChanged` (PendingDreamId 클리어 — OQ-DTS-2 RESOLVED) | `UPrimaryDataAsset` for Dream assets (LOW), **MEDIUM**: `UAssetManager::GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"))` — DefaultEngine.ini의 PrimaryAssetTypesToScan에 DreamData 등록 의무 (Data Pipeline R2 BLOCKER 4), `Collection.InitializeDependency<UDataPipelineSubsystem>()` (LOW), `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam` (LOW) |
| **Stillness Budget (#10)** | `EStillnessChannel` (Motion/Particle/Sound — 채널 간 슬롯 공유 없음), `EStillnessPriority` (Background/Standard/Narrative), `FStillnessHandle` (RAII, move-only, idempotent Release), 슬롯 lifecycle (Request/Release, no timeout), preemption rules (고우선이 저우선 선점, 동일 우선순위 FIFO), Reduced Motion API (`bReducedMotionEnabled` Motion/Particle 0 취급, Sound 영향 없음), Formula 1 (CanGrant) + Formula 2 (EffLimit), DegradedFallback (UStillnessBudgetAsset nullptr → 전면 Deny), `UStillnessBudgetAsset` (MotionSlots=2, ParticleSlots=2, SoundSlots=3) | `Request(EStillnessChannel, EStillnessPriority) → FStillnessHandle`, `Release(FStillnessHandle)` (idempotent), `IsReducedMotion() → bool`, `OnBudgetRestored(EStillnessChannel)` delegate (RM OFF 복귀 시) | Data Pipeline `GetStillnessBudgetAsset()` (Initialize 1회 pull + 캐싱), Accessibility Layer (VS — Reduced Motion toggle 변경) | `UPrimaryDataAsset` (LOW), RAII pattern (LOW). **ADR 후보**: UE Sound Concurrency / Niagara Scalability와의 통합 전략 (OQ-1) — 게임 레이어 예산이 엔진 내장 concurrency와 협업 방식 |

### Presentation Layer

| System | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Card Hand UI (#12)** | 6-state machine (Hidden/Revealing/Idle/Dragging/Offering/Hiding), drag spatial logic (커서 추적 + 카드 추적), Offer Zone (`OfferZoneRadiusPx` 기본 80px 원형 — F-CHU-3), Reveal sequence (좌→중→우 staggered), confirm offer animation (0.5s), card return animation (0.3s), Hover-only 금지 강제, `UCardHandUIConfigAsset`, F-CHU-1 카드 수평 배치 (3장), Gamepad 대체 인터랙션 (CR-CHU-9 — VS 구현) | Card 위젯 표시, 드래그 인터랙션 (player input layer) | Card `GetDailyHand` + `ConfirmOffer`, Input Abstraction `IA_OfferCard` Hold + `IA_Select` + `IA_PointerMove` + `IA_NavigateUI` + `OnInputModeChanged`, GSM `FOnGameStateChanged` (Offering 진입/이탈), Stillness Budget `Request(Motion/Particle, Standard)`, Data Pipeline `GetCardRow` for display data | UMG `UUserWidget` (LOW), `UCanvasPanel` + `UHorizontalBox` (LOW), `NativeOnMouseButtonDown/Move` 또는 UMG DragDropOperation (**ADR 후보** OQ-CHU-1 — UMG 네이티브 vs Enhanced Input subscription), `UWidgetInteractionComponent` 또는 `UGameViewportClient::WorldToScreen()` for jar screen position (LOW), `FTSTicker` for jar position update (LOW), `TSoftObjectPtr<UTexture2D>` async load for card icons (LOW), `UUserInterfaceSettings::GetDPIScaleBasedOnSize()` for OfferZoneRadiusPx DPI 보정 (OQ-CHU-3) |
| **Dream Journal UI (#13)** | 4-state machine (Closed/Opening/Reading/Closing), 진입점 가시성 (Waiting state에서만 활성 — CR-1 Access Control), `bHasUnseenDream` 소비 (icon pulse), 일기 페이지 navigation (이전/다음/목록), 빈 상태 처리 (`EmptyStateText` FText), 슬롯 indicator 정책 (채워진 슬롯만 — OQ-DJ-2), `UDreamJournalConfig` (FontSize/PageTransitionDurationSec/OpenCloseDurationSec/IconPulseDurationSec), F-DJ-1 가시 행 수 + F-DJ-2 가시 항목 수 | (no delegates — pure consumer) Dream journal display, icon pulse rendering | Save/Load `UnlockedDreamIds` + `bHasUnseenDream` (read-only), Data Pipeline `GetDreamBodyText(FName)`, GSM `FOnGameStateChanged` (Waiting 진입/이탈, Dream/Farewell 강제 닫기), Stillness Budget `Request(Motion, Standard)` for transitions, Input Abstraction `IA_OpenJournal` + `IA_Back` + `IA_NavigateUI` | UMG `UUserWidget` (LOW), `UWidgetSwitcher` for page navigation (LOW), `UWidgetAnimation` (LOW), `Rich Text Block` for poetic typography (LOW), `FText::FromStringTable()` for localization (LOW). **HARD CONSTRAINT**: `SaveAsync()` 직접 호출 금지 (AC-DJ-17 grep 검증) — `bHasUnseenDream = false`는 in-memory 변경만 |

### Meta Layer

| System | Owns | Exposes | Consumes | Engine APIs (risk) |
|---|---|---|---|---|
| **Window & Display (#14)** | `EWindowDisplayMode` (Windowed/BorderlessWindowed/Fullscreen), `FWindowState` (Position/Size/bAlwaysOnTop/WindowMode/DPIScaleOverride), 최소 800×600 강제, 기본 1024×768, Always-on-top toggle, DPI scale 계산 (Formula 1: `SystemDPI / 96` clamp [0.5, 3.0]), 씬 비율 letterbox (Formula 2 — 4:3 유지), focus event debounce (50ms), 모달 절대 금지, Steam Deck Fullscreen 자동 전환 | `OnWindowFocusLost()` callback, `OnWindowFocusGained()` callback, `FWindowState` to Save/Load (영속화), `GetCurrentDPIScale() → float` | Input Abstraction `OnInputModeChanged(EInputMode)` for cursor visibility, Save/Load (`FWindowState` write) | `SWindow` + `FArguments::MinWidth/MinHeight` 강제 (LOW), `FGenericWindow::SetWindowMode` (LOW), `FDisplayMetrics::PrimaryDisplayWidth/Height` (LOW), Per-Monitor DPI Awareness V2 — `app.manifest` `<dpiAwareness>PerMonitorV2</dpiAwareness>` (LOW), `WM_DPICHANGED` 처리 (LOW), Windows native API `::SetWindowPos(HWND_TOPMOST)` for Always-on-top (LOW, Steam Deck Proton 호환 OQ-4 검증 필요), `UGameViewportClient::Activated/Deactivated` for minimize 감지 (LOW), `FCoreDelegates` focus events **MEDIUM** (Windows 발화 보장 확인 필요), `FTimerHandle` for focus debounce (LOW) |

### ASCII Dependency Diagram

```text
                                     ┌─────────────────────────────┐
                                     │  Card Hand UI #12 (drag)    │
                                     │  Dream Journal UI #13       │
                                     └─────┬───────────────────────┘
                                           │ (UMG depends on)
                ┌──────────────────────────┴──────────────────────────────────────┐
                │                                                                 │
                ▼                                                                 ▼
     ┌─────────────────────┐    FOnCardOffered    ┌─────────────────────┐
     │  Card System #8     │ ───────────────────► │  Growth #7          │
     │                     │                       │                     │
     │ - DailyHand         │                       │ - FGrowthState      │
     │ - ConfirmOffer()    │                       │ - GetTagVector()    │
     └───┬─────────────┬───┘                       └──┬──────────────────┘
         │             │                              │ GetMaterialHints
         │             ▼                              ▼
         │   ┌─────────────────────┐         ┌─────────────────────┐
         │   │  Dream Trigger #9   │         │  Moss Baby #6       │
         │   │  (FOnCardOffered)   │         │  (Reacting/Growth)  │
         │   └──────────┬──────────┘         └──────────┬──────────┘
         │              │ FOnDreamTriggered             │
         │              ▼                                │
         │   ┌─────────────────────┐                    │ Request(Motion,...)
         │   │  Stillness Budget   │◄───────────────────┘
         │   │  #10 (FStillnessHandle)
         │   └──────────┬──────────┘
         │              ▲
         │              │ Request(Particle, Background)
         │              │
         ▼              │
     ┌─────────────────────┐         ┌─────────────────────┐         ┌─────────────────────┐
     │  Game State Machine │◄────────┤  Lumen Dusk #11     │         │  Window&Display #14  │
     │  #5 (+Visual Dir.)  │ FOnGSC  │  (PSO + scene)      │         │  (Meta — focus)     │
     │  - EGameState 6     │────────►│                     │         └──────────┬──────────┘
     │  - MPC orchestration│  MPC    └─────────────────────┘                    │
     └────┬───────┬───────┘  read                                               │
          │       │                                                             │
          │       │ FOnLoadComplete                                             │
          │       ▼                                                             ▼
          │  ┌─────────────────────┐  ┌─────────────────────┐  ┌─────────────────────┐
          │  │  Save/Load #3       │  │  Time & Session #1   │  │  Input Abstraction #4│
          │  │  - USaveGame        │  │  - 8 rules           │  │  - 6 InputAction    │
          │  │  - dual-slot        │◄─┤  - FSessionRecord    │  │  - 2 IMC            │
          │  │  - ESaveReason      │  │  - FOnDayAdvanced    │  │  - OnInputModeChanged│
          │  └─────────────────────┘  └─────────────────────┘  └─────────────────────┘
          │   (Foundation — 4개 시스템 간 의존 0건; UE engine 위에 직접)
          │
          └────────────────────────────────────────────────────────────────────►
                                                                            ▲
                                                                            │ FOnTimeActionResolved
                                                                            │
```

**핵심 관찰**:
- Foundation 4개 시스템 간 의존 0건 (병렬 설계 가능)
- `FOnDayAdvanced`(Time 발행)는 4개 시스템 구독: GSM(via Resolved), Card, Growth(via Card 경유 + 자체), Dream(자체 평가 시)
- `FOnCardOffered`(Card 발행)는 4개 시스템 구독: Growth, GSM, MBC, Dream Trigger — **OQ-CS-3 BLOCKING ADR**: UE Multicast 등록 순서 비결정성으로 Day 21에 Growth(태그 가산) 먼저 → GSM(Farewell P0) 나중 보장 필요
- Stillness Budget은 7개 시스템이 구독 (GSM, MBC, Card Hand UI, Dream Journal UI, Lumen Dusk, Audio System VS, Dream Trigger 간접) — cross-cutting concurrency budget
- MPC는 GSM이 유일 writer, MBC + Lumen Dusk가 read-only consumer

---

## Data Flow (Phase 3)

GDD에서 추출한 6개 데이터 흐름 시나리오 + 3개 critical race condition.

### 3.1 Initialisation Order (앱 시작 시 1회)

```text
UGameInstance::Init()
  ↓
Subsystem 자동 생성 (순서 비결정적 by default)
  ↓ 각 Subsystem이 Initialize() 내에서 InitializeDependency<>() 호출로 결정성 강제
  ↓
[1] Data Pipeline.Initialize()
    - 4 카탈로그 R2 순서 등록: Card DT → FinalForm DT → Dream DataAsset → Stillness Budget DA
    - 첫 프레임 전 < 50ms (D2 budget, T_init Full ≈ 8.2ms SSD warm)
    - 결과: Ready 또는 DegradedFallback
  ↓
[2] Save/Load.Initialize()
    - 슬롯 A·B 헤더 읽기 → Formula 1 (TrustedSlot) → Formula 3 (Validity) → Formula 4 (Migration steps)
    - 두 슬롯 모두 Invalid → FreshStart 모드 (default-constructed FSessionRecord)
    - 마이그레이션 chain in-memory 적용 (디스크 미쓰기)
    - 발행: FOnLoadComplete(bFreshStart, bHadPreviousData) — 4-quadrant 신호
  ↓
[3] Time & Session.Initialize()
    - IMossClockSource 주입 (FRealClockSource)
    - Save/Load.Get<FSessionRecord>() pull
    - GetWorld() null — timer 등록·Action 발행 금지
    - FWorldDelegates::OnPostWorldInitialization 구독
  ↓
[4] GSM, Card, Growth, Dream Trigger, Stillness Budget, Window&Display.Initialize()
    - 각각 Pipeline 의존 선언 (InitializeDependency<UDataPipelineSubsystem>)
    - Stillness Budget: GetStillnessBudgetAsset() 1회 pull + 캐싱
    - Card: PlaythroughSeed read (FreshStart면 FMath::Rand() 생성)
    - Save/Load의 FOnLoadComplete 구독 (GSM, Title UI)
  ↓
FWorldDelegates::OnPostWorldInitialization 발화
  ↓
[5] Time.OnWorldReady() → ClassifyOnStartup() → ETimeAction 발행
    - FTSTicker 1Hz tick 등록 (게임 pause 무관)
    - 8 rules 평가 → ETimeAction 결정 → FOnTimeActionResolved 브로드캐스트
  ↓
[6] GSM 초기 상태 결정 (E12 buffering 처리: FOnLoadComplete 미수신 상태에서 Time Action 도착 시 단일 버퍼 보관)
    - START_DAY_ONE → Menu→Dawn
    - ADVANCE_SILENT/WITH_NARRATIVE + DayIndex 변화 → Dawn→Offering
    - LONG_GAP_SILENT → Menu→Farewell P0
    - 같은 날 + 카드 건넴 완료 → Menu→Waiting
    - 같은 날 + 카드 미건넴 → Menu→Offering
  ↓
Game loop 진입 (60fps render + 1Hz Time tick + on-demand UI input)
```

**핵심 보장**:
- Pipeline → Save/Load → Time → 다른 시스템 순서가 `InitializeDependency<>()`로 강제됨 (`PostInitialize` 사용 금지 — lifecycle 모호)
- Time는 `Initialize()` 시점에 timer 등록 금지 (GetWorld() null) — `OnWorldReady` 콜백에서 시작
- GSM은 두 신호(`FOnLoadComplete` + `FOnTimeActionResolved`)를 buffering — 순서 역전 허용 (E12)

### 3.2 Frame Update Path (60fps loop)

```text
[Game Thread, every frame]
Enhanced Input poll
  → APlayerController → UEnhancedInputComponent::BindAction handlers
  → Card Hand UI / Dream Journal UI / GSM (IA_Back routing) 분기

[Game Thread, 1Hz FTSTicker]
Time.TickInSession()
  → IMossClockSource 폴 → 분류기 (Rule 5-8) → ETimeAction 결정
  → 1Hz tick은 FTSTicker (pause-independent), FTimerManager 사용 금지
  → focus loss(WM_ACTIVATE) 동안에도 계속 tick (Pillar 1)

[Game Thread, every frame Tick]
GSM.TickMPC() (FTSTicker 0초 콜백 — 매 frame)
  → MPC scalar Lerp 진행 (Formula 1 SmoothStep)
  → UMaterialParameterCollectionInstance::SetScalarParameterValue
  → 캐싱된 FName 파라미터 이름 사용 (per-frame 탐색 비용 절감)

MBC.Tick (Actor Tick)
  → 머티리얼 파라미터 Lerp (Formula 1 GrowthTransition)
  → 브리딩 사인파 (Formula 3, Idle 상태에서만)
  → 카드 반응 지수 감쇠 (Formula 4, Reacting 상태에서)
  → MID SetScalarParameterValue per-frame

Card Hand UI.NativeTick (Dragging 상태)
  → 드래그 중 카드를 커서 위치로 이동
  → jar screen position 갱신 (WorldToScreen — FTSTicker 또는 UMG Tick, OQ-CHU-4)
  → Offer Zone hit-test (Formula F-CHU-3)

[Render Thread]
Lumen GI (HWRT) computation — 5ms budget (LumenGPUBudgetMs)
MPC scalar 참조 → 머티리얼 셰이더 sample
MID parameter sample → 픽셀 셰이더
Niagara particle simulation (Background 슬롯 점유 시만)
PSO 필요 시 점진 컴파일 (UE 5.6 변경 — 첫 실행 후에는 cached)
```

**핵심 관찰**:
- Game loop는 Time/GSM/Save 시스템을 직접 폴링하지 않음 — 모두 이벤트 기반
- `FTSTicker` 사용 (engine-level, pause-independent) — `FTimerManager`는 pause 시 정지로 사용 금지
- Stillness Budget은 Tick 없음 (순수 쿼리 서비스 — Request/Release 시점에만)
- Save/Load는 Tick 없음 (이벤트 기반)

### 3.3 Event / Signal Path (12 delegate flows)

| Producer | Delegate | Subscribers | Notes |
|---|---|---|---|
| Time #1 | `FOnTimeActionResolved(ETimeAction)` | GSM #5 | 1Hz tick + startup classify, BlueprintAssignable |
| Time #1 | `FOnDayAdvanced(int32 NewDayIndex)` | Card #8, Growth #7, Dream #9, Data Pipeline #2 (forward-decl) | Day 전진 시 1회 |
| Time #1 | `FOnFarewellReached(EFarewellReason)` | Farewell Archive #17 (VS) | 정상 완주 또는 LongGapAutoFarewell |
| Save/Load #3 | `FOnLoadComplete(bool bFreshStart, bool bHadPreviousData)` | GSM #5, Title UI #16 (VS) | one-shot, 4-quadrant, Initialize 종료 후 1회 |
| Save/Load #3 | `FOnSaveDegradedReached(FMossSlotMetadata)` | Farewell Archive #17 (VS) | E10 양 슬롯 Invalid 시 1회 |
| GSM #5 | `FOnGameStateChanged(EGameState OldState, EGameState NewState)` | MBC #6, Lumen Dusk #11, Card Hand UI #12, Dream Journal UI #13, Dream #9 (PendingDreamId clear), Audio #15 (VS), Title UI #16 (VS), Farewell #17 (VS) | TwoParams — OldState 포함, MPC Lerp 시작 시점에 발행 |
| Card #8 | `FOnCardOffered(FName CardId)` | Growth #7, GSM #5, MBC #6, Dream Trigger #9 | **🔴 OQ-CS-3 BLOCKING ADR-0005**: UE Multicast 등록 순서 비결정성 — Day 21에 Growth 먼저 → GSM Farewell P0 보장 필요 |
| Growth #7 | `FOnGrowthStageChanged(EGrowthStage)` | MBC #6 | 단계 변경 시 1회 (다단계 점프는 최종 단계만) |
| Growth #7 | `FOnFinalFormReached(FName)` | MBC #6, Farewell #17 (VS) | Day 21 1회, 로드 시 Resolved 직행이면 재발행 |
| Dream #9 | `FOnDreamTriggered(FName DreamId)` | GSM #5 | Waiting→Dream 전환 요청. GSM이 Stillness Budget Narrative 슬롯 획득 후 전환 |
| Stillness Budget #10 | `OnBudgetRestored(EStillnessChannel)` | 7 consumers (구독자 별) | ReducedMotion OFF 복귀 시 채널별 1회 |
| Input #4 | `OnInputModeChanged(EInputMode)` | Card Hand UI #12, Dream Journal UI #13, Window&Display #14, GSM #5 | Hysteresis (Formula 1) 통과 시 |

**Callback (delegate 아님)**:
- Window&Display → Time: `OnWindowFocusLost()` / `OnWindowFocusGained()` — focus event debounce 50ms
- Card → GSM: `OnCardSystemReady(bool bDegraded)` — 직접 호출 (delegate 아님 — Card→GSM 양방향 직접 통신)
- GSM → Card: `OnPrepareRequested()` — 직접 호출

**Loose coupling 원칙**:
- Producer는 subscriber 존재 여부 모름 (Multicast Delegate)
- Subscriber는 직접 producer 상태 read 금지 — 오직 delegate payload + pull API
- 예외: GSM ↔ Card는 Prepare/Ready 직접 호출 (양방향 hard coupling — Dawn→Offering 게이트)

### 3.4 Save / Load Path (per-trigger atomic)

```text
[Trigger Sources — 4 ESaveReason]
ECardOffered      = 0  (Growth.CR-1 후 즉시)
EDreamReceived    = 1  (Dream.CR-4 후 즉시)
ENarrativeEmitted = 2  (Time.IncrementNarrativeCountAndSave 내부)
EFarewellReached  = 3  (GSM.Farewell entry)

[SaveAsync(reason) Flow — Save/Load §Detailed Design §2]

[Game Thread]
1. in-memory mutation 먼저 (Growth/Dream/Time/GSM이 자기 슬라이스 갱신)
   → atomicity contract: 같은 game-thread 함수 호출 안에서 SaveAsync 호출 (E24-E25)
2. Save/Load.SaveAsync(reason) called
3. FMossSaveSnapshot::FromSaveData(SaveData) — 값 복사 (POD-only contract, ~1ms)
   → UObject* 필드는 flatten (예: UDreamDataAsset* → FName DreamId)
4. lambda enqueue → thread pool worker → game thread 즉시 return

[Worker Thread]
5. Target slot 결정 — 활성의 반대 (ping-pong, FreshStart 예외: A 고정)
6. NextWSN = max(A.WSN, B.WSN) + 1 (uint32 wrap detection — Formula 2 §Overflow Safety)
7. UGameplayStatics::SaveGameToMemory(snapshot, OutBytes)
8. CRC32 = FCrc::MemCrc32(OutBytes, 0)  (seed=0 고정 — R3 CRITICAL-4)
9. Header build: Magic + SaveSchemaVersion + WSN + length + CRC
10. FFileHelper::SaveArrayToFile(MossData_Target.tmp)
11. IPlatformFile::MoveFile(MossData_Target.sav, .tmp)  (NTFS atomic on Windows)

[Game Thread Callback]
12. in-memory active pointer 갱신 (Target = trusted)
13. 반대 슬롯 잔여 .tmp 삭제 (best-effort)
14. coalesce 정책: 진행 중 새 SaveAsync 도착 시 latest in-memory 승리

[Crash Points (Pillar 4 보호)]
.tmp 쓰기 도중 전원 차단 → .sav 무탈 → silent rollback (E4)
rename 도중 전원 차단 → NTFS atomic → OLD or NEW 일관 (E5)
양 슬롯 Invalid → FreshStart + bHadPreviousData=true 신호 → Title UI "이끼 아이는 깊이 잠들었습니다" 분기 (E10)
```

**Slice Owners** (semantic) vs Save/Load (직렬화):

| Slice | Semantic Owner | Serialization | Read | Write |
|---|---|---|---|---|
| `FSessionRecord` | Time #1 | Save/Load | All consumers via accessor | Time only |
| `FGrowthState` | Growth #7 | Save/Load | Card, MBC | Growth only |
| `FDreamState` | Dream #9 | Save/Load | Dream Journal UI | Dream only |
| `FWindowState` | Window&Display #14 | Save/Load | (none) | Window&Display only |
| Farewell archive page | Farewell #17 (VS) | Save/Load | Title, Farewell UI | Farewell only |
| `bHasUnseenDream` flag | Save/Load (own bool) | Save/Load | Dream Journal UI | Dream Trigger (write) + Dream Journal UI (in-memory false 변경) |

### 3.5 Cross-Thread Boundary

| Thread | Responsible Systems | Notes |
|---|---|---|
| **Game Thread** | All Subsystem Initialize/Deinitialize, GSM MPC update, Time tick, all delegate broadcast, Card pool generation, Growth tag accumulation, Dream evaluation, Stillness Budget Request/Release, UI rendering, FRealClockSource OS suspend/resume marshalling | UE 표준 패턴 |
| **Worker Thread (Save/Load)** | FMossSaveSnapshot 직렬화 + CRC32 + 파일 I/O + atomic rename | `FMossSaveSnapshot` POD-only contract — UObject 직접 참조 금지 (GC 경합 방지) |
| **Render Thread** | Lumen HWRT GI computation, MPC scalar 참조, MID parameter sample, Niagara particle simulation | UE 자동 처리 |
| **No async tasks in game logic** | Card, Growth, Dream, GSM, Time 모두 동기 처리 | atomicity contract 보호 |

### 3.6 MPC Update Path (특수)

```text
[GSM, every frame]
TickMPC() → Formula 1 (SmoothStep Lerp)
  → MaterialParameterCollectionInstance::SetScalarParameterValue/SetVectorParameterValue
  → 8 parameters (ColorTemperatureK, LightAngleDeg, ContrastLevel, AmbientR/G/B, AmbientIntensity, MossBabySSSBoost)

[Render Thread, next frame]
Material shader 참조 → 픽셀 셰이더 적용
  → MBC 머티리얼: AmbientMoodBlend로 GSM MPC 자동 반영
  → Lumen Dusk material: Contrast/Ambient 자동 반영
  → PP_MossDusk Post Process Volume: ContrastLevel 자동 반영

[🔴 KNOWN GAP — ADR-0004 (= GSM OQ-6 = Lumen Dusk OQ-1)]
MPC scalar는 머티리얼에서만 읽힘.
DirectionalLight Actor의 Temperature/Intensity는 자동 반영 안 됨.
SkyLight Intensity도 동일.

해결 후보 (ADR에서 결정):
(a) GSM이 MPC 갱신과 별도로 Light Actor 프로퍼티 직접 set
(b) Post-process LUT 블렌딩으로 색온도 인지 대체
(c) Blueprint 바인딩으로 MPC→Light 복사 (Construction Script)
(d) hybrid: 핵심 색온도는 (a), 미세 보정은 (b)
```

### 3.7 Critical Race Conditions (3건)

| # | Scenario | Mitigation | ADR |
|---|---|---|---|
| **1** | **Day 21 FOnCardOffered race** — Growth(태그 가산 → FOnFinalFormReached) → GSM(Farewell P0 전환) 순서 보장 필요. UE Multicast Delegate 등록 순서는 비공개 계약 | 명시적 호출 체인 또는 2단계 delegate 패턴 (Card → Growth 직접 호출 → Growth가 별도 delegate 발행 → GSM 구독) | **ADR-0005 BLOCKING** |
| **2** | **FOnLoadComplete vs FOnTimeActionResolved 순서 역전** (E12) — GSM이 Time 신호 먼저 받을 수 있음 | GSM은 단일 버퍼에 Time 신호 보관 → FOnLoadComplete 후 분기 (이미 GSM GDD AC-GSM-15에 포함됨) | (해소됨, ADR 불필요) |
| **3** | **Compound event sequence atomicity** — Day 13 카드 + 꿈 + narrative 동시 트리거 시 sequence atomic 미보장 (Save/Load는 per-trigger만 보장) | GSM이 BeginCompoundEvent/EndCompoundEvent boundary로 in-memory mutation을 batch한 뒤 마지막에 1회 SaveAsync | **ADR-0009 P1** |

---

## API Boundaries (Phase 4)

각 시스템의 public contract를 UE C++ 의사코드로 정의. 14개 시스템 + Cross-Layer Invariants Summary.

### Foundation Layer

```cpp
// ─── Time & Session System (#1) ──────────────────────────────────────────
UCLASS()
class UMossTimeSessionSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Single atomic entry point — public; private wrappers below
    void IncrementNarrativeCountAndSave();

    int32  GetCurrentDayIndex() const;
    int32  GetSessionCountToday() const;
    EGameState GetLastPersistedState() const;  // GSM 복원 보조

    UPROPERTY(BlueprintAssignable) FOnTimeActionResolved OnTimeActionResolved;
    UPROPERTY(BlueprintAssignable) FOnDayAdvanced        OnDayAdvanced;
    UPROPERTY(BlueprintAssignable) FOnFarewellReached    OnFarewellReached;

private:
    void IncrementNarrativeCount();      // PRIVATE — direct call forbidden (AC)
    void TriggerSaveForNarrative();      // PRIVATE
    bool TickInSession(float DeltaTime); // FTSTicker (not FTimerManager)
    void OnWorldReady(UWorld*, const UWorld::InitializationValues);
    TSharedPtr<IMossClockSource> ClockSource;  // injected at Initialize
};
```
**Invariants (caller)**: `Initialize()`에서 timer 등록·Action 발행 금지 (GetWorld() null) · `IncrementNarrativeCountAndSave`만 외부 호출 (AC `NARRATIVE_COUNT_ATOMIC_COMMIT`) · Time 내부 상태(Active/Suspended/FarewellExit) 직접 read 금지
**Guarantees**: 1Hz tick은 game pause / focus loss와 무관하게 진행 · ETimeAction first-match-wins (Rule 1→8) · `IncrementNarrativeCountAndSave`는 `NarrativeCount += 1 + SaveAsync(ENarrativeEmitted)` atomic · `LastWallUtc`는 BACKWARD_GAP에서 절대 갱신되지 않음 · `LastMonotonicSec`은 항상 `double`

```cpp
// ─── Data Pipeline (#2) ──────────────────────────────────────────────────
UCLASS()
class UDataPipelineSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool                          IsReady() const;                     // R3 fail-close 전제
    EDataPipelineState            GetState() const;
    TArray<FName>                 GetFailedCatalogs() const;           // E10 partial fail 진단

    // Pull-only catalog APIs (R3: null-safe + fail-close + sync)
    TArray<FName>                       GetAllCardIds() const;
    TOptional<FGiftCardRow>             GetCardRow(FName CardId) const;
    FSoftObjectPath                     GetCardIconPath(FName CardId) const;
    TArray<TWeakObjectPtr<UDreamDataAsset>> GetAllDreamAssets() const;
    FText                               GetDreamBodyText(FName DreamId) const;
    TOptional<FFinalFormRow>            GetGrowthFormRow(FName FormId) const;
    TArray<FFinalFormRow>               GetAllGrowthFormRows() const;
    UStillnessBudgetAsset*              GetStillnessBudgetAsset() const;

    void RefreshCatalog();  // PIE only; Cooked는 변경 없음

private:
    UPROPERTY() TMap<FName, TWeakObjectPtr<UObject>> ContentRegistry;  // GC-safe
    EDataPipelineState CurrentState = EDataPipelineState::Uninitialized;
};
```
**Invariants**: `Pipeline.IsReady() == true` 확인 후에만 조회 호출 (`Initialize()` 직후 `checkf` 발화) · downstream Subsystem은 `Collection.InitializeDependency<UDataPipelineSubsystem>()` 선언 의무 · row 추가 시 C1 schema 가드 준수 (수치 stat 필드 금지) · Dream 코드 내 리터럴 임계값 금지 (C2 schema 가드)
**Guarantees**: `nullptr`/`TOptional<>` empty 반환 시 fail-close (default row 절대 발행 안 함) · `MaxInitTimeBudgetMs(50ms)` 내 Ready 도달 (D2) · DegradedFallback 시에도 성공 카탈로그는 정상 조회 가능 · 4 카탈로그 등록 순서 deterministic (Card→FinalForm→Dream→Stillness)

```cpp
// ─── Save/Load Persistence (#3) ──────────────────────────────────────────
UCLASS()
class UMossSaveData : public USaveGame {
    GENERATED_BODY()
public:
    UPROPERTY() FSessionRecord  SessionRecord;
    UPROPERTY() FGrowthState    GrowthState;
    UPROPERTY() FDreamState     DreamState;
    UPROPERTY() FWindowState    WindowState;
    UPROPERTY() bool            bHasUnseenDream = false;
    // Farewell archive page (VS) 추가 예정
};

UCLASS()
class UMossSaveLoadSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase&) override;
    virtual void Deinitialize() override;

    void SaveAsync(ESaveReason Reason);  // coalesced; thread-pool worker

    // Typed slice accessors — 의미적 owner는 분리, 직렬화는 여기서
    FSessionRecord& GetSessionRecord();           // Time만 write
    FGrowthState&   GetGrowthState();             // Growth만 write
    FDreamState&    GetDreamState();              // Dream만 write
    FWindowState&   GetWindowState();             // Window&Display만 write
    bool&           GetHasUnseenDreamFlag();      // Dream Trigger write, UI in-memory false 가능

    bool IsDegraded() const;                      // bSaveDegraded
    TOptional<FMossSlotMetadata> GetLastValidSlotMetadata() const;  // Farewell #17 hook

    UPROPERTY(BlueprintAssignable) FOnLoadComplete         OnLoadComplete;
    UPROPERTY(BlueprintAssignable) FOnSaveDegradedReached  OnSaveDegradedReached;

private:
    UPROPERTY() TObjectPtr<UMossSaveData> InMemorySaveData;
    void FlushAndDeinit();   // T1/T3/T4 — bDeinitFlushInProgress guard
    void FlushOnly();        // T2 only (Alt-Tab) — does NOT set bDeinitFlushInProgress
    FThreadSafeBool bDeinitFlushInProgress {false};
};
```
**Invariants**: in-memory mutation 후 동일 game-thread 함수에서 `SaveAsync` 호출 (per-trigger atomicity) · `UGameplayStatics::SaveGameToSlot/LoadGameFromSlot` 호출 금지 (AC `NO_SLOT_UTIL_SAVETOFILE`) · UI 진행 인디케이터 위젯 생성 금지 (AC `NO_SAVE_INDICATOR_UI`) · forward-tamper 탐지 코드 금지 (AC `NO_ANTITAMPER_LOGIC`)
**Guarantees**: 매 `SaveAsync` 호출은 atomic — fail 시 직전 정상 슬롯 무탈 (per-trigger) · 양 슬롯 동시 Invalid 시 `FreshStart + bHadPreviousData=true` silent 진입 (Pillar 1 — 에러 다이얼로그 절대 없음) · CRC32 seed=0 고정 (R3 CRITICAL-4) · `WriteSeqNumber` overflow detection + reset 절차 (Formula 2) · NTFS atomic rename 보장 (Windows MoveFileExW)

```cpp
// ─── Input Abstraction Layer (#4) ────────────────────────────────────────
// Subsystem 없음 — APlayerController + Enhanced Input 자산으로 구현

// 정의된 InputAction 자산 (DataAsset, Content/Input/):
//   IA_PointerMove (Axis2D)
//   IA_Select       (bool, Released)
//   IA_OfferCard    (bool, Hold trigger)
//   IA_NavigateUI   (Axis2D)
//   IA_OpenJournal  (bool)
//   IA_Back         (bool)

// 정의된 InputMappingContext 자산:
//   IMC_MouseKeyboard (Priority 1, MVP 활성)
//   IMC_Gamepad       (Priority 1, VS 활성)

// PlayerController의 InputComponent에서 BindAction:
EnhancedInputComponent->BindAction(IA_OfferCard, ETriggerEvent::Triggered, this, &OnOfferCardHold);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputModeChanged, EInputMode, NewMode);
// 발행자: APlayerController 또는 전용 UInputModeWatcher (구현 결정)
```
**Invariants**: raw key/button 바인딩 금지 (`BindKey`/`BindAxis` legacy/`EKeys::` 직접 — AC `NO_RAW_KEY_BINDING`) · Hover-only로 게임 상태 변경 금지 (AC `HOVER_ONLY_PROHIBITION`) · keyboard 단축키는 편의 — 마우스 클릭만으로 모든 기능 접근 가능 (AC `MOUSE_ONLY_COMPLETENESS`)
**Guarantees**: `IA_OfferCard` Hold threshold는 컨텍스트별 분리 (Mouse 0.15s / Gamepad 0.5s) — Formula 2 boundary `>=` · `OnInputModeChanged` 발행 시 알림·팝업 절대 없음 · hysteresis 강제 (`InputModeMouseThreshold` 3px boundary `>`) · 게임패드 분리 시 silent fallback (Pillar 1)

### Core Layer

```cpp
// ─── Game State Machine (#5, +Visual State Director 흡수) ─────────────────
UCLASS()
class UMossGameStateSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase&) override;
    virtual void Deinitialize() override;

    EGameState GetCurrentState() const;
    void OnPrepareRequested();                 // Card에서 호출
    void OnCardSystemReady(bool bDegraded);    // Card에서 호출

    UPROPERTY(BlueprintAssignable)
    FOnGameStateChanged OnGameStateChanged;    // TwoParams (Old, New)

private:
    void OnTimeActionResolved(ETimeAction);
    void OnLoadCompleteHandler(bool bFreshStart, bool bHadPreviousData);
    void OnDreamTriggered(FName DreamId);
    void RequestStateTransition(EGameState NewState, EPriority Priority);
    bool TickMPC(float DeltaTime);             // FTSTicker 매 frame
    UPROPERTY() TObjectPtr<UMaterialParameterCollectionInstance> MPCInstance;
    UPROPERTY() TObjectPtr<UGameStateMPCAsset> MPCAsset;
    FStillnessHandle CurrentNarrativeHandle;   // Dream/Farewell 진입 시 Request
};
```
**Invariants**: `Farewell → Any` 전환 분기 코드 금지 (AC-GSM-05, grep) · MPC 파라미터 코드 내 리터럴 금지 — `UGameStateMPCAsset`에서만 (AC-GSM-06) · `LastPersistedState` 갱신은 Waiting/Farewell 진입 시에만 · `FOnGameStateChanged`는 MPC Lerp **시작 시점**에 발행 (블렌드 완료 아님)
**Guarantees**: 전환 우선순위 P0(인터럽트)>P1(자동순서)>P2(조건부)>P3(플레이어 명시) — 높은 우선순위가 진행 중 블렌드 인터럽트 가능 · Dream 진입 시 Stillness Budget Narrative 슬롯 획득 후에만 전환 (Defer until OnBudgetRestored 또는 `DreamDeferMaxSec` 60s 초과 시 강제 선점) · Day 21 카드 건넴 직후 `FOnDayAdvanced` 미발행 + 즉시 Farewell P0 (E13) · withered 진입은 `bWithered = true` 영속화 + 카드 건넴으로 해제

```cpp
// ─── Moss Baby Character (#6) ────────────────────────────────────────────
UCLASS()
class AMossBaby : public AActor {
    GENERATED_BODY()
public:
    AMossBaby();
    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;
    void OnGrowthStageChangedHandler(EGrowthStage NewStage);
    void OnFinalFormReachedHandler(FName FinalFormId);
    void OnCardOfferedHandler(FName CardId);
    void OnGameStateChangedHandler(EGameState OldState, EGameState NewState);
    void ApplyMaterialPreset(EGrowthStage Stage);  // CR-2 preset table
    void ApplyDryingOverlay(float DryingFactor);   // Formula 5 — clamp 책임 MBC

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> MeshComponent;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
    UPROPERTY() TObjectPtr<UMossBabyPresetAsset> PresetAsset;
    ECharacterState CurrentState;       // FinalReveal > GrowthTransition > Reacting > Drying/Idle
    FStillnessHandle CurrentMotionHandle;
};
```
**Invariants**: 머티리얼 파라미터 리터럴 초기화 금지 — `UMossBabyPresetAsset`에서만 (AC-MBC-03 grep) · 브리딩 사인파는 `Idle` 상태에서만 호출 (AC-MBC-07) · `BreathingAmplitude A ≤ E_base(Sprout 0.02)` 유지 (Formula 3 clamp) · Reacting 도중 추가 카드 반응 Request 시 Stillness Budget Deny (AC-MBC-17 자연 쿨다운)
**Guarantees**: 우선순위 강제 (FinalReveal > GrowthTransition > Reacting > Drying/Idle — AC-MBC-10/13/14) · 다단계 건너뛰기 시 중간 메시 미렌더링 + 최종 단계 직행 (E1, AC-MBC-02) · Stillness Budget Deny 시 효과 건너뜀 (AC-MBC-05) · ReducedMotion ON 시 Lerp 즉시 적용 (D=0 예외, AC-MBC-11) · DryingFactor `[0.0, 1.0]` clamp + 정수 나눗셈 방지 float 캐스팅 (AC-MBC-08)

```cpp
// ─── Lumen Dusk Scene (#11) — owns PSO Precaching ─────────────────────────
UCLASS()
class ALumenDuskSceneActor : public AActor {  // 또는 Level Blueprint
    GENERATED_BODY()
public:
    void OnGameStateChangedHandler(EGameState OldState, EGameState NewState);
    void StartPSOPrecaching();
    bool IsPSOReady() const;

private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> JarMesh;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> DeskMesh;
    UPROPERTY() TObjectPtr<UDirectionalLightComponent> KeyLight;
    UPROPERTY() TObjectPtr<USkyLightComponent>          SkyLight;
    UPROPERTY() TObjectPtr<UCameraComponent>            FixedCamera;
    UPROPERTY() TObjectPtr<UPostProcessComponent>       PostProcess;
    UPROPERTY() TObjectPtr<ULumenDuskAsset>             Config;
    FStillnessHandle ParticleHandle;
    FStillnessHandle MotionHandle;
};
```
**Invariants**: 카메라 Actor 이동 코드 절대 금지 — `SetActorLocation/Rotation` grep 0 (AC-LDS-01) · 모든 정적 메시에 `bNaniteEnabled = false` (AC-LDS-02) · MPC `SetScalarParameterValue/SetVectorParameterValue` 호출 금지 (씬은 read-only — AC-LDS-06 grep) · `r.MotionBlur.Amount = 0.0` (AC-LDS-09)
**Guarantees**: MPC 변경은 다음 frame 자동 반영 · Farewell Vignette 1.5–2.0초에 걸쳐 점진 증가 (AC-LDS-10) · Dream DoF Formula 1 `BaseFStop + DreamFStopDelta × DreamBlend` clamp `[1.4, 4.0]` (AC-LDS-16) · ReducedMotion ON 시 파티클 Request 호출 자체 없음 (AC-LDS-15) · PSO 번들 미존재 시 DegradedFallback (크래시 없음, AC-LDS-12)

### Feature Layer

```cpp
// ─── Growth Accumulation Engine (#7) ──────────────────────────────────────
UCLASS()
class UGrowthAccumulationSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase&) override;

    // Pull APIs (Dream Trigger, MBC가 소비)
    const TMap<FName, float>& GetTagVector() const;
    EGrowthStage              GetCurrentStage() const;
    int32                     GetLastCardDay() const;     // MBC DryingFactor 입력
    FGrowthMaterialHints      GetMaterialHints() const;   // raw 오프셋 (clamp는 MBC)
    FName                     GetFinalFormId() const;     // Resolved 후

    UPROPERTY(BlueprintAssignable) FOnGrowthStageChanged OnGrowthStageChanged;
    UPROPERTY(BlueprintAssignable) FOnFinalFormReached   OnFinalFormReached;

private:
    void OnCardOfferedHandler(FName CardId);     // CR-1 + atomic SaveAsync
    void OnDayAdvancedHandler(int32 NewDayIndex); // CR-4
    void EvaluateFinalForm();                     // CR-5 + SaveAsync(ECardOffered)
    UPROPERTY() TObjectPtr<UGrowthConfigAsset> Config;
};
```
**Invariants**: CR-1 step 2-5(태그 가산·LastCardDay·TotalCardsOffered·MaterialHints 갱신) + step 6(`SaveAsync(ECardOffered)`) 사이에 `co_await`/`AsyncTask`/`Tick` 분기 없음 (AC-GAE-02) · 빈 Tags 카드는 `UEditorValidatorBase`에서 차단 (CR-3) · MBC가 raw 오프셋에 preset 가산 후 clamp (`HueShift_final = clamp(preset + offset, -0.1, 0.1)`)
**Guarantees**: `FOnGrowthStageChanged` 다단계 건너뛰기 시 최종 단계만 1회 발행 (CR-4, AC-GAE-05) · `FinalFormId == ""` 가드 — TotalCardsOffered=0이면 `DefaultFormId` 폴백 (EC-1, AC-GAE-07) · Resolved 상태에서 모든 이벤트 무시 (EC-6/7, AC-GAE-17) · `GetMaterialHints()`는 raw 오프셋 — clamp 미포함

```cpp
// ─── Card System (#8) ────────────────────────────────────────────────────
UCLASS()
class UCardSystemSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase&) override;

    void OnPrepareRequested();                // GSM에서 호출
    TArray<FName> GetDailyHand() const;       // Card Hand UI에서 호출
    bool ConfirmOffer(FName CardId);          // Card Hand UI에서 호출 — 검증 게이트 3종
    void OnDayAdvancedHandler(int32 NewDayIndex);

    UPROPERTY(BlueprintAssignable) FOnCardOffered OnCardOffered;

private:
    void RegenerateDailyHand(int32 DayIndex);  // CR-1 deterministic seed
    ECardSystemState CurrentState;             // Uninitialized/Preparing/Ready/Offered
    TArray<FName> DailyHand;
    bool bHandOffered = false;
    FRandomStream RandomStream;                // 외부 주입 가능 (테스트)
    UPROPERTY() TObjectPtr<UCardSystemConfigAsset> Config;
};
```
**Invariants**: `CardId`는 항상 `FName` — `FGuid` 사용 금지 (AC-CS-17 grep across Card/Growth/GSM/MBC) · `FGiftCardRow`에 수치 효과 필드 정의 금지 (AC-CS-10 = Pipeline C1 schema gate) · `ConfirmOffer` 검증 3 게이트 (state==Ready / DailyHand.Contains(CardId) / !bHandOffered) — false 반환 시 UI 드래그 롤백
**Guarantees**: `FOnCardOffered`는 하루 최대 1회 발행 (CR-CS-4) · 동일 DayIndex `FOnDayAdvanced` 중복 수신 시 재생성 스킵 (AC-CS-03) · 카탈로그 < 3장 → Degraded Ready (`bDegraded=true`) — UI에 빈 슬롯 (CR-CS-7) · `PlaythroughSeed + DayIndex` 결정론적 시드 — 앱 재시작 시 동일 핸드 (CR-CS-5) · 5초 timeout 초과 시 GSM이 강제 Offering 진입 (CR-CS-3, GSM E9)

```cpp
// ─── Dream Trigger System (#9) ───────────────────────────────────────────
UCLASS()
class UDreamTriggerSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase&) override;

    TArray<FName> GetUnlockedDreamIds() const; // Dream Journal UI 소비

    UPROPERTY(BlueprintAssignable) FOnDreamTriggered OnDreamTriggered;

private:
    void OnCardOfferedHandler(FName CardId);   // CR-1 평가 트리거
    void OnGameStateChangedHandler(EGameState OldState, EGameState NewState);
                                                // Dream→Waiting 시 PendingDreamId 클리어
    bool EvaluateRarityGates() const;          // CR-2 게이트 A(cooldown) + B(cap)
    TArray<UDreamDataAsset*> CollectCandidates() const;  // CR-3 4 조건
    UDreamDataAsset* SelectDream(const TArray<UDreamDataAsset*>&) const;  // CR-4 max TagScore
    UPROPERTY() TObjectPtr<UDreamConfigAsset> Config;
};
```
**Invariants**: 코드 내 트리거 임계 리터럴 금지 — `UDreamDataAsset` 필드만 사용 (Pipeline C2 schema gate) · `GameState != Waiting` 또는 `DayIndex == 1`이면 평가 스킵 (CR-1 Guard) · `FDreamState` mutation + `SaveAsync(EDreamReceived)` 동일 call stack (CR-4)
**Guarantees**: 두 게이트(MinDreamIntervalDays cooldown + MaxDreamsPerPlaythrough cap) 모두 통과 시에만 트리거 (CR-2) · Day 21 + `DreamsUnlockedCount < MinimumGuaranteedDreams` 시 게이트 우회 1회 강제 트리거 (EC-1) · `PendingDreamId` 비어있지 않으면 다음 세션에서 재발행 (EC-5) · 후보 동점 시 `DreamId` 사전순 첫 번째 (CR-4) · `DegradedFallback` 시 빈 배열 반환 + 평가 종료 (EC-8)

```cpp
// ─── Stillness Budget (#10) ──────────────────────────────────────────────
USTRUCT()
struct FStillnessHandle {
    GENERATED_BODY()
    UPROPERTY() bool bValid = false;
    UPROPERTY() EStillnessChannel Channel;
    UPROPERTY() EStillnessPriority Priority;
    // move-only semantics, RAII Release in destructor
};

UCLASS()
class UStillnessBudgetSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase&) override;

    [[nodiscard]] FStillnessHandle Request(EStillnessChannel Ch, EStillnessPriority Pr);
    void Release(FStillnessHandle& Handle);  // idempotent
    bool IsReducedMotion() const;

    UPROPERTY(BlueprintAssignable) FOnBudgetRestored OnBudgetRestored;

private:
    UPROPERTY() TObjectPtr<UStillnessBudgetAsset> BudgetAsset;
    TMap<EStillnessChannel, TArray<FStillnessHandle*>> ActiveHandles;
};
```
**Invariants**: `OnPreempted` 콜백 내에서 동일 채널 `Request()` 재호출 금지 (E5 재진입 가드) · 슬롯 수 리터럴 코드 금지 — `UStillnessBudgetAsset`만 사용 (AC-SB-10 grep) · `FStillnessHandle` 복사 금지 — move-only · Release 누락은 버그 (no timeout)
**Guarantees**: 동일 우선순위 간 선점 없음 (FIFO) · Narrative > Standard > Background 선점 가능 (Rule 3) · ReducedMotion ON 시 Motion/Particle EffLimit=0 — Sound 영향 없음 (Rule 6, AC-SB-06/07) · DegradedFallback 시 모든 Request Deny (`UE_LOG Warning` 1회만, AC-SB-08/09) · Release idempotent (이중 호출 no-op, AC-SB-11)

### Presentation Layer

```cpp
// ─── Card Hand UI (#12) ──────────────────────────────────────────────────
UCLASS()
class UCardHandWidget : public UUserWidget {
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

    void OnGameStateChangedHandler(EGameState OldState, EGameState NewState);
    void OnInputModeChangedHandler(EInputMode NewMode);
    void OnOfferCardHoldStarted();         // IA_OfferCard Hold Triggered
    void OnPointerMoveHandler(FVector2D NewPos);
    bool IsInOfferZone(FVector2D CardCenter) const;  // Formula F-CHU-3

private:
    UPROPERTY() TObjectPtr<UCardHandUIConfigAsset> Config;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UHorizontalBox> CardSlotsContainer;
    TArray<TObjectPtr<UCardSlotWidget>> CardSlots;
    ECardHandUIState State;                // Hidden/Revealing/Idle/Dragging/Offering/Hiding
    TWeakObjectPtr<UCardSlotWidget> DraggedCard;
    FStillnessHandle CurrentMotionHandle;
};
```
**Invariants**: `FGiftCardRow.Tags`/수치 필드/확률을 UI에 바인딩 금지 (AC-CHU-03 grep — DisplayName + Description + IconPath만) · Hover 상태로 게임 상태 변경 금지 (AC-CHU-11 = Input Rule 4) · Dragging 상태에서 새 `IA_OfferCard` Hold 무시 (EC-CHU-8, 한 번에 1 카드) · 카드 텍스트 800×600 최소 폰트 16px 보장
**Guarantees**: ConfirmOffer false 반환 시 카드 원래 위치로 부드럽게 복귀 (`CardReturnDurationSec` 0.3s, EC-CHU-2) · `FOnGameStateChanged(Offering, _)` 강제 이탈 시 드래그 즉시 취소 + 전체 Hide (EC-CHU-4) · ReducedMotion ON 시 모든 애니메이션 즉시 표시·Hide (AC-CHU-13) · 마우스만으로 전체 게임 기능 접근 가능 (AC-CHU-12 = Input Rule 5)

```cpp
// ─── Dream Journal UI (#13) ──────────────────────────────────────────────
UCLASS()
class UDreamJournalWidget : public UUserWidget {
    GENERATED_BODY()
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    void OnGameStateChangedHandler(EGameState OldState, EGameState NewState);
    void OnIconClicked();                  // Open journal
    void OnPageNavigated(EJournalNavigation Direction);

private:
    UPROPERTY() TObjectPtr<UDreamJournalConfig> Config;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UWidgetSwitcher> PageSwitcher;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<URichTextBlock>  BodyTextBlock;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> IconButton;
    EJournalState State;                   // Closed/Opening/Reading/Closing
    FStillnessHandle CurrentMotionHandle;
};
```
**Invariants**: **`UMossSaveLoadSubsystem::SaveAsync()` 직접 호출 금지** (AC-DJ-17 grep — `bHasUnseenDream = false`는 in-memory 변경만) · 진입점 아이콘은 `EGameState::Waiting`에서만 활성 (CR-1 Access Control) · 통계·수치(N/M, percent)·태그 정보 화면 표시 금지 (AC-DJ-21/22 grep)
**Guarantees**: 꿈이 1개뿐이면 이전/다음 버튼 숨김 (AC-DJ-08) · `Pipeline.GetDreamBodyText()` 빈 반환 시 빈 페이지 + 탐색 버튼 (AC-DJ-20, 에러 다이얼로그 없음) · ReducedMotion ON 시 모든 트랜지션 즉시 (AC-DJ-19) · `bHasUnseenDream` 세이브 round-trip 보장 (AC-DJ-16)

### Meta Layer

```cpp
// ─── Window & Display Management (#14) ───────────────────────────────────
UCLASS(Config=Game)
class UMossWindowDisplaySettings : public UDeveloperSettings {
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere) int32 MinWindowWidth = 800;
    UPROPERTY(Config, EditAnywhere) int32 MinWindowHeight = 600;
    UPROPERTY(Config, EditAnywhere) bool  bAlwaysOnTopDefault = false;
    UPROPERTY(Config, EditAnywhere) float DPIScaleOverride = 0.0f;     // 0=auto
    UPROPERTY(Config, EditAnywhere) int32 FocusEventDebounceMsec = 50;
    // ... 나머지 10 knobs
};

UCLASS()
class UMossWindowSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase&) override;

    void  ApplyWindowMode(EWindowDisplayMode Mode);
    void  SetAlwaysOnTop(bool bEnabled);          // Windows ::SetWindowPos
    float GetCurrentDPIScale() const;             // Formula 1
    void  PersistWindowState();                   // Save/Load.Get<FWindowState>()

    DECLARE_MULTICAST_DELEGATE(FOnWindowFocusEvent);
    FOnWindowFocusEvent OnWindowFocusLost;        // 50ms debounce
    FOnWindowFocusEvent OnWindowFocusGained;

private:
    void OnInputModeChangedHandler(EInputMode NewMode);  // cursor visibility
    void OnDPIChanged(uint32 NewDPI);                    // WM_DPICHANGED
    FTimerHandle FocusDebounceTimer;
};
```
**Invariants**: 어떠한 모달 다이얼로그도 생성 금지 (Rule 10, AC-WD-13 — `Pillar 1`) · 창 크기 800×600 미만 허용 금지 (`SWindow::FArguments::MinWidth/Height`로 OS 강제, EC-08) · 포커스 상실 시 게임 일시정지 금지 + 자동 음소거 금지 (Rule 6, AC-WD-06)
**Guarantees**: Always-on-top 활성화 시 다른 앱 위 표시 — 단, 포커스 강제 빼앗음 없음 (Rule 7, AC-WD-10) · Per-Monitor DPI Awareness V2 처리 — 모니터 이동 시 `WM_DPICHANGED` 자동 재계산 (EC-02, AC-WD-04) · 저장된 창 위치가 화면 밖이면 기본 모니터 중앙 이동 + 50% 가시성 보장 (EC-07, AC-WD-14) · 최소화 시 `bDisableWorldRendering = true` (CPU/GPU 절약)

### Cross-Layer Invariants Summary

| 카테고리 | Invariant |
|---|---|
| **Lifecycle** | Subsystem 의존은 `Collection.InitializeDependency<>()`로 강제. `PostInitialize` 사용 금지. Time/GSM은 `OnWorldReady` 콜백에서 timer 등록 |
| **Threading** | Game thread 중심. Save/Load worker thread는 `FMossSaveSnapshot` POD-only contract 통과 후에만 접근 (UObject 직접 참조 금지) |
| **Tick** | `FTSTicker` (engine-level, pause-independent). `FTimerManager`는 게임 pause 시 정지 → 사용 금지 |
| **Atomicity** | 모든 in-memory mutation은 같은 game-thread 함수 안에서 `SaveAsync(reason)` 호출 (per-trigger atomicity). Sequence atomicity는 GSM batch boundary 책임 (ADR-0009) |
| **Tuning Externalization** | 모든 수치 상수는 `UDeveloperSettings` 또는 `UPrimaryDataAsset` 외부화. 코드 내 리터럴 금지 (각 시스템 grep AC) |
| **Pillar 1 (조용한 존재)** | 모달·팝업·"저장 중..." 인디케이터·태스크바 깜빡임 절대 금지 (Save/Load + Window&Display + Input + GSM 공통) |
| **Pillar 2 (선물의 시학)** | 카드/꿈 UI에 수치·확률·태그 레이블 표시 금지 (Card Hand UI + Dream Journal UI + Pipeline C1) |
| **Pillar 3 (말할 때만 말한다)** | NarrativeCount cap (3) + Dream rarity gates (3-5회/21일) — 두 카운터 독립 |
| **Pillar 4 (끝이 있다)** | Save/Load atomic 보호 + Farewell 종단 상태 강제 (`Farewell → Any` 분기 grep 0건) |

---

## ADR Audit (Phase 5)

### 5.1 ADR Quality Check

| ADR | Engine Compat | Version | GDD Linkage | Conflicts | Valid |
|---|---|---|---|---|---|
| *(none)* | N/A | N/A | N/A | N/A | N/A |

**결과**: 현재 `docs/architecture/` 하위 ADR 파일 **0건**. ADR Quality Check는 본 architecture session 이후 작성될 ADR에 적용됨.

### 5.2 Traceability Coverage Check

| Metric | Count |
|---|---|
| Technical Requirements Baseline (Phase 0b 카운트) | **240+** TR |
| 기존 ADR | 0 |
| TR ↔ ADR 매핑 | 0 |
| **Coverage Gap** | **100%** |

모든 240+ TR이 GDD 단계 contract로 정의되어 있으나 ADR로 정식화된 결정 0건. 이는 Pre-Production architecture 첫 진입 정상 상태.

### 5.3 Phase 1-4에서 발견된 추가 ADR 후보 검토

| Phase | 발견된 추가 결정 사항 | ADR 필요 여부 |
|---|---|---|
| Phase 1 (Layer Map) | Layer Boundary Rules 5건 | **NO** — Architecture Principle (§Architecture Principles) + Coding Standards / Control Manifest 사안 |
| Phase 2 (Module Ownership) | 모든 ownership boundary 명확 (각 GDD가 이미 정의) | **NO** — engineering 결정이 GDD 단계에서 완료됨 |
| Phase 3 (Data Flow) | 3 race conditions 식별 — race #1 (Day 21), race #3 (Compound atomicity) | **YES** — race #1 = ADR-0005, race #3 = ADR-0009. race #2(E12)는 GSM AC-GSM-15에서 해소 |
| Phase 4 (API Boundaries) | Subsystem 명명 컨벤션, Tick mechanism (FTSTicker) | **NO** — Coding Standards / Control Manifest로 정식화 |

**결론**: Phase 0d 12건 후보가 그대로 유효. Phase 1-4에서 추가 ADR 식별 0건.

---

## Required ADRs (Phase 6)

**13건 ADR**을 우선순위·소비 GDD·해소 OQ·layer별로 정식화. Foundation layer 우선. **2026-04-19 갱신**: PSO Precaching이 Lumen Dusk HIGH risk 대응으로 별도 ADR-0013로 분리되어 원래 12건에서 13건으로 확장. **모두 Accepted 완료**.

### B1 BLOCKING (구현 전 필수 — Pre-Production gate 통과 필요)

| ADR | 제목 | 출처 OQ / Cross-review | Layer | 영향 시스템 | 결정 후보 |
|---|---|---|---|---|---|
| **ADR-0001** | **Forward Time Tamper 명시적 수용 정책** | Time `NO_FORWARD_TAMPER_DETECTION` AC + `NO_FORWARD_TAMPER_DETECTION_ADR` | Foundation | Time #1, Save/Load #3 | 시스템 시각 앞당김은 정상 경과와 알고리즘적으로 구분 불가 → 탐지 시도 자체 금지 ADR로 못박음. PR 코드리뷰가 forward tamper 탐지 로직 추가 시도를 차단 |
| **ADR-0004** | **MPC ↔ Light Actor 동기화 아키텍처** | GSM OQ-6 = Lumen Dusk OQ-1 | Core / Presentation | GSM #5, Lumen Dusk #11, MBC #6 | UE 5.6 한계: MPC scalar는 머티리얼만 자동 반영. (a) GSM이 별도 Light Actor 직접 set / (b) Post-process LUT 블렌딩 / (c) Blueprint 바인딩 / (d) hybrid. **첫 구현 마일스톤 전 필수**. **Accepted by ADR-0004 (Hybrid (a)+(b)) 2026-04-19** |
| **ADR-0005** | **`FOnCardOffered` 처리 순서 보장 (Day 21 race)** | Card OQ-CS-3 + Cross-review D-2 | Feature | Card #8, Growth #7, GSM #5, MBC #6 | UE Multicast 등록 순서 비결정 — Day 21에 Growth(태그) 먼저 → GSM(Farewell P0) 나중. (a) Card → Growth 직접 호출 → Growth 별도 delegate / (b) 2단계 delegate 패턴 / (c) Phase A/B priority broadcast. **Accepted by ADR-0005 (2-Stage Delegate) 2026-04-19** |
| **ADR-0013** | **PSO Precaching 전략 (Lumen Dusk HIGH risk)** | Lumen Dusk §CR-6 + AC-LDS-11/12 + 첫 milestone GPU 프로파일링 게이트 | Core | Lumen Dusk #11, GSM #5 (Dawn Entry check) | UE 5.6 PSO 점진 컴파일 — 6 GameState 전환 시 first-load hitching 리스크. (a) Splash precompile 금지 (Pillar 1) / (b) Async Bundle + Graceful Degradation / (c) 점진 컴파일 그대로 감수. **Lumen Dusk 첫 milestone 전 필수**. **Accepted by ADR-0013 (Async Bundle + Graceful Degradation) 2026-04-19** |

### P0 (구현 전 필요 — first sprint 전)

| ADR | 제목 | 출처 OQ | Layer | 영향 시스템 | 결정 후보 |
|---|---|---|---|---|---|
| **ADR-0002** | **Data Pipeline 컨테이너 선택** | Pipeline OQ-ADR-1 | Foundation | Pipeline + 5 downstream | UDataTable vs UPrimaryDataAsset vs UDataRegistrySubsystem. 사전 가이드: Card=DT, Dream=DataAsset, FinalForm=DT, Stillness=DataAsset. UDataRegistry는 5.6 검증 부족 (MEDIUM) — 100행 이하 이점 없음 |
| **ADR-0003** | **Data Pipeline 로딩 전략 (Sync vs Async)** | Pipeline OQ-ADR-2 | Foundation | Pipeline + 5 downstream (R3 동기 contract) | Sync 일괄 (MVP < 50ms) vs Async Bundle (HDD cold-start ≈ 61ms 시 budget 초과). Trigger: T_init > 50ms 측정 시 Async 전환 |
| **ADR-0010** | **`FFinalFormRow` 저장 형식** | Growth CR-8 | Feature | Growth #7, Pipeline #2 | UE DataTable이 `TMap` 인라인 편집 미지원. (a) `TArray<FTagWeightEntry>` 대체 / (b) `UPrimaryDataAsset` 격상. ADR-0002와 동시 결정 권장 |

### P1 (시스템 구현 시)

| ADR | 제목 | 출처 OQ | Layer | 영향 시스템 | 결정 후보 |
|---|---|---|---|---|---|
| **ADR-0006** | **카드 풀 재생성 시점 (Eager vs Lazy)** | Card OQ-CS-5 | Feature | Card #8 | 현재 Eager. (a) Eager 유지 + 성능 측정 / (b) Lazy (Offering 진입 시) — Dawn 체류 시간 증가 |
| **ADR-0007** | **Stillness Budget ↔ UE Sound Concurrency / Niagara Scalability** | Stillness OQ-1 | Feature | Stillness #10, Audio #15 (VS), Niagara #11 | (a) Stillness가 single source / (b) 두 시스템 별도 / (c) hybrid — Sound Concurrency Group을 채널 매핑 |
| **ADR-0008** | **UMG 드래그 구현 방식** | Card Hand UI OQ-CHU-1 | Presentation | Card Hand UI #12 | UMG `NativeOnMouse*` vs Input Abstraction `IA_PointerMove` + `IA_OfferCard` 구독. 후자가 Input Rule 1 부합. **GDD 설계 영향 없음** |
| **ADR-0009** | **Compound Event Sequence Atomicity** | Save/Load OQ-8 + race #3 | Foundation / Core | Save/Load #3, GSM #5, Growth #7, Dream #9 | Save/Load는 per-trigger atomic만 보장. (a) GSM `Begin/EndCompoundEvent` boundary / (b) Save/Load sequence-level API / (c) reason별 즉시 commit + 부분 손실 허용 |

### P2 (Defer 가능)

| ADR | 제목 | 출처 OQ | Layer | 영향 시스템 | 결정 후보 |
|---|---|---|---|---|---|
| **ADR-0011** | **Tuning Knob 노출 (`UDeveloperSettings` 정식화)** | Time OQ-5 (이미 권장 채택) | Cross-Layer | All systems with knobs (10+) | `UDeveloperSettings` 통일 — Project Settings 자동 통합, hot reload, ini 외부화. UDataAsset 변종 사용 시 예외 사유 명시 |
| **ADR-0012** | **Growth `GetLastCardDay()` pull API 형태** | Card OQ-CS-2 | Feature | Card #8, Growth #7, MBC #6 | EC-CS-13 `bHandOffered` 복원용. (a) `int32 GetLastCardDay() const` / (b) `TOptional<int32>` / (c) `bool TryGetLastCardDay(int32&)` |

### ADR ↔ Layer / Affected Systems Summary

| ADR | Layer | Affects | Required By |
|---|---|---|---|
| ADR-0001 (Forward Tamper) | Foundation | Time, Save/Load | Pre-Production gate |
| ADR-0002 (Pipeline Container) | Foundation | Pipeline + 5 downstream | Foundation sprint |
| ADR-0003 (Pipeline Loading) | Foundation | Pipeline + 5 downstream | Foundation sprint |
| ADR-0004 (MPC↔Light) | Core/Presentation | GSM, Lumen Dusk, MBC | **Lumen Dusk first milestone** |
| ADR-0005 (Day 21 race) | Feature | Card, Growth, GSM, MBC | **Day 21 integration test 전** |
| ADR-0006 (Card pool timing) | Feature | Card | Card sprint |
| ADR-0007 (Stillness ↔ Engine) | Feature | Stillness, Audio, Niagara | Audio System (VS) sprint |
| ADR-0008 (UMG drag impl) | Presentation | Card Hand UI | Card Hand UI sprint |
| ADR-0009 (Compound atomicity) | Foundation/Core | Save/Load, GSM | Day 13 compound event 발생 시 |
| ADR-0010 (FFinalFormRow format) | Feature | Growth, Pipeline | Growth sprint |
| ADR-0011 (UDeveloperSettings) | Cross-Layer | All | Foundation sprint (consistency) |
| ADR-0012 (GetLastCardDay API) | Feature | Card, Growth, MBC | Card sprint |
| ADR-0013 (PSO Precaching) | Core | Lumen Dusk, GSM (Dawn Entry) | **Lumen Dusk first milestone** |

### 작성 순서 권장

1. **ADR-0001, ADR-0011** — 정책·컨벤션 (즉시 작성 가능 — 결정 사항 명확)
2. **ADR-0002, ADR-0003, ADR-0010** — Data Pipeline 의존 — Foundation sprint 시작 전 묶어서 결정
3. **ADR-0004, ADR-0013** — Lumen Dusk 첫 구현 마일스톤 직전 (병행 Accepted — HIGH risk 공동 대응)
4. **ADR-0005** — Card System 구현 시 (Day 21 통합 테스트 직전)
5. **ADR-0009** — Day 13 compound event 시나리오 발생 시
6. **ADR-0006, ADR-0007, ADR-0008, ADR-0012** — 각 시스템 sprint 진입 시

---

## Architecture Principles

Game Concept + 14개 GDD + Technical Preferences에서 도출한 5개 핵심 원칙. 모든 기술 결정의 기준선.

### Principle 1 — Pillar-First Engineering

모든 기술 결정은 Game Pillars 1-4에 우선 부합한다. 성능·생산성 trade-off가 발생할 때 **Pillar 보호가 우선**. 이 원칙이 없으면 architecture는 일반 cozy desktop 앱과 구별되지 않는다.

- **Pillar 1 (조용한 존재)** → Save/Load 모달·"저장 중..." 인디케이터 절대 금지 (AC `NO_SAVE_INDICATOR_UI`); Window&Display 모든 모달 다이얼로그 금지 (Rule 10); Input 장치 전환 알림 없음; Time long-gap 자동 farewell (silent)
- **Pillar 2 (선물의 시학)** → Card/Dream Journal UI 수치·확률·태그 레이블 표시 금지 (AC `NO_HARDCODED_STATS_UI`); FGiftCardRow 수치 효과 필드 컴파일 타임 차단 (C1 schema gate)
- **Pillar 3 (말할 때만 말한다)** → NarrativeCount cap (3/playthrough) atomic 영속화; Dream rarity gates (3-5회/21일, MinDreamIntervalDays + MaxDreamsPerPlaythrough); Stillness Budget 동시 이벤트 상한
- **Pillar 4 (끝이 있다)** → Save/Load atomic + dual-slot ping-pong (21일 보존); GSM `Farewell → Any` 전환 분기 grep 금지 (AC-GSM-05); LongGapAutoFarewell silent 진입

**Trade-off accepted**: Save/Load worker thread + atomic rename + WSN overflow 절차의 복잡성을 받아들임. Stillness Budget의 RAII handle + preemption logic 복잡성을 받아들임.

### Principle 2 — Schema as Pillar Guard

Pillar 위반은 **코드 리뷰가 아닌 데이터 형태**가 차단한다. 미래 에이전트나 디자이너가 Pillar를 우회하려는 시도 자체가 컴파일·저장 단계에서 실패하도록 설계.

- **Compile-time layer (가장 강한 보장)**: `FGiftCardRow` USTRUCT에 `int32`/`float`/`double` 류 수치형 효과 필드를 *정의하지 않는다* — 새 필드 추가 시 C1 위반 가능성을 GDD 변경과 동시에 평가
- **Editor save layer**: `UEditorValidatorBase` 서브클래스가 DataTable 모든 row를 저장 시점 검증 — 빈 Tags, NAME_None CardId, RequiredTagWeights 합 ≠ 1.0 등 차단
- **CI grep layer**: 정적 분석으로 금지 패턴 검출 — `NO_FORWARD_TAMPER_DETECTION`, `NO_SAVE_INDICATOR_UI`, `NO_RAW_KEY_BINDING`, `NO_HARDCODED_STATS_UI`, `Farewell → Any` 분기 검색
- **Agent rule layer**: `.claude/rules/` 항목으로 PR 단계 보강

> **이유**: 코드 리뷰가 Pillar를 지키는 게 아니라 *데이터 형태가* 지킨다 (Data Pipeline §Player Fantasy). 이는 Stardew Valley / Spiritfarer / Unpacking이 데이터 schema로 톤을 강제한 사례를 따른다.

### Principle 3 — Per-Trigger Atomicity, Sequence by GSM

**Save/Load는 단일 ESaveReason commit의 atomicity만 보장한다**. in-memory mutation + `SaveAsync(reason)`는 동일 game-thread 함수 호출 안에서. Compound event sequence atomicity (Day 13 카드 + 꿈 + narrative 동시)는 GSM batch boundary 책임 (ADR-0009).

이는 명확한 책임 분리:
- **Save/Load**: storage durability (per-trigger atomic, NTFS rename, dual-slot, CRC32, schema migration)
- **GSM**: game logic ordering (compound event boundary, state transition priority P0-P3, MPC blend orchestration)

GDD AC가 이 원칙을 강제: `NARRATIVE_COUNT_ATOMIC_COMMIT` (Time IncrementNarrativeCountAndSave private 강제), `AC-GAE-02` (Growth CR-1 step 2-6 동일 call stack), `COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY` (Save/Load negative AC).

### Principle 4 — Foundation Independence + Strict Layer Boundaries

**Foundation 4개 시스템 (Time / Pipeline / Save-Load / Input) 간 의존 0건** — 병렬 설계·구현·테스트 가능. 이는 systems-index R2의 Dependency Map과 일치.

Layer Boundary Rules (Phase 1에서 정의):
1. 단방향 의존만 허용 (상위 layer → 하위 layer). 역방향·순환 금지
2. Foundation 시스템 간 의존 0건
3. Same-layer 의존 허용 (Card → Growth, GSM → Save/Load 양방향)
4. Meta layer (Window&Display)는 cross-cutting — Foundation 의존하면서도 Save/Load에 write 허용
5. **Engine API 직접 호출은 Foundation/Meta에 기본 한정** — 예외: 해당 Core 시스템의 본질적 책임이 특정 엔진 서브시스템 통합이면 직접 호출 허용 (ADR에 명시 필수). **게임 로직** 수준의 엔진 API 사용은 Foundation 추상화를 거친다. **허용된 예외**: GSM의 Light Actor 구동 (ADR-0004 Hybrid), Lumen Dusk Scene의 Lumen HWRT 설정 (ADR-0004), Lumen Dusk Scene의 PSO Precaching (ADR-0013) — 렌더링/씬 인프라 통합이 이들 시스템의 본질적 책임이므로 허용. Rule 5의 정신은 "업무 로직은 엔진에 직접 커플링되지 않는다"이며, 렌더링/씬 시스템은 본질적으로 engine-adjacent (Phase 1 §Layer Boundary Rules 5와 동일 wording)

> **예**: Card System은 `FDateTime::UtcNow()` 직접 호출 금지 — Time의 `GetCurrentDayIndex()` 폴링만. Lumen Dusk Scene은 MPC scalar 쓰기 금지 (read-only) — GSM이 유일 writer (AC-LDS-06 grep).

### Principle 5 — Engine-Aware Design with Explicit Mitigation

UE 5.6 (LLM training cutoff 직후 출시 — May 2025 → June 2025) HIGH/MEDIUM risk 도메인을 **GDD 단계에서 명시적으로 식별**하고 mitigation을 적재한다. 위험을 숨기지 않고 추적 가능한 형태로 architecture에 노출.

- **5.6-VERIFY 라벨 AC**: AC-DP-06, AC-DP-07, AC-DP-17, AC-DP-18, AC-DP-09, Gap-5 (E8) — 5.6 행위 검증이 필요한 AC를 별도 라벨링
- **Implementation 단계 검증 task**: Time OQ-1 (FPlatformTime suspend Win10/11 실기), Pipeline OQ-E-4/5/6 (GameInstance::Init 순서 / Cook FSoftRef Error 격상 / CI Validator 자동화)
- **ADR-flagged engine constraints**: ADR-0004 (MPC↔Light Actor sync — UE 5.6 한계), ADR-0002 (UDataRegistrySubsystem 5.6 검증 부족 → 사용 회피), ADR-0009 (Compound atomicity — engine sequence 불보장)
- **Inline risk flags in architecture**: Lumen Dusk HIGH risk + 첫 milestone GPU 프로파일링 게이트 의무, Save/Load Windows lifecycle 4-trigger 패턴, ensure* Shipping strip 회피 (checkf)

> **이유**: UE 5.6 변경 없이 5.5 LLM 지식만으로 architecture를 작성했다면 다음 6건이 발견되지 않았을 것 — Lumen HWRT CPU bottleneck 개선, PSO Precaching 점진 컴파일, FCoreDelegates 모바일 vs Windows 차이, ensure* Shipping strip, AssetManager Cooked PrimaryAssetType 등록, Gameplay Camera auto-spawn 제거. 이 6건이 architecture에 모두 반영됨.

---

## Open Questions

*Required ADR로 정식화된 12건 외 deferred decisions.*

### VS / Full Vision 시스템 GDD 미작성 (4건)

| OQ | 시스템 | Trigger |
|---|---|---|
| OQ-VS-1 | **Audio System (#15)** GDD | MVP 종료 후, VS sprint 진입 전 |
| OQ-VS-2 | **Title & Settings UI (#16)** GDD | Save/Load `bHadPreviousData=true` 분기 처리 + Reduced Motion toggle UX |
| OQ-VS-3 | **Farewell Archive (#17)** GDD | Save/Load `GetLastValidSlotMetadata()` infra hook 소비 (E10 감정 문법 결정) — 단일 farewell vs partial farewell vs hybrid |
| OQ-VS-4 | **Accessibility Layer (#18)** GDD | Text Scale + Colorblind. Reduced Motion은 이미 Stillness Budget 공개 API |

### Cross-System Decisions (Cross-review 2026-04-18에서 deferred)

| OQ | 질문 | Resolves In |
|---|---|---|
| OQ-CS-D3 | **Dawn 상태 Stillness Budget 소비 규칙** (cross-review D-3) — Dawn 진입 시 어떤 채널/우선순위 슬롯 사용? | GSM GDD 또는 Stillness Budget GDD R2 |
| OQ-CS-D4 | **Day 15-20 "징조 단계" 구현 시스템 부재** (cross-review D-4) — Dream Trigger GDD가 이 시기를 어떻게 표현할지 | Dream Trigger GDD 후속 revision |
| OQ-CS-D5 | **Element/Emotion 카드 시각 피드백 공백** (cross-review D-5) — Growth OQ-GAE-3 Full에서 미도입 시 Player Fantasy 달성 불가 | Full Vision Growth GDD revision |
| OQ-CS-D7 | **"Nothing happened" 세션 (Day 8-15) 리텐션 기준** (cross-review D-7) | Producer + game-designer, MVP 첫 playtest 후 |
| OQ-CS-D8 | **MVP Day 7 이벤트 과부하** (cross-review D-8) — 시즌 전환 + Mature + Final Form 동시 발생 시 Stillness Narrative 슬롯 경합 | MVP 구현 시 자연 해소 또는 GSM 우선순위 보강 |
| OQ-CS-D9 | **Day 1 FOnDayAdvanced 발행 시점** (cross-review D-9) — Time 분류 완료 즉시 vs GSM Dawn 후 | Time + GSM cross-system sequence 명시 (Implementation 단계) |
| OQ-CS-S3 | **GrowthTransition 슬롯 등급** (cross-review scenario 3) — Standard vs Narrative? | MBC GDD 또는 Stillness Budget GDD |

### Implementation 단계 검증 Task (5건 — engine 5.6-VERIFY)

| OQ | 검증 항목 | Owner |
|---|---|---|
| OQ-IMP-1 | Windows 10/11 `FPlatformTime::Seconds()` suspend 동작 실기 측정 (Time OQ-1) | gameplay-programmer + qa-lead |
| OQ-IMP-2 | UE 5.6 `GameInstance::Init()` 순서 보장 예외 플랫폼 (Pipeline OQ-E-4) | engine-programmer / unreal-specialist |
| OQ-IMP-3 | Cook 파이프라인 FSoftObjectPath 누락 → Error 격상 옵션 (Pipeline OQ-E-5) | devops-engineer |
| OQ-IMP-4 | DataTable 저장 외 CI Validator 자동화 훅 (Pipeline OQ-E-6) | devops-engineer |
| OQ-IMP-5 | Steam Deck 데스크톱 모드 Proton에서 `SetWindowPos(HWND_TOPMOST)` 동작 (Window OQ-4) | unreal-specialist + qa-lead |

### Player UX Open Questions

| OQ | 질문 | Resolves In |
|---|---|---|
| OQ-UX-1 | **Day 21 이후 UX (restart ritual)** | Farewell Archive GDD #17 (VS) |
| OQ-UX-2 | **Slot indicator 정책** — 채워진 슬롯만 vs 완전 비표시 (Dream Journal OQ-DJ-2) | art-director 협의 |
| OQ-UX-3 | **bSaveDegraded 사용자 노출 정책** (Save/Load OQ-3) — Pillar 1 vs informed user | narrative-director + ux-designer (Title&Settings UI 작성 시) |
| OQ-UX-4 | **Steam Cloud 통합** (Save/Load OQ-1) | release-manager (VS/Full 단계) |

### Content Production

| OQ | Content | Owner | Trigger |
|---|---|---|---|
| OQ-CON-1 | MVP 5개 꿈 텍스트 작성 (Dream Trigger OQ-DTS-4) | Producer + narrative-director + writer | MVP 구현 전 (사전 작업으로 분리) |
| OQ-CON-2 | Art Bible 정식화 (Lean mode AD-CONCEPT-VISUAL skipped) | art-director | `/art-bible` 실행 (game-concept §Visual Identity Anchor 잠정 방향 → 정식 Art Bible) |

---

## Document Status (Phase 7b — TD-ARCHITECTURE Self-Review)

### Self-Review Verdict: **APPROVED WITH CONCERNS**

TD-ARCHITECTURE 게이트 4개 체크 항목 자체 평가 (lean mode self-review):

| # | 체크 항목 | 평가 | 근거 |
|---|---|---|---|
| 1 | 모든 TR이 architectural decision으로 커버되는가? | 🟡 **CONCERNS** | 240+ TR 모두 GDD contract로 정의되어 있으나 ADR로 정식화 0건. 12 Required ADR가 모든 핵심 ambiguity를 cover하도록 식별됐으나 미작성 — Foundation sprint 시작 전 ADR-0001/0002/0003/0011 작성 필수 |
| 2 | 모든 HIGH risk 엔진 도메인이 명시적으로 다루어지거나 OQ로 플래그됐는가? | ✅ **APPROVED** | HIGH 1개 (Lumen HWRT + PSO) → ADR-0004 + AC-LDS-04 [5.6-VERIFY] + 첫 milestone GPU 프로파일링 게이트 의무. MEDIUM 5개 → 모두 GDD 5.6-VERIFY AC 또는 Implementation 검증 task로 명시 |
| 3 | API boundaries가 깨끗하고 최소이며 구현 가능한가? | ✅ **APPROVED** | 14개 시스템 의사코드 contract 작성 (Phase 4). 각 시스템 public method 5-10개 minimal. delegate signature registry §delegates와 일치. invariants/guarantees 명시. Cross-Layer Invariants Summary 9건으로 일관성 확보 |
| 4 | Foundation layer ADR gap이 구현 전 해소됐는가? | 🟡 **CONCERNS** | Foundation에 ADR-0001 BLOCKING + ADR-0002, 0003, 0011 P0/P2 식별됨. **모두 미작성** — Foundation sprint 시작 전 작성 필수. 본 architecture session의 정상 출력 (식별 = 충분, 작성은 next step) |

### CONCERNS 요약 (구현 시작 전 해소 필요)

1. ~~**12 Required ADR 모두 미작성**~~ → **13/13 Accepted 완료 (2026-04-19 최종 갱신)**: ADR-0001~0013 모두 Accepted. ADR-0013 (PSO Precaching)는 Lumen Dusk HIGH risk 대응으로 추가 작성되어 원래 12건 → 13건으로 확장
2. ~~**Foundation ADR 4건 (0001, 0002, 0003, 0011)** Foundation sprint 시작 전 필수~~ → **모두 Accepted**
3. ~~**TR ↔ ADR 매핑 정식화**~~ → **30 TR spot-check 100% Covered** — `docs/architecture/architecture-traceability.md` 참조
4. ~~**모든 ADR이 Proposed 상태**~~ → **모든 ADR Accepted** (`/architecture-review` 2026-04-19 fresh re-verification 통과)

### 최종 Document Status

- **Version**: 1.0 (Master Architecture — incremental authoring 완료)
- **Last Updated**: 2026-04-18
- **Engine**: Unreal Engine 5.6 (hotfix 5.6.1 권장)
- **Review Mode**: lean — Phase 7b LP-FEASIBILITY 자동 스킵
- **GDDs Covered**: 14 MVP GDDs (전부 APPROVED 2026-04-18)
- **ADRs Referenced**: 0 작성됨 (12 Required ADR 식별 완료, Phase 6 참조)
- **Cross-Review Blocker 해소**: C-1, C-2, D-1 모두 해소 / D-2 → ADR-0005로 이관
- **Technical Director Sign-Off**: **APPROVED** (2026-04-19 갱신, self-review — lean mode)
  - 초기 verdict (2026-04-18): APPROVED WITH CONCERNS — 12 Required ADR 미작성
  - 갱신 verdict (2026-04-19): **APPROVED** — Foundation ADR 4건 + BLOCKING ADR 2건 + P0 1건 = 7/12 Proposed. 남은 5건 P1-P2는 각 sprint 진입 시 (skill 권장 작성 순서 6단계에 따라)
  - APPROVED 근거: HIGH risk 엔진 도메인 처리 명시 + API boundaries clean + 5 Architecture Principles + Foundation + BLOCKING ADR 완성
- **Lead Programmer Feasibility**: **SKIPPED** (lean mode auto-skip — production/review-mode.txt)
- **다음 게이트**: `/gate-check pre-production` (모든 Required ADR 작성 + Architecture sign-off 완료 후)

---

## Handoff (Phase 8)

### 다음 작성 권장 ADR (top 3)

1. **ADR-0001 (Forward Tamper 명시적 수용 정책)** — 결정 사항 명확, 즉시 작성 가능. PR 코드리뷰 차단 효과 즉시 발생
2. **ADR-0011 (UDeveloperSettings 정식화)** — 결정 사항 명확 (Time OQ-5에서 사실상 채택), 즉시 작성 가능. 모든 Tuning Knob 일관성
3. **ADR-0004 (MPC↔Light Actor 동기화)** — Lumen Dusk 첫 구현 마일스톤 직전 필수. 4 후보 (a/b/c/d) 중 결정 필요

### 권장 명령

```bash
# 즉시 작성 가능한 명확한 결정
/architecture-decision "Forward Time Tamper 명시적 수용 정책"

# Tuning Knob 컨벤션 일관성
/architecture-decision "Tuning Knob 노출 방식 - UDeveloperSettings 정식 채택"

# Foundation sprint 시작 전 묶음 결정 (3건 동시)
/architecture-decision "Data Pipeline 컨테이너 선택"
/architecture-decision "Data Pipeline 로딩 전략 (Sync vs Async Bundle)"
/architecture-decision "FFinalFormRow 저장 형식 (DataTable TMap 회피)"

# MVP 첫 milestone 직전 (Lumen Dusk 구현 시)
/architecture-decision "MPC ↔ Light Actor 동기화 아키텍처"

# 모든 Required ADR 작성 + sign-off 완료 후
/gate-check pre-production
```

### 후속 단계

1. ADR-0001 + ADR-0011 즉시 작성 (1-2 세션)
2. Foundation sprint 진입 전 ADR-0002 / 0003 / 0010 묶음 결정 (1 세션)
3. `/create-control-manifest` — 모든 Required ADR 작성 후, 시스템·layer별 코드 규칙 manifest 생성
4. `/gate-check pre-production` — 게이트 통과 후 sprint planning
5. Implementation 단계: 5.6-VERIFY AC 5건 + Implementation 검증 task 5건을 sprint 0에 별도 task로 등록
