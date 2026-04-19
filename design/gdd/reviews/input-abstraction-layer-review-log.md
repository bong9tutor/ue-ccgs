# Input Abstraction Layer — Review Log

## Review R1 — 2026-04-17 — Verdict: NEEDS REVISION → REVISED (재리뷰 대기)
Scope signal: S
Depth: lean (단일 세션, specialist 미위임)
Completeness: 8/8 sections
AC count: 9 → 12 (수정 후)

### Blocking Items (2건 — 모두 수정 완료)

**B1. 키→액션 매핑 테이블 누락** — GDD가 7개 Action과 2개 Context를 정의했으나 실제 하드웨어 바인딩 테이블 없음.
→ **Fix**: Rule 2a 추가. `IMC_MouseKeyboard` 6개 + `IMC_Gamepad` 6개 바인딩 명시. Select/OfferCard 공유 키(LMB/A) 해소 노트 포함.

**B2. IA_OfferCard 마우스 모드 트리거 미확정** — Action이 `bool (Hold trigger)`로 정의되었으나 Mouse drag vs Gamepad hold 차이 미명시.
→ **Fix**: Mouse: LMB Hold(`OfferDragThresholdSec` = 0.15s), Gamepad: A Hold(`OfferHoldDurationSec` = 0.5s). Formula 2를 dual-threshold로 확장. Tuning Knob 추가.

### Design Decisions (리뷰 중 확정)

**D1. IA_Cancel + IA_Pause → IA_Back 통합** — Action 7→6개. GSM이 맥락별 라우팅 (UI 열림→닫기, 최상위→메뉴 토글, 메뉴→뒤로). 2 Context 유지 (3번째 Context 불필요).

### Recommended Revisions (4건 — 모두 적용)

- R1. Rule 5에 `IA_Back` 마우스-only 접근 명시 (일기 열기·메뉴 진입·UI 닫기 모두 클릭 경로)
- R2. E9 추가 — Formula 1 경계 "정확히 3.0px = 전환 안 됨"
- R3. Formula 1(`>`) vs Formula 2(`≥`) 비대칭 근거 추가
- R4. E5 구현 검증 노트 — `UInputTriggerHold` 포커스 상실 자동 리셋 확인 필요

### Nice-to-Have (적용)

- N1. 섹션명 "Detailed Design" → "Detailed Rules"

### AC 변경

- ACTION_COUNT_COMPLETE: 7→6 액션, IA_Cancel/IA_Pause → IA_Back
- OFFER_HOLD_BOUNDARY → OFFER_HOLD_BOUNDARY_GAMEPAD (컨텍스트 명확화)
- OFFER_HOLD_CANCEL → OFFER_HOLD_CANCEL_GAMEPAD
- 신규: BINDING_TABLE_COMPLETE, OFFER_DRAG_THRESHOLD_MOUSE, OFFER_CLICK_SELECT_MOUSE
- Total: 9 → 12 AC

### 재리뷰 시 확인 포인트

- B1·B2 수정이 문서 전체에 일관되게 반영되었는지
- IA_Back 통합이 downstream GDD 작성 시 혼란 없는지 (Bidirectional Notes 검증)
- Formula 2 dual-threshold 구조가 명확한지
- Select/OfferCard 공유 키 해소(Implicit Trigger)가 UE 5.6에서 실제 동작하는지 구현 검증

---

## Review R2 — 2026-04-17 — Verdict: APPROVED
Scope signal: S
Depth: lean (단일 세션, specialist 미위임)
Completeness: 8/8 sections
AC count: 12 (변동 없음)
Blocking items: 0 | Recommended: 2
Prior verdict resolved: Yes — R1의 7개 항목(B1·B2·R1–R4·N1) 전부 해소 확인

### Summary
R1에서 blocking이었던 키→액션 매핑 테이블(B1)과 IA_OfferCard 마우스 트리거(B2)가 Rule 2a 추가 및 Formula 2 dual-threshold 확장으로 완전 해소됨. 12개 AC가 독립 검증 가능한 수준이며, Formula 경계 비대칭(`>` vs `≥`)과 Select/OfferCard 공유 키 해소가 명확히 문서화됨. Foundation Layer 4번째이자 마지막 GDD로, 이 승인으로 Foundation 설계 완료.

### Recommended (구현/ADR 이관)
1. IA_PointerMove 값 타입(delta vs absolute) 명확화 — Card Hand UI가 절대 좌표 필요 시 PlayerController API 사용이 Rule 1 위반 아님을 명시
2. UMG 위젯의 Enhanced Input 소비 경로 노트 — PlayerController 중계 등 브릿지 패턴 구현 참고

### Nice-to-Have
1. Key remapping 미래 경로 — Steam 가이드라인 + Accessibility Layer(VS) 연결을 Open Questions에 추가
