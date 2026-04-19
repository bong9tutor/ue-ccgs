# ADR-0012: Growth `GetLastCardDay()` Pull API 형태

## Status

**Accepted** (2026-04-19 — int32 sentinel 0 결정 명확. Card sprint Implementation 시 즉시 채택)

## Date

2026-04-19

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | Core / Cross-System API |
| **Knowledge Risk** | LOW — `int32` 반환 + UObject const method 표준 |
| **References Consulted** | card-system.md §EC-CS-13 + OQ-CS-2, growth-accumulation-engine.md §CR-7 + §Interactions §3, moss-baby-character-system.md §E11 |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | 없음 — 표준 C++ const getter |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None |
| **Enables** | Card System `bHandOffered` 앱 재시작 복원 (EC-CS-13). MBC `DryingFactor` 계산 (Formula 2) |
| **Blocks** | Card sprint — `bHandOffered` restore 구현 불가 없이는 Pillar 4 위반 (카드 건넨 날 재접속 시 카드 재건넴 허용 가능) |
| **Ordering Note** | Card sprint 진입 전 |

## Context

### Problem Statement

Card System이 앱 재시작 시 `bHandOffered` 상태를 복원할 때 `FGrowthState.LastCardDay == CurrentDayIndex` 비교를 수행 (EC-CS-13). `LastCardDay`는 `FGrowthState` 내부 필드 — Growth 유일 writer. Card + MBC가 이를 read하려면 Growth가 pull API를 노출해야 함.

**API 형태 3 후보**:
- (a) `int32 GetLastCardDay() const` — 0 = 미건넴 sentinel, 단순
- (b) `TOptional<int32> GetLastCardDay() const` — unambiguous "미건넴"
- (c) `bool TryGetLastCardDay(int32& OutDay) const` — explicit success/fail

### Constraints

- `FGrowthState.LastCardDay` 필드 타입은 이미 `int32` (CR-7) — 0이 sentinel ("미건넴")
- Card 및 MBC가 각각 다른 usage:
  - Card: `bHandOffered = (GetLastCardDay() == CurrentDayIndex)` — 비교만
  - MBC: `DayGap = max(0, CurrentDayIndex - GetLastCardDay())` — Formula 2
- `FGrowthState.LastCardDay` 값 범위 `[0, 21]` (EC-CS-19 방어 후)
- Growth `FGrowthState`는 `UMossSaveData` 내부 POD struct — getter 반환값은 copy (int32)

### Requirements

- Consumer (Card, MBC) 코드 단순
- "미건넴" 상태 명확 (값 혼동 위험 없음)
- UE C++ 표준 idiom

## Decision

**Option (a) 채택** — `int32 GetLastCardDay() const`. Sentinel 값 `0` = 미건넴.

```cpp
// UGrowthAccumulationSubsystem
int32 GetLastCardDay() const {
    // FGrowthState.LastCardDay — 기본 0 (미건넴), 카드 건넴 시 CurrentDayIndex 갱신
    return GrowthStateCache.LastCardDay;
}

// Card System 사용
const int32 LastCard = Growth->GetLastCardDay();
const bool bHandOffered = (LastCard == CurrentDayIndex);

// MBC 사용 (Formula 2 DryingFactor)
const int32 LastCard = Growth->GetLastCardDay();
const int32 DayGap = FMath::Max(0, CurrentDayIndex - LastCard);  // EC-11 가드
```

### Sentinel `0` 정당화

- `DayIndex` 범위는 `[1, 21]` (Time GDD Rule 1 `START_DAY_ONE`) — `0`은 유효한 DayIndex에서 제외
- `FGrowthState` 기본값 (struct default) → `LastCardDay = 0` — `FreshStart` 모드에서 자동으로 "미건넴" 상태
- Consumer 로직:
  - Card: `LastCard == CurrentDayIndex` — `LastCard = 0` (FreshStart) + `CurrentDayIndex = 1` → false (`bHandOffered = false`) ✅
  - MBC: `max(0, DayIndex - 0) = DayIndex` → FreshStart 시 DryingFactor 평가가 전체 DayIndex를 DayGap으로 → Formula 2 clamp로 `DryingFactor = 1.0`까지 가능하나 실제로는 `FreshStart`면 `DayIndex = 1` → DryingFactor = 0 (`DayGap < DryingThreshold = 2`) ✅

## Alternatives Considered

### Alternative 1: `TOptional<int32>`

