# Unreal Automation Tests — Moss Baby

**Framework**: UE Automation Testing Framework (Engine 5.6 내장)
**Compilation**: `MadeByClaudeCode` module에 자동 포함 (Build.cs 변경 불필요 — `Source/MadeByClaudeCode/` 하위 모든 .cpp 자동 컴파일)
**Category Prefix**: `MossBaby.`

## Naming Convention

- **파일**: `Moss[System]Tests.cpp` (예: `MossFormulaTests.cpp`, `MossTimeSessionTests.cpp`)
- **Class**: `F[TestName]Test` 패턴 (UE 관례 — `F` prefix)
- **Category**: `MossBaby.[System].[Feature]` 3-part (예: `MossBaby.Time.Formula2MonoDelta`)
- **Flags**: `EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter` (에디터 + 헤드리스 모두 실행)

## 예시 Test Pattern

```cpp
// MossFormulaTests.cpp 참조

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMossGrowthVectorNormTest,
    "MossBaby.Growth.Formula2VectorNorm",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMossGrowthVectorNormTest::RunTest(const FString& Parameters) {
    // Given: TagVector = {Spring: 3.0, Calm: 2.0, Water: 1.0}
    TMap<FName, float> TagVector;
    TagVector.Add(TEXT("Season.Spring"), 3.0f);
    TagVector.Add(TEXT("Emotion.Calm"), 2.0f);
    TagVector.Add(TEXT("Element.Water"), 1.0f);

    // When: F2 VectorNorm
    float TotalWeight = 0.0f;
    for (const auto& Pair : TagVector) TotalWeight += Pair.Value;

    // Then: V_norm 합계 1.0 + Spring 0.500
    TestEqual(TEXT("TotalWeight"), TotalWeight, 6.0f);
    const float SpringNorm = TagVector[TEXT("Season.Spring")] / TotalWeight;
    TestEqual(TEXT("Season.Spring V_norm"), SpringNorm, 0.500f, KINDA_SMALL_NUMBER);

    return true;
}
```

## Test Category 매핑 (Architecture 14 시스템)

각 시스템별 권장 카테고리:

| 시스템 | Category Prefix |
|---|---|
| Time & Session (#1) | `MossBaby.Time.*` |
| Data Pipeline (#2) | `MossBaby.Pipeline.*` |
| Save/Load (#3) | `MossBaby.SaveLoad.*` |
| Input Abstraction (#4) | `MossBaby.Input.*` |
| Game State Machine (#5) | `MossBaby.GSM.*` |
| Moss Baby Character (#6) | `MossBaby.Character.*` |
| Growth Accumulation (#7) | `MossBaby.Growth.*` |
| Card System (#8) | `MossBaby.Card.*` |
| Dream Trigger (#9) | `MossBaby.Dream.*` |
| Stillness Budget (#10) | `MossBaby.Stillness.*` |
| Lumen Dusk Scene (#11) | `MossBaby.Lumen.*` — 단, 대부분 MANUAL (시각 검증) |
| Card Hand UI (#12) | `MossBaby.CardHandUI.*` — Logic만 자동화, 시각은 MANUAL |
| Dream Journal UI (#13) | `MossBaby.DreamJournal.*` — 동일 |
| Window & Display (#14) | `MossBaby.Window.*` — 일부 AUTOMATED 가능 |

## 실행 방법

### 에디터 (개발 중)

1. UnrealEditor 실행 → `Tools` → `Session Frontend` → `Automation` 탭
2. `MossBaby.` 카테고리 expand → 원하는 테스트 체크
3. `Start Tests` → 결과 확인

### 헤드리스 (로컬 또는 CI)

```bash
# Windows (예시)
"C:/Program Files/Epic Games/UE_5.6/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "$(pwd)/MadeByClaudeCode.uproject" \
  -ExecCmds="Automation RunTests MossBaby.; Quit" \
  -nullrhi -nosound -log -unattended \
  -Buildmachine -stdout -AllowStdOutLogVerbosity
```

## AC ↔ Test 매핑 원칙

각 GDD §Acceptance Criteria의 **AUTOMATED** 라벨 AC는 본 디렉토리의 test로 1:1 매핑. 예:

- `AC-GAE-13` Growth F2 정규화 → `MossFormulaTests.cpp::FMossGrowthVectorNormTest`
- `AC-GSM-07` GSM Formula 1 SmoothStep → `FMossGSMSmoothStepTest`
- `AC-CS-13` Card System 계절 경계 → `FMossCardSeasonBoundaryTest`

테스트 file의 주석에 출처 AC ID 명시.

## Related

- `tests/README.md` — top-level test infrastructure
- `.claude/docs/coding-standards.md §Testing Standards`
- 각 GDD §Acceptance Criteria
- `docs/architecture/architecture.md` API Boundaries § Phase 4 (14 시스템 contract)
