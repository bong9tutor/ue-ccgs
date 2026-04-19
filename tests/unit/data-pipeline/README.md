# Data Pipeline Unit Tests

Story: foundation-data-pipeline / Story 1-4 (Schema Types)
Epic: foundation-data-pipeline
Layer: Foundation

## 실제 테스트 파일

```
MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineSchemaTests.cpp
```

## 테스트 카테고리

```
MossBaby.Data.Schema.*
```

## 헤드리스 실행 명령

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.Data.Schema.; Quit" \
  -nullrhi -nosound -log -unattended
```

## AC 매핑

| Automation Test | AC | 타입 | Gate |
|---|---|---|---|
| `MossBaby.Data.Schema.DataPipelineSettingsCDO` | AC-5 (UMossDataPipelineSettings 9 knobs CDO) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Schema.FinalFormAssetGetPrimaryAssetId` | AC-2 (UMossFinalFormAsset GetPrimaryAssetId) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Schema.DreamDataAssetGetPrimaryAssetId` | AC-3 (UDreamDataAsset GetPrimaryAssetId) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Schema.DreamDataAssetDefaults` | AC-3 (DreamDataAsset C2 schema gate 기본값) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Schema.StillnessBudgetAssetDefaults` | AC-4 (UStillnessBudgetAsset 기본값) | AUTOMATED | BLOCKING |
| C1 schema gate CI grep (아래 참조) | AC-6 (FGiftCardRow 수치 필드 금지) | CODE_REVIEW | BLOCKING |

## C1 Schema Gate grep (AC-6 — CODE_REVIEW)

FGiftCardRow에 수치 효과 필드(int32/float/double)가 없는지 CI에서 검증:

```bash
grep -rE "(int32|float|double) [A-Z][A-Za-z]*" \
  MadeByClaudeCode/Source/MadeByClaudeCode/Data/GiftCardRow.h
```

기대 결과: **0 매치** (매치 발생 시 C1 schema gate 위반 — PR 차단)

허용 타입: `FName`, `TArray<FName>`, `FText`, `FSoftObjectPath`만.

## 관련 ADR

- ADR-0002: Data Pipeline 컨테이너 선택 (C1/C2 schema gate 정의)
- ADR-0010: FFinalFormRow 저장 형식 — UPrimaryDataAsset 격상
- ADR-0011: Tuning Knob 노출 방식 — UDeveloperSettings 채택

---

## Story 1-5 — Pipeline Subsystem Skeleton + 4-State Machine

### 실제 테스트 파일

```
MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineSubsystemTests.cpp
```

### 카테고리

```
MossBaby.Data.Pipeline.*
```

### 헤드리스 실행 명령

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.Data.Pipeline.; Quit" \
  -nullrhi -nosound -log -unattended
```

### AC 매핑

