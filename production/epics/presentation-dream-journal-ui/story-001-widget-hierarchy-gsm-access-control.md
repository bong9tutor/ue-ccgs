# Story 001 — Widget 계층 골격 + GSM `Waiting` 전용 접근 제어

> **Epic**: [presentation-dream-journal-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/dream-journal-ui.md](../../../design/gdd/dream-journal-ui.md) §Detailed Design CR-1 (접근 제어 — Waiting 전용) + §Implementation Notes (UMG 위젯 구조) + §Interactions with GSM
- **TR-ID**: (epic 수준 — 위젯 계층 bootstrap, TR-dj-003 `NO_HARDCODED_STATS_UI` 기반)
- **Governing ADR**: *(직접 governing ADR 없음 — 표준 UMG 패턴)*
- **Engine Risk**: LOW — `UUserWidget` + `UWidgetSwitcher` 모두 UE 5.6 stable
- **Engine Notes**: `NativeConstruct`에서 GSM subscribe, `NativeDestruct`에서 RemoveDynamic. `GetWorld()` null 체크 필수 (에디터 PIE 환경). GSM 상태가 `Waiting` 이외에서는 Icon 위젯 `Collapsed` — 에러 다이얼로그 대신 entry point 숨김으로 접근 차단 (Pillar 1).
- **Control Manifest Rules**:
  - Presentation Layer Rules §Required Patterns — Dream Journal UI 위젯 구조 (dream-journal-ui.md)
  - Global Rules §Naming Conventions — `UDreamJournalWidget`, `UDreamPageWidget`, `WBP_*` prefix
  - Global Rules §Cross-Cutting — UI 알림·모달·팝업 금지 (Pillar 1)

## Acceptance Criteria

- **AC-DJ-01** [`AUTOMATED`/BLOCKING] — `EGameState::Offering` 상태에서 아이콘 클릭 시도 → 아이콘 `Collapsed` / 비활성. 일기 위젯 미생성.
- **AC-DJ-02** [`AUTOMATED`/BLOCKING] — `EGameState::Waiting` 상태에서 아이콘 클릭 → Opening 트랜지션 후 Reading 상태 진입 (본 story는 상태 진입까지, Opening 애니메이션 세부는 story 004).
- **AC-DJ-03** [`AUTOMATED`/BLOCKING] — Reading 중 `FOnGameStateChanged(Waiting, Dream)` 수신 → 일기 위젯 즉시 제거 (Closing 트랜지션 없음).

## Implementation Notes

1. **Widget 계층 구성** (GDD §Implementation Notes UMG 위젯 구조 인용):
   ```
   WBP_DreamJournalOverlay (UDreamJournalWidget, 루트)
     ├── WBP_DreamJournalIcon (UDreamJournalIconWidget, Waiting 전용)
     └── WBP_DreamJournalPanel (UDreamJournalPanelWidget, Opening/Reading/Closing)
           ├── WBP_DreamList (UDreamListWidget, 목록)
           │     ├── WBP_DreamListItem (반복)
           │     └── WBP_EmptyState
           └── WBP_DreamPage (UDreamPageWidget)
                 ├── TXT_DateLabel
                 ├── TXT_BodyText (URichTextBlock)
                 ├── BTN_Previous
                 └── BTN_Next
   ```
   - `Source/MadeByClaudeCode/UI/DreamJournal/` 하위 배치.

2. **`EDreamJournalUIState` enum** (GDD §States):
   ```cpp
   UENUM(BlueprintType)
   enum class EDreamJournalUIState : uint8 {
       Closed = 0,
       Opening = 1,
       Reading = 2,
       Closing = 3
   };
   static_assert(static_cast<uint8>(EDreamJournalUIState::Closing) == 3, "state drift");
   ```

3. **`UDreamJournalWidget::NativeConstruct`** — GSM 구독:
   ```cpp
   void UDreamJournalWidget::NativeConstruct() {
       Super::NativeConstruct();
       if (!GetWorld()) return;
       if (auto* GSM = GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>()) {
           GSM->OnGameStateChanged.AddDynamic(this, &UDreamJournalWidget::HandleGameStateChanged);
           // 초기 상태 반영
           HandleGameStateChanged(EGameState::Menu, GSM->GetCurrentState());
       }
   }

   void UDreamJournalWidget::NativeDestruct() {
       if (auto* GSM = GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>()) {
           GSM->OnGameStateChanged.RemoveDynamic(this, &UDreamJournalWidget::HandleGameStateChanged);
       }
       Super::NativeDestruct();
   }
   ```

