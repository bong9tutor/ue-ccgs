# Story 004 — GrowthTransition Mesh 교체 + Formula 1 SmoothStep Lerp + E1 Multi-skip

> **Epic**: [core-moss-baby-character](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 3-4h

## Context

- **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md) §CR-1 (성장 단계 시각 매핑) + §Formula 1 (머티리얼 파라미터 Lerp) + §Edge Cases E1, E6
- **TR-ID**: TR-mbc-001 (AC-MBC-03) + TR-mbc-002 (priority)
- **Governing ADR**: N/A — GDD contract.
- **Engine Risk**: LOW (MBC 머티리얼은 GSM MPC 자동 참조 — 직접 GPU 영향 없음)
- **Engine Notes**: 메시 교체 `SetStaticMesh()` 즉시 반환, 시각 반영 다음 렌더 프레임. Lumen Surface Cache 재빌드로 1-2 프레임 GI 흐려짐 — 소형 씬 허용 범위. Formula 1 SmoothStep은 GSM Formula 1과 동일.
- **Control Manifest Rules**: Cross-Cutting — Hardcoded 문자열 경로 금지 (`FSoftObjectPath` + DataTable/DataAsset 참조).

## Acceptance Criteria

- **AC-MBC-01** [`AUTOMATED`/BLOCKING] — Sprout 상태에서 GrowthTransition 이벤트 발생 시 메시 교체 수행되어 StaticMesh 레퍼런스가 Growing 메시로 교체 + 이전 메시 참조 해제 (CR-1).
- **AC-MBC-02** [`INTEGRATION`/BLOCKING] — Sprout에서 Mature로 다단계 스킵 시 성장 단계 2 이상 건너뛰기 시 StaticMesh 레퍼런스가 Growing을 중간값으로 경유하지 않고 Mature로 직행 + MaterialLerp 호출 횟수 = 1 (E1).
- **AC-MBC-11** [`AUTOMATED`/BLOCKING] — ReducedMotion ON + 성장 전환 트리거 시 MaterialLerp 호출에서 duration=0, 목표값 즉시 도달 (E6).

## Implementation Notes

1. **Growth Subscription**:
   - `AMossBaby::BeginPlay`에서 `GrowthSubsystem->OnGrowthStageChanged.AddDynamic(this, &AMossBaby::OnGrowthStageChangedHandler);`
2. **`OnGrowthStageChangedHandler(EGrowthStage NewStage)`** (AC-MBC-01, AC-MBC-02 E1):
   - `if (!RequestStateChange(EMossCharacterState::GrowthTransition)) { return; }` — priority check (FinalReveal 중이면 skip).
   - **Stage 다단계 스킵 (E1)**: `CurrentStage`를 `NewStage`로 즉시 업데이트 (중간 단계 경유 없음). 메시 및 Lerp는 **최종 Stage로 직행** — Growing 경유 없이 Mature mesh swap + Mature preset Lerp.
   - `MeshComponent->SetStaticMesh(PresetAsset->GetMeshForStage(NewStage));` — `SetStaticMesh()` 1회 호출.
   - `const FMossBabyMaterialPreset& Target = PresetAsset->StagePresets[NewStage];`
   - `StartMaterialLerp(Target, GetSettings()->GrowthTransitionDurationSec);` — Formula 1 순차 Lerp.
3. **`StartMaterialLerp(Target, D_transition)`** (Formula 1):
   - **E6 ReducedMotion**: `if (IsReducedMotion() || D_transition <= 0.001f) { ApplyPreset(Target); return; }` — 즉시 적용 (AC-MBC-11).
   - 그 외: LerpStart/Target 저장 + 60Hz timer tick.
4. **LerpTickHandler**:
   - `float T = GetWorld()->GetTimeSeconds() - LerpStartTime;`
   - `T = FMath::Clamp(T, 0.0f, LerpDuration);` — 오버런 방어 (GDD §Formula 1 "t 클램프").
   - `float X = T / LerpDuration;`
   - `float Ease = X * X * (3.0f - 2.0f * X);` — SmoothStep.
   - 6 파라미터 각각 `V = Lerp(V_current, V_target, Ease);` → `ApplyPreset` 경유.
   - `if (T >= LerpDuration) { ExitGrowthTransition(); }`.
5. **MaterialLerp 호출 카운트 검증 (AC-MBC-02)**:
   - Mock 경로: `StartMaterialLerp` 호출 counter 도입 — unit test에서 1회만 호출 확인.

## Out of Scope

- FinalReveal Lerp — 동일 패턴이나 별도 story 007 (priority, 2.5s D_transition)
- QuietRest DryingOverlay (story 005)
- Mesh load failure E8 (story 007에서 fallback 처리)
- Growth Subsystem 구현 자체 (Feature epic)

## QA Test Cases

**Test Case 1 — AC-MBC-01 메시 교체**
- **Given**: `CurrentStage = Sprout`, `MeshComponent->GetStaticMesh() == SproutMesh`.
- **When**: Growth broadcasts `FOnGrowthStageChanged(Growing)`.
- **Then**: `MeshComponent->GetStaticMesh() == GrowingMesh` (직접 비교). 이전 SproutMesh 참조 `TSoftObjectPtr<UStaticMesh>::IsValid() == false` (or explicit Release).

**Test Case 2 — AC-MBC-02 E1 multi-skip**
- **Given**: `CurrentStage = Sprout`.
- **When**: Growth broadcasts `FOnGrowthStageChanged(Mature)` (Growing skipped — 3일 부재).
- **Then**: 
  - `MeshComponent->GetStaticMesh() == MatureMesh` (Growing mesh 미 거침).
  - `StartMaterialLerp` 호출 카운트 = 1.
  - 최종 프레임에서 SSS_Intensity = 0.75 (Mature preset), Growing preset (0.55) 경유 없음.

**Test Case 3 — AC-MBC-11 ReducedMotion**
- **Given**: `IsReducedMotion() == true`, `CurrentStage = Sprout`.
- **When**: `FOnGrowthStageChanged(Growing)` 수신.
- **Then**: 다음 frame에서 MID SSS_Intensity == 0.55 (Growing target 즉시 적용). LerpTick timer 미시작.

## Test Evidence

- [ ] `tests/unit/moss-baby/mesh_swap_test.cpp` — AC-MBC-01
- [ ] `tests/integration/moss-baby/multi_skip_e1_test.cpp` — AC-MBC-02
- [ ] `tests/unit/moss-baby/reducedmotion_lerp_skip_test.cpp` — AC-MBC-11

## Dependencies

- **Depends on**: story-001 (MossBaby + Preset), story-002 (state machine)
- **Unlocks**: story-007 (FinalReveal reuses pattern), story-008 (E4 compound QuietRest+GrowthTransition)
