# Story 003 — Rich Text Block 본문 렌더링 + DegradedFallback 빈 본문

> **Epic**: [presentation-dream-journal-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2-3h

## Context

- **GDD**: [design/gdd/dream-journal-ui.md](../../../design/gdd/dream-journal-ui.md) §Detailed Design CR-3 꿈 텍스트 표시 + §Interactions with Data Pipeline (`GetDreamBodyText` API) + §Edge Cases E-DJ-5 Pipeline DegradedFallback + §G Tuning Knobs (FontSizeBody, LineHeightMultiplier)
- **TR-ID**: (epic 수준 — Pipeline 통합, 한국어 시적 포맷팅)
- **Governing ADR**: *(직접 없음)*
- **Engine Risk**: LOW — `URichTextBlock` + String Table + Nanum Myeongjo/Noto Serif KR 폰트 UE 5.6 stable
- **Engine Notes**: `Pipeline.GetDreamBodyText(FName)`는 sync 반환 (Data Pipeline §R2 Eager load). DegradedFallback 시 `FText::GetEmpty()` 반환 — 일기는 열려 있으므로 빈 페이지 + 탐색 버튼만 표시 (E-DJ-5). URichTextBlock은 String Table과 연동해 한국어 시적 포맷팅 지원 — `<poetry>`, `<emphasis>` 등 inline decorator 가능.
- **Control Manifest Rules**:
  - Cross-Layer Rules §Cross-Cutting — 수치·태그 UI 표시 금지 (Pillar 2)
  - Global Rules §Cross-Cutting — 모든 텍스트는 `FText` (현지화 대응)

## Acceptance Criteria

- **AC-DJ-18** [`MANUAL`/ADVISORY] — 800×600 해상도 → 본문 텍스트 읽기 가능 (최소 16px, 겹침·잘림 없음).
- **AC-DJ-20** [`AUTOMATED`/BLOCKING] — `Pipeline.GetDreamBodyText()` → `FText::GetEmpty()` 반환 시뮬 (E-DJ-5 DegradedFallback) → 빈 텍스트 표시. 에러 메시지 없음. 탐색 버튼 정상 작동. `UE_LOG(LogDreamJournalUI, Warning, ...)` 1회.

## Implementation Notes

1. **`RenderBodyText` on `UDreamPageWidget`**:
   ```cpp
   void UDreamPageWidget::RenderBodyText(const FName& DreamId) {
       auto* Pipeline = GetGameInstance()->GetSubsystem<UDataPipelineSubsystem>();
       if (!Pipeline) return;

       const FText BodyText = Pipeline->GetDreamBodyText(DreamId);

       // E-DJ-5: DegradedFallback 또는 Pipeline 실패 → 빈 페이지 + 탐색 버튼 활성
       if (BodyText.IsEmpty()) {
           UE_LOG(LogDreamJournalUI, Warning,
                  TEXT("GetDreamBodyText returned empty for DreamId=%s — showing blank page (E-DJ-5)"),
                  *DreamId.ToString());
           TXT_BodyText->SetText(FText::GetEmpty());
           // 탐색 버튼은 유지 — 에러 메시지 없이 silent fallback (Pillar 1)
       } else {
           TXT_BodyText->SetText(BodyText);
       }

       // DateLabel
       const auto* SaveData = UMossGameInstance::GetSaveData();
       const int32 Day = SaveData->DreamReceivedDay.FindRef(DreamId);
       TXT_DateLabel->SetText(FText::Format(
           NSLOCTEXT("DreamJournal", "DayN", "Day {0}"),
           FText::AsNumber(Day)));
   }
   ```

2. **URichTextBlock 설정** (UMG Widget Blueprint — `WBP_DreamPage`):
   - `TXT_BodyText`는 `URichTextBlock` 타입.
   - Default Text Style Set: `DT_DreamJournalStyle` (DataTable `FRichTextStyleRow`) — Nanum Myeongjo 또는 Noto Serif KR 폰트.
   - `FontSizeBody = 16px`, `LineHeightMultiplier = 1.5` (Config Asset 바인딩 — `UDreamJournalConfig`).

3. **String Table 구성** (한국어 시적 포맷팅):
   - `ST_DreamJournal.uasset` — 꿈 본문 기본 key 매핑 (DreamDataAsset 내부 `BodyText`와 별개로, UI 템플릿 문자열 저장).
   - `NSLOCTEXT("DreamJournal", "DayN", "Day {0}")` 외에 `EmptyStateText` 등 UI 텍스트.

4. **Config Asset 바인딩** (`UDreamJournalConfig : UPrimaryDataAsset`):
   - `FontSizeBody`, `FontSizeDate`, `LineHeightMultiplier`, `PaddingHorizontal/Vertical` 등 10+ knob.
   - 본 story는 `FontSizeBody` + `LineHeightMultiplier` 적용 확인.

## Out of Scope

- 800×600 실기 스크린샷 검증 (AC-DJ-18 MANUAL은 별도 pass)
- Opening/Closing 애니메이션 (story 004)
- 펄스 애니메이션 (story 005)
- F-DJ-1 스크롤 정책 (OQ-DJ-3 — 작가 텍스트 길이 결정 후)
- `NO_HARDCODED_STATS_UI` grep (story 006)

## QA Test Cases

**Test Case 1 — 정상 본문 렌더링**
- **Setup**: Mock Pipeline returns `FText::FromString("이끼 아이는 오늘 구름 속 향기를 기억했다.")` for `GetDreamBodyText("Dream_A")`.
- **Verify**:
  - `ShowDream("Dream_A")` 호출 후 `TXT_BodyText->GetText().ToString() == "이끼 아이는 오늘 구름 속 향기를 기억했다."`.
  - DateLabel 형식 "Day N".
- **Pass**: 두 조건 충족.

**Test Case 2 — DegradedFallback 빈 본문 (AC-DJ-20, E-DJ-5)**
- **Setup**: Mock Pipeline `IsDegradedFallback() == true`, `GetDreamBodyText` returns `FText::GetEmpty()`.
- **Verify**:
  - `TXT_BodyText->GetText().IsEmpty() == true`.
  - BTN_Previous, BTN_Next 정상 가시성 (탐색 버튼 활성 유지).
  - `UE_LOG(LogDreamJournalUI, Warning, ...)` 카운트 == 1.
  - 에러 위젯·모달 표시 없음 (UI 위젯 트리에 추가된 error popup 0건).
- **Pass**: 4 조건 충족.

**Test Case 3 — 폰트·행간 적용 (AC-DJ-18 지원)**
- **Setup**: `UDreamJournalConfig::FontSizeBody = 16`, `LineHeightMultiplier = 1.5`.
- **Verify**:
  - `TXT_BodyText->GetDefaultFontInfo().Size == 16`.
  - Line height 적용 확인 (RichTextBlock style row).
- **Pass**: 두 조건 충족.

**Test Case 4 — MANUAL 800×600 스크린샷 (AC-DJ-18)**
- **Setup**: `production/qa/evidence/ac-dj-18-800x600-readability.md` 체크리스트.
- Steps:
  - 800×600 해상도 창에서 꿈 페이지 열기.
  - 본문 텍스트 가독성 관찰: 16px 이상, 겹침·잘림 없음.
  - 스크린샷 캡처.
  - 리드 서명.

## Test Evidence

- [ ] `tests/unit/ui/dream_page_render_test.cpp` — Test Cases 1, 3.
- [ ] `tests/integration/ui/dream_page_degraded_fallback_test.cpp` — Test Case 2 (AC-DJ-20).
- [ ] `production/qa/evidence/ac-dj-18-800x600-readability.md` — Test Case 4.

## Dependencies

- **Depends on**: story-002 (목록 → 페이지 전환). Foundation Data Pipeline (`GetDreamBodyText` API, DegradedFallback 상태).
- **Unlocks**: story-004 (Opening/Closing 애니메이션 통합 후 페이지 전환 시 Motion Standard 재요청).
