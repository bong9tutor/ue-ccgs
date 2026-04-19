# Story 003 — `UGameStateMPCAsset` + `FTSTicker` MPC Lerp 루프

> **Epic**: [core-game-state-machine](EPIC.md)
> **Layer**: Core
> **Type**: Logic
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/game-state-machine.md](../../../design/gdd/game-state-machine.md) §Rule 3 (MPC 매핑) + §Rule 4 (블렌딩) + §Formula 1 (SmoothStep Lerp) + §Formula 2 (StateBlendDuration)
- **TR-ID**: TR-gsm-002 (MPC writer uniqueness AC-LDS-06)
- **Governing ADR**: **ADR-0004** §Key Interfaces §`TickMPC` pseudocode — MPC scalar 8개 + AmbientRGB vector 순차 갱신 패턴.
- **Engine Risk**: MEDIUM (Lumen GI 상호작용 — HIGH risk상속은 story 005/006의 Light Actor integration에서 발생)
- **Engine Notes**: `UGameInstanceSubsystem`은 Tick 없음 → `FTSTicker::GetCoreTicker().AddTicker(...)` 로 매 프레임 콜백 등록 (Time System과 동일 패턴). `FTSTicker`는 Game pause 상태와 무관. `Deinitialize()`에서 ticker handle 해제 필수. MPC 갱신 시 `UMaterialParameterCollectionInstance*` + `FName` 파라미터 이름 캐싱 (프레임당 탐색 비용 절감).
- **Control Manifest Rules**: Core Layer Rules §Required Patterns — `TickMPC()` 순차 갱신. Cross-Layer Rules §Exceptions — `UGameStateMPCAsset`은 `UPrimaryDataAsset` 유지 (ADR-0011 예외). Global Rules §Performance Budgets — GSM `TickMPC` CPU cost 50-100μs (16.6ms의 0.5%).

## Acceptance Criteria

- **AC-GSM-06** [`CODE_REVIEW`/BLOCKING] — GSM 및 Visual State Director 구현 파일에서 MPC 파라미터 리터럴 값 grep 시 하드코딩 없음. 모든 값이 `UGameStateMPCAsset`에서만 참조.
- **AC-GSM-07** [`AUTOMATED`/BLOCKING] — Formula 1 SmoothStep 구현에 x=0, x=0.5, x=1.0 입력 시 각각 0.0, 0.5, 1.0 출력, [0,1] 범위 초과 없음.
- **AC-GSM-09** [`AUTOMATED`/BLOCKING] — `BlendDurationBias`=0.0, 0.5, 1.0에 대해 Formula 2로 Any→Farewell 블렌드 시간 계산 시 1.5s, 1.75s, 2.0s.

## Implementation Notes

1. **`UGameStateMPCAsset : UPrimaryDataAsset`** (`Source/MadeByClaudeCode/Core/GameState/GameStateMPCAsset.h`):
   - `UPROPERTY(EditAnywhere)` per-state target values (GDD §Rule 3 표 — 6개 state × 8 parameters).
   - `TMap<EGameState, FGameStateMPCTarget>` struct. `FGameStateMPCTarget` = `float ColorTempK; float LightAngleDeg; float Contrast; FLinearColor AmbientRGB; float AmbientIntensity; float MossBabySSSBoost; float KeyLightIntensity;`.
   - `GetPrimaryAssetId()` override → `FPrimaryAssetId("GameStateMPC", "Default")`.
2. **`FTSTicker` 등록** (`UMossGameStateSubsystem::Initialize`):
   - `TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UMossGameStateSubsystem::TickMPC), 0.0f);`
   - `Deinitialize()`에서 `FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);`
3. **`TickMPC(float DeltaTime) -> bool`**: ADR-0004 §Key Interfaces pseudocode §\[1\] 블록만 이 story 범위 — MPC SetScalar 6 + SetVector 1. Light Actor 구동(블록 \[2\])은 story 005.
4. **Formula 1 SmoothStep** (`ease(x) = x²(3 − 2x)`): 
   - `static float SmoothStep(float X) { X = FMath::Clamp(X, 0.0f, 1.0f); return X * X * (3.0f - 2.0f * X); }` — GDD §Formula 1 "부동소수점 방어" 명시.
5. **Formula 2 StateBlendDuration**: `D_blend = D_min + (D_max - D_min) * clamp(BlendDurationBias, 0, 1)`. D_min/D_max는 `UGameStateMPCAsset`의 transition table에서 조회.
6. **MPC Instance 캐싱**: `Initialize`에서 `GetWorld()->GetParameterCollectionInstance(MPCAsset)` 1회 획득 + `TWeakObjectPtr<UMaterialParameterCollectionInstance>` 캐시. null check 매 frame 필수.
7. **`V_start ≈ V_target` 최적화**: `|V_target − V_start| < 0.001f` 이면 Lerp 생략, 즉시 완료 처리 (GDD §Formula 1 precondition).

## Out of Scope

- Light Actor 구동 (story 005)
- Post-process Vignette/DoF (story 012 Lumen Dusk)
- ReducedMotion 토글 (story 007)

## QA Test Cases

**Test Case 1 — AC-GSM-07 SmoothStep math**
- **Given**: `UMossGameStateSubsystem::SmoothStep` 순수 함수.
- **When**: `x = 0.0, 0.25, 0.5, 0.75, 1.0, -0.1, 1.1` 입력.
- **Then**: 출력 `0.0, 0.15625, 0.5, 0.84375, 1.0, 0.0 (clamp), 1.0 (clamp)` (±1e-6).

**Test Case 2 — AC-GSM-09 Formula 2 bias**
- **Given**: Any→Farewell transition row (D_min=1.5, D_max=2.0).
- **When**: `BlendDurationBias = 0.0, 0.5, 1.0`.
- **Then**: 출력 `1.5s, 1.75s, 2.0s` (±1e-6).

**Test Case 3 — AC-GSM-06 MPC literal grep**
- **Setup**: `grep -rnE "SetScalarParameterValue.*[0-9]+\.[0-9]+" Source/MadeByClaudeCode/Core/GameState/`
- **Verify**: 매치 0건 (하드코딩 리터럴 금지). 모든 값이 `UGameStateMPCAsset::Get*()` 경유.
- **Pass**: grep empty.

**Test Case 4 — TickMPC 성능 (control-manifest guardrail)**
- **Given**: Shipping 빌드 + `stat GameTickMPC` 커스텀 stat.
- **When**: Waiting 상태 정상 60s.
- **Then**: `TickMPC` wall-clock per-frame < 100μs (guardrail 50-100μs).

## Test Evidence

- [ ] `tests/unit/game-state/smoothstep_test.cpp` — Formula 1 F1 0/0.5/1 + clamp
- [ ] `tests/unit/game-state/formula2_bias_test.cpp` — Formula 2 0/0.5/1 출력
- [ ] `tests/unit/game-state/mpc_literal_grep.md` — AC-GSM-06 명령 문서화
- [ ] `tests/integration/game-state/ticker_lifecycle_test.cpp` — `FTSTicker` add/remove lifecycle

## Dependencies

- **Depends on**: story-001 (Subsystem skeleton), story-002 (transition table)
- **Unlocks**: story-005 (Light Actor integration), story-007 (ReducedMotion E10)
