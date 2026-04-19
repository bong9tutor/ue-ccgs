# Story 006 — CODE_REVIEW grep 게이트 — `SaveAsync` 금지 + `NO_HARDCODED_STATS_UI` + 접근성 수동

> **Epic**: [presentation-dream-journal-ui](EPIC.md)
> **Layer**: Presentation
> **Type**: UI
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2h

## Context

- **GDD**: [design/gdd/dream-journal-ui.md](../../../design/gdd/dream-journal-ui.md) §Acceptance Criteria AC-DJ-13, AC-DJ-17, AC-DJ-21, AC-DJ-22 + §Interactions with Save/Load Persistence (read-only)
- **TR-ID**: TR-dj-001 (AC-DJ-17 `SaveAsync` 금지), TR-dj-003 (AC-DJ-13 `NO_HARDCODED_STATS_UI`)
- **Governing ADR**: *(직접 없음 — architecture §Cross-Layer + control-manifest Cross-Layer/Cross-Cutting)*
- **Engine Risk**: LOW — grep CI hook + 수동 QA checklist
- **Engine Notes**: CODE_REVIEW type AC는 런타임 실행 없이 소스 grep + PR 리뷰로 검증 (.claude/docs/coding-standards.md §Acceptance Criteria Types). CI 정적 분석 hook 또는 `/code-review` skill에서 grep 실행. AC-DJ-10 키보드 완결 탐색과 AC-DJ-04 Dream 상태 아이콘 미표시는 MANUAL — production/qa/evidence/에 checklist.
- **Control Manifest Rules**:
  - Presentation Layer Rules §Forbidden Approaches — "Never call `SaveAsync()` from Dream Journal UI source — `grep 'SaveAsync' Source/MadeByClaudeCode/UI/Dream*` = 0건"
  - Cross-Layer Rules §Cross-Cutting — "수치·확률·태그 레이블 UI 표시 금지 — AC `NO_HARDCODED_STATS_UI`, Pillar 2"
  - Cross-Layer Rules §Cross-Cutting — 알림·팝업·모달 금지 (Pillar 1)

## Acceptance Criteria

- **AC-DJ-17** / **TR-dj-001** [`CODE_REVIEW`/BLOCKING] — `grep "SaveAsync" Source/MadeByClaudeCode/UI/DreamJournal/` = 0건. `bHasUnseenDream = false` in-memory 갱신만 존재.
- **AC-DJ-13** / **TR-dj-003** [`CODE_REVIEW`/BLOCKING] — 빈 상태 화면 텍스트 내용: "0개", "0/N", 숫자·퍼센트 미포함.
- **AC-DJ-21** [`CODE_REVIEW`/BLOCKING] — Dream Journal UI 전체 코드 및 위젯 검토: 통계·수치·힌트·태그 정보 UI 미표시. PR 리뷰 체크리스트.
- **AC-DJ-22** [`CODE_REVIEW`/BLOCKING] — 꿈 목록 표시 코드: "N/M 언록됨", "N개 남음" 류 문자열 없음. `grep` 패턴 `[0-9]+/[0-9]+`, `remaining`, `total`, `언록됨`, `남음` = 0건.
- **AC-DJ-04** [`MANUAL`/ADVISORY] — `EGameState::Dream` 진행 중 → 일기 아이콘 미표시 또는 비활성.
- **AC-DJ-10** [`MANUAL`/ADVISORY] — 키보드만 사용 → 일기 열기 → 탐색 → 닫기 전 과정 완료 가능.

## Implementation Notes

1. **CI grep hook — AC-DJ-17**:
   ```bash
   # .claude/hooks/validate-dream-journal.sh 또는 /code-review skill 내부
   if grep -rE "SaveAsync" Source/MadeByClaudeCode/UI/DreamJournal/ 2>/dev/null; then
       echo "ERROR: Dream Journal UI must not call SaveAsync directly (AC-DJ-17)"
       exit 1
   fi
   ```

2. **CI grep hook — AC-DJ-22 `NO_HARDCODED_STATS_UI`**:
   ```bash
   # 숫자/N 패턴 검출
   FORBIDDEN_PATTERNS=(
       '[0-9]+\/[0-9]+'         # "3/5" 형식
       'remaining'              # "N개 남음"
       'total'                  # "총 N개"
       '언록됨'                 # Korean stats label
       '남음'
       'NumUnlocked'
       'TotalDreams'
       'DreamCount'
   )
   FOUND=0
   for P in "${FORBIDDEN_PATTERNS[@]}"; do
       if grep -rE "$P" Source/MadeByClaudeCode/UI/DreamJournal/ 2>/dev/null; then
           FOUND=1
           echo "ERROR: Forbidden stats pattern '$P' found in Dream Journal UI (AC-DJ-22)"
       fi
   done
   [ $FOUND -eq 0 ]
   ```

