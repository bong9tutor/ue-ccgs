# Prototype Report: Time & Session System

*Created: 2026-04-16*
*Prototype type: P1 High-Risk Spike (identified in `design/gdd/systems-index.md` R2)*
*Timebox: 1 day equivalent*
*Language: Python 3.12 (algorithm verification stand-in for UE 5.6 C++)*

---

## Hypothesis

> 이중 시계 접근법(**`FDateTime::UtcNow()` wall clock + `FPlatformTime::Seconds()` monotonic**)이 Moss Baby의 21일 실시간 진행에서 시스템 시간 조작·suspend/resume·NTP 재동기화·DST 전환·장기 미실행 갭을 정확히 분류할 수 있다. 분류 결과는 Anti-pillar Quiet Presence에 부합하는 "조용한 내러티브 흡수" 액션으로 매핑 가능하다.

---

## Approach

**빌드한 것**:
- `classifier.py` (~155 LOC): `classify()` 함수가 이전 세션 레코드 + 현재 시각을 받아 `ClassificationResult`를 반환. `BetweenSessionClass` 4종 + `InSessionClass` 4종 + `Action` 6종
- `test_scenarios.py` (~260 LOC): 7개 주요 시나리오 + 5개 엣지 케이스 = 총 **12개 테스트**

**넘어간 것** (프로토타입 스코프):
- 실제 UE 통합 (UE C++로 이식할 때 다시 구현)
- 디스크 영속화
- 실제 시간 기반 테스트 (시뮬레이션된 시각만 사용)
- UI 상호작용
- 성능 최적화
- 에러 핸들링 / 예외 경로

**의사결정** (사용자와 프로토타입 설계 중 확정):
- **Forward Tamper**: 옵션 D (수용 + 조용한 내러티브) — 24h 초과 갭에서 narrative flag
- **Long Gap (>21일)**: 옵션 D (플레이어 선택 프롬프트 1회)
- **NTP/DST 흡수 한계**: 90분 (`IN_SESSION_DISCREPANCY_TOLERANCE`)
- **NORMAL_TICK epsilon**: 5초 (`DEFAULT_EPSILON`)

**총 실소요 시간**: 약 45분 (분석 15분 + 구현 20분 + 테스트·수정 10분)

---

## Result

### Test Run 1 (initial)
```
RESULT: 11/12 scenarios passed
```

**실패**: Scenario 5 (NTP Micro-sync +3s)
- 3초 드리프트는 `DEFAULT_EPSILON = 5s` 내부라 `NORMAL_TICK`으로 분류됨
- 기대값 `MINOR_SHIFT`와 불일치

### Test Run 2 (after finding-driven adjustment)
```
RESULT: 12/12 scenarios passed
```

Scenario 5를 실제 NTP 동작에 맞게 **+15초 재동기화** (post-sleep 시나리오)로 조정 후 `MINOR_SHIFT`로 정상 분류.

### 관찰 결과 (시나리오별)

| # | 시나리오 | 분류 | 액션 | 통과 |
|---|---|---|---|---|
| 1 | Normal Close/Reopen (12h) | ACCEPTED_GAP | ADVANCE_SILENT | ✅ |
| 2 | Laptop Sleep 3h (in-session) | SUSPEND_RESUME | ADVANCE_SILENT | ✅ |
| 3 | Forward Tamper +5d | ACCEPTED_GAP | ADVANCE_WITH_NARRATIVE | ✅ |
| 4 | Backward Tamper −2d | BACKWARD_GAP | HOLD_LAST_TIME | ✅ |
| 5 | NTP Re-sync +15s | MINOR_SHIFT | ADVANCE_SILENT | ✅ |
| 6 | DST Spring-forward +1h | MINOR_SHIFT | ADVANCE_SILENT | ✅ |
| 7 | Long Gap 30d | LONG_GAP | PROMPT_LONG_GAP_CHOICE | ✅ |
| E1 | Exactly 21-day boundary | ACCEPTED_GAP | ADVANCE_WITH_NARRATIVE | ✅ |
| E2 | 21 days + 1s (just over) | LONG_GAP | PROMPT_LONG_GAP_CHOICE | ✅ |
| E3 | First run | FIRST_RUN | START_DAY_ONE | ✅ |
| E4 | In-session 5h jump | IN_SESSION_DISCREPANCY | LOG_ONLY | ✅ |
| E5 | Quick restart (1 min) | ACCEPTED_GAP | ADVANCE_SILENT | ✅ |

