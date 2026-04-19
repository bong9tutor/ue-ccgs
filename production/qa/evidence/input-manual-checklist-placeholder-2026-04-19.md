# Input Abstraction MANUAL 체크리스트 — PLACEHOLDER

**Story**: `production/epics/foundation-input-abstraction/story-005-no-raw-key-mouse-only-hover-gamepad-silent.md`
**Status**: ⏳ Awaiting MANUAL PIE Verification (Story 1-11 에셋 생성 후)
**Created**: 2026-04-19
**Type**: Config/Data MANUAL

---

## 전제 조건

- **Story 1-11 에셋** (TD-008) 생성 완료
- **Story 1-15 DefaultEngine.ini PrimaryAssetType** 등록 (현재 완료)
- UE Editor Package Project → Cooked .exe 또는 PIE 실행 가능

---

## MOUSE_ONLY_COMPLETENESS

**환경**: 게임패드 미연결, 키보드 미사용.

- [ ] 카드 3장 중 1장 선택 (마우스 클릭)
- [ ] 카드 건넴 (마우스 drag — LMB Hold 0.15s+)
- [ ] 일기 열기 (UI 버튼 클릭)
- [ ] 일기 닫기 (UI 닫기 버튼 또는 외부 클릭)
- [ ] 메뉴 진입 (UI 버튼 클릭)
- [ ] 메뉴 퇴장 (UI Esc 또는 Back 버튼)

**Pass**: 6 항목 모두 통과 + 스크린 녹화 첨부

---

## HOVER_ONLY_PROHIBITION

**환경**: Card Hand UI 활성 (Card Hand UI epic 완료 후)

- [ ] 카드 3장 표시 확인
- [ ] 각 카드 위 3초 마우스 hover
- [ ] 시각 highlight만 발생 (카드 border glow 등)
- [ ] `OnCardSelected` delegate 미발행 (Output Log 관찰)
- [ ] `FSessionRecord.NarrativeCount` 불변

**Pass**: 모든 체크 통과 + 로그 캡처 첨부

**비고**: 이 AC는 Card Hand UI sprint에서 Card Hand UI 구현 후 최종 검증. 현재는 placeholder.

---

## GAMEPAD_DISCONNECT_SILENT

**환경**: Gamepad 연결 상태 시작

- [ ] 게임 시작 + Gamepad A버튼 1회 눌러 Mode=Gamepad 전환 확인
- [ ] 마우스 cursor 숨겨짐 확인
- [ ] Gamepad USB 케이블 물리적 분리
- [ ] 팝업/모달/알림 **미발생** 확인 (스크린 관찰)
- [ ] 마우스 이동 시 Mouse 모드 자동 전환 + 커서 표시
- [ ] 카드 선택/건넴 정상 작동

**Pass**: 3 checkpoint + 팝업 부재 + 마우스 전환 작동 = 5 checkpoint 통과 + 스크린 녹화

---

## CI grep Hook 검증

Agent가 작성한 2개 CI hook이 배포·활성화된 상태에서 **CI 파이프라인 exit 0** 확인:

- [ ] `.claude/hooks/input-no-raw-key-binding.sh` 실행 → exit 0
- [ ] `.claude/hooks/input-no-nativeonmouse.sh` 실행 → exit 0

**False positive 실험** (선택):

- [ ] 더미 `InputComponent->BindKey(EKeys::Q, ...)` 삽입 (Input/ 외 파일) → hook exit 1 확인
- [ ] 더미 `virtual FReply NativeOnMouseButtonDown(...) override;` 삽입 (.h) → hook exit 1 확인

---

## Results (사용자 작성)

**Date**: [사용자 기입]
**UE Version**: 5.6.x
**Build Type**: [PIE / Development / Shipping]

| 체크리스트 | 통과? | 로그/스크린샷 |
|---|---|---|
| MOUSE_ONLY_COMPLETENESS | ⏳ | |
| HOVER_ONLY_PROHIBITION | ⏳ | |
| GAMEPAD_DISCONNECT_SILENT | ⏳ | |
| NO_RAW_KEY_BINDING grep | ⏳ | |
| NO_NATIVEONMOUSE grep | ⏳ | |

**Verdict**: [PASS / FAIL / BLOCKED]

---

## 완료 후

1. ✅ 체크리스트 전체 체크 + Results 채움
2. ✅ Story 1-21 Status: Complete (CI + Placeholder) → Complete 전환 (MANUAL 완료 후)
3. ✅ 스크린샷/녹화 파일 `production/qa/evidence/input-manual-screenshots-[date]/` 저장
4. ✅ tech-debt TD-015 close

---

## 참고

- `docs/architecture/adr-0008-umg-drag-implementation.md` §Forbidden Approaches
- GDD §Rule 1 (NO_RAW_KEY_BINDING), §Rule 4/5 (Mouse-only, Hover 금지), §E2 (Gamepad disconnect silent)
- Pillar 1: 장치 전환 알림 금지