3. **AC-DJ-21 PR 리뷰 체크리스트** (`production/qa/evidence/ac-dj-21-pillar2-review.md`):
   - [ ] UDreamListItemWidget에 바인딩되는 필드: DayLabel(FText) + 선택적 Title(FText) 만. Tags, TriggerWeights, Progression 필드 바인딩 0건.
   - [ ] UDreamPageWidget 본문: URichTextBlock 하나만. 힌트 텍스트·수치 표시 0건.
   - [ ] Icon 펄스는 단일 상태(`bHasUnseenDream`) 기반 — "N new" 숫자 표시 없음.

4. **AC-DJ-13 EmptyStateText 검증**:
   - `UDreamJournalConfig::EmptyStateText` FText 값은 Writer 결정 (예: "이끼 아이는 아직 꿈을 꾸지 않았다.").
   - 테스트: `grep 'EmptyStateText = .*[0-9]' Source/MadeByClaudeCode/` = 0건 + PR 리뷰에서 FText 내용 확인.

5. **AC-DJ-04 MANUAL QA script** (`production/qa/evidence/ac-dj-04-dream-state-hide.md`):
   - Step 1: Offering 완료 → Dream 상태 진입.
   - Step 2: 화면에서 일기 아이콘 가시성 확인 — 미표시 또는 비활성 예상.
   - Step 3: 아이콘 클릭 시도 — 반응 없음.
   - Pass: Dream 상태에서 아이콘 UX 차단.

6. **AC-DJ-10 MANUAL QA script** (`production/qa/evidence/ac-dj-10-keyboard-only.md`):
   - Step 1: 마우스 disconnect, 게임패드 disconnect.
   - Step 2: Tab 키로 일기 아이콘 포커스 → Enter 또는 Space 키로 열기.
   - Step 3: 좌/우 방향키로 꿈 페이지 이동.
   - Step 4: ESC 또는 백스페이스로 닫기.
   - Pass: 모든 Step 키보드만으로 완료.

## Out of Scope

- 자동 키보드 네비게이션 테스트 (AC-DJ-10는 MANUAL)
- OQ-DJ-2 (슬롯 표시 UX — 아트 디렉터 결정)
- OQ-DJ-3 (본문 스크롤 vs auto-fit — Writer 결정)
- OQ-DJ-4 (강제 닫힘 읽음 처리 기본값 — E-DJ-3 "읽음" 권장)

## QA Test Cases

**Test Case 1 — `SaveAsync` 금지 grep (AC-DJ-17)**
- **Setup**: `Source/MadeByClaudeCode/UI/DreamJournal/` 전 파일.
- **Verify**:
  - `grep -rE "SaveAsync" Source/MadeByClaudeCode/UI/DreamJournal/` 매치 0건.
  - `grep -rE "SaveAsync" Source/MadeByClaudeCode/UI/Dream*.cpp` 매치 0건.
- **Pass**: 두 grep 모두 0건.

**Test Case 2 — `NO_HARDCODED_STATS_UI` grep (AC-DJ-22)**
- **Setup**: `Source/MadeByClaudeCode/UI/DreamJournal/` 전 파일 + Widget Blueprint에서 추출한 FText 문자열 덤프.
- **Verify**:
  - `grep -rE "[0-9]+\/[0-9]+"` 매치 0건 (단, 주석 제외).
  - `grep -rE "remaining|total|언록됨|남음|NumUnlocked|TotalDreams|DreamCount"` 매치 0건.
- **Pass**: 모든 패턴 0건.

**Test Case 3 — EmptyStateText 숫자 없음 (AC-DJ-13)**
- **Setup**: `UDreamJournalConfig::EmptyStateText` 값 로드.
- **Verify**:
  - 문자열 내 `[0-9]` 매치 0건.
  - 문자열 내 `/`, `%` 문자 없음.
- **Pass**: 두 조건 충족.

**Test Case 4 — AC-DJ-21 PR 리뷰**
- Evidence: `production/qa/evidence/ac-dj-21-pillar2-review.md` 체크리스트 + 리드 서명.

**Test Case 5 — AC-DJ-04 Dream 상태 아이콘 (MANUAL)**
- Evidence: `production/qa/evidence/ac-dj-04-dream-state-hide.md` 플레이테스터 체크리스트.

**Test Case 6 — AC-DJ-10 키보드 완결 (MANUAL)**
- Evidence: `production/qa/evidence/ac-dj-10-keyboard-only.md` 플레이테스터 체크리스트.

## Test Evidence

- [ ] CI grep hook `.claude/hooks/validate-dream-journal.sh` — Test Cases 1, 2 (자동화).
- [ ] `production/qa/evidence/ac-dj-13-empty-state-text.md` — Test Case 3.
- [ ] `production/qa/evidence/ac-dj-21-pillar2-review.md` — Test Case 4.
- [ ] `production/qa/evidence/ac-dj-04-dream-state-hide.md` — Test Case 5.
- [ ] `production/qa/evidence/ac-dj-10-keyboard-only.md` — Test Case 6.

## Dependencies

- **Depends on**: story-001~005 (구현 완료 후 grep + manual QA).
- **Unlocks**: Epic Done — `/story-done` 후 Epic 종료.
