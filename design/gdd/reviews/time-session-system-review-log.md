# Review Log — `time-session-system.md`

이 파일은 `design/gdd/time-session-system.md`의 모든 `/design-review` 결과를 시간순으로 누적한다. 새 리뷰 시 가장 최근 항목 아래에 추가하고, 이전 항목은 보존한다.

---

## Review — 2026-04-16 — Verdict: MAJOR REVISION NEEDED

Scope signal: **L** (upgraded from M — cross-cuts Farewell Archive, Dream Trigger, GSM, UE subsystem lifecycle, test infrastructure)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, narrative-director, creative-director (senior synthesis)
Blocking items: **6** | Recommended: **8** | Nice-to-have: 3
Prior verdict resolved: First review

### Summary (creative-director synthesis)

세 명의 에이전트(game-designer, qa-lead, narrative-director)가 서로 다른 관점에서 독립적으로 동일한 두 핵심 문제로 수렴했다 — 가능한 가장 강한 신호. 프로토타입의 12/12 통과는 GDD가 견고하다는 증거가 아니라, 모호한 스펙의 한 가지 합리적 해석에 맞춰 프로토타입이 만들어졌다는 증거. 지금 승인하면 두 명의 구현자가 분기된 분류기를 만들고 출시 빌드에 Pillar 1 위반이 들어갈 위험. 수정 비용은 ~1–2일 (재설계 아닌 재작성), 프로토타입 알고리즘 대부분은 살아남음 — Pillar 수준의 컷(LONG_GAP modal 제거, 내러티브 빈도 상한)만이 행동 변경 사항.

### Top 3 blockers

1. **LONG_GAP modal prompt 제거 (Rule 3 + E7).** 게임 진입 차단으로 continue/farewell 선택 강요는 구조적 Pillar 1 위반. 4개 에이전트 합의. `LONG_GAP`을 Farewell Archive가 소비하는 silent state로 재분류; `PROMPT_LONG_GAP_CHOICE` action 의미와 E7 재프롬프트 동작 삭제. Pillar 4("끝이 있다")는 Day 21 종료 이벤트로 충분히 보장됨.

