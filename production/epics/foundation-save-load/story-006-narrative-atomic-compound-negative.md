# Story 006: NarrativeCount Atomic Commit + Compound Event Negative AC (ADR-0009 qualifier)

> **Epic**: Save/Load Persistence
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/save-load-persistence.md`
**Requirement**: `TR-save-001` (OQ-8 compound event sequence atomicity), NARRATIVE_COUNT_ATOMIC_COMMIT invariant
**ADR Governing Implementation**: ADR-0009 (GSM BeginCompoundEvent / EndCompoundEvent boundary — Save/Load는 coalesce 정책만 제공)
**ADR Decision Summary**: Save/Load는 per-trigger atomicity만 보장. Compound atomicity는 GSM 책임 (`BeginCompoundEvent` boundary). Time `IncrementNarrativeCountAndSave()` single public method 강제 — 3중 검증 (컴파일 + static analysis + unit test).
**Engine**: UE 5.6 | **Risk**: LOW
**Engine Notes**: private method 가시성은 컴파일러가 강제. Release 빌드 인라인 우려는 static analysis + negative unit test로 해소 (GDD §5 R2 BLOCKER 3).

**Control Manifest Rules (Foundation layer)**:
- Required: "`Time.IncrementNarrativeCountAndSave` private 강제" — 단일 public method (ADR-0009)
- Required: "Save/Load coalesce 정책 활용으로 단일 commit" (ADR-0009)
- Forbidden: Save/Load 측에 sequence atomicity API 작성 금지 (ADR-0009)

---

## Acceptance Criteria

*From GDD §Acceptance Criteria (R2 — BLOCKER 3 재작성):*

- [ ] `NARRATIVE_COUNT_ATOMIC_COMMIT` — 3중 검증:
  - **1) 저장 round-trip**: `NarrativeCount=2` (cap=3), Idle → Time `IncrementNarrativeCountAndSave()` 호출 완료 → 저장된 슬롯 NarrativeCount=3
  - **2) Static analysis (CI gate)**: `grep -rE "IncrementNarrativeCount\b|TriggerSaveForNarrative\b" Source/MadeByClaudeCode/` 결과 — 각 function private 호출 count = 정의 1 + wrapper 1, 외부 callsite = 0
  - **3) Negative unit test (friend class)**: friend class로 `IncrementNarrativeCount()` 단독 호출 → process kill 시뮬 → 재시작 후 in-memory NarrativeCount=3이지만 disk=2 (cap leak 입증). Wrapper 사용 시 동일 시나리오에서 disk=3 (no leak)
- [ ] `COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY` (Negative AC — AUTOMATED): Day 13 시뮬 — `ECardOffered` → `EDreamReceived` (fault injection, fail) → `ENarrativeEmitted`. Disk에는 ECardOffered + ENarrativeEmitted 반영, EDreamReceived 미반영. **이는 의도된 동작** (per-trigger atomic만 보장)
- [ ] Time `IncrementNarrativeCount()` / `TriggerSaveForNarrative()` 가 **`private:` 가시성**으로 선언됨 (컴파일 강제)

---

## Implementation Notes

*Derived from GDD §5 R2 BLOCKER 3 재작성 + ADR-0009 §GSM 사용 패턴:*

```cpp
// foundation-time-session Story 003에서 이미 단일 public method 선언
// Source/MadeByClaudeCode/Time/MossTimeSessionSubsystem.h
public:
    void IncrementNarrativeCountAndSave();  // 유일 진입점
private:
    void IncrementNarrativeCount();        // in-memory only
    void TriggerSaveForNarrative();        // SaveAsync wrapper

// .cpp
void UMossTimeSessionSubsystem::IncrementNarrativeCountAndSave() {
    // Same function body — same game-thread execution
    IncrementNarrativeCount();      // in-memory mutation
    TriggerSaveForNarrative();      // SaveAsync(ENarrativeEmitted)
}

void UMossTimeSessionSubsystem::IncrementNarrativeCount() {
    CurrentRecord.NarrativeCount++;
}