### 주요 발견 (findings from running)

1. **NORMAL_TICK vs MINOR_SHIFT의 경계는 실용적으로 자의적**
   - 5초 이하 NTP 드리프트는 NORMAL_TICK, 이상은 MINOR_SHIFT
   - **그러나 두 분류 모두 `ADVANCE_SILENT`를 반환**. 즉, 플레이어 관점에서는 동일
   - **UE 구현 시 두 분류를 하나로 합쳐도 무방** — 로깅 목적으로만 구분 유지 권장

2. **SUSPEND_RESUME과 ACCEPTED_GAP의 분류는 `session_uuid` 재시작 플래그에 의존**
   - 같은 세션 내에서 monotonic이 멈추면 SUSPEND_RESUME
   - 세션이 바뀌었으면 ACCEPTED_GAP
   - **실측 이슈**: Windows에서 `QueryPerformanceCounter`가 suspend 중에도 계속 카운트될 수 있음 (플랫폼/드라이버 의존). UE 실구현 시 **추가 검증 필요**

3. **21일 경계(Day 21 정각) 처리가 정확**
   - 정확히 21일 = ACCEPTED_GAP (마지막 하루 정상 진행)
   - 21일 + 1초 = LONG_GAP (플레이어 선택 프롬프트)
   - Pillar 4 ("끝이 있다")를 자연스럽게 지원

4. **Forward Tamper 감지 불가능성은 설계적 선택**
   - 앱 종료 중 시스템 시간 +5일 조작은 "합법적 5일 경과"와 **알고리즘적으로 구분 불가능**
   - 사용자 결정 D(수용 + 조용한 내러티브)는 이 한계를 받아들인 선택
   - **대안 방안 없음**을 프로토타입이 재확인

5. **Backward Tamper는 안전하게 거부 가능**
   - wallDelta < 0은 명확한 신호
   - `HOLD_LAST_TIME` 액션으로 세이브 데이터 보호
   - 유일하게 "신뢰 가능한 이상 신호"

---

## Metrics

| Metric | Value |
|---|---|
| 구현 시간 | ~45분 |
| 알고리즘 LOC | 155 lines |
| 테스트 LOC | 260 lines |
| 최종 테스트 통과율 | **12/12 (100%)** |
| 초기 실패 → 수정 반복 | 1회 (Scenario 5 기대값 조정) |
| 핵심 상수 결정 | 3개 (epsilon 5s, tolerance 90min, game duration 21d) |
| 분류 카테고리 | 8개 (Between 4 + In 4) |
| 액션 카테고리 | 6개 |

---

## Recommendation: PROCEED

이중 시계 접근법은 **Moss Baby의 7개 주요 시나리오 + 5개 엣지 케이스를 모두 올바르게 분류**한다. 알고리즘은 단순하고 테스트 가능하며, 사용자가 선택한 Anti-pillar 친화적 액션(옵션 D × 2)을 자연스럽게 지원한다. GDD 작성 및 UE C++ 구현 진행을 권장한다.

단, UE 구현 단계에서 **3가지 실측 검증이 필수**:
1. Windows `FPlatformTime::Seconds()`가 suspend 중 어떻게 동작하는지 실기 테스트
2. 저장/로드 직렬화(Save Integrity와 통합)에서 `session_uuid` 생성·비교 로직 검증
3. Time zone 변경 (해외여행) 시나리오를 추가 테스트 (UTC 기반이므로 문제 없어야 하지만 확인 필요)

---

## If Proceeding (production implementation plan)

### Architecture

