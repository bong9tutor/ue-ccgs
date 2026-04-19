# Story 004 — Opening/Closing 트랜지션 + 페이지 넘김 + ReducedMotion 분기

> **Epic**: [presentation-dream-journal-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: Visual/Feel
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/dream-journal-ui.md](../../../design/gdd/dream-journal-ui.md) §States (Opening/Closing 트랜지션 설계) + §Interactions with Stillness Budget (Motion Standard 슬롯) + §G Tuning Knobs (OpenCloseDurationSec, PageTransitionDurationSec) + AC-DJ-19 Reduced Motion
- **TR-ID**: TR-dj-004 (AC-DJ-14 펄스 — story 005로 분리), ReducedMotion 일관 (AC-DJ-19)
- **Governing ADR**: *(직접 없음 — Stillness Budget 통합 패턴)*
- **Engine Risk**: LOW — UMG Animation + `FWidgetSwitcher` 모두 UE 5.6 stable
- **Engine Notes**: `UMG Animation`은 `PlayAnimation()` 호출로 재생 — ReducedMotion 시 `Stop()` + 최종 프레임 상태 직접 적용 (GDD §Implementation Notes UE 5.6 구현 참고 인용). Motion Standard 슬롯은 RAII로 관리 — `Stillness->Request` 후 `FStillnessHandle` 보관, 완료 콜백에서 `Release`. Budget Deny는 차단이 아닌 즉시 전환 (일기는 항상 열림).
- **Control Manifest Rules**:
  - Feature Layer Rules §Required Patterns — Stillness Budget Motion Standard Request → Deny 시 즉시 전환 (ADR-0007)
  - Cross-Layer Rules §Cross-Cutting — 알림·팝업 없이 silent 처리 (Pillar 1)

## Acceptance Criteria

- **AC-DJ-19** [`MANUAL`/ADVISORY] — Reduced Motion 활성 → 모든 트랜지션 즉시 전환. 텍스트 정상 표시.
- **AC-DJ-Opening** [`AUTOMATED`/BLOCKING, derived from GDD §States] — Closed → 아이콘 클릭 → Opening (Stillness Motion Standard Request) → `OpenCloseDurationSec` 후 Reading. Deny 시 즉시 Reading.
- **AC-DJ-Closing** [`AUTOMATED`/BLOCKING] — Reading → ESC/닫기 → Closing → `OpenCloseDurationSec` 후 Closed. Handle Release.
- **AC-DJ-PageFlip** [`AUTOMATED`/BLOCKING] — 꿈 페이지 이전/다음 클릭 → `PageTransitionDurationSec`(0.25s) 페이지 넘김 애니메이션 (Motion Standard).

## Implementation Notes

1. **`StartOpening` 구현** (CR-1 호출 경로 — 아이콘 클릭 → Panel):
   ```cpp
   void UDreamJournalPanelWidget::StartOpening() {
       State = EDreamJournalUIState::Opening;
       PanelRoot->AddToViewport();

       if (Stillness && Stillness->IsReducedMotion()) {
           // AC-DJ-19: 즉시 Reading (트랜지션 생략)
           State = EDreamJournalUIState::Reading;
           List->Populate();
           return;
       }

       MotionHandle = Stillness->Request(EStillnessChannel::Motion, EStillnessPriority::Standard);
       if (!MotionHandle.IsValid()) {
           // Budget Deny — 즉시 Reading (일기는 항상 열림)
           State = EDreamJournalUIState::Reading;
           List->Populate();
           return;
       }

       PlayAnimation(OpenAnimation);
       // Animation 완료 콜백 → OnOpenComplete
   }

   void UDreamJournalPanelWidget::OnOpenComplete() {
       State = EDreamJournalUIState::Reading;
       List->Populate();
       Stillness->Release(MotionHandle);
   }
   ```

2. **`StartClosing` 구현** (ESC/닫기 버튼 호출):
   ```cpp
   void UDreamJournalPanelWidget::StartClosing() {
       State = EDreamJournalUIState::Closing;

       if (Stillness && Stillness->IsReducedMotion()) {
           OnCloseComplete();
           return;
       }

       MotionHandle = Stillness->Request(EStillnessChannel::Motion, EStillnessPriority::Standard);
       if (!MotionHandle.IsValid()) {
           OnCloseComplete();
           return;
       }

       PlayAnimation(CloseAnimation);
   }

   void UDreamJournalPanelWidget::OnCloseComplete() {
       State = EDreamJournalUIState::Closed;
       PanelRoot->RemoveFromParent();
       Stillness->Release(MotionHandle);
   }
   ```

