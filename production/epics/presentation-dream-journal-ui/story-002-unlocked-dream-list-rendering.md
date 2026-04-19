# Story 002 — `UnlockedDreamIds` 목록 렌더링 + 빈 상태 + 탐색 네비게이션

> **Epic**: [presentation-dream-journal-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/dream-journal-ui.md](../../../design/gdd/dream-journal-ui.md) §Detailed Design CR-2 꿈 목록 표시 + CR-4 탐색 + CR-5 빈 상태 + §Interactions with Save/Load Persistence + §Interactions with Data Pipeline
- **TR-ID**: (epic 수준 — 데이터 흐름 bootstrap, 이후 TR-dj-003 `NO_HARDCODED_STATS_UI` 검증은 story 006)
- **Governing ADR**: *(직접 없음 — Save/Load read-only + Pipeline pull only 패턴)*
- **Engine Risk**: LOW — `UListView` 또는 `UVerticalBox` 반복 + read-only Save/Load 접근 모두 UE 5.6 stable
- **Engine Notes**: `UMossSaveData.UnlockedDreamIds`는 `TArray<FName>` — Dream Trigger (#9)가 쓰기 소유권 (Save/Load GDD §7). Dream Journal UI는 read-only. Pipeline GDD §R2 Dream DataAsset은 Initialize 시 일괄 로드 — 페이지 진입 시 비동기 로드 불필요. DayIndex 정렬은 Save/Load의 `TMap<FName, int32> DreamReceivedDay` (Dream Trigger 쓰기) 참조 — 없으면 UnlockedDreamIds 배열 순서 (Dream Trigger가 DayIndex 오름차순으로 append한다고 가정).
- **Control Manifest Rules**:
  - Presentation Layer Rules §Required Patterns — Dream Journal UI `SaveAsync()` 직접 호출 금지 (AC-DJ-17 — story 006)
  - Cross-Layer Rules §Cross-Cutting — 수치·확률·레이블 UI 표시 금지 (`NO_HARDCODED_STATS_UI`, Pillar 2)

## Acceptance Criteria

- **AC-DJ-05** [`AUTOMATED`/BLOCKING] — 꿈 3개 언록된 상태에서 일기 열기 → 목록 확인. 받은 날 오름차순 정렬, DayIndex 작은 것부터.
- **AC-DJ-06** [`AUTOMATED`/BLOCKING] — 꿈 3개 언록, 첫 꿈 페이지 → 이전 버튼 숨김 (첫 항목이므로).
- **AC-DJ-07** [`AUTOMATED`/BLOCKING] — 꿈 3개, 마지막 꿈 페이지 → 다음 버튼 숨김.
- **AC-DJ-08** [`AUTOMATED`/BLOCKING] — 꿈 1개 언록 → 이전/다음 버튼 모두 숨김.
- **AC-DJ-09** [`AUTOMATED`/BLOCKING] — Reading 상태에서 ESC 입력 → Closing → Closed.
- **AC-DJ-11** [`AUTOMATED`/BLOCKING] — 꿈 0개 (Fresh Start) → 목록 없이 빈 상태 화면 표시, `EmptyStateText` FText 표시됨.
- **AC-DJ-12** [`AUTOMATED`/BLOCKING] — 빈 상태 화면 → 이전/다음 버튼 없음.

## Implementation Notes

1. **목록 구성 — `UDreamListWidget::Populate`**:
   ```cpp
   void UDreamListWidget::Populate() {
       const auto* SaveData = UMossGameInstance::GetSaveData();
       if (!SaveData || SaveData->UnlockedDreamIds.Num() == 0) {
           // AC-DJ-11: Fresh Start 빈 상태
           ListContainer->SetVisibility(ESlateVisibility::Collapsed);
           EmptyStateWidget->SetVisibility(ESlateVisibility::Visible);
           const auto* Config = UDreamJournalConfig::Get();
           EmptyStateText->SetText(Config->EmptyStateText);
           return;
       }

       EmptyStateWidget->SetVisibility(ESlateVisibility::Collapsed);
       ListContainer->SetVisibility(ESlateVisibility::Visible);

       // AC-DJ-05: DayIndex 오름차순 정렬
       TArray<FName> SortedIds = SaveData->UnlockedDreamIds;
       SortedIds.Sort([SaveData](const FName& A, const FName& B) {
           const int32 DayA = SaveData->DreamReceivedDay.FindRef(A);
           const int32 DayB = SaveData->DreamReceivedDay.FindRef(B);
           return DayA < DayB;
       });

       ListContainer->ClearChildren();
       for (const FName& DreamId : SortedIds) {
           auto* Item = CreateWidget<UDreamListItemWidget>(this, DreamListItemClass);
           Item->SetDreamId(DreamId);
           const int32 Day = SaveData->DreamReceivedDay.FindRef(DreamId);
           Item->SetDayLabel(FText::Format(
               NSLOCTEXT("DreamJournal", "DayN", "Day {0}"),
               FText::AsNumber(Day)));
           // CR-2: 최소 정보만 — DayIndex 또는 계절 이름. 태그·수치 절대 금지
           ListContainer->AddChild(Item);
       }
   }
   ```

2. **꿈 페이지 전환 + 탐색 버튼 가시성** (`UDreamPageWidget`):
   ```cpp
   void UDreamPageWidget::ShowDream(const FName& DreamId) {
       CurrentDreamId = DreamId;
       const int32 Index = SortedDreamIds.IndexOfByKey(DreamId);
       // AC-DJ-06/07/08: 단일 꿈이거나 경계면 버튼 숨김
       BTN_Previous->SetVisibility(Index > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
       BTN_Next->SetVisibility(
           Index < SortedDreamIds.Num() - 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

       // 본문 렌더링은 story 003
       RenderBodyText(DreamId);
   }
   ```

3. **ESC 입력 → Closing (AC-DJ-09)**:
   - `FReply UDreamJournalPanelWidget::NativeOnKeyDown(const FGeometry&, const FKeyEvent& E)`:
     - Note: ESC 입력 처리는 Enhanced Input 권장 (IA_Back). 그러나 본 story는 UMG 표준 `NativeOnKeyDown`으로 ESC 감지 — Input Abstraction 계약은 `IA_Back` Gamepad 대체와 연동, MVP는 UMG 표준 허용.
   - `if (E.GetKey() == EKeys::Escape) { StartClosing(); return FReply::Handled(); }`.
   - `StartClosing()`은 story 004에서 구체화 — 본 story는 `State = Closing` + `OnClosingRequested.Broadcast()` stub.

4. **Binding 이전/다음 버튼**:
   - `BTN_Previous->OnClicked.AddDynamic(this, &UDreamPageWidget::OnPreviousClicked)`.
   - `OnPreviousClicked`: Index > 0 → `ShowDream(SortedDreamIds[Index - 1])`.

## Out of Scope

- Rich Text 본문 렌더링 (story 003)
- Opening/Closing 애니메이션 (story 004)
- `bHasUnseenDream` 펄스 + in-memory 토글 (story 005)
- Pipeline DegradedFallback 빈 본문 처리 (story 003)
- AC-DJ-13 `NO_HARDCODED_STATS_UI` grep (story 006)
- AC-DJ-17 `SaveAsync` 금지 grep (story 006)

## QA Test Cases

**Test Case 1 — 3개 꿈 정렬 (AC-DJ-05)**
- **Setup**: Mock SaveData `UnlockedDreamIds = [DreamB, DreamA, DreamC]`, `DreamReceivedDay = {DreamA:3, DreamB:7, DreamC:15}`.
- **Verify**:
  - `ListContainer->GetChildrenCount() == 3`.
  - Child 0 DreamId == DreamA (Day 3).
  - Child 1 DreamId == DreamB (Day 7).
  - Child 2 DreamId == DreamC (Day 15).
- **Pass**: 4 조건 충족.

**Test Case 2 — 첫/마지막 경계 버튼 가시성 (AC-DJ-06, AC-DJ-07)**
- **Setup**: 꿈 3개 (DreamA, DreamB, DreamC).
- **Verify**:
  - `ShowDream(DreamA)`: BTN_Previous Collapsed, BTN_Next Visible.
  - `ShowDream(DreamC)`: BTN_Previous Visible, BTN_Next Collapsed.
- **Pass**: 두 조건 모두 충족.

**Test Case 3 — 단일 꿈 (AC-DJ-08)**
- **Setup**: 꿈 1개 (DreamA).
- **Verify**: `ShowDream(DreamA)`: BTN_Previous Collapsed, BTN_Next Collapsed.
- **Pass**: 조건 충족.

**Test Case 4 — 빈 상태 (AC-DJ-11, AC-DJ-12)**
- **Setup**: Mock SaveData `UnlockedDreamIds = []`.
- **Verify**:
  - `ListContainer` Collapsed.
  - `EmptyStateWidget` Visible.
  - `EmptyStateText` 내용 == `UDreamJournalConfig::EmptyStateText`.
  - BTN_Previous/Next 존재하지 않음 (Panel → List 모드에서 button widget 미생성).
- **Pass**: 4 조건 충족.

**Test Case 5 — ESC Closing (AC-DJ-09)**
- **Setup**: State = `Reading`, Panel 포커스.
- **Verify**:
  - ESC 키 이벤트 시뮬 → `OnClosingRequested` broadcast 1회.
  - `State == EDreamJournalUIState::Closing`.
- **Pass**: 두 조건 충족.

## Test Evidence

- [ ] `tests/unit/ui/dream_list_sort_test.cpp` — AC-DJ-05.
- [ ] `tests/unit/ui/dream_page_button_visibility_test.cpp` — AC-DJ-06/07/08.
- [ ] `tests/unit/ui/dream_list_empty_state_test.cpp` — AC-DJ-11/12.
- [ ] `tests/unit/ui/dream_journal_esc_test.cpp` — AC-DJ-09.

## Dependencies

- **Depends on**: story-001 (위젯 골격 + GSM 구독), Foundation Save/Load (`UnlockedDreamIds`, `DreamReceivedDay` read), Foundation Data Pipeline (`GetAllDreamAssets`).
- **Unlocks**: story-003 (본문 렌더링), story-005 (`bHasUnseenDream` 토글 시 DreamId 기준).