void UMossTimeSessionSubsystem::TriggerSaveForNarrative() {
    // Save subsystem lookup
    UMossSaveLoadSubsystem* Save = GetWorld()->GetGameInstance()
        ->GetSubsystem<UMossSaveLoadSubsystem>();
    // Save subsystem writes CurrentRecord into UMossSaveData
    Save->UpdateSessionRecord(CurrentRecord);  // in-memory 동기화
    Save->SaveAsync(ESaveReason::ENarrativeEmitted);
}
```

**CI static analysis hook**: `.claude/hooks/narrative-count-atomic-grep.sh`

```bash
#!/usr/bin/env bash
# NARRATIVE_COUNT_ATOMIC_COMMIT — 외부 callsite = 0 강제
set -e
MATCHES=$(grep -rEn "\.IncrementNarrativeCount\b|\.TriggerSaveForNarrative\b|->IncrementNarrativeCount\b|->TriggerSaveForNarrative\b" \
    Source/MadeByClaudeCode/ --include="*.cpp" --include="*.h" \
    | grep -vE "(IncrementNarrativeCount\s*\(\)|TriggerSaveForNarrative\s*\(\))\s*\{" \
    || true)
# Count external callsites (exclude definition 1 + wrapper 1)
COUNT=$(echo "$MATCHES" | grep -v "^$" | wc -l)
if (( COUNT > 2 )); then
    echo "ERROR: IncrementNarrativeCount/TriggerSaveForNarrative external callsites detected:"
    echo "$MATCHES"
    exit 1
fi
echo "OK: NARRATIVE_COUNT_ATOMIC_COMMIT 통과 ($COUNT callsite)"
```

**Compound event negative AC 구현** (Day 13 fault injection):

```cpp
// tests/unit/save-load/compound_event_no_sequence_atomicity_test.cpp
void RunCompoundSequenceTest() {
    // 1. ECardOffered SaveAsync — commit 성공
    Subsystem->SaveAsync(ESaveReason::ECardOffered);
    WaitForCommit();
    VerifyDiskLastReason("ECardOffered");

    // 2. EDreamReceived SaveAsync — fault injection fail
    InjectWorkerThreadFault();
    Subsystem->SaveAsync(ESaveReason::EDreamReceived);
    WaitForCommit();  // silent retry (R3 fallback)
    // disk still LastReason == "ECardOffered" (EDreamReceived unreflected)
    VerifyDiskLastReason("ECardOffered");

    // 3. ENarrativeEmitted SaveAsync — commit 성공
    ClearFaultInjection();
    Subsystem->SaveAsync(ESaveReason::ENarrativeEmitted);
    WaitForCommit();
    VerifyDiskLastReason("ENarrativeEmitted");  // skips EDreamReceived

    // AC 명시적 검증: sequence atomicity 없음
    // Dream unlock 상태는 미반영 (intended)
}
```

---

## Out of Scope

- Story 007: E14 disk full + E15 non-ASCII
- GSM `BeginCompoundEvent` / `EndCompoundEvent` 실제 구현 (Core layer `core-game-state-machine` epic — ADR-0009)

---

## QA Test Cases

**For Logic story:**
- **NARRATIVE_COUNT_ATOMIC_COMMIT (1) round-trip**
  - Given: NarrativeCount=2, Idle
  - When: `Time->IncrementNarrativeCountAndSave()` 완료
  - Then: 저장된 슬롯 로드 → NarrativeCount=3
- **NARRATIVE_COUNT_ATOMIC_COMMIT (2) static analysis**
  - Setup: `.claude/hooks/narrative-count-atomic-grep.sh`
  - Verify: exit code 0
  - Pass: 외부 callsite 0 + 정의 1 + wrapper 1 = 총 ≤ 2 매치
- **NARRATIVE_COUNT_ATOMIC_COMMIT (3) negative unit test**
  - Given: friend class가 `IncrementNarrativeCount()` 단독 호출
  - When: process kill 시뮬
  - Then: 재시작 후 in-memory=3 but disk=2 (cap leak)
  - Control: Wrapper 사용 시 disk=3
- **COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY**
  - Given: Day 13 시뮬 + EDreamReceived에 fault injection
  - When: 3 reason 순차 호출
  - Then: disk `LastSaveReason = "ENarrativeEmitted"` (마지막 성공 commit), EDreamReceived 미반영
  - Edge cases: 3개 모두 fault injection → disk unchanged (직전 정상 슬롯 무탈)
- **private visibility (컴파일 강제)**
  - Given: 외부 소스가 `Time->IncrementNarrativeCount()` 직접 호출 시도
  - Then: 빌드 실패 ("is private") — 강제 확인

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- `tests/unit/save-load/narrative_count_atomic_test.cpp` (round-trip)
- `.claude/hooks/narrative-count-atomic-grep.sh` (static analysis CI hook)
- `tests/unit/save-load/narrative_negative_test.cpp` (friend class cap leak proof)
- `tests/unit/save-load/compound_event_negative_test.cpp` (D2 AC)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 004 (Atomic write), **foundation-time-session Story 003** (IncrementNarrativeCountAndSave single public method — cross-epic)
- Unlocks: Story 007
