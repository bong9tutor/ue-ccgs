# ADR-0013: PSO Precaching 전략 — Async Bundle + Graceful Degradation

## Status

**Accepted** (2026-04-19 — Async Bundle + Graceful Degradation 결정 명확. Lumen Dusk 첫 milestone GPU 프로파일링 (AC-LDS-04/11 [5.6-VERIFY])은 Implementation 단계 검증 — 결정 자체는 안정. ADR-0002 + ADR-0003 Accepted 의존 충족)

## Date

2026-04-19

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.6 (hotfix 5.6.1 권장) |
| **Domain** | Core / Rendering / Shader Compilation |
| **Knowledge Risk** | **HIGH** — UE 5.6에서 **PSO(Pipeline State Object) 점진 컴파일 방식으로 변경**. 런타임 중 PSO가 점진적으로 컴파일되어 첫 로드 시 히칭 가능 (`breaking-changes.md §Rendering Pipeline / PSO 컴파일 방식 변경` MEDIUM 영향). 5.6 특정 API 검증 필수 |
| **References Consulted** | `breaking-changes.md §Rendering Pipeline`, `current-best-practices.md §1 Rendering §PSO Precaching`, lumen-dusk-scene.md §CR-6 + §AC-LDS-11 + §E4-E5, ADR-0004 (MPC+Light hybrid) |
| **Post-Cutoff APIs Used** | UE 5.6 `FShaderPipelineCache::SetBatchMode` + `NumPrecompilesRemaining()` polling (Lumen Dusk 구현 참고) — 5.6 런타임 동작 실기 검증 필수 |
| **Verification Required** | AC-LDS-11 수동 검증 + 첫 구현 마일스톤 heuristic GPU profiling (6 GameState 각각 first-load 히칭 frame count 측정) |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0002 (Data Pipeline container — PSO 번들은 별도 asset), ADR-0003 (Sync 로딩 — Pipeline Initialize 시 PSO 번들 경로 조회) |
| **Enables** | Lumen Dusk Scene 첫 구현 마일스톤 시작. 6 GameState 전환 히칭 최소화. Pillar 1 "조용한 응답성" 유지 |
| **Blocks** | **Lumen Dusk Scene 첫 구현 마일스톤** — 본 ADR Accepted 전 PSO 전략 없이 렌더 히칭 리스크 감수 불가 |
| **Ordering Note** | ADR-0004와 병행 (Lumen Dusk Scene 첫 milestone 전에 둘 다 Accepted). 본 ADR은 Lumen Dusk GDD §CR-6을 architecture-level 결정으로 격상 |

## Context

### Problem Statement

UE 5.6은 PSO (Pipeline State Object) 컴파일을 **런타임 중 점진적으로** 수행한다 (breaking-changes.md §Rendering Pipeline). 최초 로드 시 모든 permutation을 사전 준비하던 이전 동작과 다르다. Moss Baby는 에셋 수가 적지만 6 GameState 전환 시 머티리얼·post-process permutation이 **전환 시점에 처음 컴파일**되면 Dream→Waiting, Any→Farewell 같은 **앵커 모먼트에서 frame hitching 발생**할 위험.

Pillar 1 (조용한 존재)은 "응답성 유지"를 요구 — 앱 첫 로드 시 200ms 이상 stall은 "응답 없음" 체감 유발. PSO 미준비로 Dream 전환에서 500ms 히칭이 발생하면 anchor moment (Waiting 구리빛 → Dream 달빛 1.35s blend) 가치가 손상됨.

### Constraints

- **Pillar 1 — splash screen 금지**: Alternative "앱 시작 splash로 모든 PSO 컴파일" 배제
- **Pillar 4 — anchor moment 보호**: Dream/Farewell 전환 시 hitching은 감정 설계 직접 훼손
- **UE 5.6 점진 컴파일 default 동작**: 이를 그대로 두면 첫 번째 상태 전환마다 PSO 컴파일 히칭
- **배포 빌드만 적용**: PIE/Editor는 점진 컴파일이 정상. Shipping/Cooked 빌드만 precaching 대상
- **작은 씬 규모**: 에셋 ~60개 (ADR-0002 기준) — PSO permutation 수는 제한적

### Requirements

- Shipping 빌드에서 6 GameState 전환 시 first-load hitching 최소화 (frame time < 100ms)
- Pillar 1 "splash screen 금지" 유지 — Dawn 시퀀스 자체 외 추가 대기 화면 없음
- DegradedFallback 허용 — PSO 번들 없거나 로드 실패 시 점진 컴파일 정상 진행 (크래시 없음)
- Lumen Dusk Scene GDD §CR-6 기존 설계 (PSO 번들 파일 `PSO_MossDusk.bin`)와 호환

