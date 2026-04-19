# Data Pipeline

> **Status**: R2 — 8 BLOCKER 수정 완료. `/design-review` 재실행 대기.
> **Author**: bong9tutor + systems-designer + unreal-specialist + creative-director + qa-lead (consultative spawns)
> **Last Updated**: 2026-04-17 (R2 — 8 BLOCKER fix, 18 AC, 12 OQ 중 4 resolved)
> **Implements Pillar**: Pillar 2 (선물의 시학 — 데이터 주도 콘텐츠), Pillar 3 (말할 때만 말한다 — 희소성 튜닝 외부화)
> **Review Mode (this revision)**: lean — CD-GDD-ALIGN auto-skip, Phase Gate에서 일괄 재실행
> **ADR Pending**: ADR-001 (컨테이너 선택), ADR-002 (로딩 전략) — `/design-review` 통과 후 작성

## Overview

Data Pipeline은 카드·꿈·최종 형태 같은 모든 디자이너-편집 콘텐츠를 코드와 분리해
런타임에 제공하는 Foundation 레이어다. 책임은 세 가지로 좁혀진다 — **(1) 콘텐츠
정의의 단일 출처(single source of truth)**, **(2) 타입 안전한 조회 인터페이스**,
**(3) 메모리 한도 1.5 GB 안에서의 로딩 정책**. 이 시스템이 없으면 카드 한 장,
꿈 한 줄, 임계값 0.05 변경마다 엔진 재빌드·재배포가 필요하고, 21일 실시간
세이브가 진행 중인 플레이어의 진행이 패치마다 위험에 노출된다.

플레이어는 Data Pipeline을 *직접* 인지하지 않는다. 그들이 느끼는 것은 이 시스템이
*enable* 하는 것 — 매일 손에 도달하는 카드의 시·계절·감정, 어쩌다 받는 꿈의 한
줄, 21일 끝의 최종 형태. Data Pipeline은 그 무대에 머물고, 디자이너만 느낀다.

이 GDD는 동시에 **Pillar 운용 레이어** 역할을 한다. 디자이너가 의도치 않게
Pillar를 배반하지 못하도록 schema-level contract를 강제한다 — row struct에서
수치형 stat 필드 금지(Pillar 2: "선물의 시학"), 꿈 트리거의 임계·간격은 반드시
외부 데이터로 노출(Pillar 3: "말할 때만 말한다"). 코드 리뷰가 아닌 *데이터 형태
자체*가 가드가 되도록 설계한다.

