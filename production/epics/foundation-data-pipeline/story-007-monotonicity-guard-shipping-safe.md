# Story 007: 단조성 Shipping-safe guard + C1/C2 schema gate CI grep hooks

> **Epic**: Data Pipeline
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/data-pipeline.md`
**Requirement**: `TR-dp-002` (AC-DP-17 단조성 Shipping-safe guard), C1/C2 schema gate 강제
**ADR Governing Implementation**: ADR-0011 (Tuning Knob 런타임 안전성)
**ADR Decision Summary**: `ensureMsgf`는 Shipping에서 strip됨 → `checkf` 또는 명시적 `if + UE_LOG Fatal` 패턴으로 교체. 단조성 위반 (Warn% > Error% 등) 검출 시 카탈로그 등록 진입 차단.
**Engine**: UE 5.6 | **Risk**: LOW
**Engine Notes**: Forbidden APIs "`ensure*` macros in Shipping critical paths" (control-manifest §Forbidden APIs). Shipping strip 테스트는 Development build로 충분 (Shipping 빌드 테스트 환경 구성 시 [5.6-VERIFY] 격상).

**Control Manifest Rules (Foundation layer + Global)**:
- Required: "`ensure*` macros in Shipping critical paths" 대신 "`checkf` 또는 명시적 `if + UE_LOG Fatal` 패턴"
- Forbidden: `grep -rE "int32|float|double" Source/MadeByClaudeCode/Data/GiftCardRow.h` — C1 매치 0건
- Forbidden: Dream 코드 내 `if (Tag.X >= 0.6f)` 리터럴 — C2 grep 차단

---

## Acceptance Criteria

*From GDD §Acceptance Criteria + §Core Rules R4/R5:*

- [ ] AC-DP-17 [5.6-VERIFY]: `Warn% = 1.5`, `Error% = 1.2` (단조성 위반) 설정 시 `Initialize()` 진입 즉시 카탈로그 등록 단계 **진입 차단** + `UE_LOG Fatal` 또는 `checkf`로 위반 knob 이름과 값 메시지 출력. **Shipping 빌드 포함 모든 빌드에서 동일 동작** (Logic · BLOCKING · P0)
- [ ] C1 schema gate CI grep: `.claude/hooks/data-pipeline-c1-grep.sh` 스크립트 — `grep -rEn "UPROPERTY.*[[:space:]]+(int32|float|double)[[:space:]]+[A-Z]" Source/MadeByClaudeCode/Data/GiftCardRow.h` 0 매치 (CODE_REVIEW)
- [ ] C2 schema gate CI grep: `.claude/hooks/data-pipeline-c2-grep.sh` 스크립트 — Dream 관련 `*.cpp` 파일에서 `if.*>.*0\.[0-9]f` 또는 `if.*Days >= [0-9]+` 리터럴 패턴 0 매치 (CODE_REVIEW)
- [ ] `ensureMsgf` 사용을 Pipeline 소스에서 금지 — `grep "ensure.*Msgf" Source/MadeByClaudeCode/Data/` 매치 0건 (CODE_REVIEW)

---

## Implementation Notes

*Derived from ADR-0003 §Known Compromise + GDD §G.7 Shipping-safe 재설계:*

```cpp
// DataPipelineSubsystem.cpp
void UDataPipelineSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    const auto* Settings = UMossDataPipelineSettings::Get();

    // AC-DP-17 단조성 Shipping-safe guard
    if (Settings->CatalogOverflowWarnMultiplier >= Settings->CatalogOverflowErrorMultiplier
        || Settings->CatalogOverflowErrorMultiplier >= Settings->CatalogOverflowFatalMultiplier) {
        UE_LOG(LogDataPipeline, Fatal,
            TEXT("Monotonicity violation: Warn=%.2f >= Error=%.2f OR Error=%.2f >= Fatal=%.2f — Pipeline init aborted"),
            Settings->CatalogOverflowWarnMultiplier,
            Settings->CatalogOverflowErrorMultiplier,
            Settings->CatalogOverflowErrorMultiplier,
            Settings->CatalogOverflowFatalMultiplier);
        // checkf — Shipping 포함
        checkf(false, TEXT("ADR-0003 monotonicity guard violation"));
        return;  // unreachable — checkf crashes
    }

    // 단조성 확인 후 Step 1-4 진입 (Story 004/006)
    CurrentState = EDataPipelineState::Loading;
    // ...
}
```

**CI grep scripts**:

```bash
# .claude/hooks/data-pipeline-c1-grep.sh
#!/usr/bin/env bash
# C1 schema gate — FGiftCardRow 수치 필드 금지 (ADR-0002)
set -e
MATCHES=$(grep -rEn "UPROPERTY[^)]*\)[[:space:]]*(int32|float|double)[[:space:]]+[A-Z]" \
    Source/MadeByClaudeCode/Data/GiftCardRow.h || true)