## Decision

**Async Bundle + Graceful Degradation 채택**.

### 3-Phase 전략

**Phase 1 — Build Time (빌드 파이프라인)**
- `r.ShaderPipelineCache.Enabled 1` Project Settings
- Cook 단계에서 PSO 사용 기록 → `PSO_MossDusk.bin` 번들 파일 생성 (Saved/Cooked/Windows/MadeByClaudeCode/Content/PipelineCaches/)
- CI 파이프라인이 매 릴리스 빌드에 PSO 번들 포함 확인 (미래 `/release-checklist` 체크 항목)

**Phase 2 — Runtime Initialize (UGameInstance::Init())**
- Lumen Dusk Scene Subsystem이 Pipeline Ready 후 PSO 번들 비동기 로드 시작
- `FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast)` — 백그라운드 컴파일
- 게임 플레이는 차단하지 않음 (Pillar 1) — Dawn 시퀀스는 정상 진행

**Phase 3 — PSO Ready Check (Dawn Entry)**
- GSM Dawn 진입 시 `ALumenDuskSceneActor::IsPSOReady()` 확인 (Lumen Dusk AC-LDS-12)
- `PSOWarmupTimeoutSec` (기본 10.0초) 내 미완 시 DegradedFallback 허용 — 첫 프레임 히칭 감수 (Lumen Dusk §E4)
- 로그 기록: `UE_LOG(LogLumenDusk, Warning, TEXT("PSO precaching incomplete after %.1fs — accepting first-frame hitching"), Timeout)`

### DegradedFallback 시나리오

| 상황 | 동작 |
|---|---|
| PSO 번들 파일 존재 + Phase 2 성공 | Phase 3에서 Ready — 정상 |
| PSO 번들 파일 존재 + Phase 2 타임아웃 | DegradedFallback — 첫 상태 전환에 히칭, 이후는 자연 warm up (Lumen Dusk E4) |
| PSO 번들 파일 부재 (`PSO_MossDusk.bin` 미존재) | DegradedFallback — `UE_LOG(Warning)` 1회 + 점진 컴파일 정상 진행 (Lumen Dusk E5) |
| PSO 번들 파일 손상 | DegradedFallback — CRC 검증 실패 시 부재와 동일 처리 |

### Pillar 1 준수 검증

- **splash screen 없음** ✅ — Dawn 시퀀스 자체가 시각 presentation. PSO 로드는 Dawn 1-3초 자연 체류 시간 (GSM `DawnMinDwellSec = 3.0`) 내 완료 목표
- **stall 체감 < 200ms** ✅ (목표) — `PSOWarmupTimeoutSec`가 넉넉하면 Dawn 내 완료
- **silent DegradedFallback** ✅ — 에러 다이얼로그 없음, `UE_LOG`만

### Key Interfaces

```cpp
// Lumen Dusk Scene Actor (ADR-0004의 Light Actor cache와 동일 시스템)
UCLASS()
class ALumenDuskSceneActor : public AActor {
    // ... (existing from ADR-0004)
public:
    void StartPSOPrecaching();            // Phase 2
    bool IsPSOReady() const;              // Phase 3 — GSM Dawn Entry 체크

private:
    void OnPSOCacheLoaded();               // FShaderPipelineCache callback
    FTimerHandle PSOWarmupTimer;           // PSOWarmupTimeoutSec
    bool bPSOReady = false;
    UPROPERTY() TObjectPtr<ULumenDuskAsset> Config;  // PSOWarmupTimeoutSec 등
};

// PSO Ready 확인 (GSM Dawn Entry)
bool ALumenDuskSceneActor::IsPSOReady() const {
    return bPSOReady || (Config->PSOWarmupTimeoutSec > 0.0f &&
                         GetWorld()->GetTimeSeconds() - PSOStartTime > Config->PSOWarmupTimeoutSec);
}
```

## Alternatives Considered

### Alternative 1: Splash Screen + 모든 PSO 사전 컴파일

- **Description**: 앱 시작 시 splash screen 표시 + `FShaderPipelineCache::OpenPipelineFileCache(..., BatchMode::Precompile)` 동기 완료 대기
- **Pros**: 첫 번째 상태 전환부터 완벽한 응답성
- **Cons**: **Pillar 1 splash screen 절대 금지** (Pillar 1 + Lumen Dusk §Player Fantasy "황혼이 책상 위 유리병을 감쌀 때 세계는 거기서 멈춘다" — 로딩 화면이 이 도입부를 방해)
- **Rejection Reason**: Pillar 1 직접 위반