| Automation Test | AC | 타입 | Gate |
|---|---|---|---|
| `MossBaby.Data.Pipeline.InitialState` | AC-DP-01 (skeleton: Uninitialized 초기 상태), GetState/IsReady | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.PullAPIReturnsEmptyOnReady` | AC-DP-04 skeleton (GetCardRow NAME_None 빈 TOptional) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.DelegateBind` | OnLoadComplete delegate 바인딩 + Broadcast 인자 전달 | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.DeinitClearsRegistries` | Deinitialize 후 IsReady() == false, GetState() == Uninitialized | AUTOMATED | BLOCKING |
| EDataPipelineState enum 정의 | 헤더 컴파일 (이 테스트 파일 컴파일 = enum 존재 증명) | CODE_REVIEW | BLOCKING |
| Pull API 8개 선언 | 모두 뼈대 스텁 — Story 1-6에서 실제 catalog 연동 | CODE_REVIEW | BLOCKING |

### AC-DP-13 checkf CI 전략

AC-DP-13: `Uninitialized`/`Loading` 상태에서 pull API 호출 시 `checkf` assertion 발동 (Debug/Development 빌드).

`checkf` 실패는 process abort를 유발하므로 UE Automation으로 직접 트리거 불가.

**CI 검증 방법 (CODE_REVIEW / BLOCKING)**:

```bash
# 8개 pull API 함수 모두 "AC-DP-13" 문자열 포함 checkf 존재 확인
grep -c "AC-DP-13" MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp
# 기대 결과: 8 이상 (각 pull API 함수별 checkf 1개)
```

Shipping 빌드에서의 `checkf` 동작: UE의 `checkf`는 `DO_CHECK` 매크로로 제어되며,
`Shipping` 구성에서는 기본적으로 `DO_CHECK=0`으로 비활성화될 수 있음.
ADR-0003 §AC-DP-13 권고에 따라 Shipping에서도 유지가 권장되며,
`UE_BUILD_SHIPPING` 환경에서의 동작은 Implementation 단계에서 실측 검증 필요.

### 생성/수정 파일

| 파일 | 역할 |
|---|---|
| `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.h` | UGameInstanceSubsystem 뼈대 + 4-state machine + pull API 선언 |
| `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp` | 최소 구현 (pull API 스텁 + checkf + state machine) |
| `MadeByClaudeCode/Source/MadeByClaudeCode/Data/MossFinalFormData.h` | FMossFinalFormData read-only view struct (ADR-0010) |
| `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineSubsystemTests.cpp` | UE Automation 테스트 4개 |

### Out of Scope

- AC-DP-01 Integration test (UGameInstance lifecycle 통합 — Story 1-7 또는 별도 integration)
- Story 002 (DefaultEngine.ini PrimaryAssetTypesToScan 등록)
- Story 1-6 (실제 카탈로그 등록/로딩 로직 — RegisterCard/FinalForm/Dream/StillnessCatalog)
- Story 1-7 (PIE hot-swap RefreshCatalog 완성, Save/Load 연동, OnLoadComplete 파라미터 실제화)
- AC-DP-09/10 (T_init 실측 + 3단계 임계 로그 — Story 1-6 구현 후 적용)

### 관련 ADR

- ADR-0003: Data Pipeline 로딩 전략 — Sync 일괄 로드 채택 (4-state machine)
- ADR-0002: Data Pipeline 컨테이너 선택 (pull API 반환 타입 근거)
- ADR-0010: FFinalFormRow 저장 형식 — FMossFinalFormData read-only view 근거

---

## Story 1-6 — Catalog Registration Loading + DegradedFallback

### 실제 테스트 파일

```
MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineCatalogTests.cpp
```

### 카테고리

```
MossBaby.Data.Pipeline.Catalog.*
```

### 헤드리스 실행 명령

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.Data.Pipeline.Catalog.; Quit" \
  -nullrhi -nosound -log -unattended
```

### AC 매핑

| Automation Test | AC | 타입 | Gate |
|---|---|---|---|
| `MossBaby.Data.Pipeline.Catalog.ClearAll` | TestingClearAll 레지스트리 초기화 확인 | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Catalog.CardTableInjection` | AC-DP-04 (Card Fail-close — NAME_None, nonexistent) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Catalog.DreamRegistrationFlow` | AC-DP-05 (Dream pull API 정상 + Fail-close, nullptr 금지) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Catalog.FinalFormRegistrationFlow` | AC-DP-05 (FinalForm pull API 정상 + Fail-close, ADR-0010 FromAsset 변환) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Catalog.StillnessAsset` | Stillness 주입/미주입 GetStillnessBudgetAsset 반환 | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Catalog.DegradedFallbackState` | AC-DP-03 (DegradedFallback IsReady==false, GetState, GetFailedCatalogs) | AUTOMATED | BLOCKING |

### AC 커버리지 상세

- **AC-DP-02** (등록 순서 Card→FinalForm→Dream→Stillness): Initialize 구현 확인
  (Initialize 직접 호출은 FSubsystemCollectionBase 귀속으로 불가 — integration test로 분리)
- **AC-DP-03** (DegradedFallback 진입): `FDataPipelineDegradedFallbackStateTest` + `EnterDegradedFallback` helper 구현
- **AC-DP-04** (fail-close Card): Story 1-5 `FDataPipelinePullAPIReturnsEmptyOnReadyTest` + Story 1-6 `FDataPipelineCardTableInjectionTest` 보완 커버
- **AC-DP-05** (fail-close Dream/Growth): `FDataPipelineDreamRegistrationFlowTest` + `FDataPipelineFinalFormRegistrationFlowTest`
- **AC-DP-06** [5.6-VERIFY] UEditorValidator: **DEFERRED** — tech-debt TD-007 (별도 Editor-module story)
- **AC-DP-16** Unknown CardId warning: Story 1-5 `GetCardRow` 구현에 UE_LOG 포함, integration 재검증 불필요

### Integration test 미작성 근거

`Initialize()` full path (4단계 순서 확인 + DegradedFallback 실제 전이)는 `UGameInstance` 생성 필요.
TD-005 lifecycle integration story에 포함 예정.

### Deferred / Tech-debt

| 항목 | 내용 | 추적 ID |
|---|---|---|
| AC-DP-06 UEditorValidator | UEditorValidatorBase + UnrealEd/DataValidation 모듈 — Editor-only. 별도 story | TD-007 |
| Initialize integration test | FSubsystemCollectionBase / UGameInstance 생성 필요 — lifecycle integration | TD-005 |

### 생성/수정 파일

| 파일 | 역할 |
|---|---|
| `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.h` | private helpers + 테스트 훅 추가 |
| `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp` | Initialize 4단계 구현 + RegisterXxx helpers + EnterDegradedFallback |
| `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineCatalogTests.cpp` | UE Automation 테스트 6개 (신규) |

### 관련 ADR

- ADR-0003: Data Pipeline 로딩 전략 — Sync 일괄 로드 채택 (4단계 순서 + DegradedFallback 정책)
- ADR-0002: Data Pipeline 컨테이너 선택 (C1/C2 schema gate)
- ADR-0010: FFinalFormRow 저장 형식 — FMossFinalFormData read-only view (FromAsset 변환)

---

## Story 1-19 — T_init Budget + 3-단계 Overflow Threshold

### 실제 테스트 파일

```
MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineBudgetTests.cpp
```

### 카테고리

```
MossBaby.Data.Pipeline.Budget.*
```

### 헤드리스 실행 명령

```bash
UnrealEditor-Cmd.exe MadeByClaudeCode.uproject \
  -ExecCmds="Automation RunTests MossBaby.Data.Pipeline.Budget.; Quit" \
  -nullrhi -nosound -log -unattended