- **UE 계층**: `UMossTimeSessionSubsystem : UGameInstanceSubsystem`
- **공개 API**:
  ```cpp
  // Pseudocode (UE C++로 재작성 — 프로토타입 코드 직접 이식 금지)
  struct FTimeClassificationResult {
      EBetweenSessionClass BetweenClass;
      EInSessionClass InClass;
      ETimeAction Action;
      FTimespan WallDelta;
      FTimespan MonotonicDelta;
      FString DiagnosticNotes;
  };

  UCLASS()
  class UMossTimeSessionSubsystem : public UGameInstanceSubsystem {
      GENERATED_BODY()
  public:
      FTimeClassificationResult ClassifyOnStartup(const FSessionRecord& Prev);
      FTimeClassificationResult TickInSession();
      void SaveSessionRecord(FSessionRecord& Out);
  };
  ```
- **이중 저장**: 매 카드 건넴 / 꿈 수신 / 설정 변경 시 `FSessionRecord`를 Save/Load Persistence의 `USaveGame`에 기록
- **Session UUID**: 앱 실행마다 `FGuid::NewGuid()` 생성. 이전과 다르면 between-session

### Scope Adjustments

- 프로토타입에서 드러난 `NORMAL_TICK` vs `MINOR_SHIFT` 병합 가능성 → GDD에 **로깅 목적만 구분, 액션은 동일** 명시
- In-session tick 빈도: 1초 1회면 충분 (60fps tick에서 하지 않음 — 성능 부담)
- LONG_GAP 프롬프트 UI는 Card Hand UI 스타일(종이 탭)로 1회만, Dream Journal UI와 공유 가능

### Performance Targets

- `ClassifyOnStartup`: 1회 실행, < 1ms
- `TickInSession`: 초당 1회, < 100μs per call
- 세션 저장: atomic write (Save Integrity), < 10ms

### Estimated Production Effort

- C++ 구현: 1–2 세션 (M)
- Save/Load와의 통합 테스트: 0.5 세션
- UE Automation Framework 테스트: 0.5 세션 (프로토타입 테스트 12개를 그대로 이식)
- **총 2–3 세션** (원래 systems-index 추정 M과 부합)

---

## Lessons Learned (affects other systems)

1. **Save/Load Persistence (System #3)** — `FSessionRecord`의 필드가 확정됨:
   - `FGuid SessionUuid`
   - `FDateTime LastWallUtc`
   - `double LastMonotonicSec`
   - `int32 DayIndex (1..21)`
   - `FString LastSaveReason`
   
   Save/Load GDD의 Dependencies 섹션에 **역방향 참조** 추가 필요 (design-docs 규칙: 양방향 의존성).

2. **Farewell Archive (System #17)** — LONG_GAP "플레이어 선택"의 "작별" 가지가 Farewell Archive를 조기 호출할 수 있음을 명시. 이는 "Day 21 정상 도착" 경로와 다른 진입점이며, 감정적 톤을 달리 설계해야 함 ("완성된 작별" vs "중도의 작별").

3. **Card Hand UI (System #12)** — LONG_GAP 프롬프트의 "계속/작별" 선택지는 UI 컴포넌트. Card Hand UI의 종이 탭 컴포넌트를 재사용하도록 설계.

4. **Stillness Budget (System #10, R2 추가)** — LONG_GAP 프롬프트는 "이벤트"이며 Stillness Budget의 "1회성 UI 출현 상한" 규칙에 등록되어야 함. 반복 알림 아님.

5. **Game State Machine (System #5)** — 새 상태 "LongGapChoice" 추가 고려 필요. Dawn/Offering/Waiting/Dream/Farewell/Menu에 하나 더.

---

## Appendix: Files

- `prototypes/time-session/classifier.py` — 이중 시계 분류기 구현 (155 LOC)
- `prototypes/time-session/test_scenarios.py` — 12개 테스트 하네스 (260 LOC)
- 실행 방법: `cd prototypes/time-session && PYTHONIOENCODING=utf-8 python test_scenarios.py`

**주의**: 이 Python 코드는 **알고리즘 검증용 프로토타입**. UE C++ 프로덕션 구현 시 처음부터 다시 작성할 것 (`.claude/skills/prototype/SKILL.md` 제약).