2. **`ADVANCE_WITH_NARRATIVE` per-playthrough cap 추가.** 주말 플레이어는 21일에 10–11회 발화 — 튜닝 문제 아닌 구조적 Pillar 3 위반. 권장: 최대 3회/플레이스루. 추가로 카테고리 오류 수정: "며칠이 흘렀어요"는 시스템 피드백 보이스이므로 §Player Fantasy의 "꿈 한 조각" 라벨에서 분리 (Dream Trigger #9 영역).

3. **Rule 평가 순서를 명시적 exclusive if-else 체인으로 명시.** Rule 6 vs Rule 8이 ΔW=2h/ΔM=0s에서 *서로 다른 액션*(ADVANCE_SILENT vs LOG_ONLY)을 산출 — 코어 분류기 모호. "Rules are evaluated in order 1→...→8; first match wins" 추가.

### Remaining blockers

4. **UE 5.6 lifecycle 오류 수정**: `PostInitialize` 타이머 등록 옵션 제거 (`GetWorld()` null), `OnWorldBeginPlay` 단독 사용. `ClassifyOnStartup()` 분리: `Initialize()`에서 `FSessionRecord` 로드, `OnWorldBeginPlay`에서 Action 신호 발행.

5. **`IMossClockSource`를 Dependencies에 추가**. 21개 AUTOMATED AC 중 17개가 clock-injection 인터페이스 없이 실행 불가 — 현재 "BLOCKING test infrastructure"로만 표시되어 있으나 시스템 의존성으로 격상 필요.

6. **Rule 3 ↔ `LONG_GAP_PROMPT_ONCE_PER_GAME` AC 자기모순 해소**. AC가 Rule 3 "최대 1회" 본문을 암묵적으로 오버라이드. (Blocker 1로 prompt 제거 시 자동 해소.)

### Recommended (non-blocking, 8건)

- §Player Fantasy의 해석 강제 구문 ("이후의 모든 분류·액션 테이블은 '용서의 구현 방식'으로 읽혀야 한다") 삭제
- Forward Tamper 프레이밍을 "탐지 불가능 → 수용"으로 명시; `NO_FORWARD_TAMPER_DETECTION` 부정 AC (CODE_REVIEW) 추가
- Window-focus-loss vs OS-sleep 구분; `bTickEvenWhenPaused` 명시
- E6 `LastSaveReason` marker durability — Save/Load GDD에 명시적 contract 위임
- Formula 4 intermediate 타입 명시 (int32 overflow 방어)
- AC 분리: `SESSION_RECORD_DOUBLE_PRECISION` → CODE_REVIEW + AUTOMATED 두 개; `SUSPEND_RESUME_WALL_ADVANCE` MANUAL 절차 구체화 (Windows 버전, S3/S0ix)
- OQ-5: `UDeveloperSettings` 권장 (UE 5.6 idiomatic for global runtime tunables)
- OQ-4 cart-before-horse 해소: 톤 차등 주장은 Farewell Archive GDD로 위임, 이 GDD는 `EFarewellReason` enum만 정의

### Nice-to-have (3건)

- E14 platform note에 S0ix (Modern Standby) 명시
- Rule 5/7 코드상 `ADVANCE_SILENT` 단일 분기 통합, Enum은 telemetry용 분리 유지 — 명시적 구현 규칙으로 격상
- `FDateTime` epoch 노트 (UE는 0001-01-01) — 향후 클라우드 세이브 통합용

### Specialist convergences

- **Pillar 1 (LONG_GAP modal)**: game-designer + qa-lead + narrative-director + creative-director (4-way)
- **Pillar 3 (narrative frequency)**: game-designer + narrative-director + creative-director (3-way)
- **Forward Tamper manifesto**: game-designer + narrative-director + qa-lead (3-way)
- **Rule precedence**: systems-designer (single but critical)
- **UE lifecycle**: unreal-specialist (single, but BLOCKING — wrong implementation path is licensed by current spec)

### Disagreements

없음. systems-designer의 "no dead zone in Rules 5–8" 자가 정정은 분쟁 아닌 내부 명료화.

### Decision

사용자 선택: **Stop here — 새 세션에서 수정**. 컨텍스트 크기와 수정 범위(LONG_GAP modal 제거, 내러티브 cap, rule precedence, UE lifecycle, test seam)를 고려해 `/clear` 후 새 세션에서 blocker 1–6 일괄 수정 권장. 재리뷰는 수정 후 `/design-review design/gdd/time-session-system.md` 새 세션 실행.

---

## Revision Applied — 2026-04-16 — R2 (no re-review verdict yet)

수정 모드 세션에서 선행 6 blockers + 8 recommended **모두 일괄 적용**. Specialist re-spawn 없이 prior verdict의 합의 사항을 직접 반영 (사용자가 [B] Switch to revision mode 선택).

### 사용자 설계 판단 (수정 시작 전 단일 위젯 응답)
- **LONG_GAP 해소**: Silent auto-farewell — DayIndex → 21 clamp + `LONG_GAP_SILENT` + 즉시 `OnFarewellReached(LongGapSilent)`. 플레이어 복귀 시 Farewell 연출만 재생.
- **Narrative cap**: 3회/playthrough (`NarrativeCapPerPlaythrough = 3`, MVP 7일은 cap=1 권장).
- **수정 범위**: Blockers 6 + Recommended 8 모두 (한 세션 내 full revision).

### Blocker 적용 결과 (6/6)

| # | Blocker | 핵심 변경 | 위치 |
|---|---|---|---|
| 1 | LONG_GAP modal 제거 | `LONG_GAP_SILENT` 액션 신설; `AwaitingPlayerChoice` 상태·E7 시나리오·LongGapChoice UI 섹션 삭제; `EFarewellReason::LongGapChoice` → `LongGapSilent` 개명 | Rule 3, States table, Action table, §UI |
| 2 | Narrative cap | Formula 5에 cap clause + atomic side effect; `FSessionRecord.NarrativeCount` 신설; tuning knob + 상호작용 노트 | Formula 5, §FSessionRecord, §Tuning Knobs |
| 3 | Rule precedence | "Rule 1→8 순차, first match wins" 명시 + Rule 6 vs 8 케이스 예제 | Detailed Design 전제 |
| 4 | UE 5.6 lifecycle | `Initialize()` = 로드만, `OnWorldBeginPlay()` = 분류+timer; `PostInitialize` 사용 금지; `bTickEvenWhenPaused = true` | UE 5.6 구현 참고 |
| 5 | `IMossClockSource` Dependencies 격상 | Upstream Dependencies 표 신설 | §Dependencies |
| 6 | Rule 3 ↔ AC 모순 | `LONG_GAP_PROMPT_ONCE_PER_GAME` → `LONG_GAP_SILENT_IDEMPOTENT` (Blocker 1로 자동 해소) | §AC |

### Recommended 적용 결과 (8/8)

- (1) "GDD 톤 보이스 — 용서의 구현 방식" 해석 강제 구문 삭제 → Player Fantasy
- (2) Forward Tamper 명시적 수용 + `NO_FORWARD_TAMPER_DETECTION` negative AC (CODE_REVIEW)
- (3) Window-focus vs OS-sleep 표 신설 + `bTickEvenWhenPaused` → State Machine
- (4) E6 `LastSaveReason` durability → Save/Load contract 위임
- (5) Formula 4 int32 overflow guard + Overflow Safety 단락
- (6) `SESSION_RECORD_DOUBLE_PRECISION` → TYPE_DECLARATION (CODE_REVIEW) + RUNTIME (AUTOMATED) split; `SUSPEND_RESUME_WALL_ADVANCE` Win10 S3 / Win11 S0ix 절차 + 체크리스트
- (7) OQ-5 `UDeveloperSettings` 권장 명시
- (8) OQ-4 톤 차등 → Farewell Archive GDD 위임 명시

### Coverage 변화

- AC 총 21 → 24 (+3: NARRATIVE_CAP_ENFORCED, SESSION_RECORD_DOUBLE_TYPE_DECLARATION, NO_FORWARD_TAMPER_DETECTION)
- AUTOMATED 19 → 21 / CODE_REVIEW 0 → 2 / MANUAL 2 → 1 (실제 MANUAL 카운트 정정)
- Edge Cases 15 → 16 (+E16: cap 도달 후 모든 ACCEPTED_GAP은 ADVANCE_SILENT)
- Tuning Knobs 6 → 7 (+`NarrativeCapPerPlaythrough`)

### 동반 수정

- `design/registry/entities.yaml` — `ETimeAction`(LONG_GAP_SILENT), `EFarewellReason`(LongGapSilent), `FSessionRecord`(+NarrativeCount field), `NarrativeCapPerPlaythrough` 신규 constant
- `design/gdd/systems-index.md` — Time & Session Status: NEEDS REVISION → Revised — R2 (re-review pending)
- GDD header — Status: Revised — R2; R2 변경 요약 추가; Pillar 3 implements에 추가

### Re-review 상태

**판정 미확정**. R2는 specialist re-spawn 없이 직접 적용된 수정이므로, 정식 verdict는 새 세션에서 `/design-review design/gdd/time-session-system.md` 재실행으로 확보 필요. 사용자 선택: **/clear 후 새 세션에서 full re-review**.

### Open Questions 상태

- OQ-1 ~ OQ-3: 변경 없음
- OQ-4: 본문 갱신 (Farewell Archive GDD 위임 명시)
- OQ-5: 본문 갱신 (UDeveloperSettings 권장)
- OQ-6: 변경 없음 (`IMossClockSource` 격상으로 우선순위 상승)

---

## Review — 2026-04-16 — Verdict: MAJOR REVISION NEEDED (R2 re-review)

Scope signal: **L** (변경 없음 — UE 5.6 lifecycle 재설계 + cross-GDD contract 협상 + ADR 권장 포함)
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, narrative-director, creative-director (senior synthesis) — full re-spawn
Blocking items: **6** | Recommended: **8** | Nice-to-have: 3
Prior verdict resolved: **부분 — R1 6개 blocker는 의도 측면에서 처리됨; 그러나 R2 적용 과정에서 신규 blocker 6개 발생** (artifact 무결성 regression)

### Summary (creative-director synthesis)

R2는 의도 측면에서 marginal-positive 진보를 이뤘으나 artifact 무결성 측면에서 regressive. R1 6개 blocker를 다른 blocker 세트와 교환했다 — stale 텍스트 모순 3곳, broken UE API 명세 2건, 미작성 Save/Load GDD에 의한 test infra deadlock. R2 작성자는 surgical edit를 했으나 full document sweep을 하지 않아 R1 ghost 참조가 남았다. Blocker 4 lifecycle fix는 `docs/engine-reference/unreal/`을 참조하지 않고 작성된 것으로 보임 — VERSION.md HIGH risk 등급이 방지하려던 바로 그 version-awareness 실패. 한 차례 더 수정이 필요하나 충분조건은 아님: Save/Load GDD 부재라는 구조적 blocker는 이 GDD 수정만으로 해결 불가.

### Top 3 blockers

1. **UE 5.6 lifecycle 명세에 존재하지 않는 API 두 개** (line 92): `UGameInstanceSubsystem`은 `OnWorldBeginPlay()`를 상속하지 않음 (UWorldSubsystem 전용). `FTimerManager::SetTimer`에 `bTickEvenWhenPaused` 파라미터 없음. R1 Blocker 4의 R2 fix가 자체적으로 깨짐. — unreal-specialist 단독 발견, BLOCKING.

2. **R1 잔재 stale 텍스트 4곳** (문서 무결성 위반):
   - line 343-346 §Bidirectional Consistency Notes: `LongGapChoice 상태 정의` + `LongGapChoice 두 진입 경로`
   - line 398-399 §Visual/Audio Requirements: `PROMPT_LONG_GAP_CHOICE 액션 시 ... LongGapChoice 상태로 전환, Card Hand UI 종이 탭 컴포넌트 표시`
   - line 447 AC LONG_GAP_BOUNDARY_EXACT THEN: `PROMPT_LONG_GAP_CHOICE 미발행`
   game-designer + systems-designer 2-way 합의.

3. **Test infra structurally blocked**: `IMossClockSource`에 suspend 관측 메서드 부재 (`PBT_APMSUSPEND` 수신 경로 미정의) → `FMockClockSource` 작성 불가. `LONG_GAP_SILENT_IDEMPOTENT` AC는 미작성 Save/Load GDD에 의존. systems-designer + qa-lead + unreal-specialist 3-way 수렴.

### Remaining blockers

4. **`SessionEnding` + suspend mid-save 상태 기계 구멍**: 저장 중 OS suspend 시 어느 상태로 진입하는지 미정의. NarrativeCount mid-write 손실 시 cap 누수 가능. — systems-designer.

5. **5건 AC 재작성** (qa-lead): MONOTONIC_DELTA_CLAMP_ZERO "크래시 없음" 단언 / MINOR_SHIFT_NTP·DST "경고 미발생" 단언 / NARRATIVE_CAP_ENFORCED "(선택)" 제거 / SESSION_RECORD_DOUBLE_PRECISION_RUNTIME 매크로명 / SESSION_RECORD_DOUBLE_TYPE_DECLARATION typedef 우회.

6. **5건 신규 AC 추가** (qa-lead): RULE_PRECEDENCE_FIRST_MATCH_WINS / TICK_CONTINUES_WHEN_PAUSED / FOCUS_LOSS_CONTINUES_TICK / SessionCountToday 리셋 contract / LONG_GAP_SILENT_IDEMPOTENT INTEGRATION 격상 또는 임시 픽스처 경로.

### Recommended (8건)

(7) Player Fantasy 마지막 문장 ("이 용서와 증언은 ... 안전감의 근원") — 시스템을 내러티브 행위자로 격상하는 어조 교체 (narrative-director + game-designer 2-way)
(8) "긴 시간이 흘렀네요" 문구 본문 3곳 (line 23, 253, 370) — Writer 위임 플레이스홀더로 교체 (narrative-director)
(9) `EFarewellReason::LongGapSilent` 개명 — `LongGapAutoFarewell` (narrative-director, OQ-4 위임 원칙과 일관성)
(10) Stillness Budget 슬롯 충돌 회피 contract 1줄 (narrative-director + game-designer)
(11) Formula 2 `M₀` sanity check 부재 — 손상된 세이브 보호 (systems-designer)
(12) `NO_FORWARD_TAMPER_DETECTION` 의도의 ground truth로 ADR-0001 생성 권장 (qa-lead)
(13) `CODE_REVIEW` AC 타입을 coding-standards.md에 정의 또는 AUTOMATED 격상 (qa-lead)
(14) NarrativeCap=3 count-based vs distance-based cooldown 검토 (game-designer)

### Nice-to-have (3건)

- E8 "Day 21 재경험" 설계 의도 1줄 명시 (game-designer R4)
- 새 플레이스루 리스타트 의례 위치 명시 (game-designer R7)
- `LongGapSilent` 경로 짧은 맥락 제공 가능성 검토 (game-designer Issue 1)

### Pillar Defense Scorecard

- **Pillar 1** AT-RISK: NarrativeCap=3 메커니즘이 1차 수호자이나 강제 AC 없음
- **Pillar 3** AT-RISK: 시스템 내러티브 보이스 소유권 미할당, 본문에 텍스트 누출
- **Pillar 4** AT-RISK: Farewell Archive GDD 미존재 → LONG_GAP_SILENT 종착점이 빈 참조

### Specialist convergences

- **Stale R1 text** (game-designer Issue 3 + systems-designer REC-2/3): 2-way
- **Test infra deadlock** (qa-lead 5 BLOCKING + systems-designer BLOCKING-A/B + unreal-specialist OQ-6 link): 3-way
- **System narrative voice ownership** (narrative-director Issue 3,6 + game-designer R5): 2-way
- **Narrative cap intent uncertainty** (game-designer Issue 2 + narrative-director Issue 5): 2-way

### Disagreements

없음. 독립 관측의 동일 항목 발견만 있었다.

### Decision

사용자 선택: **[A] 새 세션에서 R3 수정**. R3 권장 순서:
1. OQ-4 (Farewell tone 차등 범위) + OQ-6 (`IMossClockSource` UObject vs 순수 C++ + suspend 관측 메서드) 결정 — out-of-band 설계 판단
2. Save/Load + Farewell Archive 스켈레톤 GDD 병렬 작성 (cross-GDD reference dead-end 해소)
3. 본 GDD R3 수정 (UE 5.6 reference 사전 참조 필수 — `docs/engine-reference/unreal/current-best-practices.md`)
4. R3 re-review (5 specialist + creative-director 재스폰; unreal-specialist 사인오프 hard requirement)

추정 노력: R3 1.5–2 설계 일 + 스켈레톤 0.5일.

### Out-of-band 결정 — 2026-04-16 (R2 re-review 세션 종료 시 사용자 단독 결정)

R3 sequence 1단계로 두 OQ를 본 세션에서 결정. R3 GDD 수정 시 적용:

- **OQ-4 결정**: `EFarewellReason::LongGapSilent` → **`LongGapAutoFarewell`** 개명. 이름은 기술 의미(자동 작별)만 표현, 톤 차등(silent vs warm vs neutral)은 Farewell Archive GDD가 완전 결정. narrative-director Issue 4 권고 수용.

- **OQ-6 결정**: `IMossClockSource`는 **순수 C++ 추상 재상속 클래스** (UObject 미상속). 4개 가상 메서드:
  - `virtual FDateTime GetUtcNow() const = 0;`
  - `virtual double GetMonotonicSec() const = 0;`
  - `virtual void OnPlatformSuspend() {}` (default no-op, FRealClockSource가 오버라이드)
  - `virtual void OnPlatformResume() {}` (default no-op)
  
  `FRealClockSource`가 `FCoreDelegates::ApplicationWillEnterBackgroundDelegate` 구독으로 OS suspend/resume을 캡처해 Subsystem에 전파. UPROPERTY 참조 없으므로 TObjectPtr 불필요. `FMockClockSource`는 시각 조작 + suspend 시뮬레이션을 직접 호출 가능 → systems-designer BLOCKING-B 해소 경로 확정.

R3 GDD 수정 시 §Dependencies, OQ-6 entry, Test Infrastructure Needs 표, entities.yaml 모두 갱신 필요.

---

## Lean Review — 2026-04-17 — Status: R3 미적용 확인, MAJOR REVISION NEEDED 유지

Scope signal: **L**
Specialists: [lean mode — single-session]
Blocking items: **6** (R2 re-review와 동일, R3 미적용) | Prior verdict resolved: **No**

### Summary

R2 re-review(2026-04-16)의 6 BLOCKER 전부 미해소 확인. 단, R3 진입 조건이 개선됨: (1) Save/Load GDD APPROVED → cross-GDD dead-end 부분 해소, (2) Data Pipeline GDD APPROVED → Foundation 2/4 완료, (3) OQ-4(LongGapAutoFarewell) + OQ-6(IMossClockSource 순수 C++) out-of-band 결정 완료. 다음 세션에서 R3 수정 시작 권장.

### R3 수정 시 참조 필수

- `docs/engine-reference/unreal/current-best-practices.md` — UE 5.6 `UGameInstanceSubsystem` lifecycle 정확한 API 확인
- Save/Load GDD `ESaveReason` enum 정의 (이제 APPROVED — cross-ref 안정)
- OQ-4 결정: `LongGapSilent` → `LongGapAutoFarewell`
- OQ-6 결정: `IMossClockSource` 순수 C++ 4 virtual methods

---

## Revision Applied — 2026-04-17 — R3 (re-review 대기)

R2 re-review 6 BLOCKER + 8 RECOMMENDED 전부 일괄 적용. OQ-4/OQ-6 out-of-band 결정 반영. engine-reference 참조.

### Blocker 적용 결과 (6/6)

| # | Blocker | 핵심 변경 | 위치 |
|---|---|---|---|
| 1 | UE 5.6 lifecycle 두 API 오류 | `OnWorldBeginPlay()` → `FWorldDelegates::OnPostWorldInitialization` 구독. `FTimerManager(bTickEvenWhenPaused)` → `FTSTicker::GetCoreTicker().AddTicker()`. `Deinitialize()` cleanup 추가 | §States 구현 참고 |
| 2 | Stale R1 텍스트 4곳 | GSM `PROMPT_LONG_GAP_CHOICE` + `LongGapChoice` → `LONG_GAP_SILENT` + Farewell 전환. Farewell `LongGapChoice` → `LongGapAutoFarewell`. Visual/Audio `PROMPT_LONG_GAP_CHOICE` 줄 삭제. AC `PROMPT_LONG_GAP_CHOICE 미발행` → `LONG_GAP_SILENT 미발행` | §Bidirectional, §Visual/Audio, AC LONG_GAP_BOUNDARY_EXACT |
| 3 | IMossClockSource suspend 메서드 + test infra | 순수 C++ 4 virtual methods 전체 시그니처. `OnPlatformSuspend()`/`OnPlatformResume()` 추가. `FMockClockSource` suspend simulation 명세 | §Dependencies, Test Infrastructure |
| 4 | SessionEnding + suspend mid-save | States 표에 `SessionEnding → Suspended` 전환 추가. NarrativeCount mid-write 보호 atomic 보장 | §States 전환 세부 규칙 |
| 5 | 5 AC 재작성 | MONOTONIC_DELTA_CLAMP_ZERO 구체 assertion. MINOR_SHIFT_NTP/DST 구체 assertion. NARRATIVE_CAP_ENFORCED "(선택)" 제거. SESSION_RECORD 두 건 매크로명·typedef 정정 | §AC |
| 6 | 5 신규 AC | RULE_PRECEDENCE_FIRST_MATCH_WINS, TICK_CONTINUES_WHEN_PAUSED, FOCUS_LOSS_CONTINUES_TICK, SESSION_COUNT_TODAY_RESET_CONTRACT, NO_FORWARD_TAMPER_DETECTION_ADR | §AC (Coverage 직전) |

### OQ 결정 적용

- **OQ-4 RESOLVED**: `EFarewellReason::LongGapSilent` → `LongGapAutoFarewell` 문서 전체 rename. 네이밍은 기술 의미만, 톤은 Farewell Archive GDD 위임
- **OQ-6 RESOLVED**: `IMossClockSource` 순수 C++ 4 virtual methods. `FRealClockSource`가 `FCoreDelegates` 구독. `FMockClockSource`는 직접 호출 시뮬레이션

### Recommended 적용 결과 (8/8)

- (7) Player Fantasy 마지막 문장 어조 교정 — "이 용서와 증언은...근원" → "이 설계는...뒷받침합니다"
- (8) "긴 시간이 흘렀네요" 본문 3곳 → Writer 위임 플레이스홀더 `{{시스템 내러티브 — Writer 위임}}`
- (9) OQ-4 rename에 통합
- (10) Stillness Budget 슬롯 충돌 회피 — "배타 소비" contract 추가 (§GSM, §Formula 5)
- (11) Formula 2 M₀ sanity check — `M₀ < 0 or > 1.0×10⁹` 시 리셋 + 진단 로그
- (12) NO_FORWARD_TAMPER_DETECTION ADR-0001 권장 — §UI Requirements에 Architecture 단계 TODO 마킹
- (13) CODE_REVIEW AC 타입 — `coding-standards.md`에 AUTOMATED/INTEGRATION/CODE_REVIEW/MANUAL 4종 정의 추가
- (14) NarrativeCap count-based 확정 — distance-based cooldown 대비 장단점 명시 (§Tuning Knobs 상호작용)

### Coverage 변화

- AC 총 24 → **29** (+5: RULE_PRECEDENCE_FIRST_MATCH_WINS, TICK_CONTINUES_WHEN_PAUSED, FOCUS_LOSS_CONTINUES_TICK, SESSION_COUNT_TODAY_RESET_CONTRACT, NO_FORWARD_TAMPER_DETECTION_ADR)
- AUTOMATED 21 → 24 / INTEGRATION 0 → 1 (LONG_GAP_SILENT_IDEMPOTENT 격상) / CODE_REVIEW 2 → 3 / MANUAL 1 (불변)

### 동반 수정

- `design/registry/entities.yaml` — `EFarewellReason` values `LongGapAutoFarewell`, `IMossClockSource` 신규 entry (4 virtual methods)
- `design/gdd/systems-index.md` — Time & Session Status: Revised — R3 (재리뷰 대기)
- `.claude/docs/coding-standards.md` — §Acceptance Criteria Types 신설 (AUTOMATED/INTEGRATION/CODE_REVIEW/MANUAL)

### Re-review 상태

**판정 미확정**. R3는 engine-reference 참조 + OQ-4/OQ-6 결정 적용 + stale sweep 완료. 정식 verdict는 `/design-review design/gdd/time-session-system.md` 재실행으로 확보 필요.

---

## Review — 2026-04-17 — Verdict: APPROVED (R3 re-review)
Scope signal: **L**
Specialists: [lean mode — single-session analysis]
Blocking items: **0** | Recommended: **2**
Prior verdict resolved: **Yes — R2 re-review의 6 BLOCKER 전부 해소, 8 RECOMMENDED 전부 적용 확인**

Summary: R3 수정이 R2 re-review의 모든 blocker를 성공적으로 해소. UE 5.6 lifecycle 명세가 engine-reference(`FWorldDelegates::OnPostWorldInitialization` + `FTSTicker`)와 일관. `IMossClockSource` 순수 C++ 4 virtual methods로 test infra 경로 확보. stale R1 텍스트 완전 제거. Save/Load GDD(APPROVED)와 양방향 참조 정합 확인. 29 AC(24 AUTOMATED / 1 INTEGRATION / 3 CODE_REVIEW / 1 MANUAL) 커버리지 충분. RECOMMENDED 2건(FTimerManager 의존성 표 정리, M₀ sanity check AC 추가)은 구현 단계에서 보완 가능. Foundation 4개 중 3개 APPROVED 달성.