### Alternative 2: 완전 Lazy — 점진 컴파일 자연스럽게

- **Description**: PSO 번들 생성 안 함. UE 5.6 default 점진 컴파일에 의존
- **Pros**: 구현 비용 0
- **Cons**: 
  - Dream 전환 1.35s 블렌드 중 PSO 컴파일 → 100-500ms hitching 가능 → **anchor moment 가치 훼손 (Pillar 4 희소성 약화)**
  - Farewell 전환도 동일 위험
  - Lumen Dusk §CR-6 "PSO Precaching"이 이미 명시적으로 설계됨 — 이를 버리는 것은 GDD 변경
- **Rejection Reason**: anchor moment 보호 실패. Pillar 4 위반

### Alternative 3: Build-time 부분 Precompile + Runtime 나머지 Lazy

- **Description**: Cook 단계에서 Dawn/Offering/Waiting PSO만 번들에 포함, Dream/Farewell는 Lazy
- **Pros**: 번들 크기 감소
- **Cons**: Dream/Farewell이 정확히 anchor moment — 가장 중요한 상태가 Lazy = 최악 선택
- **Rejection Reason**: 본 ADR의 문제 의식과 정반대 — 핵심 moment를 Lazy로 두는 것은 자해

### Alternative 4: Async Bundle + Graceful Degradation (채택)

- 본 ADR §Decision

## Consequences

### Positive

- **Anchor moment 보호**: Dream/Farewell 전환에서 hitching 최소화 (Phase 2 성공 시) — Pillar 4 희소성 가치 보존
- **Pillar 1 준수**: splash 없음 + DegradedFallback silent — 모달·경고 없음
- **Lumen Dusk GDD 기존 설계 정렬**: §CR-6 PSO Precaching 소유 + §E4/E5 fallback 정책 그대로 유지
- **UE 5.6 neworkflow 활용**: BatchMode::Fast 백그라운드 컴파일이 정확히 이 use case를 위한 UE 추가 기능

### Negative

- **Cook 단계 complexity**: PSO 번들 생성을 위한 Cook Commandlet 설정 필요. 릴리스 체크리스트에 추가
- **첫 구현 마일스톤 GPU 프로파일링 의존**: AC-LDS-04/11 실측 결과가 본 ADR 정확성 검증 — ADR Proposed 상태 유지는 이 실측 결과 후 Accepted 전환
- **미래 에셋 규모 증가 시 재평가**: Full Vision 이상으로 permutation 증가 시 PSOWarmupTimeoutSec 재조정

### Risks

- **UE 5.6 `BatchMode::Fast` 실제 동작과 문서 상이**: post-cutoff API — 실기 검증 필수. **Mitigation**: AC-LDS-11 수동 검증 + `FShaderPipelineCache::NumPrecompilesRemaining()` polling으로 진행률 확인
- **PSOWarmupTimeoutSec 10초가 너무 길거나 짧음**: 하드웨어 다양성으로 범위 차이. **Mitigation**: `ULumenDuskAsset` Tuning Knob으로 노출 (기본 10.0, range [5.0, 30.0]) — 플레이테스트 결과 조정
- **Cooked 빌드 PSO 번들 파일 배포 누락**: CI에서 asset manifest 체크 필수. **Mitigation**: `/release-checklist` 스킬이 미래 PSO 번들 존재 여부 검증 항목 추가 권장
- **Lumen Surface Cache 히칭 vs PSO 히칭 구분**: 두 히칭은 다른 원인. PSO precaching은 PSO만 해결 — Surface Cache 재빌드는 ADR-0004 scope. **Mitigation**: 첫 GPU 프로파일링에서 두 히칭을 분리 측정 (`stat GPU` + `stat Lumen`)

## GDD Requirements Addressed

