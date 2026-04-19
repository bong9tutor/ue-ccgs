# Story 007: E14 Disk Full Degraded flag + E15 Non-ASCII path + bSaveDegraded accessor

> **Epic**: Save/Load Persistence
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Manifest Version**: 2026-04-19

## Context

**GDD**: `design/gdd/save-load-persistence.md`
**Requirement**: E14/E15 edge cases
**ADR Governing Implementation**: None (GDD contract)
**ADR Decision Summary**: Disk full 3회 연속 실패 시 `bSaveDegraded = true` internal flag. UI 에러 메시지 금지 (Pillar 1). 비 ASCII 경로 (한국어 사용자명)에서 round-trip 성공.
**Engine**: UE 5.6 | **Risk**: LOW (FFileHelper UTF-8 지원)
**Engine Notes**: `FFileHelper::SaveArrayToFile`은 UTF-8 경로 안전. CI 환경에 한국어 사용자명 계정 없으므로 실기 외부 환경에서 검증 (E15는 별도 환경 구성 필요).

**Control Manifest Rules (Foundation + Cross-cutting)**:
- Required: "UI 알림·팝업·모달·요구사항 알림 금지" (Global Pillar 1)
- Required: "Save file 위치: `FPlatformProcess::UserSettingsDir()` 하위"

---

## Acceptance Criteria

*From GDD §Acceptance Criteria:*

- [ ] `E14_DISK_FULL_DEGRADED_FLAG`: 목 파일시스템이 `SaveArrayToFile` 3회 연속 실패 반환 → 3회 연속 `SaveAsync` 실패 후 내부 `bSaveDegraded = true` 세팅. UI 에러 메시지 미표시. 직전 정상 슬롯 무탈
- [ ] `E15_NON_ASCII_PATH`: 사용자명 `봉구` 환경 — `UserSettingsDir()` 경로에 비 ASCII 포함 — `SaveAsync` + `Initialize()` 왕복 성공. 로드된 데이터가 저장 전과 일치
- [ ] `bSaveDegraded` accessor public: `IsSaveDegraded() const` (이미 Story 002에서 선언)
- [ ] `SaveFailureRetryThreshold` knob (Story 001)으로 실패 카운터 구성 가능

---

## Implementation Notes

*Derived from GDD §E14 + §E15:*

```cpp
// MossSaveLoadSubsystem.cpp — SaveAsync 연장 (worker thread 쪽)
void UMossSaveLoadSubsystem::OnSaveWorkerFailed() {
    ConsecutiveSaveFailures++;
    const auto* Settings = UMossSaveLoadSettings::Get();
    if (ConsecutiveSaveFailures >= Settings->SaveFailureRetryThreshold) {
        SaveData->bSaveDegraded = true;
        UE_LOG(LogMossSaveLoad, Warning,
            TEXT("Save degraded — %d consecutive failures (threshold=%d)"),
            ConsecutiveSaveFailures, Settings->SaveFailureRetryThreshold);
        // UI 에러 메시지 없음 (Pillar 1)
    }
}

void UMossSaveLoadSubsystem::OnSaveWorkerSucceeded() {
    ConsecutiveSaveFailures = 0;
    // bSaveDegraded는 auto-recover하지 않음 — 세션 재시작까지 유지 (GDD §E14 의도)
}

// Non-ASCII path 지원 — 자동 (UTF-8 OS 경로를 FString이 내부적으로 처리)
FString UMossSaveLoadSubsystem::GetSlotPath(TCHAR Slot) const {
    return FString::Printf(TEXT("%sMossBaby/SaveGames/MossData_%c.sav"),
        *FPlatformProcess::UserSettingsDir(), Slot);
    // UserSettingsDir()이 한국어 포함하면 FString이 UTF-16 보관, FFileHelper는 Windows API UTF-8/16 변환 처리
}
```

- `ConsecutiveSaveFailures`는 `UPROPERTY()` 또는 `FThreadSafeCounter` (worker → game thread 동기화 필요)
- E15 테스트는 CI 외부 환경 (Windows 계정 생성 후 빌드) — `production/qa/evidence/non-ascii-path-evidence-[date].md` 문서화

---

## Out of Scope

- `NO_SAVE_INDICATOR_UI`, `NO_ANTITAMPER_LOGIC` CODE_REVIEW grep — Story 008에서 통합

---

## QA Test Cases

**For Logic story:**
- **E14_DISK_FULL_DEGRADED_FLAG**
  - Given: Mock filesystem returns `SaveArrayToFile` failure
  - When: `SaveAsync` 3회 연속
  - Then: `bSaveDegraded = true`, `IsSaveDegraded() == true`, 화면 에러 UI 부재 확인 (screen capture), 로그 "degraded" grep
  - Edge cases: 2회 실패 → degraded=false, 4회째 성공 → degraded 여전히 true (recover 안 함)
- **E15_NON_ASCII_PATH**
  - Given: Windows 계정 `봉구` (한국어) + CI는 별도 환경
  - When: round-trip 실행 (save + reload)
  - Then: 로드된 `FSessionRecord` 필드 전체 저장 전과 일치
  - Edge cases: Space 포함 경로 (`USERNAME = "이 끼"`), 이모지 포함 (`"봉구😊"`)
- **bSaveDegraded accessor**
  - Given: SaveData 로드 완료
  - When: `IsSaveDegraded()` 호출
  - Then: 초기 false, degraded 설정 후 true

---

## Test Evidence

**Story Type**: Logic (E15 MANUAL 외부)
**Required evidence**:
- `tests/unit/save-load/disk_full_degraded_test.cpp` (E14)
- `tests/integration/save-load/non_ascii_path_test.cpp` (E15 — 별도 환경에서 실행)
- `production/qa/evidence/non-ascii-path-evidence-[date].md` (MANUAL)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 004 (Atomic write — 실패 경로 확장), Story 001 (`bSaveDegraded` 필드)
- Unlocks: Story 008