```

### AC 매핑

| Automation Test | AC | 타입 | Gate |
|---|---|---|---|
| `MossBaby.Data.Pipeline.Budget.TInitNormal` | AC-DP-09 (30ms → Normal, flag 미설정) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Budget.TInitWarning` | AC-DP-09 (52.6ms → Warning flag) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Budget.TInitError` | AC-DP-09 (76ms → Error flag, else-if 분기 확인) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Budget.TInitFatal` | AC-DP-09 (101ms → Fatal flag) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Budget.CatalogCardWarn` | AC-DP-10 (Card 210 >= 200×1.05 → Warn) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Budget.CatalogCardError` | AC-DP-10 (Card 301 >= 200×1.5 → Error) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Budget.CatalogCardFatal` | AC-DP-10 (Card 401 >= 200×2.0 → Fatal) | AUTOMATED | BLOCKING |
| `MossBaby.Data.Pipeline.Budget.GetCatalogStatsFormat` | AC-DP-08 (빈 카탈로그 Stats 포맷 검증) | AUTOMATED | BLOCKING |

### Fatal 로그 처리 방침

UE_LOG Fatal은 process 종료 유발 → test harness crash 방지를 위해 **Error 로그 + `[FATAL_THRESHOLD]` 접두어 + `bInitFatalTriggered = true`** 조합으로 구현.

Shipping 빌드에서 `checkf` 강제 강화는 **TD-013** (future, Story 1-20 후보).

### 테스트 값 근거 (CDO default 기준)

| Settings 필드 | 기본값 |
|---|---|
| MaxInitTimeBudgetMs | 50.0ms |
| MaxCatalogSizeCards | 200 |
| CatalogOverflowWarnMultiplier | 1.05 |
| CatalogOverflowErrorMultiplier | 1.5 |
| CatalogOverflowFatalMultiplier | 2.0 |

T_init Warn threshold = 50 × 1.05 = 52.5ms → 테스트에서 52.6ms 사용.
Card Error threshold = 200 × 1.5 = 300 → 테스트에서 301 사용.

### [5.6-VERIFY] 항목

AC-DP-09/10 실측 10회 반복 및 p95 측정은 **release-gate MANUAL test** (별도 분리).

### 수정 파일

| 파일 | 역할 |
|---|---|
| `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.h` | CheckCatalogSize / EvaluateTInitBudget / GetCatalogStats 선언 + Budget flag 멤버 + 테스트 훅 |
| `MadeByClaudeCode/Source/MadeByClaudeCode/Data/DataPipelineSubsystem.cpp` | Initialize 3-단계 분기 + CheckCatalogSize + EvaluateTInitBudget + GetCatalogStats 구현 |
| `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/DataPipelineBudgetTests.cpp` | UE Automation 테스트 8개 (신규) |

### 관련 ADR

- ADR-0003: Data Pipeline 로딩 전략 — T_init 성능 예산 §T_init 성능 예산
- ADR-0011: Tuning Knob 노출 방식 — UMossDataPipelineSettings CDO default 값 근거
