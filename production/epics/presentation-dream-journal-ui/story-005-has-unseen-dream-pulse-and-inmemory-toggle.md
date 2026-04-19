# Story 005 — `bHasUnseenDream` 펄스 애니메이션 + in-memory 토글 + Save/Load 왕복

> **Epic**: [presentation-dream-journal-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3h

## Context

- **GDD**: [design/gdd/dream-journal-ui.md](../../../design/gdd/dream-journal-ui.md) §Detailed Design CR-6 새 꿈 알림 + CR-7 `bHasUnseenDream` 플래그 + §Interactions with Save/Load Persistence (read-only + in-memory 갱신) + §Edge Cases E-DJ-3 (읽다가 강제 닫힘)
- **TR-ID**: TR-dj-002 (OQ-DJ-1 RESOLVED — 단일 bool), TR-dj-004 (펄스), TR-dj-005 (왕복)
- **Governing ADR**: *(직접 없음 — Save/Load GDD §7 "Dream Journal UI — read only" contract)*
- **Engine Risk**: LOW — UMG Animation 펄스 + `UMossGameInstance::GetSaveData()` in-memory mutation 모두 UE 5.6 stable
- **Engine Notes**: `bHasUnseenDream`은 `UMossSaveData`의 single bool (GDD §OQ-DJ-1 MVP 결정). Dream Journal UI는 in-memory 변경만 (`SaveData->bHasUnseenDream = false`) — 디스크 commit은 다음 `ESaveReason::ECardOffered` 또는 `ESaveReason::EDreamReceived` 저장 시 자동 포함 (Save/Load §Core Rules §1 부분 저장 없음). Reduced Motion 시 펄스 생략 — 정적 하이라이트로 대체.
- **Control Manifest Rules**:
  - Presentation Layer Rules §Required Patterns — "Dream Journal UI는 `SaveAsync()` 직접 호출 금지 — `bHasUnseenDream = false`는 in-memory 변경만, 다음 저장 이유에 포함" (AC-DJ-17)
  - Feature Layer Rules §Required Patterns — Stillness Budget Motion Standard Request (펄스)
  - Cross-Layer Rules §Cross-Cutting — 알림·팝업 금지 (Pillar 1)

## Acceptance Criteria

- **AC-DJ-14** / **TR-dj-004** [`AUTOMATED`/BLOCKING] — `bHasUnseenDream = true` → 일기 아이콘 렌더 시 펄스 애니메이션 재생. ReducedMotion 시 정적 하이라이트.
- **AC-DJ-15** / **TR-dj-005** [`AUTOMATED`/BLOCKING] — `bHasUnseenDream = true` → 꿈 페이지 열기 → `bHasUnseenDream = false` in-memory 변경. `SaveAsync` 호출 없음.
- **AC-DJ-16** / **TR-dj-005** [`AUTOMATED`/BLOCKING] — 세션 종료 후 재시작 (`bHasUnseenDream = true`로 세이브된 상태) → 앱 재시작 → Waiting 진입 → 아이콘 펄스 재생. Save/Load 왕복 후 상태 보존.

## Implementation Notes

1. **아이콘 펄스 판단 로직** (`UDreamJournalIconWidget::RefreshVisuals`):
   ```cpp
   void UDreamJournalIconWidget::RefreshVisuals() {
       const auto* SaveData = UMossGameInstance::GetSaveData();
       if (!SaveData) return;

       const bool bShouldPulse = SaveData->bHasUnseenDream;

       if (bShouldPulse) {
           if (Stillness && Stillness->IsReducedMotion()) {
               // AC-DJ-14 Reduced Motion: 정적 하이라이트
               SetStaticHighlight(true);
           } else {
               auto Handle = Stillness->Request(EStillnessChannel::Motion, EStillnessPriority::Standard);
               if (Handle.IsValid()) {
                   PlayAnimation(PulseAnimation);  // IconPulseDurationSec 1.2s 1 사이클
                   PulseHandle = Handle;
               } else {
                   // Budget Deny — 정적 하이라이트로 대체
                   SetStaticHighlight(true);
               }
           }
       } else {
           StopAnimation(PulseAnimation);
           SetStaticHighlight(false);
       }
   }
   ```

2. **`RefreshVisuals` 호출 타이밍**:
   - `NativeConstruct`에서 1회.
   - `HandleGameStateChanged(_, Waiting)` 수신 시 (story 001 `UDreamJournalWidget` 위임 → Icon refresh 호출).
   - `OnDreamReceived` 이벤트 수신 시 (Dream Trigger가 `bHasUnseenDream = true` 설정 후 Journal에 간접 알림 — `FOnDreamTriggered` delegate 구독 또는 폴링).

3. **in-memory 토글** (`UDreamPageWidget::ShowDream` 확장):
   ```cpp
   void UDreamPageWidget::ShowDream(const FName& DreamId) {
       // ... 기존 로직 (story 003)

       // AC-DJ-15: in-memory 토글 only, SaveAsync 직접 호출 금지
       auto* SaveData = UMossGameInstance::GetMutableSaveData();
       if (SaveData && SaveData->bHasUnseenDream) {
           SaveData->bHasUnseenDream = false;
           // 주의: SaveAsync 호출 없음 — 다음 ESaveReason::ECardOffered/EDreamReceived 저장 시 자동 포함
           // (Save/Load GDD §Core Rules §1 부분 저장 없음 원칙)

           // Icon refresh 트리거 — Panel이 Closing되면 Icon은 다시 표시됨
           if (auto* Overlay = GetOverlayRoot()) {
               Overlay->GetIconWidget()->RefreshVisuals();
           }
       }
   }
   ```

4. **Save/Load 왕복 (AC-DJ-16)**:
   - Dream Trigger가 꿈 언록 시 `bHasUnseenDream = true` + `SaveAsync(ESaveReason::EDreamReceived)` 저장 (Dream Trigger epic 책임).
   - 본 story는 Journal UI가 재시작 후 SaveData에서 읽어 올바른 동작 수행하는지 검증.

5. **E-DJ-3 — 읽다가 강제 닫힘**:
   - `bHasUnseenDream`가 이미 `false`로 변경된 후 `OnGameStateChanged(Waiting, Dream)` 수신 시 Panel 강제 제거 (story 001) → `bHasUnseenDream` 되돌리지 않음 (GDD E-DJ-3 "중단된 읽기도 '봤음'으로 간주").

## Out of Scope

- Dream Trigger의 `bHasUnseenDream = true` 세팅 (Feature Dream Trigger epic 책임)
- AC-DJ-13/17/21/22 grep (story 006)
- 펄스 애니메이션 시각 세부 (UMG Animation 에셋 — 아트 파이프라인)

## QA Test Cases

**Test Case 1 — 펄스 재생 (AC-DJ-14)**
- **Setup**: Mock SaveData `bHasUnseenDream = true`. Mock IsReducedMotion = false, Stillness Request returns valid.
- **Verify**:
  - `RefreshVisuals()` 호출 후 `PlayAnimation(PulseAnimation)` 호출 1회.
  - `IsPlayingAnimation(PulseAnimation) == true` (1.2s 이내).
- **Pass**: 두 조건 충족.

**Test Case 2 — 펄스 ReducedMotion ON (AC-DJ-14)**
- **Setup**: `bHasUnseenDream = true`, IsReducedMotion = true.
- **Verify**:
  - `PlayAnimation(PulseAnimation)` 호출 0회.
  - `GetStaticHighlight() == true`.
- **Pass**: 두 조건 충족.

**Test Case 3 — in-memory 토글 (AC-DJ-15)**
- **Setup**: SaveData `bHasUnseenDream = true`, `UnlockedDreamIds = ["Dream_A"]`.
- **Verify**:
  - `ShowDream("Dream_A")` 호출 후 `SaveData->bHasUnseenDream == false` (in-memory).
  - `UMossSaveSubsystem::SaveAsync` 호출 카운트 == 0 (본 story 내에서).
  - Icon `RefreshVisuals()` 후 `IsPlayingAnimation(PulseAnimation) == false`.
- **Pass**: 3 조건 충족.

**Test Case 4 — Save/Load 왕복 (AC-DJ-16)**
- **Setup**: Mock SaveLoad fixture — `bHasUnseenDream = true`, `UnlockedDreamIds = ["Dream_A"]` 세이브 파일로 저장.
- **Steps**:
  - App 종료 시뮬 → SaveLoad commit 완료.
  - App 재시작 시뮬 → LoadAsync → `UMossGameInstance::GetSaveData()` 반환.
  - GSM Waiting 진입 broadcast.
- **Verify**:
  - 재시작 후 `SaveData->bHasUnseenDream == true`.
  - Icon `PlayAnimation(PulseAnimation)` 호출 1회.
- **Pass**: 두 조건 충족.

**Test Case 5 — Budget Deny fallback**
- **Setup**: `bHasUnseenDream = true`, IsReducedMotion = false, Stillness Request returns invalid.
- **Verify**:
  - `PlayAnimation(PulseAnimation)` 호출 0회.
  - `GetStaticHighlight() == true` (fallback).
- **Pass**: 두 조건 충족.

## Test Evidence

- [ ] `tests/unit/ui/dream_journal_pulse_test.cpp` — Test Cases 1, 2, 5.
- [ ] `tests/integration/ui/dream_journal_unseen_toggle_test.cpp` — Test Case 3 (AC-DJ-15).
- [ ] `tests/integration/ui/dream_journal_save_load_roundtrip_test.cpp` — Test Case 4 (AC-DJ-16).

## Dependencies

- **Depends on**: story-001 (위젯 골격, GSM 구독), story-002 (목록), story-003 (본문), story-004 (Opening/Closing Stillness 통합). Foundation Save/Load (`bHasUnseenDream` 필드 + round-trip API), Foundation Data Pipeline, Feature Dream Trigger (`FOnDreamTriggered` — 간접), Feature Stillness Budget.
- **Unlocks**: story-006 (grep validation — 마지막 epic gate).