if [[ -n "$MATCHES" ]]; then
    echo "ERROR: C1 schema gate 위반 — FGiftCardRow에 수치 필드 검출:"
    echo "$MATCHES"
    exit 1
fi
echo "OK: C1 schema gate 통과"

# .claude/hooks/data-pipeline-c2-grep.sh
#!/usr/bin/env bash
# C2 schema gate — Dream 코드에 리터럴 임계 금지 (ADR-0002 §R5)
set -e
MATCHES=$(grep -rEn "if.*(Tag|TriggerTag|TagVector).*>=?[[:space:]]*[0-9]+\.?[0-9]*f?" \
    Source/MadeByClaudeCode/ --include="*.cpp" --include="*.h" || true)
if [[ -n "$MATCHES" ]]; then
    echo "ERROR: C2 schema gate 위반 — Dream 코드에 리터럴 임계 검출:"
    echo "$MATCHES"
    exit 1
fi
echo "OK: C2 schema gate 통과"

# ensureMsgf 금지 (Shipping critical path)
grep -r "ensureMsgf" Source/MadeByClaudeCode/Data/ && exit 1 || echo "OK: no ensureMsgf in Pipeline"
```

- CI/PR gate로 통합 — update-config skill로 `.claude/settings.json` 등록
- Shipping 빌드 테스트는 현재 Development 빌드로 갈음 (Cooked/Shipping 환경 구성은 production 단계)

---

## Out of Scope

- `.claude/rules/data-pipeline-schema-gates.md` agent rule 작성 (별도 Task)
- Shipping 빌드 실제 빌드 검증 (Production 단계)

---

## QA Test Cases

**For Logic story:**
- **AC-DP-17 [5.6-VERIFY] (단조성 guard)**
  - Given: `Warn = 1.5, Error = 1.2` (위반) → Settings save
  - When: `Initialize()`
  - Then: `checkf` fire (development) + `UE_LOG Fatal` 메시지 "Warn=1.50 >= Error=1.20" + 카탈로그 등록 미진입
  - Edge cases: `Error = Fatal` 동률 → 동일 차단. 정상 설정 (1.05/1.5/2.0) → 정상 진행
- **C1 schema gate CI grep (CODE_REVIEW)**
  - Setup: `FGiftCardRow` 파일 + CI hook
  - Verify: `./data-pipeline-c1-grep.sh` 실행 → exit 0
  - Pass: 0 매치 확인. 실험: 가짜 `UPROPERTY() int32 GrowthBonus;` 삽입 후 재실행 → exit 1
- **C2 schema gate CI grep (CODE_REVIEW)**
  - Setup: Dream 코드 전체 스캔
  - Verify: `./data-pipeline-c2-grep.sh` 실행
  - Pass: 0 매치. 실험: 가짜 `if (TagWeight >= 0.6f)` 삽입 후 재실행 → exit 1
- **ensureMsgf 금지 (CODE_REVIEW)**
  - Setup: Pipeline 소스 grep
  - Verify: `grep "ensureMsgf" Source/MadeByClaudeCode/Data/` = 0 매치

---

## Test Evidence

**Story Type**: Logic (CODE_REVIEW parts)
**Required evidence**:
- `tests/unit/data-pipeline/monotonicity_guard_test.cpp` (AC-DP-17 [5.6-VERIFY]; Development + Shipping 빌드 양쪽 목표)
- `.claude/hooks/data-pipeline-c1-grep.sh` + `.claude/hooks/data-pipeline-c2-grep.sh` (CODE_REVIEW CI hooks)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (자산 타입), Story 006 (3-단계 임계 로직과 연동)
- Unlocks: None (epic 최종 검증)
