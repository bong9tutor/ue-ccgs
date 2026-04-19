# Technical Debt Register

Claude Code Game Studios 프레임워크 기반 프로젝트의 기술 부채 추적 문서.
`/story-done`에서 advisory로 발견된 항목과 `/code-review`에서 지적된 수정 연기 항목을 기록한다.

**Last Updated**: 2026-04-19

---

## Active Items

### TD-001 — FClockSourceRealUtcDiffNonNegativeTest NTP 간헐 실패 위험

| 필드 | 값 |
|---|---|
| **Severity** | S3 (잠재) |
| **Source** | `/code-review` Story-001 (2026-04-19) |
| **Origin Story** | `production/epics/foundation-time-session/story-001-clock-source-interface.md` |
| **File** | `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/ClockSourceTests.cpp:119-139` |
| **Description** | `FRealClockSource::GetUtcNow()` 연속 호출 간 NTP 보정이 개입하면 두 번째 값이 첫 번째보다 작을 수 있음. 즉각 연속 호출이라 확률 극저이지만 CI에서 NTP 동기화 이벤트와 겹치면 flaky test 발생 가능. |
| **Impact** | CI 신뢰도. 간헐적 빌드 실패로 원인 추적 시간 소비 가능성. |
| **Proposed Fix** | (a) CI 환경 NTP 동기화 정책 확인 후 허용 범위 내라면 `// test-standards deterministic exception` 주석 추가, (b) 또는 `FMockClockSource` 기반 시나리오로 재작성하고 실제 UTC는 smoke 레벨로만 검증. |
| **Owner** | qa-lead (향후 결정) |
| **Status** | Open |
| **Link** | — |

### TD-010 — Save/Load Migration chain Formula 4 실경로 검증 (CURRENT_SCHEMA_VERSION bump 시)

| 필드 | 값 |
|---|---|
| **Severity** | S4 (Future) |
| **Source** | Story 1-16 (2026-04-19) |
| **Origin Story** | `production/epics/foundation-save-load/story-005-migration-chain-archive.md` |
| **File** | `MadeByClaudeCode/Source/MadeByClaudeCode/SaveLoad/MossSaveLoadSubsystem.cpp` (RunMigrationChain) |
| **Description** | CURRENT_SCHEMA_VERSION = 1 (static constexpr) — Formula 4 multi-step migration 경로는 현재 dormant. MigrateFromV1ToV2 + 실제 rollback 경로는 V 2+ bump 시에만 활성화. Mock migrator injection 훅 (TestingSetV1ToV2Migrator)은 준비 완료. |
| **Impact** | 미래 schema bump 시 Formula 4 회귀 발견 시점 지연. 현재 functional gap 없음 (CURRENT=1 기준). |
| **Proposed Fix** | CURRENT_SCHEMA_VERSION bump PR에서 다음 통합: (1) MigrateFromV{N}ToV{N+1} 실구현, (2) MossSaveMigrationTests에 `TestingSetV1ToV2Migrator` 기반 실제 rollback 시나리오 테스트 추가, (3) ENGINE_ARCHIVE MANUAL evidence release-gate 포함. |
| **Owner** | 미래 schema bump 담당자 |
| **Status** | Open (future-trigger) |

### TD-009 — AC-DP-18 [5.6-VERIFY] Cooked 빌드 PrimaryAssetType 실측 필요

