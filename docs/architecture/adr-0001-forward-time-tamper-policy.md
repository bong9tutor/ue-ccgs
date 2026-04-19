# ADR-0001: Forward Time Tamper 명시적 수용 정책

## Status

**Accepted** (2026-04-19 — `/architecture-review` 통과 + TD self-review APPROVED)

## Date

2026-04-18

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 (hotfix 5.6.1 권장) |
| **Domain** | Core / Time |
| **Knowledge Risk** | MEDIUM (`FPlatformTime::Seconds()` Windows suspend 동작 — Time OQ-1 Implementation 검증 task) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `breaking-changes.md`, `current-best-practices.md §5 Persistent State`, `docs/architecture/architecture.md §Phase 0d Engine Knowledge Gap` |
| **Post-Cutoff APIs Used** | None — 표준 `FDateTime::UtcNow()` + `FPlatformTime::Seconds()` (모두 5.5 이전 안정 API) |
| **Verification Required** | CI 정적 분석 hook으로 forward tamper 탐지 패턴 grep (`tamper`, `bIsForward`, `DetectClockSkew`, `IsSuspiciousAdvance`) — 결과 0 매치 보장 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None |
| **Enables** | Time GDD §Player Fantasy "의심하지 않는 시스템" 보장 + Save/Load §E27 외부 편집 비정책 + Pillar 1 인프라 강제 |
| **Blocks** | None (다른 ADR/Epic의 작성을 막지 않음). 단, 이 ADR 없이는 PR 코드 리뷰가 forward tamper 탐지 코드 추가 시도를 표준적으로 차단할 근거가 없음 |
| **Ordering Note** | Foundation sprint 시작 전 Accepted 권장. 이 정책이 명시되어 있어야 Time/Save Load 구현자가 탐지 로직 추가 충동을 PR 단계에서 차단 가능 |

## Context

### Problem Statement

플레이어가 시스템 시각을 앞당기는 행위(Forward Time Tamper, 예: Day 1 시점에 OS 날짜를 Day 21로 변경)에 대해 게임이 어떻게 반응해야 하는가? Time & Session System은 이를 *탐지·차단·경고*하려고 시도해야 하는가, 아니면 *정상 시간 경과처럼 silent 수용*해야 하는가?

### Constraints

- **알고리즘적 구분 불가능**: forward time tamper와 정상 경과(예: 사용자가 실제로 5일 후 앱 재실행)는 wall clock + monotonic counter 조합으로도 구분 불가능. 두 시계 모두 OS 시각에 의존하며, 시스템 시각 변경 후에는 모든 후속 측정값이 "정상" 분포에 부합
- **Single-player cozy 게임**: 이 게임은 단일 플레이어 desktop companion으로 anti-cheat 영역이 게임 디자인에 존재하지 않음 (`technical-preferences.md`: networking None)
- **Pillar 1 (조용한 존재)**: "의심하지 않는 시스템" 명시 — 사용자에게 "시스템 시간 이상" 알림·차단·확인 모달 절대 금지
- **Pillar 4 (끝이 있다)**: "21일은 플레이어가 출석한 날수가 아니라 이끼 아이가 살아낸 시간" — 시스템 시각이 21일을 넘기면 게임은 silent하게 Farewell로 이행 (Time `LONG_GAP_SILENT` Rule 3)

### Requirements

- forward time tamper 처리 결과는 정상 경과 처리와 **bit-exact 동일**해야 함 (코드 분기 자체 금지)
- 코드베이스 어디에도 "tampering 의도 추론" 로직 존재 금지
- 정책은 PR 코드 리뷰에서 *제도적으로* 강제되어야 함 (개별 reviewer 판단에 의존 금지)
- Save/Load 영역도 동일 정책 (E27 외부 편집 / E28 forward time tamper 비정책)

## Decision

**Forward time tamper를 명시적으로 수용한다.** Time & Session System은 이를 정상 경과로 처리하며, 탐지·차단·경고·로깅 로직 일체를 추가하지 않는다.

구체적으로:
1. **Time Rule 4 (ACCEPTED_GAP)** — `0 ≤ WallDelta ≤ 21일` 조건에서 forward tamper로 인한 큰 `WallDelta`도 정상 경과처럼 `DayIndex` 전진 (Formula 4 clamp). `NarrativeCount < cap`이면 `ADVANCE_WITH_NARRATIVE`, cap 도달 시 `ADVANCE_SILENT`
2. **Time Rule 3 (LONG_GAP_SILENT)** — `WallDelta > 21일`이면 forward tamper 여부와 무관하게 즉시 Farewell P0 전환 (`OnFarewellReached(LongGapAutoFarewell)`)
3. **Save/Load** — `FSessionRecord` 직렬화는 변경된 wall clock 값을 단순히 기록. `LastWallUtc` 갱신은 BACKWARD_GAP에서만 거부 (forward는 정상 갱신)