4. **`HandleGameStateChanged` — 접근 제어 (CR-1)**:
   ```cpp
   void UDreamJournalWidget::HandleGameStateChanged(EGameState OldState, EGameState NewState) {
       // AC-DJ-01: Waiting 이외 → 아이콘 숨김
       const bool bShowIcon = (NewState == EGameState::Waiting);
       IconWidget->SetVisibility(bShowIcon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

       // AC-DJ-03: 읽는 중 Dream 진입 → 즉시 제거
       if ((NewState == EGameState::Dream || NewState == EGameState::Farewell) &&
           State == EDreamJournalUIState::Reading) {
           PanelWidget->RemoveFromParent();
           State = EDreamJournalUIState::Closed;
           Stillness->Release(MotionHandle);
           return;
       }

       // Waiting 이탈 (Dream/Menu 등) 시에도 열려 있으면 강제 닫기
       if (OldState == EGameState::Waiting && NewState != EGameState::Waiting &&
           State == EDreamJournalUIState::Reading) {
           PanelWidget->RemoveFromParent();
           State = EDreamJournalUIState::Closed;
       }
   }
   ```

5. **`UDreamJournalIconWidget` 클릭 핸들러** (GSM 현재 상태 재확인 — defensive):
   ```cpp
   void UDreamJournalIconWidget::OnClicked() {
       auto* GSM = GetGameInstance()->GetSubsystem<UMossGameStateSubsystem>();
       if (!GSM || GSM->GetCurrentState() != EGameState::Waiting) {
           return;  // 방어 early-return (아이콘이 Collapsed면 클릭 발생 자체 없어야 함)
       }
       OnIconClicked.Broadcast();  // Panel이 구독하여 Opening 진입
   }
   ```

## Out of Scope

- Opening/Closing 애니메이션 (story 004)
- 목록 렌더링 + 빈 상태 (story 002)
- 꿈 페이지 본문 Rich Text 렌더링 (story 003)
- Save/Load `UnlockedDreamIds` 통합 (story 002)
- `bHasUnseenDream` 펄스 (story 005)

## QA Test Cases

**Test Case 1 — Waiting 이외 상태 (AC-DJ-01)**
- **Setup**: Mock GSM broadcast `OnGameStateChanged(Menu, Offering)`.
- **Verify**:
  - `IconWidget->GetVisibility() == ESlateVisibility::Collapsed`.
  - 아이콘 클릭 시뮬 → `OnIconClicked` broadcast 0회.
- **Pass**: 두 조건 충족.

**Test Case 2 — Waiting 진입 후 클릭 (AC-DJ-02)**
- **Setup**: Mock GSM `OnGameStateChanged(Offering, Waiting)`.
- **Verify**:
  - `IconWidget->GetVisibility() == ESlateVisibility::Visible`.
  - 아이콘 클릭 시뮬 → `OnIconClicked` broadcast 1회.
  - `PanelWidget` 부모에 추가됨.
  - `State == EDreamJournalUIState::Reading` (Opening 생략 또는 duration=0 후).
- **Pass**: 4 조건 충족.

**Test Case 3 — Reading 중 Dream 진입 (AC-DJ-03)**
- **Setup**: State = `Reading`, PanelWidget 부모 위젯에 추가된 상태.
- **Verify**:
  - Mock GSM `OnGameStateChanged(Waiting, Dream)` broadcast.
  - `PanelWidget->IsInViewport() == false` 즉시 (1 frame 이내).
  - `State == EDreamJournalUIState::Closed`.
  - `Stillness->GetActiveCount(Motion)` decrement (Handle Release).
- **Pass**: 4 조건 충족.

**Test Case 4 — Subscription cleanup**
- **Setup**: Widget 생성 후 NativeDestruct.
- **Verify**: GSM delegate subscriber count decrement, 이후 broadcast 핸들러 호출 없음.
- **Pass**: 두 조건 충족.

## Test Evidence

- [ ] `tests/unit/ui/dream_journal_access_control_test.cpp` — Test Cases 1-3.
- [ ] `tests/unit/ui/dream_journal_lifetime_test.cpp` — Test Case 4.

## Dependencies

- **Depends on**: Core GSM (`FOnGameStateChanged`, `EGameState`).
- **Unlocks**: story-002 (목록 렌더링), story-003 (꿈 페이지), story-004 (Opening/Closing 애니메이션).