| 필드 | 값 |
|---|---|
| **Severity** | S2 (Sprint 완료 게이트) |
| **Source** | Story 1-15 진행 (2026-04-19) |
| **Origin Story** | `production/epics/foundation-data-pipeline/story-002-defaultengine-ini-primary-asset-types.md` |
| **File** | `MadeByClaudeCode/Config/DefaultEngine.ini` (에이전트 등록 완료) |
| **Description** | Story 1-15는 Integration story. DefaultEngine.ini 등록은 완료 (3 PrimaryAssetType). Cooked 빌드 [5.6-VERIFY] 실측은 UE Editor Package Project + .exe 실행 필요 — 에이전트 자동화 범위 밖. |
| **Impact** | Sprint 1 완료 게이트. Cooked 빌드에서 `UAssetManager::GetPrimaryAssetIdList` 빈 배열 반환 시 Pipeline DegradedFallback 진입 (AC-DP-03) → 프로덕션 게임 플레이 불가. |
| **Proposed Fix** | 사용자: (1) Story 1-11 에셋 생성 후 (TD-008 선행), (2) Content/Data/Dreams/FinalForms에 샘플 자산 3+ 배치, (3) UE Editor Package Project → Cooked .exe 실행, (4) 로그에서 `DreamData count: N` 확인. 가이드: `production/qa/evidence/cooked-primary-asset-type-placeholder-2026-04-19.md`. |
| **Owner** | 사용자 (UE Editor + Package 작업) |
| **Status** | Open (TD-008 해결 후 검증) |

### TD-008 — Story 1-11 Input Actions 에셋 수동 생성 필요 (Config/Data)

| 필드 | 값 |
|---|---|
| **Severity** | S2 (Sprint 완료 게이트) |
| **Source** | Story 1-11 진행 (2026-04-19) |
| **Origin Story** | `production/epics/foundation-input-abstraction/story-001-input-actions-mapping-contexts.md` |
| **Files** | (사용자 생성 예정) `Content/Input/Actions/IA_*.uasset` (6개) + `Content/Input/Contexts/IMC_MouseKeyboard.uasset` + `Content/Input/Contexts/IMC_Gamepad.uasset` |
| **Description** | Story 1-11은 Type: Config/Data로 UE Editor GUI에서 수동 에셋 생성이 필요. Agent가 .uasset 바이너리를 직접 생성할 수 없음. Placeholder evidence 문서 생성 완료 (`production/qa/evidence/input-actions-placeholder-2026-04-19.md`) + 사용자 가이드 포함. |
| **Impact** | Sprint 1 완료 게이트 (Input 에셋 없이 1-12/1-13 실행 시 런타임 null, 테스트 실패). Sprint 1 QA sign-off 전 필수 해결. |
| **Proposed Fix** | 사용자가 UE Editor에서: (1) Content/Input/Actions/ 생성, (2) 6 UInputAction 생성, (3) IMC_MouseKeyboard 생성 + 바인딩, (4) IMC_Gamepad 구조 생성. 스크린샷 evidence 첨부. 자세한 가이드는 `input-actions-placeholder-2026-04-19.md` 참조. |
| **Owner** | 사용자 (UE Editor 작업) |
| **Status** | Open (Sprint 1 종료 전 해결 필수) |

### TD-007 — AC-DP-06 [5.6-VERIFY] UEditorValidatorBase 미구현 (별도 Editor-module story)

| 필드 | 값 |
|---|---|
| **Severity** | S3 |
| **Source** | Story 1-6 Completion Notes (2026-04-19) |
| **Origin Story** | `production/epics/foundation-data-pipeline/story-004-catalog-registration-loading.md` AC-DP-06 |
| **File** | (신규 예정) `MadeByClaudeCode/Source/MadeByClaudeCode/DataValidation/MossCardValidator.h/.cpp` |
| **Description** | `UEditorValidatorBase` 서브클래스(`UMossCardValidator`)가 Story 1-6에서 DEFERRED. C1 schema gate를 저장 시점에 강제하는 Editor-only 검증기. UnrealEd + DataValidation 모듈 의존 + `#if WITH_EDITOR` 가드 + Editor-only 빌드 타겟 필요. Story 1-6은 Logic 범위이므로 Editor-module 분리. |
| **Impact** | C1 schema gate가 현재 CI grep (README 명령)으로만 강제됨. Editor 저장 시점 실시간 검증 부재 → 수치 필드 추가 시도가 빌드 실패까지 미발견 위험. |
| **Proposed Fix** | 신규 story 등록: "Pipeline 008 — UMossCardValidator Editor-module". Sprint 02 또는 Data epic 마무리 시점. MadeByClaudeCodeEditor Editor-only 모듈 설정 + UnrealEd/DataValidation 의존 추가. |
| **Owner** | Data Pipeline epic 담당자 |
| **Status** | Open (별도 story 배정 대기) |