**금지 사항** (CI grep으로 강제):
- 변수·함수 이름에 `tamper`, `cheat`, `bIsForward`, `bIsSuspicious`, `DetectClockSkew`, `IsSuspiciousAdvance` 등의 패턴 사용 금지
- "expected max delta"와 비교하여 의심 분기 금지 (`if (WallDelta > expected_max) ...`)
- forward tamper를 위한 별도 anti-tamper hash·signature·파일 무결성 검증 추가 금지 (`USaveGame` CRC32는 무결성 보증이지 tampering 탐지가 아님 — Save/Load AC `NO_ANTITAMPER_LOGIC`과 일관)

### Architecture Diagram

```text
[OS System Clock]
     │
     ▼ (FDateTime::UtcNow / FPlatformTime::Seconds)
[IMossClockSource]  ← 클럭 주입 seam, FRealClockSource / FMockClockSource
     │
     ▼
[Time & Session 8-Rule Classifier]
     │
     │  forward tamper 분기 ✗ (존재하지 않음)
     │  탐지 로직 ✗ (존재하지 않음)
     │
     ▼
[ETimeAction] — 정상 경과와 동일한 출력
     │
     ▼
[GSM, Card, Growth, Dream, Farewell] — 정상 경과로 처리
```

### Key Interfaces

```cpp
// Time & Session System은 forward tamper 처리를 위한 별도 API를 노출하지 않는다.
// 모든 시간 입력은 IMossClockSource를 통해서만 진입.
class IMossClockSource {
public:
    virtual ~IMossClockSource() = default;
    virtual FDateTime GetUtcNow() const = 0;
    virtual double GetMonotonicSec() const = 0;
    virtual void OnPlatformSuspend() {}
    virtual void OnPlatformResume() {}
};

// 다음 멤버 함수 / 변수 / 분기는 Time/Save 어디에도 존재하지 않아야 한다:
// - bool DetectForwardTamper() const;        // 금지
// - bool bIsSuspiciousTimeJump;              // 금지
// - if (WallDelta > MaxExpectedDelta) {...}  // 금지
// - struct FAntiTamperState { ... };          // 금지
```

## Alternatives Considered

### Alternative 1: Forward Tamper 감지 + 차단

- **Description**: `WallDelta`가 "기대 최대치"(예: 24h × 5일)를 초과하면 의심으로 분류, save 진행을 거부하거나 마지막 정상 시각으로 clamp.
- **Pros**: Pillar 4 강제 — "21일 실시간 투자" 가치를 OS 차원에서 보호
- **Cons**: 알고리즘적으로 정상 경과(주말 후 복귀)와 forward tamper 구분 불가 → false positive 위험. 사용자가 의도하지 않은 시간 변경(NTP 점프, DST, 시간대 변경) 시 게임 진행 차단 가능 → Pillar 1 위반
- **Rejection Reason**: Pillar 1 직접 위반 + false positive로 정상 사용자 경험 손상 + 단일 플레이어 cozy 게임 정체성에 부합하지 않음

### Alternative 2: Forward Tamper 감지 + 경고

- **Description**: 의심 시 silent log + 다음 startup에서 cozy 톤 알림 ("시간이 좀 빨리 흘렀네요")
- **Pros**: 사용자에게 정보 제공
- **Cons**: 알림 자체가 Pillar 1 위반. NTP 점프·DST 흡수에 false positive. 알고리즘적 의심 판정 자체가 임의적 (어떤 임계가 "의심"인지 객관적 기준 없음)
- **Rejection Reason**: Pillar 1 위반 + 임계 임의성 + Alternative 1과 동일한 false positive 문제

### Alternative 3: Anti-tamper hash / signature

- **Description**: `USaveGame` CRC32 외 별도 anti-tamper 해시·서명 추가, 시간 조작 후 첫 save 시 불일치 감지
- **Pros**: tampering 탐지 자체는 가능
- **Cons**: 단일 플레이어 cozy 게임에 anti-cheat 영역 존재 자체가 game design에 없음. 구현 복잡성 + 잘못된 신호로 정상 save 거부 위험. CRC32 기존 무결성 검증과 의미 충돌
- **Rejection Reason**: 게임 디자인에 anti-cheat 영역 부재. 잘못된 문제를 푸는 솔루션