3. **페이지 넘김 애니메이션** (`UDreamPageWidget::ShowDream` 통합):
   ```cpp
   void UDreamPageWidget::ShowDream(const FName& DreamId) {
       CurrentDreamId = DreamId;

       if (Stillness && Stillness->IsReducedMotion()) {
           // 즉각 갱신
           UpdatePageContent(DreamId);
           return;
       }

       auto Handle = Stillness->Request(EStillnessChannel::Motion, EStillnessPriority::Standard);
       if (!Handle.IsValid()) {
           UpdatePageContent(DreamId);
           return;
       }

       PlayAnimation(PageFlipAnimation);
       // Animation halfway callback → UpdatePageContent (컨텐츠 교체)
       // Animation 완료 → Stillness->Release
   }
   ```

4. **`StartClosingForced` — 강제 닫기 (E-DJ-3)**:
   - GSM `OnGameStateChanged(Waiting, Dream)` 수신 시 (story 001에서 이미 처리) 애니메이션 없이 즉시 제거. 본 story의 `StartClosing`과 구별 — 강제 경로는 Closing 애니메이션 생략 (story 001 `HandleGameStateChanged`의 `PanelWidget->RemoveFromParent()` 로직 유지).

## Out of Scope

- `bHasUnseenDream` 펄스 (story 005)
- AC-DJ-13/17/21/22 grep (story 006)
- OpenAnimation/CloseAnimation/PageFlipAnimation 시각 세부 (UMG Animation 에셋 — 아트 파이프라인)

## QA Test Cases

**Test Case 1 — Opening 트랜지션 (AC-DJ-Opening)**
- **Setup**: Mock Stillness `IsReducedMotion = false`, `Request` returns valid handle.
- **Verify**:
  - `StartOpening()` 호출 → `State == Opening`, `Request(Motion, Standard)` 호출 1회.
  - `OpenCloseDurationSec`(0.3s) 후 `State == Reading`.
  - `Stillness->Release(MotionHandle)` 호출 1회.
- **Pass**: 3 조건 충족.

**Test Case 2 — Opening ReducedMotion ON (AC-DJ-19)**
- **Setup**: Mock IsReducedMotion = true.
- **Verify**:
  - `StartOpening()` 호출 → `State == Reading` 즉시.
  - `PlayAnimation(OpenAnimation)` 호출 0회.
  - `Stillness->Request` 호출 0회 (Budget 요청 skip).
- **Pass**: 3 조건 충족.

**Test Case 3 — Opening Budget Deny**
- **Setup**: Mock IsReducedMotion = false, `Request` returns invalid handle.
- **Verify**:
  - `State == Reading` 즉시 (Deny는 차단 아님).
  - `PlayAnimation(OpenAnimation)` 호출 0회.
- **Pass**: 두 조건 충족.

**Test Case 4 — Closing (AC-DJ-Closing)**
- **Setup**: State = `Reading`, Panel 뷰포트에 추가됨.
- **Verify**:
  - `StartClosing()` 호출 → `State == Closing`.
  - `OpenCloseDurationSec` 후 `State == Closed`, `PanelRoot->IsInViewport() == false`.
  - `Stillness->Release` 호출.
- **Pass**: 3 조건 충족.

**Test Case 5 — Page Flip (AC-DJ-PageFlip)**
- **Setup**: 꿈 3개. `ShowDream("Dream_A")` 완료.
- **Verify**:
  - BTN_Next 클릭 → `ShowDream("Dream_B")` 호출.
  - `Stillness->Request(Motion, Standard)` 호출 1회.
  - `PageTransitionDurationSec`(0.25s) 후 `CurrentDreamId == "Dream_B"`, 본문 갱신됨.
- **Pass**: 3 조건 충족.

## Test Evidence

- [ ] `tests/unit/ui/dream_journal_opening_test.cpp` — Test Cases 1, 2, 3.
- [ ] `tests/unit/ui/dream_journal_closing_test.cpp` — Test Case 4.
- [ ] `tests/unit/ui/dream_journal_page_flip_test.cpp` — Test Case 5.
- [ ] `production/qa/evidence/ac-dj-19-reduced-motion.md` — Manual checklist (Opening/Closing/PageFlip 3지점 ReducedMotion ON에서 즉시 전환).

## Dependencies

- **Depends on**: story-001 (위젯 골격, GSM 구독, 강제 닫기 path), story-002 (List Populate), story-003 (본문 렌더). Feature Stillness Budget (`Request` / `Release` / `IsReducedMotion`).
- **Unlocks**: story-005 (`bHasUnseenDream` 펄스), story-006 (grep validation).