### TD-006 — EGrowthStage enum 임시 위치 (Data → Characters 이동)

| 필드 | 값 |
|---|---|
| **Severity** | S4 |
| **Source** | `/code-review` Story 1-4 unreal-specialist (2026-04-19) |
| **Origin Story** | `production/epics/foundation-data-pipeline/story-001-schema-types-settings.md` |
| **File** | `MadeByClaudeCode/Source/MadeByClaudeCode/Data/GrowthStage.h` |
| **Description** | `EGrowthStage` enum이 `Data/GrowthStage.h`에 임시 정의됨. 원래 소유권은 `core-moss-baby-character` epic. `DreamDataAsset`이 include하여 현재 의존. Epic 진입 시 이동 + include 경로 업데이트 필요. |
| **Impact** | core-moss-baby-character epic 착수 시 리팩토링 범위. 추가 파일이 include하기 시작하면 범위 확장. |
| **Proposed Fix** | core-moss-baby-character epic 착수 전 전용 이동 Story 등록 ("EGrowthStage를 Characters/MossBabyTypes.h로 이동"). 이동 시 DreamDataAsset.h include 경로 동시 업데이트. |
| **Owner** | core-moss-baby-character epic 담당자 |
| **Status** | Open (epic 진입 시 해결) |

### TD-005 — UMossTimeSessionSubsystem Initialize/Deinitialize integration test 배정 필요

| 필드 | 값 |
|---|---|
| **Severity** | S3 |
| **Source** | `/code-review` Story 1-3 qa-tester (2026-04-19) |
| **Origin Story** | `production/epics/foundation-time-session/story-003-subsystem-skeleton-enums.md` |
| **File** | `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/TimeSessionSubsystemSkeletonTests.cpp` |
| **Description** | `FSubsystemCollectionBase`가 UE 내부 구조라 unit test에서 직접 생성 불가. Subsystem Initialize/Deinitialize 실제 생명주기 + `IncrementNarrativeCountAndSave` 동작 검증이 unit 범위 밖. 현재 어느 Story에서 회수할지 미지정. |
| **Impact** | Subsystem lifecycle 회귀가 sprint 후반까지 미포착될 위험. 특히 Story 1-7 Save/Load 연동 시 narrative count atomic 요구사항이 integration 없이 검증되지 않음. |
| **Proposed Fix** | Story 1-7 (`story-002-subsystem-lifecycle-triggers.md`) 또는 신규 integration story에서 UGameInstance 기반 Subsystem lifecycle 테스트 포함. |
| **Owner** | qa-lead (배정 결정) |
| **Status** | Open |

### TD-004 — EBetweenSessionClass / EInSessionClass enum 값 자동 검증 부재

| 필드 | 값 |
|---|---|
| **Severity** | S4 |
| **Source** | `/code-review` Story 1-3 qa-tester (2026-04-19) |
| **Origin Story** | `production/epics/foundation-time-session/story-003-subsystem-skeleton-enums.md` |
| **File** | `MadeByClaudeCode/Source/MadeByClaudeCode/Time/MossTimeSessionTypes.h` |
| **Description** | `EBetweenSessionClass`(5 values) + `EInSessionClass`(4 values)가 테스트 내 직접 참조 없이 CODE_REVIEW (헤더 컴파일)로만 검증. Story 004 Classifier 구현 전 값 이름·개수 변경 리스크. |
| **Impact** | Story 004 Classifier가 이 enum 값에 의존할 때, rename/drop이 CI 그린 상태로 통과할 수 있음. |
| **Proposed Fix** | Story 004 AC에 `static_assert` 또는 CI grep으로 각 enum의 값 개수 + 이름 정적 검증 추가. 또는 Story 1-3 테스트 파일에 compile-time `UEnum::StaticEnum<>()->GetMaxEnumValue()` 검증 테스트 추가. |
| **Owner** | Story 004 담당자 |
| **Status** | Open (Story 004 착수 시 해결) |