## Consequences

### Positive

- **코드 단순화**: 분류기에 의심 분기 없음, anti-tamper 데이터 구조 없음, 별도 검증 패스 없음
- **Pillar 1 강화**: 사용자가 시각 조작을 시도해도 게임은 침묵 — "의심하지 않는 시스템" 약속을 인프라에서 보장
- **Pillar 4 명시화**: 시각 조작은 본인의 경험 단축이며, 게임은 그 선택에 개입하지 않음 ("이끼 아이가 살아낸 시간"은 OS 시각 기준)
- **Cozy 정체성 유지**: 단일 플레이어 desktop companion에 anti-cheat 영역이 존재하지 않음을 architecture 차원에서 명시
- **PR 코드 리뷰 강제력**: CI grep + 코드 리뷰 체크리스트로 미래 contributor의 탐지 로직 추가 시도 자체 차단

### Negative

- **사용자가 의도적으로 21일을 단축 가능**: OS 시각을 앞당겨 빠르게 Farewell 도달 가능. 단, 본인 경험만 영향
- **Pillar 4의 "실시간 21일 투자" 약화 잠재**: 일부 사용자가 게임의 의도된 페이스를 우회 가능. 단, 그 선택은 사용자의 영역 (Pillar 1: "의심하지 않음" + Pillar 4: "21일은 이끼 아이가 살아낸 시간 — 강제 아님")
- **Anti-cheat 부재의 명시화**: Steam 출시 등 미래 확장 시 "이 게임은 anti-cheat가 없다"는 사실을 release notes에 명시할 필요 가능

### Risks

- **사용자 기대 misalign**: 일부 사용자가 "왜 시스템 시각을 바꿔도 막지 않나"라고 기대할 수 있음. **Mitigation**: Steam 페이지 / itch.io 설명에 "단일 플레이어 cozy 게임 — anti-cheat 부재. 시각 조작은 본인 경험 영역" 명시. 또한 Day 21 자동 Farewell이 LongGapAutoFarewell로 silent 처리되므로 사용자가 "강제 종료"로 느끼지 않음
- **미래 contributor의 탐지 로직 추가 시도**: 의도치 않게 forward tamper 탐지 코드를 추가하려는 PR 가능. **Mitigation**: (1) CI 정적 분석 hook으로 grep 패턴 자동 차단, (2) 코드 리뷰 체크리스트에 "ADR-0001 위반 여부" 항목 추가, (3) `.claude/rules/` agent rule로 PR 단계 보강
- **NTP 시각 점프 false positive 회피**: NTP 동기화로 인한 작은 시간 점프(±수초)는 Time `MINOR_SHIFT` (Rule 7)에서 흡수. 큰 점프(>90분)는 `IN_SESSION_DISCREPANCY` (Rule 8) `LOG_ONLY` — 진단 로그만 남김. 둘 다 게임 진행 차단 없음

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| `time-session-system.md` | §Player Fantasy "의심하지 않는 시스템" — "당신이 없었던 시간을, 게임은 당신 탓으로 돌리지 않는다" | ADR이 Forward Tamper 탐지 자체를 architecture 차원에서 금지하여, GDD의 약속을 인프라로 보장 |
| `time-session-system.md` | Rule 4 (ACCEPTED_GAP) "Forward Tamper 명시적 수용" — "코드 리뷰는 forward tamper 탐지 로직 추가 시도를 차단해야 함" | ADR이 그 차단의 정식 근거 (체크리스트 + grep 정책) |
| `time-session-system.md` | AC `NO_FORWARD_TAMPER_DETECTION` (CODE_REVIEW negative AC) | ADR Decision 섹션의 "금지 사항" 목록이 grep 패턴 명시 |
| `time-session-system.md` | AC `NO_FORWARD_TAMPER_DETECTION_ADR` — "ADR-0001 (또는 지정된 ADR 번호)에 'Forward tamper 명시적 수용' 결정이 기록되어야 함" | **본 ADR이 그 요구의 직접 충족** |
| `save-load-persistence.md` | AC `NO_ANTITAMPER_LOGIC` — "시간 검증·의도 추론·tamper 탐지 패턴 미존재. CRC32는 무결성만, '치트 감지'에 연결되지 않음" | ADR이 Save/Load 영역에도 동일 정책 명시 (E27 외부 편집 / E28 forward time tamper 비정책 일관) |
| `save-load-persistence.md` | E27/E28 외부 편집 / Forward time tamper 비정책 — "이는 anti-tamper 효과가 아니라 무결성 검증의 부수효과. 본 시스템은 tampering을 탐지·차단하지 않음" | ADR Cross-System 일관성 — Time + Save/Load 동일 정책 |