| GDD | Requirement | How Addressed |
|---|---|---|
| `lumen-dusk-scene.md` | §CR-6 PSO Precaching 3-phase 전략 | **본 ADR이 GDD 설계를 architecture-level ADR로 격상** |
| `lumen-dusk-scene.md` | §E4 PSO 컴파일 미완료 시 DegradedFallback 진행 | 본 ADR §DegradedFallback 시나리오 |
| `lumen-dusk-scene.md` | §E5 PSO 번들 파일 부재 처리 | 본 ADR §DegradedFallback 시나리오 |
| `lumen-dusk-scene.md` | AC-LDS-11 [5.6-VERIFY] 첫 프레임 히칭 수동 검증 | 본 ADR Validation Criteria §1 |
| `lumen-dusk-scene.md` | AC-LDS-12 PSO 번들 부재 시 크래시 없음 (BLOCKING) | 본 ADR §Key Interfaces `IsPSOReady()` + UE_LOG Warning |
| `breaking-changes.md` | §Rendering Pipeline / PSO 컴파일 방식 변경 MEDIUM 영향 | 본 ADR이 MEDIUM risk를 mitigation으로 전환 |
| `current-best-practices.md` | §1 Rendering §PSO Precaching 활성화 권장 | 본 ADR Decision §Phase 1이 권장 따름 |

## Performance Implications

- **CPU (Init)**: Phase 2 비동기 로드 → 메인 스레드 차단 없음. 워커 스레드에서 PSO 컴파일
- **Memory (PSO cache)**: `PSO_MossDusk.bin` 추정 크기 ~5-15MB (소형 씬 permutation). 1.5GB ceiling의 1% — 무시 가능
- **GPU (첫 프레임)**: PSO 번들 hit 시 첫 상태 전환 히칭 < 50ms (목표). Miss 시 100-500ms (Degraded)
- **Disk (`.uasset` 추가)**: PSO 번들 1 파일 + config — 무시 가능

## Migration Plan

Lumen Dusk Scene 첫 구현 마일스톤 직전:
1. **Project Settings 확인**: `r.ShaderPipelineCache.Enabled 1` (DefaultEngine.ini)
2. **Cook 설정**: `r.ShaderPipelineCache.AutoSaveTime` + `r.ShaderPipelineCache.AutoSaveTimeBoundPSO` 튜닝
3. **Phase 2 구현**: `ALumenDuskSceneActor::StartPSOPrecaching` + `FShaderPipelineCache::OpenPipelineFileCache`
4. **Phase 3 구현**: `IsPSOReady()` + GSM Dawn Entry integration
5. **GSM E9 integration**: Dawn → Offering 전환 시 `IsPSOReady()` 확인 (PSO timeout이 CardSystem timeout과 중복 가능 — Dawn 체류 연장 최대 값 결정)
6. **GPU 프로파일링 세션**: 6 GameState 각각 wall-clock + GPU frame time 측정 → AC-LDS-04 + AC-LDS-11 통과 확인
7. **release-checklist** 추후 체크 항목 추가: PSO 번들 파일 존재 + 크기 검증

## Validation Criteria

| 검증 | 방법 | 통과 기준 |
|---|---|---|
| PSO 번들 생성 | Cook 후 `Saved/Cooked/.../PipelineCaches/PSO_MossDusk.bin` 존재 | 파일 존재 + 크기 > 0 |
| 비동기 로드 미차단 | Shipping 빌드 Init ~ Dawn 진입 경과 시간 | < 50ms (Pipeline D2 budget 유지) |
| PSOWarmupTimeoutSec 내 Ready | 표준 하드웨어 (NVMe SSD, GPU 2020+)에서 `IsPSOReady() == true` 시점 | < 10초 |
| Dream 전환 히칭 최소화 | AC-LDS-11 — Dream 전환 첫 frame에서 히칭 | < 100ms stall |
| DegradedFallback 크래시 없음 | AC-LDS-12 — `PSO_MossDusk.bin` 삭제 후 앱 실행 | 크래시 없음 + Warning 로그 1회 |
| Pillar 1 준수 | 수동 관찰 — splash screen, 로딩 bar, 에러 다이얼로그 | 모두 부재 |

## Related Decisions

- **lumen-dusk-scene.md** §CR-6 — 본 ADR의 직접 출처 (architecture-level 격상)
- **ADR-0004** (MPC ↔ Light Actor hybrid) — Lumen HWRT GPU 5ms budget을 공유. 두 ADR이 함께 Lumen Dusk 첫 milestone의 렌더 안정성 확보
- **ADR-0002** (Data Pipeline container) — PSO 번들은 엔진-generated asset이므로 Pipeline 카탈로그와 별개. Pipeline의 `GetFailedCatalogs()`에 포함되지 않음
- **ADR-0011** (UDeveloperSettings) — `PSOWarmupTimeoutSec`는 `ULumenDuskAsset : UPrimaryDataAsset` (ADR-0011 예외 — per-content 자산) 유지
- **architecture.md** §Engine Knowledge Gap §HIGH RISK Domains — Lumen HWRT + PSO Precaching이 HIGH 플래그된 도메인. 본 ADR이 PSO 축을 해결