### TD-003 — FSessionRecord UPROPERTY(SaveGame) specifier 결정 (Story 1-7 의존)

| 필드 | 값 |
|---|---|
| **Severity** | S3 (설계 결정 미확정) |
| **Source** | `/code-review` Story 1-2 unreal-specialist (2026-04-19) |
| **Origin Story** | `production/epics/foundation-time-session/story-002-session-record-struct.md` |
| **File** | `MadeByClaudeCode/Source/MadeByClaudeCode/Time/SessionRecord.h` |
| **Description** | Story 1-2 `FSessionRecord`의 모든 필드가 `UPROPERTY()` 단순 선언. 현재 round-trip 테스트는 `ArIsSaveGame=false` 경로로 통과. 그러나 Story 1-7 Save/Load Subsystem이 `USaveGame` 기반 `ArIsSaveGame=true` 경로를 선택하면, 모든 필드에 `UPROPERTY(SaveGame)` 소급 추가 필요. 선택 시기: Story 1-7 착수 시. |
| **Impact** | Story 1-7에서 직렬화 경로 결정 시 헤더 일괄 수정 필요. 미처리 시 Save 파일이 빈 FSessionRecord로 저장됨 (silent data loss). |
| **Proposed Fix** | Story 1-7 `story-001-save-data-schema-enum.md`에 "직렬화 경로 결정 (ArIsSaveGame true vs false)" 전제 조건 섹션 추가. true 선택 시 SessionRecord.h 필드 전체에 `(SaveGame)` 추가. |
| **Owner** | Story 1-7 담당자 |
| **Status** | Open (Story 1-7 착수 시 해결) |
| **Link** | Story 1-2 Completion Notes ADVISORY 섹션 |

### TD-002 — test-standards.md가 UE IMPLEMENT_SIMPLE_AUTOMATION_TEST idiom 예외 미명시

| 필드 | 값 |
|---|---|
| **Severity** | S4 (문서 갭) |
| **Source** | `/code-review` Story-001 qa-tester (2026-04-19) |
| **Origin Story** | `production/epics/foundation-time-session/story-001-clock-source-interface.md` |
| **File** | `.claude/rules/test-standards.md` |
| **Description** | `test-standards.md`는 `test_[system]_[scenario]_[expected]` 네이밍 패턴을 강제. 하지만 UE Automation Framework의 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` 매크로는 `F[Name]Test` 클래스명을 생성하므로 규칙 준수 불가. 엔진 idiom 예외 조항이 필요. |
| **Impact** | 미래 UE 테스트 구현자가 `/code-review` 또는 `/story-done`에서 불필요한 advisory를 받음. 프레임워크 규칙과 엔진 관용구의 충돌. |
| **Proposed Fix** | `test-standards.md`에 "엔진별 idiom 예외" 섹션 추가. UE: `F[TestName]Test` + `MossBaby.[System].[Category].*` 카테고리 규칙 명시. |
| **Owner** | qa-lead |
| **Status** | Open |
| **Link** | — |

---

## Resolved Items

*(None yet)*

---

## Severity Legend

| Level | 설명 |
|---|---|
| **S1** | Critical — 릴리스 차단 |
| **S2** | High — 다음 sprint 내 해결 필수 |
| **S3** | Medium — 같은 layer/epic 내 해결 권장 |
| **S4** | Low — 편의 개선, 백로그 |

## Process

- 새 항목은 **Active Items** 상단에 append (`TD-NNN` 연번)
- 해결 시 **Resolved Items**로 이동 + PR/commit 링크 추가
- `/tech-debt` 스킬로 주기적 리뷰·트리아지