## Performance Implications

- **CPU**: **개선** (음의 비용) — 탐지 분기·로직 부재로 1Hz tick에서 추가 비교·로깅 없음 (분기 1-2개 절약, 무시 가능 수준이지만 명시적 0)
- **Memory**: **개선** — anti-tamper 상태 변수·hash·signature 데이터 구조 없음 (수십 바이트 절약, 무시 가능)
- **Load Time**: 영향 없음 — Save/Load 헤더에 anti-tamper 필드 없음
- **Network**: N/A (단일 플레이어)

## Migration Plan

신규 결정. 기존 코드 없음 (architecture 단계). Implementation 단계 진입 시:

1. Time & Session 구현자가 Rule 4 작성 시 본 ADR 참조 의무
2. Save/Load 구현자가 E27/E28 비정책 구현 시 본 ADR 참조 의무
3. CI 정적 분석 hook 추가 (`grep -r "tamper\|DetectClockSkew\|bIsForward\|bIsSuspicious" Source/MadeByClaudeCode/` = 0 매치 강제)
4. PR 템플릿 체크리스트에 "ADR-0001 (Forward Tamper Acceptance) 위반 여부" 항목 추가
5. `.claude/rules/forward-tamper-policy.md` agent rule 작성 — PR 단계 코드 리뷰 보강

## Validation Criteria

본 ADR이 올바르게 적용되는지 다음으로 검증:

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| AC `NO_FORWARD_TAMPER_DETECTION` | `grep -r "tamper\|DetectClockSkew\|bIsForward\|bIsSuspicious\|IsSuspiciousAdvance" Source/MadeByClaudeCode/` | 0 매치 |
| AC `NO_FORWARD_TAMPER_DETECTION_ADR` | `docs/architecture/adr-0001-forward-time-tamper-policy.md` 파일 존재 + `## Status: Accepted` | 본 ADR이 Accepted 후 통과 |
| AC `NO_ANTITAMPER_LOGIC` (Save/Load) | `grep -ri "tamper\|cheat\|anti.*detect\|forward.*jump" Source/MadeByClaudeCode/` | 0 매치 |
| Time Rule 4 정상 동작 | Time GDD AC `ACCEPTED_GAP_NARRATIVE_THRESHOLD` 통과 | AC PASS |
| Time Rule 3 정상 동작 | Time GDD AC `LONG_GAP_SILENT_BOUNDARY_OVER` 통과 | AC PASS |
| forward tamper 시나리오 silent 처리 | 수동: OS 시각을 5일 앞당긴 후 앱 재실행 → ACCEPTED_GAP + ADVANCE_WITH_NARRATIVE (또는 cap이면 ADVANCE_SILENT). 알림·모달 미생성 | QA 체크리스트 통과 |
| forward tamper 21일+ 시나리오 silent Farewell | 수동: OS 시각을 30일 앞당긴 후 앱 재실행 → LONG_GAP_SILENT + LongGapAutoFarewell 즉시 발행. 알림·모달 미생성 | QA 체크리스트 통과 |

## Related Decisions

- **time-session-system.md** §Rule 4 (Forward Tamper 수용) — 본 ADR의 GDD 출처
- **time-session-system.md** §AC `NO_FORWARD_TAMPER_DETECTION_ADR` — 본 ADR을 명시적으로 요구
- **save-load-persistence.md** §E27 (외부 편집 비정책) + §E28 (Forward time tamper 비정책) — Save/Load 영역의 cross-system 일관성
- **ADR-0011** (Tuning Knob 노출 — UDeveloperSettings) — Time GDD §Tuning Knobs와 연계 (관련도 낮음)
- **architecture.md** §Architecture Principles §Principle 1 (Pillar-First Engineering) — 본 ADR의 상위 원칙
- **architecture.md** §Architecture Principles §Principle 2 (Schema as Pillar Guard) — 본 ADR이 grep / 코드 리뷰 layer로 강제력 보장하는 패턴 일관
