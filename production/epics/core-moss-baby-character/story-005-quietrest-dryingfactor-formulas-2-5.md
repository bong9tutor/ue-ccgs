# Story 005 — QuietRest (고요한 대기) + `DryingFactor` Formula 2/5 + Vocabulary Rule

> **Epic**: [core-moss-baby-character](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3h

## Context

- **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md) §CR-5 (고요한 대기 상태 / Quiet Rest) + §Formula 2 (DryingFactor 계산) + §Formula 5 (DryingFactor 오버레이)
- **TR-ID**: TR-mbc-003 (CR-5 "Quiet Rest" vocabulary — drying/yellowing 시각 큐 금지) + TR-mbc-005 (Formula 5 DryingOverlay)
- **Governing ADR**: ADR-0012 §Decision — Growth `int32 GetLastCardDay() const` API (0 = 미건넴 sentinel). MBC가 `DayGap` 계산에 이 API 사용.
- **Engine Risk**: LOW
- **Engine Notes**: Formula 2 정수 나눗셈 방지 — 모든 피연산자를 float 캐스팅. Formula 5는 성장 단계 프리셋 ↔ QuietRest 선형 보간.
- **Control Manifest Rules**:
  - **Cross-Cutting Constraints**: 수치 없는 시각적 성장 (Pillar 2). "Quiet Rest" vocabulary — drying/yellowing 시각 큐 금지. `bWithered=true`는 MBC DryingFactor 상승의 영속화 플래그일 뿐, GSM이 독립적 visual layer를 추가하지 않음 (GSM §R3 Vocabulary Rule 참조).
  - **GSM §Withered Lifecycle Vocabulary Rule + MBC CR-5 "Quiet Rest"**: 금지: drying/yellowing/탈색 시각 큐. `bWithered=true`는 기술 identifier, visual은 MBC CR-5 'Quiet Rest' semantics 준수. 디자인 테스트: 복귀한 플레이어의 첫 반응이 "미안"이면 실패, "오래 잤네"이면 성공.

## Acceptance Criteria

- **AC-MBC-08** [`AUTOMATED`/BLOCKING] — Formula 2 (DryingFactor, T=2, F=5)에서 DayGap=0, 2, 3, 4, 7, 100 입력 시 출력 0.0, 0.0, 0.2, 0.4, 1.0, 1.0 (F2, 정수 나눗셈 방지 float 캐스팅 검증 포함).
- **AC-MBC-09** [`INTEGRATION`/BLOCKING] — DryingFactor > 0 상태에서 첫 카드 건넴(`FOnCardOffered` 이벤트 수신) 시 DryingFactor 즉시 0.0 리셋 확인 (CR-5).
- **AC-MBC-16** [`AUTOMATED`/BLOCKING] — Formula 5 (DryingOverlay), Mature 단계 프리셋 SSS_Intensity=0.75, QuietRest SSS=0.25에서 DryingFactor=0.0, 0.5, 1.0 입력 시 V_effective = 0.75, 0.50, 0.25 (±0.001). HueShift 검증: Mature(−0.04), QuietRest(−0.02), DF=0.5 → V_effective = −0.03 (±0.001) (F5).

## Implementation Notes

1. **Formula 2 pure function** (`MossBabyFormulas.h`):
   ```cpp
   static float DryingFactor(int32 DayGap, int32 T, int32 F) {
       return FMath::Clamp(
           (static_cast<float>(DayGap) - static_cast<float>(T)) / static_cast<float>(F),
           0.0f, 1.0f);
   }
   ```
   - **절대 주의**: 모든 피연산자 float 캐스팅. C++ 정수 나눗셈 시 DayGap 3-6 구간이 전부 0을 반환하여 점진적 변화가 사라짐 (GDD §Formula 2 ⚠️).
2. **Formula 5 pure function**:
   ```cpp
   static float DryingOverlay(float VStage, float VQuiet, float DryingFactor) {
       return VStage + (VQuiet - VStage) * DryingFactor;
   }
   ```
3. **QuietRest entry 조건** (앱 시작 시 감지):
   - `AMossBaby::BeginPlay` 또는 `OnGameStateChangedHandler(Waiting)`에서:
     - `const int32 DayGap = CurrentDayIndex - Growth->GetLastCardDay();` (ADR-0012).
     - `if (Growth->GetLastCardDay() == 0) { /* 첫 플레이 — QuietRest 진입 안 함 */ return; }`
     - `const int32 T = GetSettings()->DryingThresholdDays; // 기본 2`
     - `if (DayGap >= T) { RequestStateChange(EMossCharacterState::QuietRest); }`