- **Description**: `TOptional<int32> GetLastCardDay() const` — 미건넴이면 `{}`, 건넴이면 `TOptional(day)`
- **Pros**: "미건넴" 상태 타입 레벨 명시. Sentinel 값 혼동 제거
- **Cons**: 
  - Consumer 코드 복잡 — `.IsSet()` / `.GetValue()` unwrap 필요
  - `FGrowthState.LastCardDay`는 이미 `int32` — 내부 타입 변경 없이 getter에서 `TOptional` 래핑은 artificial
  - Sentinel `0`은 자연스럽게 작동 (DayIndex 범위 제외값)
- **Rejection Reason**: 복잡도 > 이점. Sentinel `0`이 타입 시스템 밖에서 명시적 (MBC의 Formula 2 `max(0, ...)` 가드와 호환)

### Alternative 2: `bool TryGetLastCardDay(int32&)`

- **Description**: `bool TryGetLastCardDay(int32& OutDay) const` — .NET style TryGet 패턴
- **Pros**: 명시적 성공/실패
- **Cons**:
  - UE C++ idiom 아님 — `TOptional`이 표준
  - Out-parameter pattern은 move semantics 친화 아님
  - Card/MBC 양쪽 usage 패턴에 불필요한 복잡도
- **Rejection Reason**: UE idiom 위반

## Consequences

### Positive

- **단순 구현**: getter 1줄 + consumer 1줄. 분기·unwrap 없음
- **UE idiom 일치**: 정수 getter + sentinel — `FSessionRecord.DayIndex` (1부터, 0 = 미정) 패턴과 일관
- **FGrowthState default 호환**: struct 기본값 자동으로 "미건넴" 상태

### Negative

- **Sentinel 값 관례 전달**: 미래 contributor가 "왜 0이 sentinel인가?" 혼동 가능. **Mitigation**: getter 주석에 "0 = 미건넴" 명시 + Growth GDD §CR-7 필드 설명 참조

### Risks

- **DayIndex 0이 유효값이 되면 sentinel 충돌**: Time GDD Rule 1이 `DayIndex`를 `[1, 21]` 보장 — 0은 영원히 sentinel. **Mitigation**: Time GDD 변경 시 (매우 희박) 본 ADR 재검토

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `card-system.md` | §OQ-CS-2 "EC-CS-13: `bHandOffered` 복원을 위한 Growth `GetLastCardDay()` pull API" | **본 ADR이 OQ-CS-2의 정식 결정** |
| `card-system.md` | EC-CS-13 앱 재시작 당일 건넴 완료 복원 | `LastCard == CurrentDayIndex` 비교 경로 |
| `growth-accumulation-engine.md` | §CR-7 `FGrowthState.LastCardDay: int32` | 그대로 노출 |
| `growth-accumulation-engine.md` | §Interactions §3 MBC — `GetLastCardDay()` pull API | 형태 확정 |
| `moss-baby-character-system.md` | Formula 2 DryingFactor + §E11 Cross-System Contract (DayGap = max(0, ...)) | Sentinel 0 + max(0, ...) 가드 호환 |

## Performance Implications

- CPU: getter 1회 호출당 int32 load — 1 CPU cycle 미만. 무시 가능
- Memory: 추가 필드 없음

## Migration Plan

Card sprint 진입 시:
1. `UGrowthAccumulationSubsystem::GetLastCardDay() const` 선언 + 구현 (헤더 주석에 sentinel 0 명시)
2. Card `bHandOffered` 복원 경로에 호출 (EC-CS-13)
3. MBC DryingFactor 계산에 호출 (Formula 2 + EC-11 max(0, ...) 가드)

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| Sentinel 0 = 미건넴 | FreshStart `FGrowthState` 생성 후 `GetLastCardDay()` | 반환값 0 |
| 정상 건넴 후 | `FOnCardOffered` 처리 후 `GetLastCardDay()` | 반환값 == CurrentDayIndex |
| EC-11 가드 호환 | BACKWARD_GAP으로 LastCardDay > CurrentDayIndex 시뮬 → MBC DryingFactor 계산 | `max(0, ...)` → DayGap = 0, DryingFactor = 0 |

## Related Decisions

- **card-system.md** §OQ-CS-2 — 본 ADR의 직접 출처
- **ADR-0005** (FOnCardOffered 2-stage delegate) — Growth의 Stage 2 publisher 역할과 별개; 본 ADR은 read-only pull API
- **moss-baby-character-system.md** §EC-11 — Cross-System Contract가 본 ADR의 sentinel 0 + max 가드 호환성 보장
