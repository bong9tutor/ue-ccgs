# ADR-0006: 카드 풀 재생성 시점 — Eager (FOnDayAdvanced 즉시)

## Status

**Accepted** (2026-04-19 — Eager 결정 명확. Card sprint Implementation 시 즉시 채택)

## Date

2026-04-19

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 |
| **Domain** | Core / Gameplay Logic |
| **Knowledge Risk** | LOW — `FRandomStream`, `HashCombine`, `TArray` 모두 4.x+ 안정 |
| **References Consulted** | card-system.md §CR-CS-1 + §CR-CS-2 + OQ-CS-5, ADR-0003 (Pipeline 동기 로드) |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | Full Vision (40장) 규모 시 샘플링 시간 < 1ms 측정 |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None (Card System 내부 결정) |
| **Enables** | Card System 구현 단순화. GSM Prepare/Ready 프로토콜 예측 가능 |
| **Blocks** | Card sprint — 재생성 시점 결정 없이 구현 불가 |
| **Ordering Note** | Card sprint 진입 전 |

## Context

### Problem Statement

Card System `FOnDayAdvanced(NewDayIndex)` 수신 시 일일 카드 풀 (DailyHand 3장)을 언제 재생성할 것인가? 

- **Eager**: FOnDayAdvanced 수신 즉시 샘플링 → Ready 상태 진입
- **Lazy**: GSM이 Offering 진입 시 `OnPrepareRequested()` 호출 시점에 샘플링

두 선택이 Dawn 체류 시간 + GSM 대기 timeout + 메모리 특성에 영향.

### Constraints

- **MVP 10장 / Full 40장** 카탈로그 규모 (Pipeline D1)
- **Dawn 최소 체류 시간**: `DawnMinDwellSec` 3.0초 (GSM Formula 3)
- **CardSystemReadyTimeoutSec**: 5.0초 (GSM E9) — 초과 시 Dawn → Offering 강제 전환
- **샘플링 비용 추정**: 40장 × 가중치 계산 + `FRandomStream::FRandRange` × 3 ≈ 0.1-0.5ms (Full Vision 기준)
- **Pillar 1**: Dawn 체류가 불필요하게 길어지면 "기다리게 만듦" → 위반
- **결정론적 시드**: `PlaythroughSeed + DayIndex` (CR-CS-1 Step 3) — Eager/Lazy 상관 없이 동일 결과

### Requirements

- 샘플링 결과 결정론적 (동일 DayIndex에서 재샘플링 시 동일 핸드)
- Dawn → Offering 전환 지연 최소화
- GSM Prepare → Ready 응답 < 5초 (timeout 여유)

## Decision

**Eager 채택** — `FOnDayAdvanced(NewDayIndex)` 수신 즉시 샘플링 실행, Ready 상태 진입.

```cpp
void UCardSystemSubsystem::OnDayAdvancedHandler(int32 NewDayIndex) {
    if (NewDayIndex <= CurrentDayIndex) return;  // CR-CS-2 중복 가드
    CurrentState = ECardSystemState::Preparing;
    RegenerateDailyHand(NewDayIndex);   // CR-CS-1 step 1-3
    CurrentState = ECardSystemState::Ready;
    // GSM이 OnPrepareRequested()를 호출하면 즉시 OnCardSystemReady(bDegraded=false) 응답
}
```

### GSM Prepare/Ready 프로토콜 (본 ADR과 함께)

```cpp
// GSM이 Dawn 진입 후 호출
void UCardSystemSubsystem::OnPrepareRequested() {
    if (CurrentState == ECardSystemState::Ready) {
        // 이미 Ready — Eager 덕분에 즉시 응답
        GSM->OnCardSystemReady(/*bDegraded*/ DailyHand.Num() < 3);
    } else if (CurrentState == ECardSystemState::Preparing) {
        // 샘플링 진행 중 (Pipeline DegradedFallback 등 예외 케이스) — 완료 후 응답
        // (Normal case에서는 Preparing은 ~1ms이므로 여기 도달 희귀)
    } else if (CurrentState == ECardSystemState::Uninitialized) {
        // FOnDayAdvanced 미수신 — on-demand 샘플링 (EC-CS-4)
        const int32 DayIdx = GetDayIndexFromSessionRecord();
        RegenerateDailyHand(DayIdx);
        CurrentState = ECardSystemState::Ready;
        GSM->OnCardSystemReady(/*bDegraded*/ DailyHand.Num() < 3);
    }
}
```

## Alternatives Considered

### Alternative 1: Lazy — Offering 진입 시 샘플링