4. **QuietRest tick** (매 frame 또는 state 진입 시 1회):
   - `float DF = DryingFactor(DayGap, T, GetSettings()->DryingFullDays);`
   - 6 파라미터 각각 `V_eff = DryingOverlay(StagePreset.Param, QuietRestPreset.Param, DF);` → `ApplyPreset` 경유.
5. **QuietRest → Idle 복귀 (AC-MBC-09 CR-5)**:
   - Stage 1 `OnCardOfferedHandler` (story 003)에 보강: 
     - `if (CurrentState == EMossCharacterState::QuietRest) { DryingFactor = 0.0f; ApplyPreset(StagePresets[CurrentStage]); /* 성장 단계 프리셋 즉시 복원 */ }`
     - 그 후 `RequestStateChange(Reacting)` — Formula 4 (story 003) 진입. **V_base는 리셋 후의 성장 단계 프리셋 값** — DryingOverlay 적용값 아님 (GDD §Formula 4 QuietRest→Reacting 전환 순서 명시).
6. **Vocabulary enforcement (Pillar 1 보호)**:
   - 코드/UI/에셋 명명 금지: `drying`, `yellowing`, `wilting`, `decay` (Reacting 감쇠는 exponential-decay로 OK).
   - 허용 identifier: `DryingFactor`, `DryingOverlay`, `DryingThresholdDays` (기술 math 용어) — UI 표시에는 사용 금지.
   - 사용자 대면 vocabulary: "Quiet Rest" / "고요한 대기" / "오래 잤네".

## Out of Scope

- `DayIndex > 21` clamp (E5 — Farewell이 선행, QuietRest 시각 불필요)
- Stage 1 `FOnCardOffered` 구독 자체 (story 003)
- Reacting decay Formula 4 (story 003)
- GSM `bWithered` 영속화 (GSM epic story 008)

## QA Test Cases

**Test Case 1 — AC-MBC-08 Formula 2 boundary**
- **Given**: `T=2, F=5`.
- **When**: `DayGap = 0, 2, 3, 4, 7, 100` 순수 함수 호출.
- **Then**: 출력 `0.0, 0.0, 0.2, 0.4, 1.0, 1.0` (±1e-6).

**Test Case 1b — Formula 2 integer division 방어**
- **Setup**: int 버전 (잘못된 구현) vs float 버전 비교.
- **Given**: `DayGap=3, T=2, F=5`.
- **Verify**: int 나눗셈: `(3-2)/5 = 0` (잘못된 결과). Float 캐스팅: `(3.0-2.0)/5.0 = 0.2` (올바름).
- **Pass**: 구현이 0.2 반환.

**Test Case 2 — AC-MBC-09 QuietRest → Idle on card**
- **Given**: `CurrentState = QuietRest`, `DryingFactor = 0.6`, MID의 `DryingFactor` param = 0.6.
- **When**: `FOnCardOffered(Card001)` 발행.
- **Then**: 
  - `DryingFactor = 0.0` (즉시).
  - MID `DryingFactor` param = 0.0 (`ApplyPreset(StagePresets[CurrentStage])` 효과).
  - 그 후 `CurrentState = Reacting` (story 003 연동).

**Test Case 3 — AC-MBC-16 Formula 5 math**
- **Given**: Mature preset `SSS_Intensity = 0.75`, QuietRest `SSS = 0.25`.
- **When**: `DryingFactor = 0.0, 0.5, 1.0` 입력.
- **Then**: 출력 `0.75, 0.50, 0.25` (±0.001).
- **Additional**: HueShift `Mature=-0.04, Quiet=-0.02, DF=0.5` → `-0.03` (±0.001).

**Test Case 4 — Vocabulary grep**
- **Setup**: `grep -rniE "yellowing|wilting|dry\s*out|wither" Source/MadeByClaudeCode/Core/MossBaby/ Content/`
- **Verify**: 매치 0건 (기술 identifier `DryingFactor`는 `drying`과 다름 — `yellowing/wilting/wither` 검사).
- **Pass**: grep empty.

## Test Evidence

- [ ] `tests/unit/moss-baby/formula2_test.cpp` — AC-MBC-08 + integer division 방어 테스트
- [ ] `tests/unit/moss-baby/formula5_overlay_test.cpp` — AC-MBC-16
- [ ] `tests/integration/moss-baby/quietrest_reset_test.cpp` — AC-MBC-09
- [ ] `tests/unit/moss-baby/vocabulary_grep.md` — Pillar 1 vocabulary 검증

## Dependencies

- **Depends on**: story-001 (MossBaby + Preset), story-002 (state machine), story-003 (Reacting — Stage 1 handler 확장)
- **Unlocks**: story-008 (E4 QuietRest+GrowthTransition compound)