### 책임 경계 (이 시스템이 *아닌* 것)
- **세이브 데이터 컨테이너 아님** — 런타임 mutable state는 Save/Load Persistence
  (#3) 소관. Data Pipeline은 read-mostly.
- **콘텐츠 자체 아님** — 각 row의 의미·필드 셋은 Card (#8) / Dream (#9) /
  Growth (#7) GDD가 정의. 이 GDD는 그릇·로딩·조회·명명만 정한다.
- **로컬라이제이션 시스템 아님** — 한국어 텍스트 처리는 위임 (UE `FText` +
  String Table). 단, row에 `FText` 필드 사용 contract는 강제.

### 구현 결정 → ADR 후보
- **ADR-후보-1**: 데이터 컨테이너 선택 — `UDataTable` vs `UPrimaryDataAsset`
  vs `UDataRegistrySubsystem` (UE 5.6 권장은 use-case별 혼합. `current-best-
  practices.md §7`은 카드=DataTable / 꿈=DataAsset로 사전 가이드)
- **ADR-후보-2**: 로딩 전략 — Sync 일괄(MVP 10장 카드 규모) vs Async Bundle
  (Full 40장 + 50꿈 시 메모리 전략 분기)

두 결정의 *behavior* 결과(예: "꿈 텍스트는 첫 트리거 평가 전까지 메모리에
없을 수 있다")는 본 GDD의 Detailed Design / Edge Cases에 기술하되, *how*는
GDD 완성 후 `/architecture-decision`으로 분리.

## Player Fantasy

Data Pipeline의 Player Fantasy는 **직접적이지 않다**. 플레이어는 이 시스템을
인지하지 않는다. 대신 Pillar 2 ("선물의 시학")와 Pillar 4 ("끝이 있다")가
21일차 작별의 순간에 무너지지 않도록, 데이터 형태 자체로 보장하는 역할을
한다.

### Anchor: 21일차의 작별

21일차, 플레이어가 마지막 카드를 건넨다. 화면에는 숫자도 게이지도 없다. 이끼
아이가 어떤 형태로 깨어날지는 지난 20일 동안 건넨 카드들의 *결*이 결정한다.
플레이어는 그 인과를 계산하지 못한다 — 다만 "내가 건넨 것들이 이 아이를
만들었다"는 사실만 안다. Data Pipeline은 이 순간을 가능케 한다: 모든
카드·꿈·형태가 동일한 schema 아래 있기에, 플레이어의 21일은 *일관된 한 편*
으로 도착한다.

### Designer Experience (Secondary)

Foundation 시스템에서 디자이너는 두 번째 사용자다. 디자이너는 row struct에
새 카드 한 줄을 추가할 때, *Pillar를 배반할 수 있는 형태의 필드 자체가
존재하지 않음*을 안다. 수치형 stat 칸이 없으므로 "효율적인" 카드를 만들 수
없다. 꿈 임계값은 외부 DataAsset에 노출되어 있어, 코드 변경 없이 희소성을
조정할 수 있다 — 그러나 "매일 발화" 옵션은 schema가 거부한다.

이 안도감이 Pipeline의 두 번째 fantasy다. 코드 리뷰가 Pillar를 지키는 게
아니라 *데이터의 형태가* 지킨다.

### Reference Anchors
- **Stardew Valley / Spiritfarer**: 데이터 schema가 톤을 강제한 사례. cozy
  장르에서 콘텐츠 파이프라인이 게임 정체성의 일부로 다뤄진 정신.
- **Unpacking**: 시스템 자체는 invisible하지만 *허용/금지의 형태*가 게임의
  목소리를 만든 사례.

### 적용 contract
이 fantasy는 다음 두 조항으로 본 GDD의 Detailed Design / Edge Cases에서
검증 가능한 형태로 떨어진다:
- **C1**: Card row struct에 `int32 StatBonus` 류 수치형 효과 필드 정의 금지 (Pillar 2)
- **C2**: Dream 임계는 반드시 데이터 자산에 노출, 코드에 하드코드 금지 (Pillar 3)

이 조항이 깨지면 두 Pillar가 서서히 마모된다. 깨지지 않으면 21일차의 anchor
순간이 보존된다.

## Detailed Design

### Core Rules

**R1. 초기화 진입점** (R2 — BLOCKER 5 의존성 순서 명시)
`UDataPipelineSubsystem`은 `UGameInstanceSubsystem`을 상속한다. `UGameInstance::Init()`
직후 — 어떤 GameMode·World·Actor가 존재하기 전 — `Initialize()`가 호출된다. Pipeline이
**Ready** 상태에 도달하기 전에는 어떤 downstream 시스템도 조회 API를 호출해서는 안 된다.

**R2 BLOCKER 5 — Subsystem 초기화 순서 보장**: `UGameInstanceSubsystem` 간 `Initialize()`
호출 순서는 기본적으로 비결정적. Pipeline에 의존하는 downstream subsystem은 반드시
자신의 `Initialize()` 내에서 `Collection.InitializeDependency<UDataPipelineSubsystem>()`
호출로 선행 의존을 선언해야 한다. 본 GDD는 이 contract를 §Dependencies에 명시하며,
각 downstream GDD(#7, #8, #9 등)의 §Dependencies에도 동일 선언을 의무화.

**R2 BLOCKER 4 — AssetManager PrimaryAssetType 등록**: `UDreamDataAsset`을
`UAssetManager::GetPrimaryAssetIdList()`로 type-bulk 로드하려면 `DefaultEngine.ini`에
`[/Script/Engine.AssetManagerSettings]` → `PrimaryAssetTypesToScan` 항목에 `DreamData`
type 등록 필수. **Cooked 빌드에서 미등록 시 빈 결과** → Dream 카탈로그 전체 실패.
등록 절차: (1) DefaultEngine.ini에 PrimaryAssetType 추가, (2) `UAssetManager` 에디터
재시작으로 인덱싱 확인, (3) Cooked 빌드에서 `GetPrimaryAssetIdList()` 결과 비어있지
않음을 AC로 검증 (신규 AC-DP-18 참조).

**R2 BLOCKER 6 — GC 안전성**: `UDreamDataAsset*` 등 `UObject*` 캐시를 보관하는 모든
멤버 변수는 반드시 `UPROPERTY()` 매크로로 마크해야 한다. 미마크 시 GC pass에서 해당
오브젝트가 수집되어 dangling pointer 발생. `UDataPipelineSubsystem` 내 캐시 배열
(`TArray<UDreamDataAsset*>`, `UStillnessBudgetAsset*` 등) 모두 `UPROPERTY()` 필수.

**R2. 초기화 순서 (deterministic)**
① Card 카탈로그 (`UDataTable`) → ② Final Form 카탈로그 (`UDataTable`) → ③ Dream 자산
목록 (`UPrimaryDataAsset` per dream, `UAssetManager`로 type-bulk 로드) → ④ Stillness
Budget 튜닝값. 순서는 fixed. 한 단계가 실패하면 즉시 **DegradedFallback**으로 전환하고
이후 단계를 시도하지 않는다.

**R3. 조회 contract — 동기 + null-safe + fail-close**
모든 조회 함수는 동기 반환. 자산이 없거나 로드 실패 시 반환값은 `nullptr`(객체) 또는
`TOptional<FRow>`(struct). Pipeline은 **빈 row를 조용히 리턴하지 않는다 (fail-close)**.
빈 카드가 플레이어 손에 도달하면 Pillar 2 침묵 위반이므로, null을 받은 downstream은
*명시적으로* 그 day의 동작을 건너뛰어야 한다.

**R4. C1 schema 가드 — 수치 stat 필드 금지 (Pillar 2)**
강제 메커니즘은 두 층으로 운영한다:
- **컴파일 타임 (가장 강한 보장)**: `FGiftCardRow` USTRUCT에 `int32`·`float`·`double`
  류 수치형 효과 필드를 *정의하지 않는다*. 허용 필드는 `FName` (CardId), `TArray<FName>`
  (태그 배열), `FText` (이름·설명), `FSoftObjectPath` (아이콘·SFX). 새 필드 추가 시
  C1 위반 가능성을 GDD 변경과 동시에 평가한다.
- **에디터 저장 시점**: `UEditorValidatorBase` 서브클래스가 DataTable의 모든 row에 대해
  허용 필드 외 사용·non-zero 수치값 패턴을 저장 시 검출. 빌드 cook 시 `-RunAssetValidation`
  플래그로 동일 검증 재실행.
- **코드 리뷰 + agent rule**: `.claude/rules/` 항목으로 PR 단계 보강.

**R5. C2 schema 가드 — Dream 임계 외부 노출 (Pillar 3)**
Dream 트리거 임계(태그 벡터 임계값, 최소 누적 일수, 최소 dream-간 간격)는 각
`UDreamDataAsset` 인스턴스의 `UPROPERTY(EditAnywhere)` 필드에만 존재한다. 코드 내
리터럴(`>= 0.6f`, `if (Days >= 7)` 등) 금지. 이 contract는 컴파일러 강제 불가 →
**code review + agent rule + Coding Standards 명시 항목**으로 운영. 깨질 경우 `/design-review`
가 적발해야 한다.

**R6. `FSoftObjectPath` 강제 — 자산 참조 (안티패턴 차단)**
모든 자산 참조는 `FSoftObjectPath` 또는 `TSoftObjectPtr<T>`로 선언. `LoadObject<>`
직접 호출 / `FindObject<>` 런타임 조회 / `FStringAssetReference` (구식) 모두 금지.
일괄 로드는 R2가 `UAssetManager` 경유로 처리한다.

**R7. PIE Hot-reload vs Cooked 빌드**
- **PIE**: DataTable에 row 추가 / DataAsset 필드 변경 시 `UDataPipelineSubsystem::
  RefreshCatalog()` 수동 호출로 즉시 반영. Hot-swap *보장은 약함* — PIE 재시작이
  안전.
- **Cooked**: 카탈로그가 패키지 시점에 고정. 런타임 변경 없음. 패치 시 디자이너가
  데이터-only 변경을 한 경우, USaveGame과의 schema 호환은 Save/Load Persistence (#3)
  소관 (Pipeline은 데이터 형태만 보장).

**R8. 디자이너 콘텐츠 추가 워크플로우**
① `Content/Data/Cards/DT_Cards`에 새 row 추가 (또는 `Content/Data/Dreams/`에 새
`UDreamDataAsset` 생성) → ② 필수 필드 입력 (`CardId` GUID/FName, 태그 배열, FText
이름·설명, 아이콘 SoftPath) → ③ 저장 (`UEditorValidator` 자동 검증) → ④ PIE 진입
또는 `RefreshCatalog()` 호출 → ⑤ `LogDataPipeline` 카테고리에서 카탈로그 등록 로그
확인. 엔진 재빌드 불필요.

### Container Choices (rationale)

`docs/engine-reference/unreal/current-best-practices.md §7` 가이드에 따르고, 5.6 사실관계로
보강:

| 콘텐츠 | 컨테이너 | 근거 |
|--------|---------|------|
| 카드 (MVP 10 / Full 40) | `UDataTable` + `FGiftCardRow` USTRUCT | 행 기반·CSV 재임포트로 디자이너 편집 자연스러움. 단일 `.uasset` 패키징. Blueprint 직접 접근. |
| 꿈 (MVP 5 / Full 50) | `UPrimaryDataAsset` per dream | 트리거 조건 + FText 본문이 한 자산에 응집. `UAssetManager`가 type-bulk 로드. 디자이너 편집 단위 = 자산 단위. |
| 최종 형태 (MVP 1 / Full 8) | `UDataTable` + `FFinalFormRow` USTRUCT | 8행 규모, 태그 벡터 → 형태 매핑이 행 조회에 자연. |
| Stillness Budget 튜닝값 | `UPrimaryDataAsset` (single instance) | 단일 자산. 외부 노출 필요한 모든 limit·threshold 보유. |

**의도적 미사용**: `UDataRegistrySubsystem` — 5.6 production 권장도가 검증되지 않음
(LLM 학습 경계, **MEDIUM risk**). 총 100행 이하 규모에서 DataRegistry의 런타임
스왑·스트리밍 이점은 불필요. ADR-후보-1에서 재검토.

### States and Transitions

| 상태 | 진입 조건 | 허용 동작 | 다음 상태 |
|------|----------|----------|----------|
| **Uninitialized** | Subsystem 생성 직후 (Init 진입 전) | 없음 (조회 호출 = assert 실패) | Loading |
| **Loading** | `Initialize()` 진입 | 카탈로그 등록 진행 중 | Ready / DegradedFallback |
| **Ready** | 4개 카탈로그 모두 등록 성공 | 모든 조회 API 허용 | (앱 종료까지 유지) |
| **DegradedFallback** | 한 카탈로그라도 등록 실패 | 성공 카탈로그만 조회 허용; 실패 카탈로그 조회 시 `nullptr`/`{}`. **재시도 없음** | (앱 종료까지 유지) |

**전환 정책**: DegradedFallback에서 자동 재시도 없음. MVP 규모(≤100행)에서 등록 실패는
*에디터 설정 오류*이므로 재시도보다 명확한 에러 로그 (`UE_LOG(LogDataPipeline, Error,
...)`)가 우선. 사용자에게 보이는 행동은 R3 fail-close에 위임.

### Interactions with Other Systems

| Downstream | API | Trigger | 데이터 owner |
|-----------|-----|---------|------------|
| **Card System (#8)** | `Pipeline.GetCardRow(FName CardId) → TOptional<FGiftCardRow>` | Card System이 일일 카드 풀 갱신 시 pull | Card GDD가 row 필드 정의 (이 GDD는 C1 가드만) |
| **Card System (#8)** | `Pipeline.GetAllCardIds() → TArray<FName>` | 시즌 가중 랜덤 선택 시 pull | Pipeline (단순 카탈로그 키 조회) |
| **Dream Trigger (#9)** | `Pipeline.GetAllDreamAssets() → TArray<UDreamDataAsset*>` | `FOnDayAdvanced` 수신 후 트리거 평가 직전 pull | Dream GDD가 trigger 필드 정의 (이 GDD는 C2 가드만) |
| **Growth Accumulation (#7)** | `Pipeline.GetGrowthFormRow(FName FormId) → TOptional<FFinalFormRow>` | 21일차 (또는 MVP 7일차) 최종 형태 결정 시 pull | Growth GDD가 row 필드 정의 |
| **Moss Baby Character (#6)** | `Pipeline.GetCardIconPath(FName CardId) → FSoftObjectPath` | 카드 건넴 애니메이션 직전 (텍스처 비동기 로드는 호출자 책임) | Card row의 SoftPath 위임 |
| **Stillness Budget (#10)** | `Pipeline.GetStillnessBudgetAsset() → UStillnessBudgetAsset*` | Subsystem Initialize 시 1회 pull, 캐싱 | Stillness Budget GDD |
| **Dream Journal UI (#13)** | `Pipeline.GetDreamBodyText(FName DreamId) → FText` | UI가 일지 페이지 열람 시 on-demand pull | Dream DataAsset의 FText 위임 |

**비-인터랙션 (이 시스템이 *하지 않는* 것)**:
- Pipeline은 어떤 downstream에도 *push* 하지 않는다 (이벤트 발행 없음). 모두 *pull*.
- Save/Load Persistence (#3)와 직접 통신하지 않는다 — Pipeline의 데이터는 read-mostly,
  Save/Load는 mutable runtime state. 두 시스템은 schema 버전을 통해 *간접적으로*만 합의
  (R7 참조).

### 안티패턴 차단 (디자이너·미래 에이전트가 시도할 가능성)

1. **`FStringAssetReference` 직접 사용** → 구식 API. `FSoftObjectPath` /
   `TSoftObjectPtr<T>` 사용.
2. **`LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/..."))` 런타임 직접 호출** →
   하드코드 경로 + 동기 강제 로드. R2 카탈로그 등록 경유로 대체.
3. **`FindObject<>` 런타임 조회** → 메모리 미상주 자산은 nullptr를 조용히 반환. R6
   금지.
4. **카드 row에 `int32 GrowthBonus` 류 필드 추가** → C1 위반. R4 schema 강제로 차단.
5. **Dream 코드 내 `if (TagVector.Spring >= 0.6f)` 같은 리터럴 임계** → C2 위반. 임계는
   반드시 `UDreamDataAsset` 필드.

### 잠재적 위험

- **위험 1 — fail-open 시 Pillar 2 침묵 위반**: Pipeline이 default `FGiftCardRow`를
  fallback으로 내보내면 빈 카드가 손에 도달. R3 fail-close로 차단됨.
- **위험 2 — DegradedFallback + 꿈 자산 누락**: Dream 카탈로그 등록 실패 시 21일 전체
  꿈 미발화 → Pillar 3 위반 아님(희소성 과잉)이지만 Game Concept 의도 훼손. 에디터
  단계에서 `LogDataPipeline Error`로 적발 필수. (→ OQ-2)
- **위험 3 — DataRegistry 5.6 변경 가능성**: 미사용 권고지만 ADR-후보-1에서 재평가
  필요. 만약 5.6에서 DataTable의 cooking 동작이 변했다면 본 GDD R7 수정 필요. (→ OQ-3)

## Formulas

전통적 balance 공식은 없다 (Foundation/infrastructure). 대신 *인프라 budget 공식* 3개를
명시한다 — Card / Dream / Growth GDD가 자기 콘텐츠를 추가할 때 *역산*해야 할 한도.

### D1. `Pipeline.MemoryFootprintBytes`

런타임에 Pipeline이 점유하는 총 CPU-side 메모리 (텍스처 실체 제외).

`M = (N_card × S_card) + (N_dream × S_dream) + (N_form × S_form) + (N_path × S_path)`

| 변수 | 심볼 | 타입 | 범위 | 설명 |
|------|------|------|------|------|
| 카드 행 수 | N_card | int | 0–1000 | 로드된 `FGiftCardRow` 행 수 |
| 카드 평균 크기 | S_card | int (bytes) | 64–256 | 카드 1행 (struct + 메타) |
| 꿈 자산 수 | N_dream | int | 0–500 | `UDreamDataAsset` 인스턴스 수 |
| 꿈 평균 크기 | S_dream | int (bytes) | 512–2048 | FText 본문 + 트리거 메타 |
| 형태 수 | N_form | int | 0–50 | `FFinalFormRow` 행 수 |
| 형태 평균 크기 | S_form | int (bytes) | 128–512 | 형태 1개 |
| SoftPath 수 | N_path | int | = N_card | 텍스처 SoftPath (카드당 1개) |
| SoftPath 크기 | S_path | int (bytes) | 8–16 | 포인터만 (텍스처 자체 제외) |
| **Pipeline 총 점유** | M | int (bytes) | 0 – unbounded | CPU-side 카탈로그만 |

**Output range:** 하한 없음. 상한은 *텍스처 streaming pool을 제외한* CPU-side 카탈로그만.
텍스처 실체는 별도 streaming pool에서 차감 (→ OQ-D-1).

**실계산 예시 (1.5 GB ceiling 대비):**

| 시점 | N_card | N_dream | N_form | M 추정 | ceiling 대비 |
|------|--------|---------|--------|--------|------------|
| MVP | 10 | 5 | 1 | ≈ 12 KB | < 0.001 % |
| VS | 20 | 15 | 4 | ≈ 40 KB | < 0.003 % |
| Full | 40 | 50 | 8 | ≈ 120 KB | < 0.01 % |

**결론**: Pipeline 카탈로그 자체의 CPU 메모리 부담은 **무시 가능 수준**. 1.5 GB 압박은
Lumen + 텍스처 streaming pool에서 온다.

**C1·C2 contract 연결**: 공식 1 위반 (N_card × S_card 급증)은 R2 카탈로그 4개 등록 시
GameInstance::Init() 스택·heap 압박으로 이어짐.

---

### D2. `Pipeline.InitializationTimeBudgetMs`

`Initialize()` 시작 → Ready 도달까지 허용 시간.

`T_init = T_base + (N_card × C_row) + (N_dream × C_asset) + (N_form × C_asset)`

| 변수 | 심볼 | 타입 | 범위 | 설명 |
|------|------|------|------|------|
| 기본 오버헤드 | T_base | float (ms) | 1–5 | 테이블 핸들 취득·등록 |
| 카드 행 수 | N_card | int | 0–1000 | DataTable 행 |
| 행 파싱 비용 | C_row | float (ms) | 0.005–0.02 | DataTable 행 1개 동기 파싱 |
| 꿈 자산 수 | N_dream | int | 0–500 | `UPrimaryDataAsset` |
| 형태 수 | N_form | int | 0–50 | `FFinalFormRow` |
| 자산 로드 비용 | C_asset | float (ms) | 0.05–0.2 | PrimaryDataAsset 동기 로드 1개 |
| **총 초기화 시간** | T_init | float (ms) | 0 – unbounded | Init→Ready |

**Output range / budget**: 첫 프레임 렌더 전 완료 조건으로 역산. 창 표시 전 허용 약 **3–5
프레임** (16.6 ms × 3 ≈ 50 ms). UE 5.6 권고: GameInstance::Init() 동기 블로킹 **< 50 ms**
(초과 시 Async Bundle 전환 권장).

**실계산 예시:**

| 시점 | 계산 | T_init |
|------|------|--------|
| MVP | 2 + (10 × 0.01) + (6 × 0.1) | ≈ 2.7 ms |
| Full | 2 + (40 × 0.01) + (58 × 0.1) | ≈ 8.2 ms |

**결론**: Full Vision **SSD warm-load** 기준에서 50 ms budget 이내. 동기 일괄 로드 유지 가능.

**R2 BLOCKER 8 — HDD cold-load 시나리오 명시**: 위 계산은 SSD warm-load 전제 (C_asset ≈
0.05–0.2ms). **HDD cold-start** 사용자에서 C_asset은 **0.5–2.0ms**로 증가 (디스크 seek +
OS 파일 캐시 미적중):

| 시점 | 계산 (HDD cold) | T_init |
|------|-----------------|--------|
| MVP | 2 + (10 × 0.02) + (6 × 1.0) | ≈ 8.2 ms |
| Full | 2 + (40 × 0.02) + (58 × 1.0) | ≈ 60.8 ms |

Full Vision HDD cold-start에서 T_init ≈ 61ms로 **50ms budget 초과 가능**. 이 경우
E12의 3-단계 임계에서 Warning (≤ 1.05× = 52.5ms) 또는 Error (≤ 1.5× = 75ms) 발화.
**정책**: 50ms budget 유지 (SSD가 PC 게이머 대다수 환경). HDD에서 budget 초과 시
Async Bundle 전환 검토 플래그를 ADR-후보-2에서 처리. splash screen 없이 blocking이므로
"응답 없음" 깜빡임은 200ms 초과 시에만 발생 — 61ms는 체감 불가.

**전체 메모리 합산 표 (1.5 GB ceiling 대비)**:

| 구성 요소 | 점유 추정 | 출처 |
|-----------|----------|------|
| UE 5.6 baseline (엔진 + 에디터 제외) | ~800 MB | UE docs 일반 추정 |
| Lumen HWRT BVH + Surface Cache | ~150 MB | `VERSION.md` 5.6 highlights |
| Save/Load peak (live + snapshot + buffer) | ~30 MB | Save/Load GDD §Tuning Knobs |
| **Data Pipeline 카탈로그 (Full)** | **~120 KB** | D1 실계산 |
| 텍스처 streaming pool (카드 아이콘 등) | ~100 MB | 추정 (OQ-D-1 미확정) |
| **합계** | **~1080 MB (72%)** | **여유 ~420 MB** |

Pipeline 카탈로그 기여는 0.01% — 무시 가능. 1.5 GB 압박은 Lumen + 텍스처에서 온다.

**C1·C2 contract 연결**: T_init > 50 ms이면 R1 "동기 일괄 로드" 전제가 깨짐 → Async
Bundle 전환 필요 → R3 "동기 조회" contract 재협상 필요.

---

### D3. `Pipeline.MaxCatalogSize` (카테고리별 권장 상한) (R2 — BLOCKER 7 naming + clamp)

```
N_max_cat   = floor(M_budget_cat / max(S_cat, 1))     — 메모리 기준 역산 (R2: 분모 0 클램프)
T_max_cat   = floor((T_budget − T_base) / max(C_cat, 0.001)) — 시간 기준 역산 (R2: 분모 0 클램프)
MaxCatalogSize = max(0, min(N_max_cat, T_max_cat))    — R2: 음수 underflow 클램프
```

> **R2 BLOCKER 7 naming 정정**: "안전 범위"(safe range)가 아닌 **"권장 상한"(recommended cap)**.
> 이 수치를 초과해도 즉시 실패하지 않으며, E12의 3-단계 임계(Warning/Error/Fatal)로 점진
> 경고한다. "안전 범위"는 초과 시 안전하지 않다는 오도적 의미를 내포.

| 변수 | 심볼 | 타입 | 범위 | 설명 |
|------|------|------|------|------|
| 카테고리 메모리 budget | M_budget_cat | int (bytes) | 설계 결정 | 카테고리 배분 메모리 |
| 항목 평균 크기 | S_cat | int (bytes) | D1 S 값 | 카테고리별 |
| 전체 시간 budget | T_budget | float (ms) | ≤ 50 | D2 권고 |
| 기본 오버헤드 | T_base | float (ms) | 1–5 | D2 동일 |
| 항목 로드 비용 | C_cat | float (ms) | D2 C 값 | 카테고리별 |
| **권장 상한** | MaxCatalogSize | int | 0 – unbounded | 두 제약의 하한 (R2 naming 정정) |

**카테고리별 실용 상한 (Full Vision 헤드룸 포함):**

| 카테고리 | Full Vision 콘텐츠 | 권장 상한 | 헤드룸 |
|---------|------------------|----------|--------|
| 카드 (DataTable 행) | 40 | **200** | 5× |
| 꿈 (DataAsset) | 50 | **150** | 3× |
| 형태 (DataAsset) | 8 | **30** | 3.75× |

헤드룸 기준: T_budget=50 ms 도달 전 여유분으로 산출. 상한 초과 시 자동 경고 → Async
Bundle 전환 검토 플래그 (DegradedFallback이 아닌 사전 경고).

**C1·C2 contract 연결**: MaxCatalogSize 초과는 R3 "null-safe·fail-close 동기 조회"
가정을 위협. 카탈로그 크기가 상한에 근접하면 fail-close 응답 시간이 늘어나며 첫 프레임
지연이 사용자에게 체감 단계로 진입.

---

**OQ-D-1**: 텍스처 streaming pool 할당 크기가 미확정. 카드 아이콘 텍스처(N_card개)의
동시 상주 크기 + Lumen GI 점유의 합산이 1.5 GB ceiling에서 차지하는 비율을 **Lumen Dusk
Scene GDD (#11)**에서 확정해야 D1의 "실질 압박" 수치가 구체화된다. 본 GDD는 **CPU-side
카탈로그만** 책임지며, 텍스처 실체는 streaming pool 소관.

## Edge Cases

각 항목은 *정확한 outcome*과 *책임 주체*를 명시. "handle gracefully" 표현 금지.

**E1. CardId 중복 행이 DataTable에 등록되면** (R2 — OQ-E-1 resolved): `UDataTable` 기본
동작은 마지막 행이 앞 행을 무음 덮어쓰기. **Pipeline 책임**: 등록 직후 row key 중복 검사.
**R2 정책 (`DuplicateCardIdPolicy = WarnOnly`)**: 첫 row 승리 규칙 — insertion order 기준
먼저 발견된 row가 유효, 나머지 동일 ID row 무시. `UE_LOG(LogDataPipeline, Warning,
TEXT("Duplicate CardId '%s' detected — first row wins, %d duplicate(s) ignored"), ...)` 발행.
카탈로그는 Ready 유지 (DegradedFallback 강등 없음 — Pillar 4 보호).

**E2. CardId / FormId가 `NAME_None`(빈 FName)이면**: `GetCardRow(NAME_None)`는 R3 fail-close에
따라 `nullptr` 반환. 그러나 빈 Tags 배열로 누적 진입하면 Growth가 "투명 카드"로 처리할 위험.
**Pipeline 책임**: 등록 시점 `UEditorValidator`로 NAME_None 행 거부; 런타임 진입부 `ensure()`.

**E3. Tags 배열이 빈 카드 행이 등록되면**: Pipeline 통과시키지만 Growth가 누적 기여 없는
"투명 카드"로 처리. **위임**: 빈 Tags를 schema 오류로 막을지 허용할지는 → **OQ-E-2** (Growth
GDD #7 소관). 본 GDD는 등록 단계에서 막지 않음 (decision deferred).

**E4. FText 필드의 String Table key가 존재하지 않으면**: `FText::FromStringTable()`이 key
미발견 시 `"?? key ??"`를 *에러 없이* 반환 → 화면에 깨진 텍스트 노출. Pipeline은 런타임
로컬라이제이션 검증 불가능. **디자이너·빌드 책임**: Cook Warning + `/localize validate` 체크.
→ **OQ-E-3**: Pipeline이 `FText.IsEmpty()` 수준 최소 가드 추가 여부.

**E5. Initialize 완료 전 downstream이 `GetCardRow`를 호출하면**: Loading/Uninitialized 상태에서
조회 진입은 `checkf` assert. **Pipeline 책임**: `IsReady() → bool` 공개 노출. **Downstream
책임**: 조회 전 `IsReady()` 확인. 보장 수단이 없으면 race. → **OQ-E-4**: 5.6에서 GameInstance::Init
순서가 보장되지 않는 예외 플랫폼 존재 여부 확인 필요 (Windows 단일 타깃이지만 보강 필요).

**E6. `RefreshCatalog()`가 Loading 상태에서 재진입 호출되면**: 등록 순서(R2) 깨짐 또는 부분
ContentRegistry 노출. **Pipeline 책임**: 진입부 "Loading 중이면 즉시 return + Warning" guard
필수. 중첩 PIE 재시작 시나리오에서 발생 확률 높음.

**E7. PIE에서 row 삭제 후 `RefreshCatalog()`, downstream이 옛 CardId를 캐시 보유하면**:
downstream의 캐시된 FName 리스트가 사라진 row를 참조 → 다음 `GetCardRow()`에서 nullptr →
fail-close 미테스트 경로면 assert. **Downstream 책임**: 캐시된 CardId는 사용 직전 재조회.
R7가 "PIE 재시작 권장"의 이유로 본 항목을 명시.

**E8. Cook 단계에서 `FSoftObjectPath` 대상 텍스처가 누락되면**: Cook 통과, 런타임에
`UAssetManager::LoadAsset()` nullptr. R6 + R3로 null-safe는 유지되지만 카드 아이콘이 빈
채로 노출. **디자이너·빌드 책임**: Cook Warning 모니터링 + `UE_ASSET_DEPENDENCY_CHECK`
탐색. → **OQ-E-5**: UE 5.6 Cook 파이프라인이 FSoftObjectPath 누락을 Warning → Error로
격상하는 옵션 존재 여부.

**E9. C1 `UEditorValidatorBase`가 실수로 비활성화/Editor-only 컴파일에서 제외되면**: 수치
stat 필드 PR이 Validator를 우회. **Pipeline 책임**: Validator 클래스를 별도 Editor Module로
격리, CI에서 `-editor` 빌드 컴파일 테스트 포함. → **OQ-E-6**: DataTable 저장 외 CI에서도
Validator를 실행할 수 있는 5.6 자동화 훅 존재 여부.

**E10. Card 카탈로그만 DegradedFallback이고 꿈·형태는 Ready인 채로 세션 시작** (R2 —
OQ-E-7 resolved): **R2 정책: pending day + 다음 앱 복구**. game-concept.md "메마름 → 첫
카드 복구" 패턴 차용.
- **당일 동작**: Card System이 카드 풀 요청 시 빈 결과 → 해당 day 카드 건넴 skip
  (Offering → Waiting 직행). 에러 다이얼로그 없음 (Pillar 1).
- **다음 앱 시작**: Pipeline이 카드 카탈로그 재시도. 수정 시 Ready 복원 → 정상 진행.
- **21일 전체 skip 시**: Growth "무입력" 형태 수렴, 게임은 종료됨 (Pillar 4 유지).
- **Pipeline 책임**: `GetFailedCatalogs() → TArray<FName>` API로 실패 카탈로그 노출.

**E11. 패치 후 디자이너가 CardId rename, 기존 Save에 옛 CardId가 저장되어 있으면**: Save에는
"mossy_dawn_v1", Pipeline에는 "mossy_dawn"만 존재. `GetCardRow("mossy_dawn_v1") → nullptr`.
fail-close가 무음 처리하지만 저장된 History(어떤 카드를 건넸는지)가 깨짐. **Save/Load 책임**:
SaveSchemaVersion bump + CardId 마이그레이션 매핑. **Pipeline 책임**: "알 수 없는 CardId"
조회를 별도 Warning 로그로 구분(단순 nullptr와 구분). → **OQ-E-8**: CardId를 SaveSchemaVersion에
묶어 rename 금지 정책 강제 vs Pipeline에 alias 테이블.

**E12. MaxCatalogSize를 5% / 50% / 200% 초과할 때 점진적 행동**:
- **5% 초과**: T_init이 budget(50ms) 미달, `Warning` 로그만, 정상 동작.
- **50% 초과**: T_init이 budget 근접·초과, 첫 프레임 지연 체감 가능, Async Bundle 전환 검토
  `Warning` 발행.
- **200% 초과**: T_init >> 50ms, GameInstance::Init 블로킹 1초 이상.
- **Pipeline 책임**: 3단계 (`5%` / `floor(budget * 0.8)ms` / `>= budget`) 기준으로 Warning /
  Error / FatalError 로그 레벨 분기. → **OQ-E-9**: 200% 초과를 런타임 에러로 강등할 배수
  기준값을 Tuning Knob에 노출할지 여부.

---

**Surface된 OQ 요약** (Section L Open Questions에서 정리):
| OQ-E-# | 질문 | 책임 GDD |
|--------|------|---------|
| 1 | 중복 CardId 발견 시 DegradedFallback 강등 vs 경고만 | 본 GDD (정책 결정) |
| 2 | 빈 Tags 카드 schema 차단 vs 허용 | Growth GDD #7 |
| 3 | Pipeline이 FText.IsEmpty() 가드 추가 여부 | 본 GDD (정책 결정) |
| 4 | 5.6에서 GameInstance::Init 순서 보장 예외 플랫폼 | 본 GDD (검증 task) |
| 5 | Cook FSoftObjectPath 누락을 Error로 격상 옵션 | 본 GDD (검증 task) |
| 6 | DataTable 저장 외 CI Validator 자동화 훅 | 본 GDD (검증 task) |
| 7 | Card 단독 실패 시 게임 지속 vs Fatal | game-designer 판단 |
| 8 | CardId rename 정책 (금지 vs alias) | Save/Load + Card GDD 협의 |
| 9 | MaxCatalogSize 200% 초과 배수를 Tuning Knob 노출 | 본 GDD (정책 결정) |

## Dependencies

### Upstream (이 시스템이 의존하는 시스템)

**없음.** Data Pipeline은 Foundation 레이어 최하단. UE 5.6 엔진 자체 외에는 다른 게임
시스템에 의존하지 않는다. `UGameInstance::Init()` 진입점은 엔진 라이프사이클이며,
`UAssetManager`·`UDataTable`·`UPrimaryDataAsset`은 엔진 기본 클래스다.

### Downstream (이 시스템에 의존하는 시스템)

Section C "Interactions with Other Systems" 표가 정식 인터페이스 계약. 여기서는 의존
관계의 종류와 강도만 요약.

| # | System | 의존 강도 | 데이터 종류 | 인터페이스 |
|---|--------|----------|------------|----------|
| 6 | Moss Baby Character | **soft** | 카드 아이콘 SoftPath | `Pipeline.GetCardIconPath` |
| 7 | Growth Accumulation Engine | **hard** | FFinalFormRow + (간접) FGiftCardRow.Tags | `Pipeline.GetGrowthFormRow` |
| 8 | Card System | **hard** | FGiftCardRow 전체 | `Pipeline.GetCardRow`, `GetAllCardIds` |
| 9 | Dream Trigger System | **hard** | UDreamDataAsset 전체 | `Pipeline.GetAllDreamAssets` |
| 10 | Stillness Budget | **hard (NEW — 본 GDD에서 발견)** | UStillnessBudgetAsset 단일 인스턴스 | `Pipeline.GetStillnessBudgetAsset` |
| 13 | Dream Journal UI | **soft (NEW — 본 GDD에서 발견)** | Dream FText 본문 | `Pipeline.GetDreamBodyText` (Dream DataAsset 경유) |

**의존 강도 정의**:
- **hard**: 이 시스템 없이 작동 불가능. Pipeline DegradedFallback 시 해당 카탈로그가
  실패하면 시스템 핵심 기능이 죽는다.
- **soft**: 부분 기능 또는 시각 표현이 약화되지만 핵심 루프는 지속 가능.

### 우회 의존 (간접)

- **Time & Session System (#1)** ← `FOnDayAdvanced` delegate (Time GDD가 owner). Pipeline은
  *구독하지 않으나*, 본 GDD Section C Interactions가 "Dream Trigger가 FOnDayAdvanced 수신
  후 Pipeline.GetAllDreamAssets pull"이라는 시퀀스를 *기술*한다. 따라서 `FOnDayAdvanced`의
  registry `referenced_by`에 본 GDD 추가됨 (등록 완료).
- **Save/Load Persistence (#3)** ← 직접 통신 없음. 단, R7 (Cooked 패치 후 schema 호환)이
  `SaveSchemaVersion` 상수를 *언급*한다. registry `referenced_by` 추가됨.

### 양방향 정합성 (Bidirectional Consistency)

R5 정책 ("dependencies must be bidirectional") 점검 결과:

| 양방향 정합 상태 | 시스템 |
|-----------------|--------|
| ✅ 일치 (systems-index에 이미 명시) | #6, #7, #8, #9 — 모두 "depends on Data Pipeline" 기록 |
| ⚠️ **불일치 — systems-index 갱신 필요** | #10 Stillness Budget — 인덱스는 "depends on: 최소"라 기록, 본 GDD가 Pipeline 의존 추가 |
| ⚠️ **불일치 — systems-index 갱신 필요** | #13 Dream Journal UI — 인덱스는 "Dream Trigger, Save/Load, Stillness Budget"만 기록, 본 GDD가 Pipeline 직접 의존 추가 |

**조치**: GDD 완료 후 Phase 5d "Update Systems Index"에서 #10·#13 행에 "Data Pipeline" 의존
추가. **각 시스템의 GDD가 작성될 때 본 GDD를 referenced_by에 등록해야 한다.**

### 미래 외부 의존 가능성 (현재 없음)

- **Localization (`/localize`)**: FText 사용으로 내재적 의존이 있으나 Pipeline은 내용 검증을
  하지 않음 (E4 위임). `/localize validate`가 별도 파이프라인.
- **Steam Cloud Save**: 데스크톱 PC 단일 타깃. 향후 Steam 출시 시 Save/Load가 추가 책임.
  Pipeline은 영향 없음.

## Tuning Knobs

Data Pipeline은 Foundation 시스템이므로 *플레이어용* 튜닝 노브는 없다. 모든 노브는
**디자이너·엔지니어용 안전 가드**이며, 기본값은 D1·D2·D3 공식과 E1–E12 정책에서
*직접 유도*된다 (각 default rationale은 §G.8).

### G.1 Variable Taxonomy — D 공식 변수 중 *진짜 knob*만 추출

D1 (8 변수), D2 (6 변수), D3 (5 변수) 중 대부분은 *측정값*(N_*) 또는 *컴파일-타임
상수*(S_*) — knob이 아니다. 진짜 runtime-tunable는 **상한 4개 + 정책 5개**.

| 공식 변수 출처 | knob 이름 | 종류 |
|--------------|-----------|------|
| D2 T_budget | `MaxInitTimeBudgetMs` | numeric |
| D3 N_max_card | `MaxCatalogSizeCards` | numeric |
| D3 N_max_dream | `MaxCatalogSizeDreams` | numeric |
| D3 N_max_form | `MaxCatalogSizeForms` | numeric |

N_card / N_dream / N_form은 콘텐츠가 결정하는 *측정값* — 이를 직접 knob화하지 않고
"상한"을 knob화한다. S_card / S_dream / S_form은 USTRUCT 레이아웃에 묶여 있어
빌드-타임 상수다 (코드 변경 없이 바뀌지 않음).

### G.2 카테고리 A — Performance Budget Knobs (P0)

이미 registry 등록 완료. 본 섹션은 사용·범위·재시작 정책만 명시.

| Knob | 타입 | Default | 안전 범위 | 노출 위치 | 재시작 |
|------|------|---------|----------|---------|--------|
| `MaxInitTimeBudgetMs` | float (ms) | **50** | [10, 200] | `DefaultGame.ini` `[DataPipeline]` (UPROPERTY Config) | Yes (Init 1회 read) |
| `MaxCatalogSizeCards` | int32 | **200** | [10, 200]† | `DefaultGame.ini` `[DataPipeline]` | Yes |
| `MaxCatalogSizeDreams` | int32 | **150** | [5, 150]† | `DefaultGame.ini` `[DataPipeline]` | Yes |
| `MaxCatalogSizeForms` | int32 | **30** | [1, 30]† | `DefaultGame.ini` `[DataPipeline]` | Yes |

† 상한 *값* 자체가 권장 상한. 디자이너가 더 큰 값으로 올릴 수도 있으나 D3 식 재계산
및 ADR 검토 필요 (G.6 interaction (a) 참조).

### G.3 카테고리 B — Behavior Policy Knobs (P0)

| Knob | 타입 | Default | 노출 위치 | 재시작 | 해소 OQ |
|------|------|---------|---------|--------|--------|
| `DuplicateCardIdPolicy` | enum `{WarnOnly, DegradedFallback}` | **WarnOnly** (R2 변경) | `DefaultGame.ini` | Yes | OQ-E-1 |
| `bFTextEmptyGuard` | bool | **true** | `DefaultGame.ini` | Yes | OQ-E-3 |
| `bSoftRefMissingError` | bool (Cook/Editor only) | **false** | `DefaultGame.ini` (Editor/Cook 전용, Shipping 무시) | N/A | OQ-E-5 (검증 후 true 가능) |

**`DuplicateCardIdPolicy = WarnOnly`** (R2 변경 — R1 BLOCKER 2 해소) — 중복 CardId
발견 시 **첫 row 승리 규칙** 적용 + `UE_LOG(LogDataPipeline, Warning, ...)` 발행.
DegradedFallback은 디자이너 실수 1건으로 카드 카탈로그 전체 무력화 → 21일 무입력 끝
→ **Pillar 4 ("끝이 있다") 직접 위반**이었음. WarnOnly + 첫 row 승리는 결정적(deterministic)
동작을 보장하면서 카탈로그를 살린다. 중복 발생은 디자이너에게 Warning으로 알림.
**"첫 row 승리"**: `UDataTable`의 행 순회 순서(insertion order)에서 먼저 발견된 row가
유효 — 나머지 동일 ID row는 무시. 이 순서는 DataTable 파일 직렬화 순서와 일치.

**`bFTextEmptyGuard = true`** — 등록 시점에 `FText.IsEmpty()` 카드를 거부 (행 단위 fail,
카탈로그 전체 fail 아님). 빈 카드가 플레이어 손에 닿는 것은 Pillar 2 직접 위반. 가드
비용은 D2 T_init에 미미 (N_card=200 × 0.001ms ≈ 0.2ms).

**`bSoftRefMissingError = false`** (현재) — UE 5.6 기본 동작 유지. OQ-E-5 검증
완료 시 true 권장 (Cook 단계에서 Error로 격상 = 런타임 nullptr 경로 자체 차단).

### G.4 카테고리 C — Threshold Scaling Knobs (P1)

E12의 3-단계 임계 (5% / 50% / 200% 초과)를 knob으로 노출. **단조성 강제 필수** (G.6
interaction (f)).

| Knob | 타입 | Default | 안전 범위 | 의미 |
|------|------|---------|----------|------|
| `CatalogOverflowWarnPercent` | float | **1.05** | [1.0, 1.5] | 권장 상한 × N% 초과 시 `Warning` 로그 |
| `CatalogOverflowErrorPercent` | float | **1.5** | [1.1, 2.0] | 권장 상한 × N% 초과 시 `Error` 로그 (D2 budget 80% 도달 근사) |
| `CatalogOverflowFatalMultiplier` | float | **2.0** | [1.5, 5.0] | 권장 상한 × N배 초과 시 `Fatal` (Init 1초 블로킹 임계) — OQ-E-9 해소 |

### G.5 카테고리 D — Cook/Build Pipeline Flags (P2)

| Knob | 타입 | Default | 노출 위치 |
|------|------|---------|---------|
| `bRunValidatorOnCook` | bool | **true** | `DefaultEditorPerProjectUserSettings.ini` 또는 Project Settings → Editor Validators |
| `bRunAssetValidationOnCook` | bool | **true** | `DefaultGame.ini` (Cook commandlet `-RunAssetValidation`) |

### G.6 Knob Interaction Matrix

| 페어 | 상호작용 패턴 | 본 GDD의 운영 결정 |
|------|--------------|-------------------|
| (a) `MaxInitTimeBudgetMs` ↔ `MaxCatalogSize*` | 둘 다 D3 식의 양변. 한쪽 변경이 다른 쪽의 *수학적 상한*을 바꿈 | **자동 재계산 금지**. 두 knob은 독립 운영. 변경 시 D3 식을 수동 재계산 + ADR 검토 필요. 콘텐츠 팀 contract가 자동으로 흔들리지 않도록. |
| (b) `DuplicateCardIdPolicy` ↔ R3 fail-close | R2: `WarnOnly` default — 중복 row는 Warning + 첫 row 승리로 결정적 처리. fail-close는 *존재하지 않는 ID 조회* 시에만 발동 | `WarnOnly` = Pillar 4 보호 (카탈로그 생존) + deterministic. `DegradedFallback`은 Pillar 4 직접 위반이었음 (R1 BLOCKER 2). |
| (c) `bRunValidatorOnCook` ↔ C1 schema guard | C1은 3층 (USTRUCT 필드 omission / Validator 저장 시 / 코드리뷰). Validator 비활성화 시 층 (2) 사라짐 | C1 contract는 컴파일 타임 층 (1)이 *기본 방어*. Validator는 *추가 안전망*. Validator off + USTRUCT에 새 float 추가 PR이 동시 발생하면 두 층 동시 뚫림 — agent rule (`.claude/rules/`)이 셋째 층 |
| (d) `bSoftRefMissingError` ↔ E8 런타임 nullptr | true 시 Cook Error → 패키지 미생성 → 런타임 nullptr 경로 자체 차단 | **R3 강화** (약화 아님). 두 메커니즘 상호 보완. |
| (e) `bFTextEmptyGuard` ↔ E4 silent fallback | true 시 빈 FText 카드는 *행 단위* 거부 (카탈로그 단위 fail 아님). 카탈로그 전체는 정상 등록. | "?? key ??" 텍스트가 손에 닿지 않도록 차단하면서 다른 카드 영향 없음 |
| (f) `Warn% ≤ Error% ≤ Fatal×` 단조성 | 위반 시 Fatal이 Warning보다 먼저 발화 → 경고 없이 게임 죽음 | **Initialize() 진입부에서 `checkf` 또는 `if+UE_LOG Fatal` 강제** (G.7 R2). 단조성 위반 = 카탈로그 등록 미진입. Shipping 빌드 포함 (R3 BLOCKER 1 해소) |

### G.7 Initialize() Validation Contract (R2 — Shipping-safe 재설계)

`UDataPipelineSubsystem::Initialize()` 진입 직후, 카탈로그 등록 *전에* 다음 정합성 검증
조항을 *behavior 수준에서* 보장 (구체 코드 형태는 ADR 소관):

- `MaxInitTimeBudgetMs ∈ [10, 200]` — 안전 범위 검증
- `MaxCatalogSizeCards ∈ [10, 1000]` (확장 한계 1000 = G.9 I/O 병목 임계 직전)
- `MaxCatalogSizeDreams ∈ [5, 500]`, `MaxCatalogSizeForms ∈ [1, 50]`
- `CatalogOverflowWarnPercent ≤ CatalogOverflowErrorPercent ≤ CatalogOverflowFatalMultiplier`
  (단조성 — 위반 시 Fatal 우선 발화 차단)

**R2 BLOCKER 1 — `ensureMsgf` Shipping strip 대응**: `ensure*` 매크로는 UE 5.6
Shipping 빌드에서 **strip 확정** — 출시 빌드에서 검증이 무효화됨. R2에서 메커니즘 변경:

- **Development/Editor 빌드**: `checkf()` 사용 (Shipping에서도 잔존하나 crash 유발).
- **Shipping 빌드**: 명시적 `if (!Condition) { UE_LOG(LogDataPipeline, Fatal, TEXT("...")); return; }` 패턴. `UE_LOG Fatal`은 Shipping에서도 strip되지 않으며 `FDebug::AssertFailed()`를 호출해 crash report 생성.
- 구현 선택: `checkf`와 `if+Fatal` 중 하나를 ADR에서 확정. GDD 수준에서는 **"Shipping 빌드 포함 모든 빌드에서 knob 정합성 위반 시 카탈로그 등록에 진입하지 않음"**이 행동 계약.

### G.8 Default 값 근거 (Rationale)

각 default는 D 공식·E 정책에서 *유도*된 값. "관행적 50" 금지.

| Knob | Default | 유도 근거 |
|------|---------|---------|
| `MaxInitTimeBudgetMs` | 50 ms | D2 Full ≈ 8.2 ms 추정. 16.6 ms × 3프레임 = 49.8 ms. splash 체감 없는 동기 블로킹 최대 |
| `MaxCatalogSizeCards` | 200 | Full 40의 5× 콘텐츠 헤드룸. D3 시간 한계는 4800이지만 콘텐츠 정책 기준이 binding |
| `MaxCatalogSizeDreams` | 150 | Full 50의 3×. D3 시간 한계 480보다 작아 콘텐츠 기준이 binding |
| `MaxCatalogSizeForms` | 30 | Full 8의 3.75×. D3 시간 한계 >>30, 콘텐츠 기준이 binding |
| `DuplicateCardIdPolicy` | WarnOnly (R2) | R1 BLOCKER 2: DegradedFallback은 Pillar 4 위반. WarnOnly + 첫 row 승리 = deterministic + 카탈로그 생존 |
| `bFTextEmptyGuard` | true | "?? key ??" 카드가 Pillar 2 직접 위반. 가드 비용 ≈ 0.2 ms (무시 가능) |
| `bSoftRefMissingError` | false | UE 5.6 기본. OQ-E-5 검증 후 true 권장 |
| `CatalogOverflowWarnPercent` | 1.05 | 5% = Full 카드 2행 해당. 노이즈 없이 추세 신호 |
| `CatalogOverflowErrorPercent` | 1.50 | D2 T_init budget 80% 도달 근사 |
| `CatalogOverflowFatalMultiplier` | 2.00 | E12 "Init 블로킹 1초 이상" 임계 직접 유도 |

### G.9 "너무 높으면 / 너무 낮으면" — 측정 가능한 outcome

대표 knob 4개의 양 극단 행동 (모두 측정 가능):

**`MaxInitTimeBudgetMs`**:
- < 10 ms: Full 8.2 ms 마진 없음. 프로파일 편차로 PIE에서 즉시 assert.
- > 200 ms: 약 12프레임(60fps) 동기 블로킹. Windows 태스크바 "응답 없음" 깜빡임. Splash
  없으면 freeze 체감.

**`MaxCatalogSizeCards`**:
- < 40: Full Vision 콘텐츠 자체 차단. 후반부 발견 시 파이프라인 재설계.
- > 1000: T_init 약 17ms (안전), but UDataTable CSV import 시 에디터 5초+ 소요 — *I/O
  병목*이 진짜 위험.

**`DuplicateCardIdPolicy = WarnOnly`**: 디자이너가 행 순서 바꾸면 게임 동작이 조용히
변경. 재현 불가 버그 경로.

**`CatalogOverflowFatalMultiplier = 5.0`**: 카드 1000행 시점에서야 Fatal 발화. 그 전에
비정상 규모 콘텐츠가 6개월간 누적될 수 있음 (경고 없이).

### G.10 *Knob이 아닌* 항목 (contract — Section C·다른 곳 소관)

다음은 GDD 리비전 + 모든 downstream GDD 재검토 없이 변경 금지. Tuning Knob 섹션의
유연성에 속하지 않는다.

| 항목 | 속하는 섹션 | 변경이 contract인 이유 |
|------|----------|---------------------|
| C1 USTRUCT 필드 omission | C R4 | 필드 추가 = DataTable schema 변경 = SaveSchemaVersion bump 필요 |
| R3 fail-close 동작 | C R3 | 모든 downstream 호출 패턴이 "nullptr = skip" 가정으로 작성됨 |
| R2 등록 순서 | C R2 | DegradedFallback의 "어느 카탈로그 실패" 시맨틱이 순서 의존 |
| R6 FSoftObjectPath 강제 | C R6 | Cook dependency 추적이 이 패턴 전제 |
| Async Bundle 자동 fallback | (잠재적 위험) | R3 동기 contract와 직접 충돌 |
| fail-open 토글 | (금지 정책) | Pillar 2 침묵 위반 경로를 설계에 내장하는 것 |

### G.11 본 Section G가 resolve한 OQ

- **OQ-E-1 (P0)** ✅ — `DuplicateCardIdPolicy = WarnOnly` 채택 (R2: DegradedFallback → WarnOnly + 첫 row 승리)
- **OQ-E-3 (P0)** ✅ — `bFTextEmptyGuard = true` 채택
- **OQ-E-9 (P1)** ✅ — `CatalogOverflowFatalMultiplier` knob 노출 + default 2.0

남은 OQ (E-2, E-4, E-5, E-6, E-7, E-8)는 Section L (Open Questions)에서 다른 시스템·검증
task로 위임.

### G.12 Knob 변경 시 절차 (운영 노트)

`MaxCatalogSize*`, `MaxInitTimeBudgetMs`, `Overflow*` knob 변경 시:
1. `design/registry/entities.yaml`의 해당 constant `value` + `revised:` 갱신
2. 변경 사유를 `notes`에 추가
3. `/consistency-check` — 다른 GDD가 의존하지 않는지 확인
4. (a)·(b)·(c)·(f) interaction 페어를 수동 재검토
5. 영향받는 downstream GDD (#7, #8, #9, #10) 변경 통지

## Visual/Audio Requirements

**해당 없음.** Data Pipeline은 Foundation/infrastructure 레이어이며 시각·청각 출력
요소가 없다. 본 시스템이 제공하는 콘텐츠(카드 아이콘 텍스처, 꿈 텍스트, 형태 메시)의
*시각·청각 표현*은 다음 시스템들의 책임이다:

- 카드 아이콘 표현: **Card Hand UI (#12)** + **art-bible §6 Asset Style**
- 꿈 텍스트 표현: **Dream Journal UI (#13)** (typography·페이지 전환)
- 최종 형태 시각: **Moss Baby Character (#6)** + **Lumen Dusk Scene (#11)**
- 카드 건넴 SFX: **Audio System (#15)** (Vertical Slice)

Pipeline은 *데이터 그릇*에 머문다. 시각·청각 자산은 `FSoftObjectPath`로 *참조*만 하며,
해당 자산의 외관 사양은 위 시스템의 GDD가 정의.

## UI Requirements

**해당 없음.** Data Pipeline은 플레이어와 직접 상호작용하지 않으며 위젯·HUD 요소를
가지지 않는다. *디자이너용* 에디터 UX (R8 콘텐츠 추가 워크플로우)는 UE 에디터의 기본
DataTable·DataAsset 인스펙터를 사용하며 별도 커스텀 위젯이 필요 없다.

Pipeline 상태 디버그 표시 (Ready/DegradedFallback)가 개발 중 필요해질 경우, 별도
`UDataPipelineDebugWidget` 등의 *개발자용* 위젯을 만드는 것은 본 GDD scope 외 — Tools
Programmer가 별도 결정.

## Acceptance Criteria

총 **18개 AC** (P0 14 / P1 3 / P2 1). BLOCKING 16 / ADVISORY 2. Smoke set 7개. Gap 6개
(런타임 AC 불가 영역 — 대안 가드 명시). Cross-system contract 7개 (downstream GDD가 자기
측 AC 추가 의무).

### H.1 Coverage Matrix (요약)

| 영역 | AC 매핑 | Gate |
|------|---------|------|
| R1 초기화 진입점 | AC-DP-01 | BLOCKING |
| R2 등록 순서 / DegradedFallback | AC-DP-02, AC-DP-03 | BLOCKING |
| R3 fail-close (Card / Dream / Growth) | AC-DP-04, AC-DP-05 | BLOCKING |
| R4 C1 schema 가드 (Validator) | AC-DP-06 [5.6-VERIFY] | BLOCKING |
| R4 C1 USTRUCT 필드 omission (compile) | — Gap-1 | (코드 가드) |
| R5 C2 Dream 임계 hardcode 금지 | — Gap-2 | (코드리뷰) |
| R6 안티패턴 차단 | — Gap-3 | (코드리뷰 + linter) |
| R7 PIE hot-swap | AC-DP-07 [5.6-VERIFY] | BLOCKING |
| R8 디자이너 워크플로우 | — Gap-4 | (UX 검증) |
| D1 메모리 footprint | AC-DP-08 | ADVISORY |
| D2 T_init budget | AC-DP-09 [5.6-VERIFY] | BLOCKING |
| D3 MaxCatalogSize 3-단계 임계 | AC-DP-10 [5.6-VERIFY] | BLOCKING |
| E1 + G.3 DuplicateCardIdPolicy | AC-DP-11 | BLOCKING |
| E4 + G.3 bFTextEmptyGuard | AC-DP-12 | BLOCKING |
| E5 pre-init 조회 차단 | AC-DP-13 | BLOCKING |
| E6 RefreshCatalog 재진입 | AC-DP-14 | BLOCKING |
| E8 Cook FSoftRef 누락 | — Gap-5 [5.6-VERIFY] | (Cook 모니터링) |
| E10 GetFailedCatalogs API | AC-DP-15 | ADVISORY |
| E11 CardId rename 구분 로그 | AC-DP-16 | BLOCKING |
| G.7 단조성 Shipping-safe guard | AC-DP-17 [5.6-VERIFY] | BLOCKING |
| R2 BLOCKER 4 Cooked PrimaryAssetType | AC-DP-18 [5.6-VERIFY] (R3 NEW) | BLOCKING |
| Pillar 정성적 보호 | — Gap-6 | (플레이테스트) |

### H.2 핵심 AC (17개, Given-When-Then)

각 AC는 분류 (Logic/Integration/Visual/UI/Config-Data) · Gate · 증거 경로 · Headless 가능
여부 · 우선순위 (P0/P1/P2)를 메타데이터로 포함. 본 GDD는 명세, 실제 코드 위치는 ADR 또는
test plan 소관.

#### AC-DP-01 — R1 초기화 진입점 [Integration · BLOCKING · P0]
> **GIVEN** `UGameInstance::Init()` 호출 시점에 어떤 GameMode·Actor도 아직 존재하지 않는
> 상태에서, **WHEN** `UDataPipelineSubsystem::Initialize()`가 자동 호출되면, **THEN**
> `IsReady()` 또는 `GetState() == DegradedFallback`이 `Init()` 반환 전 확정되어야 하며
> 첫 World tick 전까지 상태 변경이 없어야 한다.
- 증거: `tests/integration/data-pipeline/initialization_order_test.cpp`
- Headless: Yes

#### AC-DP-02 — R2 카탈로그 등록 순서 [Logic · BLOCKING · P0]
> **GIVEN** 4개 카탈로그 Mock이 모두 정상 로드 가능한 상태에서, **WHEN** `Initialize()`가
> 실행되면, **THEN** 등록 로그 순서가 Card → FinalForm → Dream → Stillness여야 하고
> `IsReady()`가 true여야 한다.
- 증거: `tests/unit/data-pipeline/catalog_registration_order_test.cpp`
- Headless: Yes

#### AC-DP-03 — R2 DegradedFallback 진입 [Logic · BLOCKING · P0]
> **GIVEN** Card 카탈로그는 정상이고 FinalForm DT 로드가 실패하도록 설정된 상태에서,
> **WHEN** `Initialize()`가 실행되면, **THEN** 상태가 DegradedFallback이 되고 Dream·
> Stillness 등록이 시도되지 않아야 하며, `LogDataPipeline Error` 메시지가 정확히 1회
> 발행되어야 한다.
- 증거: `tests/unit/data-pipeline/degraded_fallback_transition_test.cpp`
- Headless: Yes

#### AC-DP-04 — R3 fail-close: Card / NAME_None / 삭제된 ID [Logic · BLOCKING · P0]
> **GIVEN** Pipeline이 Ready 상태이고 존재하지 않는 CardId, `NAME_None`, 삭제 후의 옛
> CardId를 조회하는 경우에서, **WHEN** `GetCardRow()`를 각각 호출하면, **THEN** 모든
> 경우 `TOptional<FGiftCardRow>`가 비어있는 채로 반환되어야 하며, default-constructed
> row가 절대 반환되어선 안 된다.
- 증거: `tests/unit/data-pipeline/fail_close_contract_test.cpp`
- Headless: Yes

#### AC-DP-05 — R3 fail-close: Dream / Growth 동일 계약 [Logic · BLOCKING · P0]
> **GIVEN** Dream 카탈로그가 DegradedFallback인 상태에서, **WHEN** `GetAllDreamAssets()`
> 및 `GetGrowthFormRow("nonexistent")`를 호출하면, **THEN** Dream은 빈 `TArray` 반환
> (배열에 nullptr 포함 금지), Growth는 `TOptional` 비어있는 값 반환.
- 증거: `tests/unit/data-pipeline/fail_close_contract_test.cpp`
- Headless: Yes

#### AC-DP-06 — R4 UEditorValidator 저장 시점 작동 [Integration · BLOCKING · P0 · 5.6-VERIFY]
> **GIVEN** Validator가 활성화된 상태에서 DataTable Row에 `int32 GrowthBonus = 10` 같은
> 수치 stat 필드를 저장 시도할 때, **WHEN** DataTable 에디터 저장 이벤트가 발생하면,
> **THEN** Validator가 `EDataValidationResult::Invalid`를 반환하고 저장이 차단되며
> `LogDataPipeline Warning` 이상에 Row ID와 위반 필드명이 포함되어야 한다.
- 증거: `tests/integration/data-pipeline/schema_guard_validator_test.cpp`
- Headless: 부분적 (Editor 모듈 필요)
- 5.6-VERIFY: `OnValidateData()` 트리거 타이밍이 5.5와 동일한지 확인 필요

#### AC-DP-07 — R7 PIE RefreshCatalog hot-swap [Integration · BLOCKING · P1 · 5.6-VERIFY]
> **GIVEN** Pipeline이 Ready 상태이고 카드 10개가 등록된 PIE 세션에서, Row 1개를 추가한
> 뒤, **WHEN** `RefreshCatalog()`를 호출하면, **THEN** `GetAllCardIds()` 결과 크기가
> 11이 되고 새 CardId가 포함되어야 한다. 완료 후 Ready 상태여야 한다.
- 증거: `tests/integration/data-pipeline/pie_refresh_catalog_test.cpp`
- Headless: No (PIE 환경 필요)
- 5.6-VERIFY: PIE 세션 간 Subsystem 수명 주기

#### AC-DP-08 — D1 메모리 footprint smoke [Config-Data · ADVISORY · P2]
> **GIVEN** Full Vision 규모 (카드 40, 꿈 50, 형태 8) 카탈로그가 로드된 상태에서,
> **WHEN** `Pipeline.MemoryFootprintBytes` 또는 `GetCatalogStats()` 조회하면, **THEN**
> 결과값이 **150,000 bytes 이하**여야 하며 텍스처 streaming pool 할당과 명확히 구분
> 되어야 한다.
- 증거: `production/qa/smoke-[date].md`
- Headless: Yes

#### AC-DP-09 — D2 T_init budget 실측 [Logic · BLOCKING · P0 · 5.6-VERIFY]
> **GIVEN** Full Vision 규모 Mock 카탈로그와 `MaxInitTimeBudgetMs = 50.0f`에서, **WHEN**
> `Initialize()` wall-clock을 측정하면, **THEN** T_init이 **50 ms 미만**이어야 한다. 50 ms
> 이상이면 `LogDataPipeline Error`. 10회 연속 중 9회 이상 통과 (스케줄러 편차 허용).
- 증거: `tests/unit/data-pipeline/init_time_budget_test.cpp`
- Headless: Yes

#### AC-DP-10 — D3·E12·G.4 MaxCatalogSize 3-단계 로그 분기 [Logic · BLOCKING · P0 · 5.6-VERIFY]
> **GIVEN** `MaxCatalogSizeCards = 200`, `Warn% = 1.05`, `Error% = 1.5`, `FatalMultiplier
> = 2.0`에서, **WHEN** 카드 210개·301개·401개 카탈로그로 `Initialize()`를 실행하면,
> **THEN** 210 → `Warning`만, 301 → `Error` (Warning 포함), 401 → `Fatal` (Warning·Error
> 포함).
- 증거: `tests/unit/data-pipeline/catalog_overflow_threshold_test.cpp`
- Headless: Yes

#### AC-DP-11 — E1·G.3 DuplicateCardIdPolicy [Logic · BLOCKING · P0] (R2 재설계)
> **GIVEN** `DuplicateCardIdPolicy = WarnOnly` (R2 default)이고 동일 CardId "dup_id" 2개
> row가 있는 상태에서, **WHEN** `Initialize()`가 실행되면, **THEN** Card 카탈로그가
> **Ready 유지**, `GetCardRow("dup_id")`는 **첫 row** 반환, `LogDataPipeline Warning`에
> 중복 ID와 무시된 row 수 포함. `DegradedFallback` 변경 시 카탈로그 전체 DegradedFallback
> 전환 + `GetCardRow("dup_id")` 빈 `TOptional` 반환.
- 증거: `tests/unit/data-pipeline/duplicate_card_id_policy_test.cpp`
- Headless: Yes

#### AC-DP-12 — E4·G.3 bFTextEmptyGuard 행 단위 거부 [Logic · BLOCKING · P0]
> **GIVEN** `bFTextEmptyGuard = true`이고 카드 5개 중 1개의 DisplayName FText가 비어
> 있는 상태에서, **WHEN** `Initialize()`가 실행되면, **THEN** 빈 FText row 1개만 등록
> 거부되고 4개는 정상 등록 + 카탈로그 Ready. `GetAllCardIds()` 크기 = 4. `false` 변경
> 시 5개 모두 등록.
- 증거: `tests/unit/data-pipeline/ftext_empty_guard_test.cpp`
- Headless: Yes

#### AC-DP-13 — E5 Pre-init 조회 차단 + IsReady() [Logic · BLOCKING · P0]
> **GIVEN** Subsystem이 Uninitialized 상태에서, **WHEN** `GetCardRow(유효한 이름)`을
> 호출하면, **THEN** `checkf` assertion이 발화되고 `IsReady() == false`. Loading 상태
> 동일 호출 시 같은 결과.
- 증거: `tests/unit/data-pipeline/pre_init_query_guard_test.cpp`
- Headless: Yes (Debug/Development 빌드)

#### AC-DP-14 — E6 RefreshCatalog 재진입 차단 [Logic · BLOCKING · P1]
> **GIVEN** Pipeline이 Loading 상태 진행 중에, **WHEN** `RefreshCatalog()`가 재진입
> 호출되면, **THEN** 즉시 return + `LogDataPipeline Warning` 발행. 등록 순서가 깨지거나
> 부분 카탈로그가 노출되어선 안 된다.
- 증거: `tests/unit/data-pipeline/refresh_reentry_guard_test.cpp`
- Headless: Yes

#### AC-DP-15 — E10 GetFailedCatalogs API [Integration · ADVISORY · P1]
> **GIVEN** Card 카탈로그만 DegradedFallback이고 Dream·FinalForm·Stillness가 Ready인
> 상태에서, **WHEN** `GetFailedCatalogs()`를 호출하면, **THEN** 반환 `TArray<FName>`에
> Card만 포함, Dream·FinalForm·Stillness는 미포함. `GetAllDreamAssets()`·`GetGrowthFormRow()`
> 는 정상 반환.
- 증거: `tests/integration/data-pipeline/degraded_fallback_api_test.cpp`
- Headless: Yes

#### AC-DP-16 — E11 CardId rename 구분 로그 [Integration · BLOCKING · P0]
> **GIVEN** 카탈로그에 "mossy_dawn"만 존재하며 "mossy_dawn_v1" (옛 ID)을 조회하는
> 경우, **WHEN** `GetCardRow("mossy_dawn_v1")`을 호출하면, **THEN** `TOptional` 비어
> 있는 값 반환 + 단순 null과 *구분되는* `LogDataPipeline Warning` ("Unknown CardId: ...
> rename 또는 migration 누락 의심") 발행.
- 증거: `tests/integration/data-pipeline/unknown_card_id_warning_test.cpp`
- Headless: Yes

#### AC-DP-17 — G.7 단조성 Shipping-safe guard [Logic · BLOCKING · P0] (R2 재설계)
> **GIVEN** `Warn% = 1.5`, `Error% = 1.2` (단조성 위반) 설정에서, **WHEN** `Initialize()`
> 가 진입하면, **THEN** 카탈로그 등록 단계에 진입하지 않아야 하며, `UE_LOG Fatal` 또는
> `checkf`로 위반 knob 이름과 값이 포함된 메시지가 출력되어야 한다. **Shipping 빌드
> 포함 모든 빌드에서 동일 동작**.
- 증거: `tests/unit/data-pipeline/monotonicity_guard_test.cpp`
- Headless: Yes (Development + Shipping 빌드 양쪽)
- R2 변경: `ensureMsgf` (Shipping strip) → `checkf` 또는 명시적 `if+UE_LOG Fatal` (Shipping 잔존)

#### AC-DP-18 — R2 BLOCKER 4 Cooked PrimaryAssetType 검증 [Integration · BLOCKING · P0 · 5.6-VERIFY] (R3 NEW)
> **GIVEN** `DefaultEngine.ini`에 `DreamData` PrimaryAssetType이 등록된 Cooked 빌드에서,
> **WHEN** `UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType("DreamData"), OutIds)`
> 를 호출하면, **THEN** `OutIds`가 비어있지 않아야 하며, 등록된 Dream DataAsset 수와
> 일치해야 한다. 미등록 시 빈 배열 반환 → Dream 카탈로그 전체 DegradedFallback.
- 증거: `tests/integration/data-pipeline/cooked_primary_asset_type_test.cpp`
- Headless: 부분적 (Cooked 빌드 환경 필요)
- 5.6-VERIFY: `PrimaryAssetTypesToScan` 설정이 5.6 Cooked 빌드에서 ���상 동작하는지 확인

### H.3 Smoke Set (PR마다 실행, 30초 이내 목표)

| # | AC-ID | 이유 |
|---|-------|------|
| 1 | AC-DP-04 | fail-close 핵심 계약, 회귀 가장 치명적 |
| 2 | AC-DP-11 | DuplicateCardIdPolicy default 변경 감지 |
| 3 | AC-DP-12 | bFTextEmptyGuard Pillar 2 직결 |
| 4 | AC-DP-13 | pre-init guard 회귀 |
| 5 | AC-DP-17 | 단조성 위반 즉시 차단 |
| 6 | AC-DP-02 | 등록 순서 회귀 |
| 7 | AC-DP-03 | DegradedFallback 진입 로직 |

### H.4 AC 불가 영역 (Gaps) + 대안 가드

| Gap | 영역 | 대안 가드 |
|-----|------|---------|
| Gap-1 | R4 C1 USTRUCT 필드 omission (compile-time) | `FGiftCardRow`에 수치 필드가 없는 것 자체가 계약. PR 코드리뷰 + `.claude/rules/` agent rule이 PR 게이트 |
| Gap-2 | R5 C2 Dream 임계 코드 hardcode 금지 | 코드리뷰 체크리스트 항목 + Coding Standards 명시 + `/design-review` 스캔. CI grep linter 추가 검토 권고 |
| Gap-3 | R6 FSoftObjectPath 강제 / `LoadObject` 금지 | `.claude/rules/` agent rule + PR 코드리뷰 + CI static analysis (cppcheck 또는 커스텀 grep) |
| Gap-4 | R8 디자이너 5단계 워크플로우 | UX 검증 — 디자이너 실습 체크리스트 + 첫 스프린트 디자이너 라이브 데모 |
| Gap-5 | E8 FSoftObjectPath Cook 누락 → Error 격상 [5.6-VERIFY] | OQ-E-5 검증 완료 전까지 Cook Warning 수동 모니터링 + bSoftRefMissingError true 전환 |
| Gap-6 | Pillar 2/3/4 정성적 보호 | Pillar별 플레이테스트 체크리스트 (`production/qa/evidence/pillar-playtest.md`) + 크리에이티브 디렉터 승인 |

### H.5 Cross-System Contract AC (downstream GDD에 의무 추가 사항)

본 GDD는 *Pipeline 측 contract*만 검증. 각 downstream GDD는 *자기 측 AC*를 추가해야 한다.

| Downstream | Pipeline 측 AC | downstream GDD 추가 필요 사항 |
|-----------|----------------|----------------------------|
| Card #8 `GetCardRow` | AC-DP-04 | Ready 확인 후 호출 / nullptr 수신 시 day skip AC |
| Card #8 `GetAllCardIds` | AC-DP-02 | 빈 배열 수신 시 가중치 랜덤 동작 AC |
| Dream Trigger #9 `GetAllDreamAssets` | AC-DP-05 | 빈 배열 수신 시 트리거 평가 skip AC |
| Growth #7 `GetGrowthFormRow` | AC-DP-05 | 21일차 FormId 조회 실패 시 fallback 형태 AC |
| Moss Baby Character #6 `GetCardIconPath` | AC-DP-04 계열 | empty SoftPath 수신 시 아이콘 없이 진행 AC |
| Stillness Budget #10 `GetStillnessBudgetAsset` | AC-DP-03 | nullptr 수신 시 hardcoded default 대신 기능 비활성화 AC |
| Dream Journal UI #13 `GetDreamBodyText` | AC-DP-12 | FText 비어있을 때 UI 표시 처리 AC |

### H.6 5.6-VERIFY AC 목록 (modules/core.md 미작성 — 검증 task)

| AC-ID | 의존 동작 | 검증 필요 |
|-------|----------|---------|
| AC-DP-06 | `UEditorValidatorBase` 호출 시점 + Editor 모듈 격리 | `OnValidateData()` 트리거 타이밍 5.5↔5.6 비교, `-editor` headless 빌드 Validator 클래스 링크 |
| AC-DP-07 | PIE `RefreshCatalog()` 중 Subsystem 재초기화 | UE 5.6 GameInstanceSubsystem PIE 세션 수명 주기 |
| AC-DP-17 | `ensureMsgf` Shipping 빌드 동작 | UE 5.6 Shipping에서 `ensure` strip 여부, 대체 로깅 필요 여부 |
| AC-DP-09 | `MaxInitTimeBudgetMs` 실측 — `-nullrhi` 타이밍 정밀도 | `FPlatformTime::Seconds()` 정밀도가 ms 단위 측정에 충분한지 |
| Gap-5 (E8) | `bSoftRefMissingError` Cook 격상 옵션 | UE 5.6 Cook commandlet 프로젝트 설정 항목 존재 여부 |

### H.7 우선순위 요약

| 우선순위 | AC | 기준 |
|---------|-----|------|
| **P0 (BLOCKING for MVP)** | AC-DP-01, 02, 03, 04, 05, 06, 09, 10, 11, 12, 13, 16, 17, 18 (14개) | Pillar 계약 직결 또는 Pipeline 기본 동작 |
| **P1 (BLOCKING for VS)** | AC-DP-07, 14, 15 (3개) | PIE 워크플로우 / DegradedFallback API / 재진입 차단 |
| **P2 (ADVISORY/Polish)** | AC-DP-08 (1개) | 메모리 footprint 수치 확인 — 안전성 무관 |

[5.6-VERIFY] 라벨 AC 5개는 스프린트 계획 시 **검증 task를 별도 항목으로 등록**하고 막힐
경우 즉시 QA 리드 에스컬레이션.

## Open Questions

GDD 작성 중 surface된 12개 미결 사항. 3개는 Section G에서 *resolve 완료*, 9개는 다른
시스템·검증 task로 위임.

| OQ-ID | 질문 | 책임 GDD / 주체 | 해결 시점 | 상태 |
|-------|------|---------------|---------|------|
| **OQ-D-1** | 텍스처 streaming pool 할당 크기 + Lumen GI 점유 합산이 1.5 GB ceiling에서 차지하는 비율 | **Lumen Dusk Scene GDD (#11)** | Lumen Dusk Scene 설계 단계 | Open |
| OQ-E-1 | 중복 CardId 발견 시 DegradedFallback 강등 vs 경고만 | 본 GDD (정책 결정) | Section G | ✅ **Resolved** — `DuplicateCardIdPolicy = DegradedFallback` |
| OQ-E-2 | 빈 Tags 카드 schema 차단 vs 허용 | **Growth Accumulation GDD (#7)** | Growth GDD 설계 단계 | Open |
| OQ-E-3 | Pipeline이 `FText.IsEmpty()` 가드 추가 여부 | 본 GDD (정책 결정) | Section G | ✅ **Resolved** — `bFTextEmptyGuard = true` |
| OQ-E-4 | UE 5.6에서 `GameInstance::Init()` 순서 보장 예외 플랫폼 존재 여부 | 본 GDD (검증 task — `engine-programmer` 또는 `unreal-specialist`) | MVP 첫 스프린트 | Open · 검증 필요 |
| OQ-E-5 | UE 5.6 Cook 파이프라인이 `FSoftObjectPath` 누락을 Warning → Error로 격상하는 옵션 존재 여부 | 본 GDD (검증 task) | MVP 첫 스프린트 | Open · 검증 필요 (해결 시 `bSoftRefMissingError = true`) |
| OQ-E-6 | DataTable 저장 외 CI에서 `UEditorValidatorBase`를 실행할 수 있는 UE 5.6 자동화 훅 존재 여부 | 본 GDD (검증 task — `devops-engineer`) | MVP 첫 스프린트 | Open · 검증 필요 |
| OQ-E-7 | Card 카탈로그 단독 실패 시 게임이 계속 진행되는 것이 Pillar 4 ("끝이 있다")에 부합하는지, 또는 Fatal Error가 맞는지 | 본 GDD (R2 정책 결정) | R2 | ✅ **Resolved** — pending day + 다음 앱 복구 (E10 R2 참조). Fatal = Pillar 1 위반, silent skip = Pillar 4 위반 → 중간 경로 |
| OQ-E-8 | CardId rename 정책 — SaveSchemaVersion bump로 강제 vs Pipeline에 alias 테이블 | **Save/Load Persistence GDD (#3) + Card System GDD (#8) 협의** | Card GDD 또는 Save/Load R2 | Open |
| OQ-E-9 | `MaxCatalogSize` 200% 초과를 런타임 에러로 강등할 배수 기준값을 Tuning Knob에 노출할지 | 본 GDD (정책 결정) | Section G | ✅ **Resolved** — `CatalogOverflowFatalMultiplier = 2.0` knob 노출 |
| ~~OQ-ADR-1~~ | ~~데이터 컨테이너 선택~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0002](../../docs/architecture/adr-0002-data-pipeline-container-selection.md) — Card=DT, FinalForm=DataAsset (ADR-0010 격상), Dream=DataAsset, Stillness=DataAsset; UDataRegistry 거부) |
| ~~OQ-ADR-2~~ | ~~로딩 전략~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0003](../../docs/architecture/adr-0003-data-pipeline-loading-strategy.md) — Sync 일괄 채택 + HDD compromise 명시) |

### Resolved 요약

- **4개 OQ resolve** (OQ-E-1, OQ-E-3, OQ-E-7, OQ-E-9) — Section G knob 결정 + R2 E10 정책 결정으로 해소
- 3개 OQ는 다른 시스템 GDD 작성 시 해소 (OQ-D-1 → Lumen #11, OQ-E-2 → Growth #7, OQ-E-8 → Save/Load + Card)
- 3개 OQ는 검증 task (OQ-E-4, OQ-E-5, OQ-E-6) — MVP 첫 스프린트에 별도 task로 등록 권고
- 2개 OQ는 ADR로 분리 (OQ-ADR-1, OQ-ADR-2) — GDD 검토 후 `/architecture-decision`으로 처리

### 검증 task 종합 (MVP 첫 스프린트 권고)

[5.6-VERIFY] AC 5개 (Section H.6) + OQ-E-4·5·6 검증 = **8개 검증 task**가 첫 스프린트
시작 시 별도 항목으로 등록 필요. 막힐 경우 `unreal-specialist` 또는 `devops-engineer`
에이전트 즉시 호출.
