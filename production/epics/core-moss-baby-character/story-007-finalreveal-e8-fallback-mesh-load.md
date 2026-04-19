# Story 007 — FinalReveal + Mesh Load Failure E8 Fallback

> **Epic**: [core-moss-baby-character](EPIC.md)
> **Layer**: Core
> **Type**: Integration
> **Status**: Ready
> **Manifest Version**: 2026-04-19
> **Estimated Effort**: 2h

## Context

- **GDD**: [design/gdd/moss-baby-character-system.md](../../../design/gdd/moss-baby-character-system.md) §States and Transitions (FinalReveal row) + §Edge Case E8 (FinalReveal 메시 로드 실패)
- **TR-ID**: TR-mbc-002 (priority — FinalReveal highest)
- **Governing ADR**: ADR-0005 §Decision §Stage 2 — `FOnFinalFormReached` Growth publish (Day 21 Stage 2 경로).
- **Engine Risk**: LOW
- **Engine Notes**: `UMossFinalFormAsset : UPrimaryDataAsset` per-form 자산 (ADR-0010). MVP 최종 형태 1종. `FinalRevealDurationSec` 기본 2.5s — `GrowthTransitionDurationSec` 1.5s보다 느림 (이 일회성이 형태에 무게를 준다 — Pillar 4).
- **Control Manifest Rules**: Cross-Cutting — Hardcoded 문자열 경로 금지 (`FSoftObjectPath`). Pillar 1 — 에러 표시 금지 (E8 메시 로드 실패 시 silent fallback).

## Acceptance Criteria

- **AC-MBC-12** [`MANUAL`/ADVISORY] — Final 메시 로드 실패 시뮬레이션 상태에서 FinalReveal 진입 시도 시 크래시 없이 Mature 메시 폴백 + 경고 로그 (E8).

## Implementation Notes

1. **Stage 2 Subscription**:
   - Story 003의 Stage 1 `FOnCardOffered` 외에 **FinalReveal 전용 이벤트**는 `FOnFinalFormReached(FName FinalFormId)` — Growth가 발행 (ADR-0005 Stage 2 chain).
   - `AMossBaby::BeginPlay`에서 `GrowthSubsystem->OnFinalFormReached.AddDynamic(this, &AMossBaby::OnFinalFormReachedHandler);`
2. **`OnFinalFormReachedHandler(FName FinalFormId)`**:
   - `RequestStateChange(EMossCharacterState::FinalReveal);` — priority highest (4), 어떤 state에서도 인터럽트.
   - `TSoftObjectPtr<UMossFinalFormAsset> FormAsset = FinalFormRegistry->GetAssetForId(FinalFormId);`
   - Sync load: `UMossFinalFormAsset* LoadedAsset = FormAsset.LoadSynchronous();`
   - **E8 fallback**: `if (!LoadedAsset) { UE_LOG(LogMossBaby, Warning, TEXT("FinalForm load failed for %s, falling back to Mature"), *FinalFormId.ToString()); FallbackToMature(); return; }`
3. **`FallbackToMature()`** (E8):
   - `MeshComponent` 유지 — 이미 Mature mesh 로드되어 있음 (story 004 Day 15-20 진입 후).
   - 머티리얼은 Mature 기반으로 Final 프리셋 적용 시도: `if (LoadedAsset && LoadedAsset->MaterialPreset.IsValid()) { StartMaterialLerp(LoadedAsset->MaterialPreset, 2.5f); }` — 머티리얼만 Final, 메시는 Mature 유지.
   - **Pillar 1**: 사용자에게 에러 표시 금지. 로그만.
4. **Normal path (성공)**:
   - `MeshComponent->SetStaticMesh(LoadedAsset->Mesh);`
   - `StartMaterialLerp(LoadedAsset->MaterialPreset, GetSettings()->FinalRevealDurationSec); // 기본 2.5s`
5. **FinalReveal Exit**:
   - Lerp 완료 시 `CurrentState = EMossCharacterState::Idle`. **단 FinalReveal은 영구 Idle** — 추가 GrowthTransition/Reacting 금지 (GDD §States and Transitions "Lerp 완료 → 영구 Idle").
   - 구현: `bFinalRevealComplete = true;` 플래그 — 이후 `RequestStateChange`는 Idle 외 reject.

## Out of Scope

- `UMossFinalFormAsset` 스키마 정의 (Feature Growth epic — ADR-0010 FFinalFormRow 격상)
- MVP 단일 최종 형태 vs Full 8-12종 분기 (OQ-3 VS/Full)
- Niagara FinalReveal 파티클 (OQ-5 VS)
- Farewell 상태의 골든 파티클은 Lumen Dusk Scene 소유 (Lumen Dusk epic)

## QA Test Cases

**Test Case 1 — Normal FinalReveal**
- **Given**: `CurrentState = Idle` (Mature stage), `UMossFinalFormAsset[MossFinal_A]` 정상 로드 가능.
- **When**: Growth broadcasts `FOnFinalFormReached("MossFinal_A")`.
- **Then**: 
  - `CurrentState = FinalReveal`.
  - `MeshComponent->GetStaticMesh() == FormAsset->Mesh`.
  - 2.5s Lerp 완료 후 `CurrentState = Idle`, `bFinalRevealComplete = true`.

**Test Case 2 — AC-MBC-12 E8 Fallback (MANUAL)**
- **Given**: `UMossFinalFormAsset` 에셋 경로에 파일 부재 (이름 오타 또는 삭제).
- **When**: Growth broadcasts `FOnFinalFormReached("MissingForm")`.
- **Then**: 
  - Crash 없음.
  - `UE_LOG(LogMossBaby, Warning, "FinalForm load failed...")` 1회 기록.
  - `MeshComponent->GetStaticMesh()` 여전히 Mature mesh.
  - UI 에러 메시지 없음 (Pillar 1).
- **Evidence**: `production/qa/evidence/mbc-e8-fallback-[date].md` — 에셋 삭제 + 녹화.

**Test Case 3 — FinalReveal 후 추가 state 거부**
- **Given**: `bFinalRevealComplete = true`, `CurrentState = Idle`.
- **When**: `RequestStateChange(Reacting)` 또는 `RequestStateChange(GrowthTransition)`.
- **Then**: Return `false`. Idle 유지.

## Test Evidence

- [ ] `tests/integration/moss-baby/finalreveal_normal_test.cpp` — 정상 경로
- [ ] `production/qa/evidence/mbc-e8-fallback-[YYYY-MM-DD].md` — AC-MBC-12 수동 검증
- [ ] `tests/unit/moss-baby/final_reveal_locked_test.cpp` — 완료 후 reject

## Dependencies

- **Depends on**: story-002 (priority), story-004 (Lerp pattern)
- **Unlocks**: Day 21 anchor moment validation (cross-epic smoke test)