- **Description**: `FOnDayAdvanced` 수신 시 상태만 Preparing으로 설정하고 실제 샘플링은 GSM `OnPrepareRequested()` 호출 시점에 실행
- **Pros**: Init 시점 작업 최소화 — 앱 시작 시 샘플링 실행 안 함 (Day 1 첫 실행 Dawn 진입 전)
- **Cons**: 
  - GSM Dawn → Offering 전환 시 5초 timeout 내 샘플링 완료 의무 — 현재 규모에서는 여유 있으나 Full 40장 + 복잡 시즌 가중치에서 마진 감소
  - Dawn 최소 체류 3초 + 샘플링 시간 → 체감 지연 가능
  - EC-CS-4 (FOnDayAdvanced 미수신 시 on-demand) 경로가 "정상 경로의 일부"로 격상됨 — 복잡도 증가
- **Rejection Reason**: 현재 규모에서 Eager 비용 무시 가능 (0.1-0.5ms). Lazy의 지연 위험이 더 큼

### Alternative 2: Hybrid — 첫 Day는 Lazy, 이후는 Eager

- **Description**: Day 1 첫 실행 시에만 Lazy (앱 시작 시 샘플링 회피), Day 2+는 Eager
- **Pros**: Day 1 앱 시작 응답성 개선
- **Cons**: 조건 분기로 인한 복잡도. Day 1 Dawn 진입 시 샘플링 지연은 여전 (Alt 1과 동일 문제)
- **Rejection Reason**: 복잡도 증가 대비 이점 미미. Eager의 앱 시작 시 샘플링은 < 1ms로 무시 가능

## Consequences

### Positive

- **예측 가능한 응답성**: `OnPrepareRequested()` 호출 시 즉시 Ready 응답 (샘플링 대기 없음) — timeout 여유 충분
- **단순 구현**: 1개 메인 경로 (`FOnDayAdvanced` → Preparing → Ready). EC-CS-4 on-demand 경로는 예외 케이스로 유지
- **결정론 유지**: `PlaythroughSeed + DayIndex` 시드 — 시점 변경 무관

### Negative

- **앱 시작 시 샘플링 실행** — Day 1 FOnDayAdvanced(1) 수신 시 샘플링 (~0.5ms 추가). GameInstance::Init 총 budget 50ms의 1% — 무시 가능
- **미래 규모 확장 시 재평가 필요**: Full 40장 + 복잡 시즌 가중치 (예: per-카테고리 가중치 10+) + 소프트 억제 + 추가 필터 시 샘플링 비용 증가 가능. > 1ms 시 본 ADR 재검토

### Risks

- **샘플링 함수 버그로 성능 저하 시 앱 시작 feel 영향**: 현재 0.5ms이지만 미래 regression 가능. **Mitigation**: AC 추가 — `FMockRandomStream`으로 고정 시드 + `FPlatformTime::Seconds()` 측정으로 < 1ms 보장

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `card-system.md` | §OQ-CS-5 "ADR 후보: 카드 풀 재생성 시점 — Eager vs Lazy" | **본 ADR이 OQ-CS-5의 정식 결정 (Eager 채택)** |
| `card-system.md` | §CR-CS-2 "Eager 생성" 명시 | 본 ADR이 GDD 기본값을 ADR로 격상 |
| `card-system.md` | §CR-CS-3 Prepare/Ready 프로토콜 | 본 ADR §Decision GSM 상호작용 구체화 |
| `game-state-machine.md` | §E9 CardSystemReadyTimeoutSec 5초 | Eager로 timeout 여유 충분 |

## Performance Implications

- **CPU (샘플링)**: MVP ≈ 0.1ms, Full ≈ 0.5ms (FRandomStream 3회 × 40장 가중 탐색) — 무시 가능
- **Memory**: `DailyHand: TArray<FName>` 3 entries = ~24 bytes — 무시 가능
- **Init Time**: Day 1 FOnDayAdvanced(1) 시 +0.5ms — 50ms budget의 1%

## Migration Plan

Card sprint 진입 시:
1. `UCardSystemSubsystem::OnDayAdvancedHandler` 구현 — 상기 Decision 의사코드
2. `OnPrepareRequested` 구현 — 3-way 분기
3. Card GDD §CR-CS-2 "Eager 생성"을 "ADR-0006 채택 (Eager)"로 cross-reference 추가

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| Eager 채택 | `OnDayAdvancedHandler`에서 `RegenerateDailyHand` 직접 호출 | 매치 1건 |
| 샘플링 시간 | AC — Mock RandomStream + Full 40장 catalog에서 10회 샘플링 wall-clock | 평균 < 1ms |
| 결정론 유지 | 고정 `PlaythroughSeed`로 샘플링 2회 → 동일 DailyHand | 동일 |
| GSM Prepare 응답성 | `OnPrepareRequested()` 호출 → `OnCardSystemReady()` 응답 경과 시간 | < 1ms (Eager 경로) |

## Related Decisions

- **card-system.md** §OQ-CS-5 — 본 ADR의 직접 출처
- **ADR-0003** (Data Pipeline sync loading) — Pipeline 카탈로그 Ready 후 Card 샘플링 가능
- **ADR-0011** (UDeveloperSettings) — `UCardSystemConfigAsset`의 `UCardSystemSettings : UDeveloperSettings` 이전 권장 항목과 무관 (본 ADR은 타이밍만)
