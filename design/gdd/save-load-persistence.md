# Save/Load Persistence (with Integrity)

> **Status**: R3 — 4 CRITICAL 수정 완료. `/design-review` 재실행 대기.
> **Author**: bong9tutor + claude-code session (systems-designer + unreal-specialist + qa-lead + game-designer + performance-analyst + creative-director consulted; R2 review 6 specialist)
> **Last Updated**: 2026-04-17 (R3 — review-log 참조: `design/gdd/reviews/save-load-persistence-review-log.md`)
> **Revision history**: v1 (2026-04-16) → R1 review (MAJOR REVISION NEEDED) → R2 (2026-04-17) → R2 review (NEEDS REVISION, 4 CRITICAL) → R3 (2026-04-17, 본 revision)
> **Implements Pillar**: Pillar 4 (끝이 있다 — 21일 진행 보존), Pillar 1 (조용한 존재 — 세이브 실패의 silent rollback)
> **Priority**: MVP | **Layer**: Foundation | **Category**: Persistence
> **Effort (R2)**: M+ (Save Integrity 흡수)
> **Creative Director Review (CD-GDD-ALIGN)**: SKIPPED 2026-04-16 — Lean mode (production/review-mode.txt). Phase Gate에서 일괄 재실행 예정.
> **R1 Disagreement Adjudication (2026-04-17)**: D1 (E10 "새 시작" Pillar 4 betrayal) → creative-director 입장 채택 (RECOMMENDED, infra hook + OQ만 추가, 감정 문법은 Farewell Archive #17 위임). D2 (Compound-event 1-commit-loss guarantee) → creative-director 입장 채택 (RECOMMENDED, per-trigger atomicity qualifier + negative AC).
> **Cross-system contracts owned by this GDD**: `ESaveReason` enum (4 values) | `SaveSchemaVersion` const | `MinCompatibleSchemaVersion` const | `FOnLoadComplete(bool, bool)` delegate | `GetLastValidSlotMetadata()` infra hook (R2 NEW — Farewell Archive #17 소비 기대) | `FOnSaveDegradedReached` delegate (R2 NEW) | atomicity contract for `LastSaveReason` + `NarrativeCount` (Time §E6, §Formula 5 응답) | per-trigger atomicity 일반화 (R2 qualifier 추가; sequence atomicity는 GSM #5 책임)

---

## Overview

Save/Load Persistence는 Moss Baby의 모든 영속 상태(이끼 아이의 21일 진행, `FSessionRecord`, 카드 건넴 이력, 꿈 일기, 성장 누적 벡터, Farewell Archive 페이지)를 디스크에 안전히 쓰고 다시 읽어들이는 **데이터 인프라 계층**이다. 단일 `USaveGame` 직렬화에 그치지 않고, 21일이라는 실시간 투자가 한 번의 전원 차단·하드 리셋·앱 강제 종료·디스크 오류로 증발하지 않도록 다음 네 가지 무결성 메커니즘을 단일 장소에서 책임진다 — (1) **temp-file + atomic rename** 패턴의 부분 쓰기 방지, (2) **이중 슬롯 A/B ping-pong**으로 활성 슬롯 손상 시 직전 정상 슬롯으로 복원, (3) **체크섬**으로 비트 부패 감지, (4) **`SaveSchemaVersion` 마이그레이션**으로 패치 후에도 진행 중 세이브 호환.

이 시스템은 두 종류의 contract를 외부에 약속한다. **Time & Session System(#1)에 대해서는** `FSessionRecord` 직렬화·역직렬화 책임 + `LastSaveReason` 마지막 쓰기의 atomic durability + `NarrativeCount`(Pillar 3 강제용 카운터)의 cap-leak-free 영속화. **모든 다른 시스템에 대해서는** "쓰기 요청을 받으면 다음 슬롯이 완전히 일관된 상태로 disk에 도달하기 전에는 성공으로 보고하지 않는다"는 atomic 약속과, "직전 정상 상태로의 silent rollback이 항상 가능하다"는 복원 약속. 이 두 약속이 Pillar 4(끝이 있다)의 21일 보존과 Pillar 1(조용한 존재)의 "실패가 플레이어에게 드러나지 않음"을 동시에 떠받친다.

## Player Fantasy

이 시스템이 플레이어에게 약속하는 것은 **"당신이 어제 건넨 카드는 오늘도 건네져 있다"** 한 줄로 요약된다. 21일이라는 실시간은 플레이어의 자기 시간 일부를 이끼 아이에게 내어주는 의식이며, 그 시간이 한 번의 노트북 강제 종료·정전·OS 업데이트 재부팅·앱 패치로 사라진다면 의식 자체가 성립하지 않는다. Save/Load Persistence는 이 약속의 물리적 토대다 — 플레이어는 시스템의 존재를 한 번도 의식하지 않지만, **매일 앱을 다시 열었을 때 어제의 이끼 아이가 정확히 거기 있다**는 사실은 21일 내내 피부로 느끼는 신뢰의 근원이 된다.

이 시스템이 무너졌을 때 가장 두려운 시나리오는 "Day 13에 카드를 건넨 직후 정전" 같은 경우다. Save/Load는 이런 순간에도 **플레이어가 절대 알아차리지 못하는 silent rollback**으로 응답한다 — 직전 정상 상태로 조용히 복원되며, 에러 다이얼로그·복구 마법사·"손상된 세이브를 발견했습니다" 같은 알림 모두 **금지**된다 (Pillar 1). 만약 양쪽 슬롯이 모두 손상되는 극단 시나리오가 발생하더라도 사용자에게 노출되는 표현은 단 하나 — Title 화면의 "이끼 아이는 깊이 잠들었습니다"와 새 시작 옵션 — 으로, 사고를 부재의 시학으로 흡수한다.

**Pillar 연결**:
- **Pillar 4 (끝이 있다)** — 21일은 플레이어가 만든 21일이다. 디스크 사고로 도중에 끊기는 종료는 *진짜 끝이 아니므로* 시스템적으로 거부됨
- **Pillar 1 (조용한 존재)** — 모든 무결성 동작은 플레이어 시야 밖에서 진행. 성공도 실패도 알리지 않음. "저장 중..." 인디케이터조차 없음 (저장은 카드 건넴 직후 100ms 내 비동기 완료)
- **Pillar 3 (말할 때만 말한다)** — `NarrativeCount` cap의 atomic 영속화로 시스템 내러티브 한도가 새지 않게 — 희소성을 인프라가 지킨다

## Detailed Design

Save/Load Persistence는 **단일 in-memory `UMossSaveData : USaveGame` 오브젝트**를 모든 영속 상태(`FSessionRecord` + 향후 Growth/Card/Dream/Farewell 데이터)의 단일 컨테이너로 사용한다. 모든 저장은 이 오브젝트 전체를 한 번에 직렬화 — **부분 저장(partial save)은 존재하지 않는다**. 네 가지 무결성 메커니즘은 이 단일 오브젝트를 둘러싸고 다음과 같이 동작한다.

### Core Rules

#### 1. 슬롯 아키텍처

`FPlatformProcess::UserSettingsDir()/MossBaby/SaveGames/` 하위에 슬롯 두 개와 임시 영역:
- `MossData_A.sav`, `MossData_B.sav` — 슬롯 본체
- `MossData_A.tmp`, `MossData_B.tmp` — 쓰기 staging (rename 후 또는 다음 시작 시 정리)

각 슬롯 파일 구조:

```
[Header Block — 고정 22 bytes]
  uint32  Magic                       // 0x4D4F5353 ('M','O','S','S')
  uint8   SaveSchemaVersion           // 현재 = 1
  uint8   MinCompatibleSchemaVersion  // 현재 = 1
  uint32  WriteSeqNumber              // 매 저장마다 +1; 신뢰 슬롯 결정 tiebreaker
  uint32  PayloadByteLength
  uint32  PayloadCrc32                // FCrc::MemCrc32() over [Payload]
[Payload]
  TArray<uint8>                       // UGameplayStatics::SaveGameToMemory(UMossSaveData)
```

**별도 "active slot pointer" 파일은 없다.** 활성 슬롯은 항상 두 헤더를 모두 읽고 무결성 통과 슬롯 중 `WriteSeqNumber`가 가장 큰 쪽으로 결정.

#### 2. 저장 절차 (atomic write)

매 저장은 thread-pool worker에서 실행 (게임 스레드는 lambda 큐잉 후 즉시 반환):

1. **Snapshot**: in-memory `UMossSaveData`를 게임 스레드에서 `FMossSaveSnapshot` USTRUCT로 값 복사 (~1ms)

   **R2 BLOCKER 7 — `FMossSaveSnapshot` value copy contract (POD-only)**:
   
   `UMossSaveData`의 `TMap`/`TArray`/`UObject*` 필드가 worker thread에서 직접 참조되면 GC
   pass·rehash 중 UAF (Use-After-Free) 위험. R2에서 `FMossSaveSnapshot`을 명시적 POD-only
   USTRUCT로 spec화:
   
   **허용 필드 타입**:
   - 기본 정수·부동소수점 (`int32`, `int64`, `uint32`, `uint64`, `float`, `double`, `bool`)
   - UE 값 타입 (`FName`, `FString`, `FText`, `FGuid`, `FDateTime`)
   - 위 타입의 `TArray<T>` / `TMap<K, V>` (UE container 자체는 deep copy semantic)
   - 위 조건을 재귀적으로 만족하는 plain USTRUCT
   
   **금지 필드 타입**:
   - `UObject*`, `TWeakObjectPtr<>`, `TStrongObjectPtr<>`, `TSoftObjectPtr<>` — GC 경합
   - Raw heap pointers (`int*`, `void*`)
   - `TSharedPtr<>`, `TSharedRef<>` (UE-managed가 아닌 ref counting)
   - Lambda·function pointers
   
   **Snapshot 생성 의무**:
   - 게임 스레드에서 `FMossSaveSnapshot::FromSaveData(const UMossSaveData&)` 정적 함수로
     생성. UObject 참조 필드는 *flatten* — 예: `UDreamDataAsset* CurrentDream`은 스냅샷에서
     `FName CurrentDreamId`로 저장
   - Snapshot 생성 후 worker thread는 *오직 snapshot만* 직렬화. `UMossSaveData` 직접
     참조 금지 (private GC-safe accessor 없이는 컴파일 실패하도록 인터페이스 분리)
   
   **검증 메커니즘 (2중)** (R3 — `TIsTriviallyCopyable` static_assert 제거. FString/TArray/TMap은
   deep copy semantic이지만 trivially copyable이 아니므로 해당 trait과 허용 타입이 모순. GC 경합
   방지라는 실제 목표는 아래 2중 검증으로 충분히 달성됨):
   1. 컴파일 타임: `FMossSaveSnapshot`을 별도 헤더에 두고 forbidden 타입 헤더 미include
      (`UObject.h`, `UObjectBaseUtility.h` 등 미포함 → UObject* 필드 선언 시 빌드 실패)
   2. Static analysis: CI에서 `grep -r "UPROPERTY.*FMossSaveSnapshot" Source/` 결과의 필드
      타입을 whitelist 외 타입 검출 시 차단
2. **Target slot 결정**: 활성 슬롯이 A면 B로, B면 A로. **항상 ping-pong** — 방금 신뢰한 슬롯을 덮어쓰지 않음. **예외 (FreshStart 직후)**: 활성 슬롯이 `⊥`(미정의)이면 첫 저장 대상은 슬롯 A로 고정. 두 번째 저장부터 정상 ping-pong 시작
3. **Next `WriteSeqNumber`**: `next = max(SlotA.WSN, SlotB.WSN) + 1`
4. **Serialize**: `UGameplayStatics::SaveGameToMemory(SaveData, OutBytes)`
5. **Checksum**: `Crc = FCrc::MemCrc32(OutBytes.GetData(), OutBytes.Num(), 0)` — **seed = 0 고정** (R3 CRITICAL-4: 저장·검증 seed 불일치 시 모든 슬롯 영구 Invalid)
6. **Header build**: magic / schema versions / WSN / payload length / CRC 채움
7. **Temp 쓰기**: `FFileHelper::SaveArrayToFile([Header ++ Payload], "MossData_<Target>.tmp")` — 파일 핸들 닫힘 시점에 OS 버퍼 flush
8. **Atomic rename**: `IPlatformFile::MoveFile("MossData_<Target>.sav", "MossData_<Target>.tmp")` — Windows NTFS에서 `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` 매핑, 동일 볼륨 atomic
9. **In-memory active pointer 갱신**: 이제 Target이 신뢰 슬롯
10. **Cleanup**: 반대 슬롯 잔여 `.tmp` 삭제 (best-effort)

어느 단계든 실패하면 저장 실패로 보고하되 **롤백 불필요** — 직전 신뢰 슬롯 무탈. 다음 저장 요청에서 동일 target 슬롯에 재시도.

> **구현 참고 (UE 5.6)**: `IPlatformFile::MoveFile()`이 `IFileManager::Get().Move()`보다 우선 — 후자는 copy+delete fallback으로 atomic 깨짐.

#### 3. 로드 절차 (corruption recovery)

서브시스템 `Initialize()`에서 1회 실행 (게임 스레드 blocking, 일반적 50ms 미만):

1. **두 헤더 읽기**: 슬롯 A·B 헤더(22 bytes 각각) 파싱
2. **각 슬롯 채점**:
   - **Invalid**: 파일 없음 / Magic 불일치 / CRC 불일치 / `SaveSchemaVersion > CURRENT` (downgrade) / `SaveSchemaVersion < MinCompatible` (too old) / payload length 불일치 / I/O 오류
   - **Valid**: 그 외
3. **신뢰 슬롯 선택**: Valid 중 `WriteSeqNumber` 최대. 동률 시 A
4. **둘 다 Invalid**: `FreshStart` 모드 — 새 `UMossSaveData` 생성, `WriteSeqNumber = 0`. 다음 저장(보통 Day 1 첫 카드)이 슬롯 A에 `WSN = 1`로 기록
5. **신뢰 슬롯이 마이그레이션 필요** (`SaveSchemaVersion < CURRENT`): 선형 chain 적용 (`MigrateFromV1ToV2` → `V2ToV3` → …) in-memory만. **디스크에 다시 쓰지 않음** — 다음 정상 저장이 새 버전으로 commit
6. **Payload deserialize**: `UGameplayStatics::LoadGameFromMemory(PayloadBytes)` → `UMossSaveData`
7. **Consumers에 hand off**: Time / GSM / Growth 등이 typed accessor로 자기 슬라이스 읽음

신뢰 슬롯이 4–6 사이에서 실패(헤더는 valid이나 본문 deserialize 실패 — 드물게)하면 반대 슬롯으로 fall back, 2단계부터 재실행. **Fall back은 최대 1회** (= 슬롯 수 − 1). 반대 슬롯도 실패하면 즉시 `FreshStart` 모드로 진입 — 무한 재귀 차단.

#### 4. Schema 마이그레이션 (envelope policy)

헤더에 두 버전 필드:
- **`SaveSchemaVersion: uint8`** — 이 파일이 쓰여진 스키마. 어떤 필드 추가·삭제·재타입·의미 변경 시마다 +1. 현재 = **1**
- **`MinCompatibleSchemaVersion: uint8`** — 이 빌드의 마이그레이션 chain이 지원하는 최저 버전. 오래된 마이그레이션 코드 정리 가능. 현재 = **1**

| Loaded `SaveSchemaVersion` | 동작 |
|---|---|
| `< MinCompatibleSchemaVersion` | 슬롯 too old → Invalid. 반대 슬롯 시도 |
| `[MinCompatible, CURRENT)` | Linear migration chain 적용 |
| `== CURRENT` | 마이그레이션 없음 |
| `> CURRENT` | 슬롯이 미래 빌드(downgrade). Invalid → 반대 슬롯 시도 |

두 슬롯 모두 either-too-old-or-too-future로 Invalid면 `FreshStart` 모드 (silent — Pillar 1, 플레이어 가시 동작은 §Edge Cases 참조).

`MigrateFromV<N>ToV<N+1>(UMossSaveData* InOut)` 함수는 `MossSaveData.cpp`에 거주, **순수**함수 — in-memory 객체만 변경. 마이그레이션은 default 채움 / deprecated 필드 drop / 파생 상태 재계산 가능.

#### 5. Atomicity Contracts

두 cross-system invariant이 atomic rename = atomic commit이라는 단일 트랜잭션 단위로 보장됨:

- **`LastSaveReason` durability** (Time §E6): 마지막으로 쓰여진 `LastSaveReason`이 크래시에서 살아남아야 함. `LastSaveReason`이 `FSessionRecord` 안에 있고, 그것이 `UMossSaveData` 안에 있으므로 매 commit이 atomic 포함. **별도 "reason 파일" 없음**
- **`NarrativeCount` cap-leak 방지 (R2 BLOCKER 3 재작성)** (Time §Formula 5): `ADVANCE_WITH_NARRATIVE` 분기 시 increment + persist atomic. **R2 구조적 강제** — "같은 함수 호출 안에서 호출하라"는 *문서 contract*가 Release 빌드 인라인·심볼 제거로 검증 불가능했던 R1 약점 해소:

  Time 서브시스템에 **단일 public method 강제**:
  ```cpp
  // Public — atomicity 보장 단일 진입점
  void UTimeSessionSubsystem::IncrementNarrativeCountAndSave();
  
  // Private — 외부 직접 호출 금지
  private:
      void IncrementNarrativeCount();        // in-memory mutation only
      void TriggerSaveForNarrative();        // SaveAsync(ENarrativeEmitted) wrapper
  ```
  
  `IncrementNarrativeCountAndSave()` 구현은 두 private method를 같은 함수 본문에서 순차
  호출. **다른 모든 callsite (Time GDD §AC, GSM, 테스트 등)는 이 단일 method만 사용**.
  
  강제 메커니즘 (3중):
  1. **컴파일 타임**: `IncrementNarrativeCount()` / `TriggerSaveForNarrative()` 가
     `private:` 가시성 → `IncrementNarrativeCountAndSave()` 외 callsite 빌드 실패
  2. **Static analysis (CI)**: `grep -r "IncrementNarrativeCount\b" Source/` 결과 ≤ 2건
     (정의 1 + wrapper 호출 1). 다른 callsite 발견 시 CI 차단
  3. **Unit test fixture**: friend class 또는 reflection으로 private method 호출 가능한
     테스트가 `IncrementNarrativeCount()` 단독 호출 후 process kill 시뮬 → cap leak 발생
     검증 (= "wrapper 안 쓰면 깨진다"의 negative test)
  
  R1 약점 ("스택 로그로 동일 frame 내 두 호출 확인"이 인라인된 Release 빌드에서 무효) 완전 해소.

> **일반화 — 모든 increment/append 연산자에게 적용**: 위 NarrativeCount 패턴은 **단순 사례가 아니라 contract**다. 다음 downstream 시스템들도 동일 패턴(in-memory mutation → 즉시 SaveAsync, 같은 game-thread 함수 호출 안에서)을 강제로 따라야 한다:
> - Growth Accumulation (#7): 태그 벡터 누적 → `SaveAsync(ECardOffered)`
> - Dream Trigger (#9): unlocked dream IDs append → `SaveAsync(EDreamReceived)`
> - Farewell Archive (#17): archive page 추가 → `SaveAsync(EFarewellReached)`
>
> 이 contract를 어기면 increment 후 SaveAsync 전 크래시 시 데이터 손실. 각 downstream GDD §Acceptance Criteria에 해당 패턴 검증 AC 1개 이상이 들어가야 한다 (cross-GDD 의무).

`SaveAsync`가 이전 저장 완료 전 재호출되면 요청은 **coalesce**됨 — 새 요청이 이전 요청을 superseed; worker 스레드 pickup 시점의 in-memory 상태가 직렬화됨. 어떤 저장 요청도 silent drop되지 않음 — 항상 가장 최근 상태가 승리.

#### 6. `ESaveReason` enum (cross-system contract)

본 GDD가 canonical owner (Time §E6 contract 응답):

```cpp
UENUM(BlueprintType)
enum class ESaveReason : uint8
{
    ECardOffered      = 0  UMETA(DisplayName="Card Offered"),
    EDreamReceived    = 1  UMETA(DisplayName="Dream Received"),
    ENarrativeEmitted = 2  UMETA(DisplayName="Narrative Emitted"),
    EFarewellReached  = 3  UMETA(DisplayName="Farewell Reached"),
};
```

- 정수 값은 **stable**: 한 번 할당된 정수는 변경 금지 (직렬화 호환성)
- `LastSaveReason: FString` 필드는 **enum 이름을 문자열로 저장** (예: `"ECardOffered"`) — ordinal 재번호화에 robust, raw 세이브 파일에서 디버그 가능
- 미래 verifying 새 reason은 **끝에 다음 정수로 append**. 중간 삽입 금지

#### 7. R2 NEW — E10 corruption infra hook (Farewell Archive #17 위임)

**R1 Disagreement 1 adjudication (creative-director 입장 채택)**: Day 13 corruption → "새
시작" 시나리오의 Pillar 4 betrayal 문제는 **감정 문법 결정을 Farewell Archive GDD #17 + Creative
Director territory로 위임**. 본 GDD는 Farewell Archive가 *partial farewell* 같은 대안 경로
설계를 가능하게 하는 **infra hook만 노출**:

- **`GetLastValidSlotMetadata() → TOptional<FMossSlotMetadata>`** (R2 NEW public API):
  로드 절차 §3 Step 3에서 `FreshStart` 모드 진입 직전, *부분적으로 valid했던* 슬롯의 메타데이터
  스냅샷을 보존. 헤더 정보(`SaveSchemaVersion`, `WriteSeqNumber`, `LastSaveReason`, payload
  byte length, 마지막 정상 commit 추정 시각 = 파일 mtime)만 포함. 페이로드 자체는 deserialize
  실패했으므로 *접근 불가*. Farewell Archive #17이 이 메타데이터를 통해 "당신은 N일을 함께
  했습니다" 같은 partial farewell 경로 설계 가능.
  - `TOptional` 비어있음 = 진짜 첫 실행 (`bHadPreviousData = false`)
  - 값 있음 = 손상 fallback (`bHadPreviousData = true`)이며 이전 슬롯 메타가 보존됨

- **`FOnSaveDegradedReached(FMossSlotMetadata)` delegate** (R2 NEW): 로드 절차에서 양 슬롯
  Invalid 감지 + `FreshStart` 진입 시 1회 발행. Farewell Archive #17 (혹은 Title&Settings UI)
  이 구독하여 partial farewell vs "새 시작" vs 다른 ritual 분기 결정.

- **본 GDD는 메타데이터 보존 책임만**. 어떤 화면·문구·BGM·페이지 작성 ritual로 표현할지는
  Farewell Archive GDD #17 + Creative Director 결정. Pillar 4 보호 결정의 *물리적 토대*만
  제공.

이 infra hook은 BLOCKING이 아닌 **R2 RECOMMENDED 적용** — D1 disagreement adjudication 결과.
Farewell Archive GDD가 작성될 때 본 hook을 소비하지 않을 수도 있음 (그 경우 hook은 dead code
지만 cost ≈ 0 — 메타데이터 보존은 어차피 수행).

#### 8. R2 NEW — Per-trigger atomicity qualifier (D2 adjudication)

**R1 Disagreement 2 adjudication (creative-director 입장 채택)**: §5 Atomicity Contracts의
"최대 1 commit 손실" 보증은 **per-trigger 단위 한정**. Compound event (Day 13 카드 + 꿈 +
narrative 같은 다중 reason 동시 트리거)의 sequence atomicity는 본 GDD 책임이 아닌 **Game State
Machine #5 책임**.

**Save/Load가 보증하는 것 (확정)**:
- 각 `SaveAsync(reason)` 호출은 atomic — 해당 호출이 commit하면 해당 in-memory 상태 전체가
  disk에 일관되게 도달 (slot ping-pong + atomic rename)
- 한 commit이 fail하면 *해당 commit*만 손실, 직전 commit은 무탈

**Save/Load가 보증하지 않는 것 (R2 명시 — D2 qualifier)**:
- Compound event "Growth tag commit + Dream unlock commit + Narrative emit commit"의 *3개
  reason이 모두 commit되거나 모두 rollback되는* sequence atomicity는 보증 없음. 중간 reason
  fail 시 이전 reason은 commit된 상태 유지, 이후 reason은 미시도.
- 이 sequence를 atomic하게 묶고 싶으면 **Game State Machine #5가 BeginCompoundEvent() /
  EndCompoundEvent() boundary를 정의**해 reason들을 batch coalesce해야 함. Save/Load는 그
  요청을 받으면 *마지막 in-memory 상태*만 1회 commit (자연스럽게 sequence 일관). 본 GDD는
  이런 API를 제공하지 않으며, GSM이 in-memory mutation을 모은 뒤 마지막에 1회 SaveAsync
  하면 Save/Load 측에서는 자동으로 1 commit으로 처리됨 (R3 부정 negative AC 참조).

### States and Transitions

| State | 설명 | 진입 트리거 | 유효 전환 |
|---|---|---|---|
| `Idle` | I/O 미진행. in-memory가 디스크 ahead일 수 있음 | `Initialize()` 완료 OR 직전 save/load 완료 | → `Saving` (consumer가 `SaveAsync` 호출) / `Loading` (드물게 — 명시적 `Reload()`만) |
| `Loading` | 초기 디스크 로드 진행 중 (게임 스레드 blocking) | 서브시스템 `Initialize()` 시작, consumer 읽기 전 | → `Migrating` (마이그레이션 필요) / `Idle` (불필요) / `FreshStart` (둘 다 Invalid) |
| `Migrating` | 선형 schema 마이그레이션 chain 실행 (게임 스레드, in-memory만) | `Loading`이 `SaveSchemaVersion < CURRENT` 발견 | → `Idle` (마이그레이션 완료, 디스크 쓰기는 다음 정상 save로 미룸) |
| `Saving` | 백그라운드 쓰기 진행 중 (thread pool) | consumer가 `SaveAsync(ESaveReason)` 호출 | → `Idle` (성공) / `Idle` (실패 — 다음 요청 시 silent retry) |
| `FreshStart` | 두 슬롯 모두 Invalid, in-memory 신규 게임 초기화됨 | `Loading`이 둘 다 Invalid 발견 | → `Idle` (새 `UMossSaveData` 생성 후) |

**동시성 규칙**:
- `Saving`은 한 번에 하나만. 진행 중 추가 `SaveAsync` 호출은 **coalesce** (latest in-memory 승리)
- `Loading` / `Migrating`은 게임 스레드 blocking (단발성, 시작 시만, 총 < 100ms 일반적)

**강제 flush 트리거 — Windows 데스크톱 lifecycle 조합 (R2 재작성 + R3 T2 분리)**:

R1 리뷰 발견: `FCoreDelegates::ApplicationWillEnterBackgroundDelegate`는 **모바일 플랫폼 전용** —
Windows에서 발화하지 않음. R2에서 4개 트리거로 교체, R3에서 T2를 FlushOnly로 분리.

| 트리거 | 발화 시점 | UE 5.6 진입점 | 동작 (R3) |
|--------|----------|--------------|-----------|
| **T1** `GameViewportClient::CloseRequested` | 사용자가 창 X 클릭 / Alt+F4 / 시스템 종료 명령 | `UGameViewportClient::CloseRequested(UWorld*)` override (UE 5.6 소스에서 exact virtual 시그니처 확인 필요) | **FlushAndDeinit()** |
| **T2** `FSlateApplication::OnApplicationActivationStateChanged` (deactivation) | Alt+Tab으로 백그라운드 / 다른 창 focus 이동 | `FSlateApplication::Get().OnApplicationActivationStateChanged().AddRaw(...)` (UE 5.6 delegate 시그니처 확인 필요). `Deinitialize()` 시 `RemoveAll(this)` 호출 필수 (dangling callback 방지) | **FlushOnly()** — 일반 `SaveAsync` 1회 (§아래 참조) |
| **T3** `FCoreDelegates::OnExit` | 엔진 종료 시퀀스 진입 (PIE 종료 또는 Shipping exit) | `FCoreDelegates::OnExit.AddRaw(...)` | **FlushAndDeinit()** |
| **T4** `UGameInstance::Shutdown` | GameInstance 파괴 직전 (서브시스템 `Deinitialize()` 호출 직전) | `UGameInstance::Shutdown()` override 또는 `UGameInstanceSubsystem::Deinitialize()` | **FlushAndDeinit()** |

**R3 T2 분리 근거**: Moss Baby는 데스크톱 상주형 앱으로 Alt+Tab이 정상 사용 흐름의 일부다.
R2에서 T2가 `FlushAndDeinit()`을 호출하면 `bDeinitFlushInProgress`가 영구 set되어 이후 T1/T3/T4
발화 시 모두 return → **마지막 저장이 디스크에 미도달한 채 프로세스 종료** (Pillar 4 직접 위반).
R3에서 T2를 `FlushOnly()`로 분리하여 이 regression을 해소.

**`FlushOnly()` (T2 전용, R3 NEW)**:
- in-memory에 dirty 상태가 있으면 일반 `SaveAsync(마지막 ESaveReason)` 1회 트리거
- `bDeinitFlushInProgress`를 **set하지 않음** — 이후 종료 경로의 FlushAndDeinit 정상 동작 보장
- 동기 대기 없음 (비동기 저장 큐잉 후 즉시 return)
- Alt+Tab 빈번 사용 시 매번 SaveAsync가 coalesce되어 1회 I/O로 수렴 (§5 coalesce 정책)

**발화 순서 보증 없음**: Windows에서 종료 경로가 다양하여(정상 X 클릭, 작업관리자 강제, OS 셧다운,
패치, 크래시) T1·T3·T4가 어떤 순서로 발화할지 deterministic 보증 없음. T2(FlushOnly)는 비종료
이벤트이므로 종료 경로와 독립.

**중복 진입 방어 (T1/T3/T4 — 종료 경로 한정)**: 3개 종료 트리거가 동일한 private method
`FlushAndDeinit()`을 호출하되, 첫 호출에서 `bDeinitFlushInProgress: FThreadSafeBool` 플래그를
set → 이후 진입은 `return`. (`FThreadSafeBool` 사용 — R3: T3 등이 비-게임 스레드에서 발화 가능)
- 첫 종료 트리거가 발화하면 flush 시작
- 다른 종료 트리거가 도중에 추가 발화해도 무시 (중복 flush 차단)
- `FlushAndDeinit()` 종료 시 플래그는 reset하지 않음 (재진입 영구 차단; Subsystem 생애주기 전체)

**flush 동작 (FlushAndDeinit)**: `bDeinitFlushInProgress` set 후 in-flight `Saving`을
`TFuture<>::Get()`으로 동기 대기 (timeout = `DeinitFlushTimeoutSec`, R2에서 하한 1.0s → **2.0s**
상향). 타임아웃 시 in-flight abandon — 직전 정상 슬롯 무탈하므로 Pillar 4 보장 유지.

**대체 가정 명시**: `ApplicationWillEnterBackgroundDelegate`는 *Windows에서 비-trigger* (no-op
listener 등록은 가능하나 발화 없음). 본 GDD는 모바일 미지원 (`technical-preferences.md` 명시:
PC Windows 단일 타깃). 미래에 모바일 포팅 시 lifecycle 재검토 필요 (당시 GDD revision).

### Interactions with Other Systems

#### 1. Time & Session System (#1) — bidirectional

| 방향 | 데이터 | 소유 | 시점 |
|---|---|---|---|
| Time → Save | `FSessionRecord` (전체 struct) | Time 생산 / Save 직렬화 | `ESaveReason` 어떤 reason의 모든 save trigger |
| Save → Time | 이전 `FSessionRecord` | Save 역직렬화 / Time 소비 | 서브시스템 `Initialize()`에서 1회, Time의 `ClassifyOnStartup()` 실행 전 |

- Time은 시작 시 Save/Load의 `Get<FSessionRecord>()` accessor 호출. `FreshStart` 모드면 default-constructed `FSessionRecord` 반환 (`SessionUuid = FGuid::NewGuid()`, `DayIndex = 1`, `NarrativeCount = 0`, `LastSaveReason = ""`, `PlaythroughSeed = FMath::Rand()`)
- Time이 `IncrementNarrativeCount()` 후 **즉시** `SaveAsync(ESaveReason::ENarrativeEmitted)` 호출 — §5의 cap-leak-free atomic commit
- Time은 `FSessionRecord` 콘텐츠를 의미적으로 소유; Save/Load는 순수 durability layer

#### 2. Game State Machine (#5) — inbound (load 완료 신호만)

GSM은 save 데이터를 직접 읽지 않음 — Initialize 후 typed slice accessor 사용.

| 방향 | 신호 | 시점 |
|---|---|---|
| Save → GSM | `OnLoadComplete(bool bFreshStart, bool bHadPreviousData)` (one-shot delegate) | `Initialize()` 종료 후 — `bFreshStart`는 신규 시작 여부, `bHadPreviousData`는 슬롯 파일이 하나라도 존재했었는지 (`true` = 손상으로 인한 FreshStart, `false` = 진짜 첫 실행). 두 신호 조합으로 GSM과 Title&Settings UI가 분기 — `(true, false)` = 첫 실행 → 표준 Dawn / `(true, true)` = 손상 fallback → "이끼 아이는 깊이 잠들었습니다" 분기 / `(false, _)` = 정상 resume |

#### 3. Growth Accumulation Engine (#7) — bidirectional

| 방향 | 데이터 | 시점 |
|---|---|---|
| Save → Growth | `FGrowthState` USTRUCT (TagVector, CurrentStage, LastCardDay, TotalCardsOffered, FinalFormId) | Growth `Initialize()`에서 1회 |
| Growth → Save | 갱신된 누적 | 카드 건넴 후 → `SaveAsync(ESaveReason::ECardOffered)` 트리거 |

- Growth 데이터는 `UMossSaveData` 안에 typed sub-struct로 거주. Save/Load는 의미를 모르고 직렬화만

#### 4. Card System (#8) — inbound (read only)

Card는 일별 카드 이력을 저장에서 읽음 (재진입 시 "오늘 핸드는 이미 건넴" UI 표시). 직접 저장 트리거하지 않음 — 트리거는 카드-건넴 사이드이펙트 후 Growth가 발행.

#### 5. Dream Trigger (#9) — bidirectional

| 방향 | 데이터 | 시점 |
|---|---|---|
| Save → Dream | 언록된 dream IDs + `SessionCountToday` 카운터 | Dream `Initialize()`에서 1회 |
| Dream → Save | 갱신된 dream 상태 | dream unlock 후 → `SaveAsync(ESaveReason::EDreamReceived)` |

#### 6. Farewell Archive (#17) — bidirectional

| 방향 | 데이터 | 시점 |
|---|---|---|
| Save → Farewell | 최종 형태 + dream journal + 카드 이력 | Farewell 화면 진입 시 read |
| Farewell → Save | Farewell archive 페이지 (생성 일자, 최종 형태 스냅샷) | 1회 → `SaveAsync(ESaveReason::EFarewellReached)` |

#### 7. Dream Journal UI (#13) and Title & Settings UI (#16) — read only

두 UI는 typed accessor로 `UMossSaveData` 읽기만. 쓰기 없음. Title&Settings UI는 `FreshStart` 모드가 *이전에 플레이한 install*에서 감지될 때 "이끼 아이는 깊이 잠들었습니다" 문자열 소비 (§Edge Cases 참조).

## Formulas

이 시스템은 밸런스 곡선이 아닌 **결정 규칙(decision rules)** 위주이다. 4개의 핵심 결정 규칙을 mathematical formalization으로 명세한다.

### Formula 1 — Trusted Slot Selection

로드 시 어느 슬롯을 신뢰 슬롯으로 선택할지 결정.

`TrustedSlot = argmax_{S ∈ {A, B} ∩ Valid} S.WriteSeqNumber`

규칙:
- 양쪽 모두 `Valid`이면 `WriteSeqNumber`가 큰 쪽
- 동률(예: 두 슬롯 모두 `WriteSeqNumber = 0` — 신규 install 후 첫 시작 직후) → 슬롯 A
- 한쪽만 `Valid`이면 그 슬롯
- 양쪽 모두 `Invalid`이면 `⊥` (= `FreshStart` 모드 진입)

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| SlotA.WriteSeqNumber | A.WSN | `uint32` | `[0, 2³² − 1]` | 슬롯 A의 헤더에서 읽은 단조 증가 카운터 |
| SlotB.WriteSeqNumber | B.WSN | `uint32` | `[0, 2³² − 1]` | 슬롯 B의 헤더에서 읽은 단조 증가 카운터 |
| Valid(S) | — | `bool` | T/F | Formula 3 결과 |
| TrustedSlot | T | enum `{A, B, ⊥}` | — | 신뢰 슬롯 또는 FreshStart 신호 |

**Output Range**: `{A, B, ⊥}`. 정상 진행 시 매 저장마다 ping-pong이라 두 슬롯의 `WSN` 차이는 정확히 1.

**Examples**:
- `A.WSN = 42, B.WSN = 41, 둘 다 Valid` → `T = A`
- `A.WSN = 42 (Invalid: CRC mismatch), B.WSN = 41 (Valid)` → `T = B`
- `A.WSN = 0, B.WSN = 0, 둘 다 Invalid` → `T = ⊥` (`FreshStart`)
- `A.WSN = 5, B.WSN = 5 (둘 다 Valid)` → `T = A` (동률 → A)

### Formula 2 — Next WriteSeqNumber

저장 시 새 헤더에 기록할 `WriteSeqNumber` 결정.

`NextWSN = max(SlotA.WriteSeqNumber, SlotB.WriteSeqNumber) + 1`

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| SlotA.WSN | A.WSN | `uint32` | `[0, 2³² − 1]` | 슬롯 A의 현재 WSN (Invalid이면 0 처리) |
| SlotB.WSN | B.WSN | `uint32` | `[0, 2³² − 1]` | 슬롯 B의 현재 WSN (Invalid이면 0 처리) |
| NextWSN | N | `uint32` | `[1, 2³² − 1]` | 새 슬롯에 기록할 WSN |

**Output Range**: 정상 시작 시 `N = 1` (둘 다 0 또는 Invalid). 정상 진행 시 매 저장마다 +1.

**Overflow Safety (R2 BLOCKER 5 — wrap detection + reset 절차)**:

21일 동안 매 카드 건넴(7~21회) + 꿈(0~5회) + Farewell(1회) + Time 내러티브(최대 3회) ≤ 약
30회 저장. `uint32` 상한 4.29×10⁹ 대비 8 자릿수 여유. 21일 게임 기간에서 오버플로 비현실적
(약 1.4억 년 연속 플레이 필요).

**그러나** GDD가 장기 canonical reference이므로 R1 "명시만 하고 처리 없음" 입장 거부됨. R2에서
다음 wrap detection + reset 절차 채택 (옵션 b — uint32 유지, schema bump 없음, wrap 발생 시
양 슬롯 동기 reset):

1. Save 절차 Step 3 NextWSN 계산:
   ```
   int64 candidate = (int64)max(SlotA.WSN, SlotB.WSN) + 1
   if (candidate > UINT32_MAX):
       trigger WSN_RESET 절차 (아래)
       NextWSN = 1
   else:
       NextWSN = (uint32)candidate
   ```
2. **WSN_RESET 절차** (cold path — 정상 게임 21일에 도달 불가능):
   - 현재 in-memory `UMossSaveData` 그대로 유지
   - 양 슬롯에 동일 페이로드를 *순차* commit:
     - 첫 번째 commit: SlotA.WSN = 1 (atomic rename)
     - 두 번째 commit: SlotB.WSN = 2 (atomic rename) — 정상 ping-pong 재시작
   - 양 commit 사이 크래시 시: SlotA만 valid (WSN=1), SlotB는 옛 값 (max). Formula 1이 max값 슬롯을 선택해 *오래된 데이터* 로드 → **이는 본 reset 절차의 known race window** (실제 도달 불가능하지만 명문화)
   - 진단 로그 `Warning` severity (`UE_LOG(LogMossSaveLoad, Warning, "WSN wrap-around reset triggered after %llu saves", TotalSaveCount)`)
3. UI 가시 동작 없음 (Pillar 1)

**왜 (a) uint64가 아닌 (b) reset인가**: (a)는 헤더 layout 변경 → SaveSchemaVersion bump 필요
→ V1→V2 마이그레이션 코드 추가 필요. 21일 게임에서 wrap 도달 불가능한 점을 감안하면 schema
bump 비용 > reset 절차 cold-path 코드. (b)가 최소 침습.

**왜 (c) "Formula 1 동률 보호 의존"이 아닌가**: wrap 후 두 슬롯 WSN이 `(2³²−1)` vs `1`이 되어
*동률이 아닌* 큰 값(=오래된 슬롯)이 선택되는 역전 가능. R1이 정확히 이 시나리오를 위험으로
지적. 동률 보호로 해소 안 됨.

**Example**:
- `A.WSN = 42, B.WSN = 41 → N = 43`
- `A.WSN = 0 (Invalid), B.WSN = 7 (Valid) → N = 8`
- `둘 다 Invalid (FreshStart 후 첫 저장) → N = 1`

### Formula 3 — Slot Validity Predicate

슬롯 파일이 로드 가능한지 판정하는 boolean 술어. 로드 절차 §3-Step 2의 채점 기준.

```
Valid(S) =  Exists(S)
          ∧ S.Magic == 0x4D4F5353
          ∧ S.PayloadByteLength == ActualPayloadSize(S)
          ∧ S.PayloadCrc32 == FCrc::MemCrc32(S.Payload, 0)
          ∧ MinCompatibleSchemaVersion ≤ S.SaveSchemaVersion ≤ CURRENT_SCHEMA_VERSION
          ∧ NOT IOError(S)
```

> **R3 — `EngineArchiveCompatible` 조건 제거 (R2 BLOCKER 4 대체)**: trial `LoadGameFromMemory()`
> 실행은 순환 의존(Validity 판정을 위해 전체 역직렬화), 2× 메모리 peak, 상태 오염 위험을 야기.
> 대신 Load 절차 Step 6의 기존 fallback 경로가 archive mismatch를 **implicit하게 처리**: CRC 통과
> 후 `LoadGameFromMemory()` 실패 → 해당 슬롯 Invalid → 반대 슬롯 fall back (1회 한정). 이
> 경로는 archive 버전 drift를 포함한 *모든* 역직렬화 실패를 포괄적으로 처리하며, 별도 trial run이
> 불필요하다. 관련 AC는 fallback 검증(VALIDITY_CRC_MISMATCH_FALLBACK + ENGINE_ARCHIVE
> round-trip)으로 재구성.

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| Exists(S) | — | `bool` | T/F | 파일 존재 여부 (`IFileManager::FileExists`) |
| S.Magic | M | `uint32` | — | 파일 첫 4바이트 |
| S.PayloadByteLength | L | `uint32` | `[0, 10⁸)` | 헤더에 기록된 페이로드 크기 |
| ActualPayloadSize(S) | L' | `uint32` | — | 실제 파일 크기 − 22 (헤더 길이) |
| S.PayloadCrc32 | C | `uint32` | — | 헤더에 기록된 CRC |
| FCrc::MemCrc32(S.Payload) | C' | `uint32` | — | 페이로드를 다시 계산한 CRC |
| S.SaveSchemaVersion | V | `uint8` | `[0, 255]` | 헤더 필드 |
| `MinCompatibleSchemaVersion` | V_min | `uint8` (const) | 현재 `1` | §Tuning Knobs |
| `CURRENT_SCHEMA_VERSION` | V_max | `uint8` (const) | 현재 `1` | 빌드 시 고정 |
| IOError(S) | — | `bool` | T/F | read syscall 실패 여부 |

**Output Range**: `{true, false}`. **6개 조건** (R3: EngineArchiveCompatible 제거, 7→6) 중 하나라도 false면 전체 false. **단락 평가(short-circuit) 권장 순서**: `NOT IOError` → `Exists` → `Magic` → `PayloadByteLength` → `MinCompatible ≤ V ≤ CURRENT` → `PayloadCrc32`. 진단 품질을 위해 *I/O 실패 / 파일 부재* 같은 명확한 사유를 먼저 평가, CRC처럼 비싼 검증은 마지막.

> **Archive 버전 호환성 처리 (R3 경로)**: UE 5.6 → 5.6.1 핫픽스가 `FArchive` 버전을 변경하면
> CRC는 통과하지만 Load 절차 Step 6의 `LoadGameFromMemory()` 단계에서 실패한다. 이 실패는
> Load 절차 마지막 문장의 fallback 경로("반대 슬롯으로 fall back, 2단계부터 재실행")로 자연스럽게
> 처리된다. **별도 Validity 조건이 아닌 Load fallback이 archive mismatch를 implicit 흡수.**

**Example**:
- 정상 슬롯 → 6개 모두 통과 → `Valid = true`
- 디스크 비트 부패로 페이로드 1바이트 변경 → `C' ≠ C` → `Valid = false`
- 베타 빌드 (V=2) 후 정식 빌드 (V_max=1) 복귀 → `V > V_max` → `Valid = false` (downgrade rejected)
- 마이그레이션 chain 정리 후 (`V_min = 5`)에 V=3 슬롯 발견 → `V < V_min` → `Valid = false` (too old)
- **R3 경로**: UE 5.6에서 저장한 슬롯을 5.6.1 핫픽스로 로드 → Validity 6개 조건 통과 → Load Step 6 `LoadGameFromMemory()` 실패 → 해당 슬롯 Invalid 처리 → 반대 슬롯 fallback

### Formula 4 — Migration Step Count

마이그레이션 chain 적용 횟수.

`Steps = CURRENT_SCHEMA_VERSION − S.SaveSchemaVersion`

**전제**: Formula 3이 `Valid(S) = true`로 통과했고, `S.SaveSchemaVersion < CURRENT_SCHEMA_VERSION`. (정확히 동일하거나 미래·구버전이면 마이그레이션 불필요 또는 다른 분기.)

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| S.SaveSchemaVersion | V | `uint8` | `[V_min, V_max − 1]` (전제 보장) | 로드된 슬롯의 스키마 버전 |
| CURRENT_SCHEMA_VERSION | V_max | `uint8` | 현재 `1` | 빌드 시 고정 |
| Steps | k | `uint8` | `[1, V_max − V_min]` | `MigrateFromV<n>ToV<n+1>` 함수 호출 횟수 |

**Output Range**: `[1, V_max − V_min]`. 현재 `V_max = V_min = 1`이라 마이그레이션 chain 자체가 비어 있음 (k = 0인 경우는 이 식 진입 전 분기됨).

**Migration sequence**:
```
For i ∈ [V, V_max − 1]:
    Call MigrateFromV<i>ToV<i+1>(InOutSaveData)
```

이 실행은 in-memory만 — 디스크 쓰기는 다음 정상 `SaveAsync` 트리거에서 발생.

**Example**:
- V = 1, V_max = 1 → 마이그레이션 분기 미진입 (`Loading` → `Idle` 직행)
- V = 2, V_max = 5 → `Steps = 3` → `MigrateFromV2ToV3`, `V3ToV4`, `V4ToV5` 순차 호출

### Formula Constants Summary

| Const | Value (현재) | Unit | Tuning Knob (§Tuning) |
|---|---|---|---|
| `CURRENT_SCHEMA_VERSION` | 1 | uint8 (build-time const) | ✗ (빌드 시 hardcode) |
| `MinCompatibleSchemaVersion` | 1 | uint8 | ✓ |
| `MAGIC_NUMBER` | `0x4D4F5353` ('MOSS') | uint32 (build-time const) | ✗ |
| `HEADER_BYTE_LENGTH` | 22 | bytes (build-time const) | ✗ (헤더 구조 변경 시만) |
| `MAX_PAYLOAD_BYTES` | `10⁷` (**10 MB — R2 BLOCKER 2 적용**) | bytes (sanity cap) | ✓ |
| `SAVE_DIRECTORY` | `MossBaby/SaveGames/` | path | ✗ |

## Edge Cases

### 경계값 (Boundary)

- **E1 — 진짜 첫 실행** (`bHadPreviousData = false`): 양 슬롯 파일 모두 미존재. `FreshStart` 모드, `OnLoadComplete(true, false)` 발행. GSM은 표준 Dawn 분기. 다음 첫 저장은 슬롯 A로 (Step 2 FreshStart 예외).
- **E2 — FreshStart 후 첫 저장**: in-memory active pointer = `⊥` 상태에서 첫 `SaveAsync` 호출 → Step 2 예외로 target = A. WSN = max(0, 0) + 1 = 1. 두 번째 저장부터 정상 ping-pong (B로).
- **E3 — `WriteSeqNumber` 오버플로 (R2 BLOCKER 5 — 정책 변경)**: 21일 게임에서 도달 불가능 (약 1.4억년 연속 플레이 필요). R1 "GDD 명시만 하고 별도 처리 없음" 거부됨. **R2 정책**: Formula 2의 wrap detection + WSN_RESET 절차 발동 (Formula 2 §Overflow Safety 참조). 양 슬롯에 동일 in-memory 페이로드를 순차 atomic commit (WSN=1 → WSN=2)으로 reset. 진단 로그 `Warning` severity. UI 가시 동작 없음. **Known race window**: 두 commit 사이 크래시 시 SlotA(WSN=1) valid + SlotB(WSN=max) 옛 값 → Formula 1 argmax가 SlotB 선택 → 오래된 데이터 로드. 이 race는 *정상 게임에 도달 불가능*하므로 cold-path 한정으로 명문화만.

### Crash / Power loss (Pillar 4 핵심)

- **E4 — Temp 파일 쓰기 도중 전원 차단**: 부분 쓰여진 `.tmp` 파일만 남고 `.sav`는 무탈. 다음 시작 시 Load는 두 `.sav`만 평가, `.tmp`는 무시. Save 절차 Step 10의 cleanup이 다음 시작 시 leftover 정리. **결과**: 직전 정상 상태로 silent restore. 데이터 손실 = 마지막 저장 사이의 1 commit (~1회 카드 건넴).
- **E5 — Rename 도중 전원 차단**: NTFS `MoveFileExW`는 atomic — 결과는 OLD 또는 NEW 둘 중 하나 (중간 상태 없음). OLD면 E4와 동일, NEW면 새 슬롯이 정상 commit된 상태. **결과**: 어느 쪽이든 일관 상태.
- **E6 — 마이그레이션 chain 도중 크래시**: 마이그레이션은 in-memory only. 디스크는 변함 없음. 다음 시작 시 동일 마이그레이션 재시도. **결과**: 무한 재시도하지만 손상 없음.
- **E7 — 마이그레이션 함수 예외 또는 silent corrupt**: `MigrateFromVN→VN+1` 함수 진입 전 in-memory `UMossSaveData`를 deep copy로 백업 → 체인 어느 단계에서든 throw 시 백업으로 롤백 → 해당 슬롯은 `Invalid`로 마크 → 반대 슬롯 시도 (Load 절차). 또한 각 마이그레이션 함수 종료 후 `IsSemanticallySane()` 사후 검증 호출 (DayIndex ∈ [1,21], NarrativeCount ∈ [0, cap], LastSaveReason 빈 문자열 OR `ESaveReason` 이름 집합 일원) — 실패 시 동일 fallback 경로.

### Corruption

- **E8 — 한쪽 슬롯 페이로드 1바이트 부패** (디스크 비트 부패): Formula 3의 CRC 검증이 미스매치 감지 → 그 슬롯 `Invalid` → 반대 슬롯이 신뢰 슬롯이 됨. 플레이어는 1 commit 손실(~1 카드 건넴). **알림 없음** (Pillar 1).
- **E9 — 한쪽 슬롯 헤더 손상** (Magic 또는 길이 필드 깨짐): 동일 처리 — Formula 3에서 Invalid → 반대 슬롯 폴백.
- **E10 — 양 슬롯 동시 Invalid** (극단): `FreshStart` 모드 진입, `bHadPreviousData = true` 신호 (적어도 한 파일이 존재했었음). Title&Settings UI가 "이끼 아이는 깊이 잠들었습니다" 분기로 분기. 새 시작 옵션 제공. **유일한 시스템 가시 실패 표현**.
- **E11 — 외부 앱이 우연히 동명 파일 생성** (Magic+CRC 우연 일치, 본문은 `UMossSaveData`가 아님): Formula 3 Valid 통과 → Step 6 `LoadGameFromMemory` 실패 → 반대 슬롯 fall back (1회 한정 — 보정 #3) → 그것도 실패면 `FreshStart` 진입. 무한 재귀 차단됨.

### Schema

- **E12 — Schema downgrade** (베타 빌드 V=2 → 정식 V=1 복귀): Formula 3에서 `V > CURRENT` → `Invalid` → 반대 슬롯 시도 (정식 빌드로 만든 V=1 슬롯이 있으면 그것으로 복원). 둘 다 V=2면 `FreshStart` + `bHadPreviousData = true`. 베타 테스트 데이터는 보존 안 됨 — 정식 빌드는 미래 schema 모름.
- **E13 — Schema too old** (마이그레이션 chain 정리 후 v3 슬롯 발견, `MinCompat = 5`): Formula 3에서 `V < MinCompat` → `Invalid` → E12와 동일 처리.

### Filesystem

- **E14 — 디스크 풀** (저장 디렉터리 볼륨 가득): `FFileHelper::SaveArrayToFile`이 temp 단계에서 실패 → 저장 실패 보고. **동일 조건 영구 silent retry 위험** → 정책: **연속 3회 실패 시 내부 `bSaveDegraded = true` 플래그 세팅** + 진단 로그. 직전 슬롯은 무탈하므로 Pillar 4 계약 유지. UI 알림 없음 (Pillar 1). 다음 성공 시 플래그 해제.
- **E15 — 비 ASCII 경로** (한국어 사용자명, 예: `C:\Users\봉구\AppData\Local\...`): UE 5.6 표준 `FFileHelper::SaveArrayToFile` + `IPlatformFile::MoveFile`은 UTF-16 변환 의존. **AC로 통합 테스트 필수** (한국 사용자가 itch.io/Steam의 주 시장 중 하나).
- **E16 — 저장 디렉터리 미존재** (첫 실행): Save 절차 첫 호출 전 `IPlatformFile::CreateDirectoryTree(SAVE_DIRECTORY)` 자동 호출. 실패 시 E14와 동일 (degraded 플래그).
- **E17 — AV 격리 / 폴더 권한 박탈**: 외부 영역 (시스템 책임 외). 다음 로드에서 양 슬롯 missing → `FreshStart`. **GDD 명시 정책**: AV 격리는 OS/사용자 환경 책임. 개발 체크리스트에 "Steam/itch.io 배포 시 AV 화이트리스트 등록 안내" 항목 추가.
- **E18 — 동일 머신 동시 인스턴스 2개**: 각각 다른 in-memory `UMossSaveData`를 가지고 같은 슬롯에 atomic rename 경쟁. NTFS `MoveFileExW`가 직렬화 — 마지막 rename이 승자. **결과**: 한쪽 인스턴스의 변경은 commit, 다른 쪽은 다음 SaveAsync에서 자기 in-memory로 덮어씀. 데이터 일관성 보장 안 됨. **단일 인스턴스 강제는 Window & Display Management(#14)의 책임** — Save/Load는 관여 안 함.

### Race / Lifecycle

- **E19 — Loading 중 SaveAsync 호출**: in-memory 오브젝트 부분 구성 상태이므로 위험. **정책**: Loading 상태에서 들어오는 모든 SaveAsync 요청은 **드롭** (silent). Loading은 단발성 < 100ms이므로 실용적 손실 없음.
- **E20 — Migrating 중 SaveAsync 호출**: Migrating은 게임 스레드 blocking이지만 같은 프레임 내 다른 코드가 호출 가능. **정책**: pending 큐(최대 1개, coalesce)에 보류 → Migrating 종료 시 `Idle` 진입 직후 1회 flush.
- **E21 — 강제 flush가 Migrating 중 트리거**: 3개 종료 트리거 (T1/T3/T4, §States 참조) 중 첫 번째가 Migrating 상태 감지 시 `FlushAndDeinit()` 진입 → `bDeinitFlushInProgress` set → 마이그레이션 완료를 동기 대기 → 이후 in-flight Saving이 있다면 `TFuture::Get()` 동기 대기 (timeout `DeinitFlushTimeoutSec`) → 둘 다 끝나면 즉시 `SaveSync` 1회 (마지막 in-memory 상태) → 핸들러 반환. 다른 종료 트리거가 도중에 추가 발화해도 `bDeinitFlushInProgress` 검사로 즉시 return (중복 flush 차단). **T2(FlushOnly)는 Migrating 중에도 비동기 SaveAsync를 큐잉만 하므로 blocking 없음 — E20과 동일 경로.**
- **E22 — 100회 SaveAsync가 50ms 내 발화** (이론적 최악): worker 스레드는 동시 1개만 처리. 99개는 coalesce되어 마지막 하나의 in-memory 상태로 1회 I/O. **보장**: 강제 flush 트리거 시 최대 대기 시간 = 1회 I/O 완료 시간 (~100ms HDD, ~10ms SSD).
- **E23 — `Deinitialize()` 무한 대기 위험**: `TFuture::Get()`이 슬로우 디스크/AV 환경에서 수 초 걸릴 수 있음. Windows는 종료 시 5초 grace period. **정책**: `Deinitialize()`의 강제 flush는 **3초 타임아웃**. 타임아웃 시 in-flight save abandon, 직전 정상 슬롯은 무탈하므로 Pillar 4 유지. 진단 로그 기록.

### Cross-system Atomicity

- **E24 — `NarrativeCount` increment 후 `SaveAsync` 전 크래시**: 시스템 보장 없음. 데이터 손실 = NarrativeCount 1 leak (재시작 후 cap에 1 추가 발화 가능). **방어**: Time GDD §Formula 5가 "increment + SaveAsync 같은 함수 호출 안에서" contract 강제. Time §Acceptance Criteria로 검증.
- **E25 — Growth 태그 벡터 / Dream unlock 동일 패턴**: §5 일반화 contract — 각 downstream GDD가 자체 AC로 검증해야 함 (Growth GDD AC, Dream GDD AC). Save/Load는 contract만 명시.
- **E26 — `LastSaveReason` 중복 이벤트 감지**: 재시작 후 Time이 `FSessionRecord.LastSaveReason == "ECardOffered"`를 발견하면, GSM은 "이 카드 건넴 이벤트가 직전 commit에 이미 처리됨" 판단 → 중복 Farewell·Dream 트리거 방지 (Time §E6). atomic rename이 LastSaveReason과 본문 모두 보장하므로 두 정보가 항상 일관.

### External / Tampering 비정책

- **E27 — 플레이어가 `.sav` 파일 수동 편집**: CRC32가 단순 편집을 높은 확률로 감지 → Invalid → 반대 슬롯 fall back → 둘 다 편집되었으면 `FreshStart`. **명시적 비정책**: 이는 *anti-tamper 효과가 아니라 무결성 검증의 부수효과*. 본 시스템은 tampering을 탐지·차단하지 않음. Time `NO_FORWARD_TAMPER_DETECTION` AC와 일관 — Save/Load는 무결성만, 의도 추론 없음. QA 보고 시 "버그 아님 — 설계 의도"로 분류.
- **E28 — Forward time tamper** (Day 1 → Day 21 시스템 시각 점프): Save/Load 영역 외. Time GDD Rule 4 + `NO_FORWARD_TAMPER_DETECTION` AC가 명시적 수용. Save/Load는 단순히 변경된 `FSessionRecord`를 직렬화할 뿐, 의도 추론 없음.

### 하드웨어 / 환경 특이

- **E29 — 네트워크 드라이브에 Save 디렉터리 위치** (드물지만 Steam roaming): 네트워크 단절 시 atomic rename 실패 → E14 retry 정책 적용. 사용자가 명시적으로 비표준 위치를 선택한 것이므로 책임 외, 단 silent degraded는 보장.
- **E30 — `SessionCountToday` 리셋 시점 모호** (cross-ref Time §E15): Save/Load 영역 외 — Dream Trigger GDD가 리셋 기준(DayIndex 변화 vs UTC 자정 vs 로컬 자정) 결정. Save/Load는 그 결과를 단순 직렬화.

## Dependencies

### Upstream Dependencies (이 시스템이 의존하는 대상)

| 의존 대상 | 의존 강도 | 인터페이스 | 비고 |
|---|---|---|---|
| UE 엔진 API | Hard — engine | `USaveGame`, `UGameplayStatics::SaveGameToMemory/LoadGameFromMemory`, `FFileHelper::SaveArrayToFile`, `IPlatformFile::MoveFile`, `IFileManager::FileExists/CreateDirectoryTree`, `FCrc::MemCrc32`, `FPlatformProcess::UserSettingsDir`, **R2 lifecycle (Windows 호환)**: `UGameViewportClient::CloseRequested`, `FSlateApplication::OnApplicationActivationStateChanged`, `FCoreDelegates::OnExit`, `UGameInstance::Shutdown`. ~~`ApplicationWillEnterBackgroundDelegate`~~ R2 제거 (모바일 전용 — Windows 발화 안 함, BLOCKER 1). `Async(EAsyncExecution::ThreadPool, ...)`, `TFuture<>`, `UGameInstanceSubsystem` lifecycle | 표준 UE 5.6 API. atomic rename은 Windows NTFS `MoveFileExW(MOVEFILE_REPLACE_EXISTING)`에 의존 |
| OS — Windows NTFS rename atomicity | Hard — OS | `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` | 동일 볼륨 동일 디렉터리 rename 한정. 비 Windows·비 NTFS 플랫폼 확장 시 재검증 필요 (현재 Windows 전용) |

**Foundation Layer 시스템 간 의존 없음** (Time, Data Pipeline, Save/Load, Input Abstraction은 모두 병렬 설계 가능).

### Downstream Dependents (이 시스템에 의존하는 시스템)

| 시스템 | 의존 강도 | 인터페이스 | 데이터 방향 |
|---|---|---|---|
| **Time & Session System (#1)** | **Hard — bidirectional** | `Get<FSessionRecord>()` accessor + `SaveAsync(ESaveReason::ENarrativeEmitted/ECardOffered/EDreamReceived/EFarewellReached)` | Save → Time (1회 read on Init), Time → Save (모든 reason의 commit) |
| **Game State Machine (#5)** | Hard — inbound 신호 | `OnLoadComplete(bool bFreshStart, bool bHadPreviousData)` one-shot delegate | Save → GSM (단방향) |
| **Growth Accumulation Engine (#7)** | Hard — bidirectional | typed slice accessor (`Get<FGrowthState>()` 또는 동등) + `SaveAsync(ECardOffered)` | 양방향 (read on Init, write per card) |
| **Card System (#8)** | Soft — read only | typed slice accessor (per-day card history) | Save → Card (단방향) |
| **Dream Trigger System (#9)** | Hard — bidirectional | typed slice accessor (unlocked dreams + `SessionCountToday`) + `SaveAsync(EDreamReceived)` | 양방향 |
| **Farewell Archive (#17)** | Hard — bidirectional | typed slice accessor (final form + journal + card history) + `SaveAsync(EFarewellReached)` | 양방향 (read on enter, write once on archive) |
| **Dream Journal UI (#13)** | Soft — read only | typed slice accessor (dreams) | Save → UI (단방향) |
| **Title & Settings UI (#16)** | Soft — read only + special signal | `OnLoadComplete` 신호의 `bHadPreviousData` 분기 | Save → UI (단방향). `(bFreshStart=true, bHadPreviousData=true)` 시 "이끼 아이는 깊이 잠들었습니다" 분기 |
| **Window & Display Management (#14)** | None | — | Save/Load는 window state를 알지 못함. 단일 인스턴스 강제는 W&D의 책임 (E18 참조) |

### Bidirectional Consistency Notes

양방향 의존성 원칙(`design/CLAUDE.md` 규칙)에 따라 각 downstream GDD에 다음 참조가 명시되어야 함:

- **Time & Session GDD (#1)**: 이미 §Dep #1에 명시됨 — "Save/Load Persistence (#3)가 `FSessionRecord` 직렬화·역직렬화 책임. `SaveSchemaVersion` 마이그레이션은 Save/Load 책임. `LastSaveReason` 마지막 쓰기 atomic durability는 Save/Load contract." **R3 수정 시점에**: Time GDD가 Save/Load의 `ESaveReason` 4-value enum 정식 정의를 cross-ref해야 함 (현재 Time §E6은 후보 명시).
- **Game State Machine GDD**: "Save/Load (#3)의 `OnLoadComplete(bFreshStart, bHadPreviousData)` 신호를 시작 분기에 사용. `(true, false)` = 신규 Dawn, `(true, true)` = 손상 fallback Title 분기, `(false, _)` = 정상 resume."
- **Growth Accumulation GDD**: "Save/Load (#3)의 atomicity contract §5 일반화 항목 준수 — 태그 벡터 increment + `SaveAsync(ECardOffered)`는 같은 game-thread 함수 호출 안에서. 검증 AC 1개 이상 포함."
- **Card System GDD**: "Save/Load (#3)에서 per-day 카드 이력 read only. Card는 직접 Save trigger하지 않음 (Growth 경유)."
- **Dream Trigger GDD**: "Save/Load (#3)의 atomicity contract 준수 — dream unlock + `SaveAsync(EDreamReceived)`는 같은 함수. `SessionCountToday` 리셋 기준은 Dream이 결정하고 Save/Load는 결과만 직렬화 (E30 참조)."
- **Farewell Archive GDD**: "Save/Load (#3)의 atomicity contract 준수 — archive page 추가 + `SaveAsync(EFarewellReached)`. 페이지 데이터(최종 형태, 일자, journal snapshot)는 Save/Load의 typed slice."
- **Dream Journal UI GDD**: "Save/Load (#3)의 dream slice를 read only로 표시. 쓰기 없음."
- **Title & Settings UI GDD**: "Save/Load (#3)의 `OnLoadComplete(bFreshStart=true, bHadPreviousData=true)` 신호 시 '이끼 아이는 깊이 잠들었습니다' 화면 분기. 정확한 카피·아트는 Title&Settings UI GDD가 결정."

### Data Ownership

| 데이터/타입 | 소유 시스템 | 직렬화 책임 | 읽기 허용 | 쓰기 허용 |
|---|---|---|---|---|
| `UMossSaveData` (USaveGame container) | Save/Load (이 GDD) | Save/Load | Save/Load 내부만 | Save/Load 내부만 |
| `FSessionRecord` (의미적) | Time & Session (#1) | Save/Load | 모든 typed slice consumer | Time (유일) |
| `FGrowthState` (의미적) | Growth (#7) | Save/Load | Card·UI | Growth (유일) |
| Dream unlocked IDs | Dream Trigger (#9) | Save/Load | Dream Journal UI·Farewell | Dream Trigger (유일) |
| Farewell archive page | Farewell Archive (#17) | Save/Load | Title·Farewell UI | Farewell Archive (유일) |
| `ESaveReason` enum (정의) | **Save/Load (이 GDD)** | — | Time, GSM, Growth, Dream, Farewell | — (정의 자체) |
| `SaveSchemaVersion` const | **Save/Load (이 GDD)** | — | Save/Load 내부만 | Save/Load 빌드 (hardcode) |
| `MinCompatibleSchemaVersion` const | **Save/Load (이 GDD)** | — | Save/Load 내부만 | Save/Load §Tuning Knobs |
| `bSaveDegraded` 내부 플래그 | **Save/Load (이 GDD)** | 직렬화 안 함 (런타임만) | 진단 로그 | Save/Load 내부 |
| `WriteSeqNumber` (헤더 필드) | **Save/Load (이 GDD)** | 헤더에 저장 | Save/Load 내부만 | Save/Load (유일) |
| `bHadPreviousData` 신호 | **Save/Load (이 GDD)** | 신호 (영속화 안 함) | GSM·Title&Settings UI | Save/Load (유일) |

### Soft Dependencies (참조 가능, 의존 없음)

- **Stillness Budget (#10)**: Save/Load는 budget을 구독하지 않음 (저장은 인지 불가능한 백그라운드 동작). 단, 강제 flush 트리거 시점이 Stillness Budget의 이벤트 슬롯과 충돌하지 않도록 Title&Settings UI가 분기 시 budget 체크 (UI 책임).
- **Lumen Dusk Scene (#11)**: 무관.

## Tuning Knobs

모든 노브는 `UMossSaveLoadConfig : UDeveloperSettings`에 노출되어 Project Settings에서 조정 가능 (Time GDD OQ-5 결정 — `UDeveloperSettings` 채택). 코드 재빌드 없이 ini 파일 수정으로 변경.

| 노브 | 기본값 | 안전 범위 | 너무 높으면 | 너무 낮으면 | 참조 |
|---|---|---|---|---|---|
| `MinCompatibleSchemaVersion` | 1 | `[1, CURRENT_SCHEMA_VERSION]` | 오래된 마이그레이션 코드 정리 → 구버전 세이브 무효화 (E13 재현 위험) | 정리 안 함 → 마이그레이션 chain 비대 | Formula 3, 4 |
| `MaxPayloadBytes` | **`10⁷` (10 MB — R2 BLOCKER 2)** | `[10⁵, 5×10⁷]` (100 KB ~ 50 MB; R2 상한 10⁹→5×10⁷ 강하) | 메모리 peak ≈ 3× cap (live `UMossSaveData` + `FMossSaveSnapshot` + OutBytes). 50 MB cap × 3 = 150 MB, 프로젝트 1.5 GB 한도의 10% — 한계. 50 MB 초과 = ADR 재예산 필수 | 정상 세이브가 sanity cap 초과해 Invalid 처리 (현재 예상 < 100 KB이므로 100배 여유) | Formula 3, sanity check |
| `SaveFailureRetryThreshold` | 3 | `[1, 10]` | degraded 모드 진입 전 silent retry 과다 → 디스크 풀에서 CPU 낭비 | 일시적 글리치(AV 검사 1회 등)에 degraded 진입 → 회복 가능한 상황을 영구 degraded로 오인 | E14 |
| `DeinitFlushTimeoutSec` | 3.0 sec | `[2.0, 5.0]` (R2: 하한 1.0 → 2.0 상향) | Windows 5초 종료 grace period 초과 시 OS가 프로세스 강제 종료 → 진단 로그조차 못 남김 | 슬로우 디스크에서 정상 저장도 abandon → 마지막 commit 손실 가능. R2 하한 2.0s 근거: AV 검사 1회 평균 ~500ms + 정상 I/O ~100ms + lifecycle handler dispatch overhead ~200ms = 안전 마진 1.2s 확보 | E23 |
| `bDevDiagnosticsEnabled` | `false` (Shipping) / `true` (Development) | bool | 운영 환경에서 verbose 로깅 → I/O 오버헤드, 로그 파일 크기 증가 | 디버깅 시 정보 부족 | 진단 로그 전반 |

**비-tunable 상수** (빌드 시 hardcode, 런타임 변경 금지):
- `CURRENT_SCHEMA_VERSION`: 빌드 시점에 결정. 코드에 hardcode (`#define CURRENT_SCHEMA_VERSION 1`)
- `MAGIC_NUMBER`: `0x4D4F5353` 영구 고정
- `HEADER_BYTE_LENGTH`: 헤더 구조 변경 = 새 schema version
- `SAVE_DIRECTORY`: `MossBaby/SaveGames/` 영구 고정 (변경 시 기존 사용자 세이브 분리됨)
- `SLOT_COUNT`: 2 (이론적 tunable이나 dual-slot 알고리즘 자체가 2를 전제 — 변경 시 §Detailed Design 재설계 필요)

### 노브 상호작용

- **`MinCompatibleSchemaVersion` ↔ `CURRENT_SCHEMA_VERSION`**: 항상 `MinCompat ≤ CURRENT`. MinCompat = CURRENT면 마이그레이션 chain 자체가 빈 상태 (정상). MinCompat > CURRENT는 빌드 오류로 처리.
- **`SaveFailureRetryThreshold` ↔ `DeinitFlushTimeoutSec`**: 강제 flush 시 retry threshold가 timeout 안에 다 소진되어야 의미 있음. `Threshold × (typical I/O time)` < `DeinitFlushTimeoutSec` 권장 — 예: 3 × 100ms = 300ms ≪ 3000ms ✓.
- **`MaxPayloadBytes` ↔ 콘텐츠 부피 (R2 BLOCKER 2 재산정)**: 이 게임 콘텐츠(카드 30장 메타데이터 + 꿈 텍스트 50개 + 카드 이력 + 성장 벡터 등) 전체 직렬화는 **< 100 KB 예상**. R2: 10 MB cap = 100배 여유. **메모리 peak budget 검증**: 저장 동시 in-memory 점유 ≈ 3× payload (live `UMossSaveData` + `FMossSaveSnapshot` 값 복사 + `OutBytes` 직렬화 버퍼). 10 MB × 3 = 30 MB peak — 프로젝트 1.5 GB 한도의 2%. Lumen HWRT BVH(150 MB) + UE baseline(약 800 MB) + 이 cap의 peak(30 MB) ≈ 1.0 GB 점유, 0.5 GB 마진 안정 확보. R1 BLOCKER 2 해소.

- **`MaxPayloadBytes` ↔ Companion Mode 등 미래 확장**: 50 MB 초과 필요 시 ADR 재예산 필수. 단순히 노브 올리는 것 금지 — 1.5 GB 한도와 Lumen·UE baseline 재계산 후 결정.

### Playtest Tuning Priorities

이 시스템은 **밸런스가 없음** — 튜닝 우선순위는 모두 운영 안정성 영역. 일반 playtest 대상 아님.

- **1순위**: `SaveFailureRetryThreshold` — 실기 (Win10/11, AV 다양) 환경에서 false positive degraded 빈도 측정 후 조정
- **2순위**: `DeinitFlushTimeoutSec` — Windows 종료 grace period 측정 후 안전 마진 확보
- **기타**: 나머지는 변경 가능성 매우 낮음

### Migration: tuning knob 추가/제거 시

새 schema version (`CURRENT_SCHEMA_VERSION` 증가) 작업 절차:
1. `UMossSaveData`에 새 필드 추가 (UPROPERTY 필수)
2. `CURRENT_SCHEMA_VERSION` += 1
3. `MigrateFromV<OLD>ToV<NEW>(UMossSaveData* InOut)` 함수 추가
4. 함수 종료 후 `IsSemanticallySane()` 검증 통과 확인
5. CI에서 모든 이전 버전 → 새 버전 round-trip 테스트 자동 실행

## Visual/Audio Requirements

이 시스템은 **인프라 레이어**로 직접적인 Visual/Audio 출력이 없다. 모든 시각·음향 영향은 다음과 같이 다른 시스템이 중계한다 (Player Fantasy §2 일관 — "성공도 실패도 알리지 않음"):

- **저장 성공·실패 모두 무음·무시각** (Pillar 1) — 인디케이터·아이콘·사운드 일체 금지
- **양 슬롯 동시 corruption (E10)** 시 Title&Settings UI(#16)가 "이끼 아이는 깊이 잠들었습니다" 화면 분기 — 정확한 카피·아트·BGM은 Title&Settings UI GDD에서 정의

> **📌 Asset Spec — 해당 없음**: 이 GDD 자체는 asset 생성 필요 없음. Title&Settings UI GDD가 작성될 때 해당 화면의 asset spec 생성.

## UI Requirements

이 시스템은 UI를 **소유하지도, 발행하지도 않는다**. 모든 UI 트리거는 두 신호를 통해 외부로 위임:

1. **`OnLoadComplete(bFreshStart, bHadPreviousData)`** — Initialize 종료 후 1회 발행
   - `(true, false)` 첫 실행 → GSM이 표준 Dawn 분기
   - `(true, true)` 손상 fallback → Title&Settings UI(#16)가 "이끼 아이는 깊이 잠들었습니다" 분기
   - `(false, _)` 정상 resume → GSM이 마지막 상태 복원
2. **(없음 — 진행 중 indicator 없음)** — `Saving` / `Migrating` / `Loading` 상태 모두 시각 노출 금지

> **📌 UX Flag — 해당 없음**: Save/Load 자체는 UX 스펙 불필요. 단, Title&Settings UI(#16) 작성 시 위 4-quadrant 신호를 어떻게 해석해 분기할지 `/ux-design` 작성 필수.

## Acceptance Criteria

> 이 섹션은 QA 테스터가 GDD를 읽지 않고도 독립적으로 검증할 수 있도록 Given-When-Then 형식으로 작성됨. 각 criterion은 `UE Automation Framework` 헤드리스 실행 가능 여부로 분류됨.

### Criterion ATOMIC_PINGPONG
- **GIVEN** 현재 신뢰 슬롯이 A이고 WSN_A = 42, WSN_B = 41인 Idle 상태
- **WHEN** `SaveAsync(ECardOffered)`가 호출되고 완료됨
- **THEN** 슬롯 B의 `.sav` 파일이 생성·갱신, 헤더 `WriteSeqNumber = 43`, 슬롯 A 무변경
- **Evidence**: 저장 완료 후 파일 헤더 hex 덤프 — B.WSN = 43, A.WSN = 42 확인
- **Type**: AUTOMATED — **Source**: §Detailed Design Step 2-3, Formula 2

### Criterion FRESHSTART_FIRST_WRITE_TO_A
- **GIVEN** 양 슬롯 파일이 전혀 없는 신규 install (FreshStart 모드, active pointer = ⊥)
- **WHEN** 첫 번째 `SaveAsync` 완료
- **THEN** `MossData_A.sav`만 생성, WSN = 1. 두 번째 저장은 슬롯 B에 WSN = 2
- **Evidence**: 파일 시스템 관찰 + 두 번째 저장 후 B.WSN = 2 헤더 확인
- **Type**: AUTOMATED — **Source**: §Detailed Design Step 2 (FreshStart 예외), E1, E2

### Criterion WSN_FORMULA_MAX_PLUS_ONE
- **GIVEN** A.WSN = 7 (Valid), B.WSN = 0 (Invalid·파일 없음)
- **WHEN** `SaveAsync` 완료
- **THEN** 대상 슬롯 헤더 `WriteSeqNumber = 8` (max(7, 0) + 1)
- **Evidence**: 헤더 hex 덤프 — WSN 필드 오프셋(bytes 6–9) 값 = 8
- **Type**: AUTOMATED — **Source**: Formula 2

### Criterion CRC32_GENERATED_ON_WRITE
- **GIVEN** 유효한 `UMossSaveData`를 가진 Idle 상태
- **WHEN** `SaveAsync` 완료
- **THEN** `.sav` 헤더의 `PayloadCrc32`가 페이로드에 대한 `FCrc::MemCrc32()` 재계산값과 일치
- **Evidence**: 저장된 파일 읽고 페이로드 CRC를 독립 CRC32 구현으로 재계산 — 헤더값과 일치 확인
- **Type**: AUTOMATED — **Source**: §Detailed Design Step 5, Formula 3

### Criterion ATOMIC_RENAME_NO_TMP_REMAINS
- **GIVEN** 저장이 정상 완료
- **WHEN** 저장 완료 직후 파일 시스템 조회
- **THEN** `MossData_<target>.tmp`가 존재하지 않음 (rename 또는 cleanup 완료)
- **Evidence**: 저장 완료 콜백 직후 `IFileManager::FileExists("MossData_B.tmp")` = false
- **Type**: AUTOMATED — **Source**: §Detailed Design Step 8, Step 10, E5

### Criterion SLOT_SELECT_HIGHEST_WSN
- **GIVEN** 슬롯 A (WSN=10, Valid), 슬롯 B (WSN=11, Valid) 두 파일 존재
- **WHEN** 시스템 `Initialize()` 실행
- **THEN** 신뢰 슬롯 = B. `OnLoadComplete(bFreshStart=false, ...)` 발행. 로드된 데이터는 B의 페이로드
- **Evidence**: 두 슬롯에 서로 다른 `NarrativeCount` 심은 뒤 로드 후 `GetSessionRecord().NarrativeCount` = B의 값 확인
- **Type**: AUTOMATED — **Source**: Formula 1, Load 절차 Step 3

### Criterion VALIDITY_CRC_MISMATCH_FALLBACK
- **GIVEN** 슬롯 A의 페이로드 한 바이트를 0xFF로 변조 (CRC 불일치 유발), 슬롯 B는 Valid
- **WHEN** `Initialize()` 실행
- **THEN** 슬롯 A는 Invalid, 슬롯 B로 폴백. `OnLoadComplete(false, _)` 발행
- **Evidence**: CRC 손상 헬퍼로 A 변조 → 로드 후 B의 페이로드 NarrativeCount 반환 확인. 에러 다이얼로그 없음
- **Type**: AUTOMATED — **Source**: Formula 3, E8

### Criterion FALLBACK_ONE_TIME_LIMIT
- **GIVEN** 슬롯 A와 B 모두 CRC 불일치로 Invalid
- **WHEN** `Initialize()` 실행
- **THEN** `FreshStart` 모드 진입. `OnLoadComplete(bFreshStart=true, bHadPreviousData=true)` 발행. 무한 재시도 없음
- **Evidence**: OnLoadComplete 델리게이트 캡처로 두 파라미터 확인. 재귀 카운터 로그 grep — fallback 횟수 ≤ 1
- **Type**: AUTOMATED — **Source**: Load 절차 마지막 문장, E10, E11

### Criterion FRESH_FIRST_RUN_SIGNALS
- **GIVEN** 슬롯 파일이 전혀 없는 환경
- **WHEN** `Initialize()` 실행
- **THEN** `OnLoadComplete(bFreshStart=true, bHadPreviousData=false)` 발행
- **Evidence**: OnLoadComplete delegate 캡처로 두 파라미터 확인
- **Type**: AUTOMATED — **Source**: E1, Dependencies §GSM

### Criterion SCHEMA_FUTURE_VERSION_REJECTED
- **GIVEN** 슬롯 A에 `SaveSchemaVersion=255` (미래 빌드 시뮬), 슬롯 B는 Valid V=1
- **WHEN** `Initialize()` 실행 (`CURRENT=1`)
- **THEN** 슬롯 A는 Invalid (Formula 3: V > CURRENT), 슬롯 B 로드. `OnLoadComplete(false, _)` 발행
- **Evidence**: 헤더 패치 헬퍼로 A의 SaveSchemaVersion 조작 후 로드 결과 확인. A 페이로드 데이터 미반환
- **Type**: AUTOMATED — **Source**: Formula 3 (V > CURRENT), E12

### Criterion SCHEMA_TOO_OLD_REJECTED
- **GIVEN** `MinCompatibleSchemaVersion = 3`으로 설정된 빌드. 슬롯 A는 V=1
- **WHEN** `Initialize()` 실행
- **THEN** 슬롯 A는 Invalid (V < MinCompat). 둘 다 too-old면 FreshStart + `bHadPreviousData=true`
- **Evidence**: `UMossSaveLoadConfig.MinCompatibleSchemaVersion = 3` 세팅 후 V=1 파일 로드 → Invalid 판정 로그 grep
- **Type**: AUTOMATED — **Source**: Formula 3 (V < MinCompat), E13

### Criterion ENGINE_ARCHIVE_VERSION_MISMATCH (R2 NEW, R3 fallback 경로로 재구성)
- **GIVEN** UE 5.6 빌드로 저장된 정상 슬롯 (CRC 일치, `SaveSchemaVersion = 1` 일치). 게임 빌드를 5.6.1 핫픽스로 재컴파일하여 archive version이 변경됨
- **WHEN** 5.6.1 빌드의 `Initialize()` 실행
- **THEN** Formula 3 Validity 6개 조건은 통과 (CRC·schema 모두 일치). **Load 절차 Step 6** `LoadGameFromMemory()` 단계에서 역직렬화 실패 (nullptr 반환 또는 필드 깨짐) → 해당 슬롯 Invalid 처리 → 반대 슬롯 fallback. 둘 다 실패면 FreshStart + `bHadPreviousData=true`. **데이터 silent migration 시도 금지** — 이는 schema migration이 아님
- **Evidence**: (1) UE 5.6에서 저장한 `.sav` 파일을 source control에 미리 보관 → (2) 5.6.1로 빌드한 테스트 바이너리에서 해당 파일 로드 → (3) `LoadGameFromMemory()` 결과 nullptr 또는 `IsSemanticallySane()` 실패 → fallback 경로 진입 확인
- **Type**: MANUAL (release-gate one-off — CI에서 두 엔진 버전 동시 운영 비쌈. 엔진 이중 빌드 환경 준비 시 AUTOMATED 격상)
- **Source**: R1 BLOCKER 4 (unreal-B4), R3 fallback implicit 처리

### Criterion ENGINE_ARCHIVE_5_6_TO_5_6_1_ROUND_TRIP (R2 NEW, R3 fallback 경로로 재구성)
- **GIVEN** 동일 `UMossSaveData` in-memory 인스턴스
- **WHEN** UE 5.6 빌드로 저장 → UE 5.6.1 빌드로 로드 (archive version 변경 시뮬)
- **THEN** 다음 3가지 결정적 outcome 중 하나만 발생, **silent semantic drift 절대 금지**:
  1. 정상 로드 (archive version 호환됨) — 모든 필드 round-trip 일치 확인
  2. Load Step 6 `LoadGameFromMemory()` 실패 → 해당 슬롯 Invalid → 반대 슬롯 폴백
  3. 양쪽 모두 Invalid → FreshStart 모드 진입
  (위 1–3 외의 결과 발생 시 = AC 실패. 예: 부분 로드, NarrativeCount 0으로 reset 등)
- **Evidence**: 양쪽 엔진 버전 빌드 환경에서 round-trip 매트릭스 테스트. 결과를 1·2·3 분류, 그 외 발생 시 즉시 BLOCKING bug
- **Type**: MANUAL (release-gate one-off — stable hotfix 시점마다 수동 검증)
- **Source**: R1 BLOCKER 4, R3 fallback implicit 처리

### Criterion MIGRATION_SANITY_CHECK_POST
- **GIVEN** V=1 → V=2 마이그레이션 chain. `MigrateFromV1ToV2`가 `DayIndex = 0` (범위 외)을 만들도록 버그 주입
- **WHEN** `Initialize()` 실행
- **THEN** `IsSemanticallySane()` 실패 → 슬롯 Invalid → 반대 슬롯 폴백
- **Evidence**: 버그 migrator 주입 후 sane-check 실패 로그 grep. B 슬롯 데이터 로드 확인
- **Type**: AUTOMATED — **Source**: E7

### Criterion MIGRATION_EXCEPTION_ROLLBACK
- **GIVEN** `MigrateFromV1ToV2`가 중간에 예외 throw하도록 주입
- **WHEN** 마이그레이션 chain 실행 중 예외 발생
- **THEN** deep-copy 백업 롤백 → 슬롯 Invalid → 반대 슬롯 폴백. 원본 디스크 파일 무변경
- **Evidence**: 예외 주입 후 디스크 파일 mtime 불변 확인. 로그에 rollback 항목
- **Type**: AUTOMATED — **Source**: E7 (deep-copy backup 정책)

### Criterion NARRATIVE_COUNT_ATOMIC_COMMIT (R2 — BLOCKER 3 재작성)
- **GIVEN** `NarrativeCount = 2` (cap = 3), Idle 상태
- **WHEN** Time 서브시스템의 **public** `IncrementNarrativeCountAndSave()` 호출 완료
- **THEN** 저장된 슬롯의 `NarrativeCount = 3`. 추가로 다음 *구조적 invariant* 만족:
  1. Time 서브시스템 헤더에서 `IncrementNarrativeCount()` / `TriggerSaveForNarrative()` 두 method가 `private:` 가시성으로 선언됨
  2. `IncrementNarrativeCountAndSave()`만 `public:` 가시성
  3. 코드베이스 전체에서 `IncrementNarrativeCount\b` 호출 callsite ≤ 2건 (정의 1 + wrapper 1)
- **Evidence (R2 — 3중 검증)**:
  1. **저장 round-trip**: 저장 완료 후 파일 로드하여 `FSessionRecord.NarrativeCount = 3` 확인
  2. **Static analysis (CI gate)**: `grep -rE "IncrementNarrativeCount\b|TriggerSaveForNarrative\b" Source/` 결과 분석 — 각 함수의 private 호출 count = 정의 1 + wrapper 1, 외부 callsite = 0
  3. **Negative unit test (friend class)**: friend class로 `IncrementNarrativeCount()` 단독 호출 → process kill 시뮬 → 재시작 후 in-memory `NarrativeCount = 3`이지만 디스크 = 2 (cap leak 입증). Wrapper 사용 시 동일 시나리오에서 디스크 = 3 (no leak)
- **Type**: AUTOMATED + STATIC_ANALYSIS — **Source**: §Detailed Design §5 Atomicity Contracts (R2 NarrativeCount 항목), E24
- **R1 → R2 변경**: "함수 호출 스택 로그로 동일 frame 내 두 호출 확인" 증거 *제거* — Release 빌드 인라인·심볼 제거에서 무효. 구조적 가시성 + grep + negative test로 *구현 불변량*을 증거화

### Criterion E4_TEMP_CRASH_RECOVERY (R2 BLOCKER 6 — Test Infra 의존성 명시)
- **GIVEN** `SaveArrayToFile` 직후 프로세스 kill (전원 차단 시뮬). `.tmp`만 존재, `.sav`는 직전 정상
- **WHEN** 다음 `Initialize()` 실행
- **THEN** Load는 `.sav`만 평가, `.tmp` 무시. 직전 정상 슬롯 데이터 로드. `OnLoadComplete(false, _)`
- **Evidence**: kill 후 재시작 → 로드된 NarrativeCount = kill 직전 commit 값과 일치. `.tmp`는 다음 저장 Step 10에서 정리
- **Type**: AUTOMATED — **Source**: E4, E5
- **R2 BLOCKER 6 — Test Infrastructure dependency 명시**: 본 AC를 AUTOMATED로 유지하려면 §Test Infrastructure Needs의 *"프로세스 kill 테스트 래퍼"*가 **BLOCKING 우선순위로 격상되어야 함** (R1 R2 격상 적용). 프로세스 kill은 UE Automation Framework in-process에서 달성 불가 — 별도 래퍼 (e.g., subprocess spawn + SIGKILL/TerminateProcess + 재시작 검증)가 sprint 0에 준비되지 않으면 본 AC는 자동 MANUAL로 강등된다. 강등 시 release-gate one-off로 전환 (수동 검증, 매 릴리스 1회).

### Criterion E14_DISK_FULL_DEGRADED_FLAG
- **GIVEN** 목 파일시스템이 `SaveArrayToFile` 연속 3회 실패 반환하도록 주입
- **WHEN** 3회 연속 `SaveAsync` 실패
- **THEN** 내부 `bSaveDegraded = true` 플래그 세팅. UI 에러 메시지 없음. 직전 정상 슬롯 무탈
- **Evidence**: 진단 로그에 "degraded" grep. `bSaveDegraded` accessor = true. 화면 캡처로 에러 UI 부재 확인
- **Type**: AUTOMATED — **Source**: E14, Pillar 1

### Criterion E15_NON_ASCII_PATH
- **GIVEN** 사용자명이 `봉구` (한국어)인 Windows 계정 — `UserSettingsDir()` 경로에 비 ASCII 포함
- **WHEN** `SaveAsync` 및 다음 `Initialize()`
- **THEN** 저장·로드 모두 성공. 로드된 데이터가 저장 전과 일치
- **Evidence**: 비 ASCII 경로 환경에서 통합 테스트 — round-trip 전후 `FSessionRecord` 필드 전체 비교
- **Type**: AUTOMATED — **Source**: E15

### Criterion E19_LOADING_STATE_DROPS_SAVES
- **GIVEN** `Initialize()` 실행 중 (Loading 상태)
- **WHEN** `SaveAsync(ECardOffered)` 호출
- **THEN** 요청 silent drop. Loading 완료 후 추가 저장 없음. 진단 로그에 "drop" 항목
- **Evidence**: Loading 중 SaveAsync 삽입 → 완료 후 슬롯 WSN이 Loading 전 값과 동일 확인
- **Type**: AUTOMATED — **Source**: E19

### Criterion E20_MIGRATING_STATE_COALESCE
- **GIVEN** Migrating 상태 (게임 스레드 blocking 중)
- **WHEN** `SaveAsync(ECardOffered)` 3회 연속 호출
- **THEN** 3 요청은 pending 큐(최대 1개)에 coalesce. Migrating 종료 후 Idle 진입 직후 1회 저장만 실행
- **Evidence**: 저장 완료 카운터로 I/O 호출 횟수 = 1 확인. 최신 in-memory 상태가 파일에 반영
- **Type**: AUTOMATED — **Source**: E20

### Criterion E22_100X_COALESCE
- **GIVEN** Idle 상태, 1회 I/O 진행 중
- **WHEN** 50ms 내 `SaveAsync` 100회 추가 호출
- **THEN** 실제 I/O는 ≤ 2회 (진행 중 1 + coalesce 1). 최종 파일은 100번째 호출 시점 in-memory 반영
- **Evidence**: I/O 카운터 델리게이트로 실제 SaveArrayToFile 호출 횟수 측정 = ≤ 2
- **Type**: AUTOMATED — **Source**: E22

### Criterion E23_DEINIT_TIMEOUT
- **GIVEN** `DeinitFlushTimeoutSec = 3.0`. 목 파일시스템이 4초 지연 후 완료
- **WHEN** 서브시스템 `Deinitialize()` 호출
- **THEN** 3초 경과 후 in-flight save abandon. `Deinitialize()` 반환. 진단 로그에 타임아웃 항목. 직전 정상 슬롯 무탈
- **Evidence**: `Deinitialize()` 반환까지 경과 시간 = 3 ± 0.5초 측정. 로그에 "timeout" grep. 이전 슬롯 mtime 불변
- **Type**: AUTOMATED — **Source**: E23

### Criterion NO_SLOT_UTIL_SAVETOFILE
- **GIVEN** Save/Load 관련 모든 C++ 소스 파일
- **WHEN** 코드 리뷰 — `UGameplayStatics::SaveGameToSlot` 검색
- **THEN** `SaveGameToSlot` 또는 `LoadGameFromSlot` 호출이 존재하지 않음. 저장은 반드시 `SaveGameToMemory` + `FFileHelper::SaveArrayToFile` + `IPlatformFile::MoveFile` 경로만
- **Evidence**: `grep -r "SaveGameToSlot\|LoadGameFromSlot" Source/` = 0건
- **Type**: CODE_REVIEW — **Source**: §Detailed Design Step 7-8, Dependencies §UE API

### Criterion NO_ANTITAMPER_LOGIC
- **GIVEN** Save/Load 관련 모든 C++ 소스 파일
- **WHEN** 코드 리뷰 — 시간 검증·의도 추론·tamper 탐지 패턴 검색
- **THEN** forward time jump 감지, 플레이어 의도 추론, "치트 감지" 코드 미존재. CRC32는 무결성만 — 반환값이 "tampering" 메시지에 연결되지 않음
- **Evidence**: `grep -ri "tamper\|cheat\|anti.*detect\|forward.*jump" Source/` = 0건. E27 정책 주석 존재 확인
- **Type**: CODE_REVIEW — **Source**: E27, E28, Time GDD `NO_FORWARD_TAMPER_DETECTION`

### Criterion NO_SAVE_INDICATOR_UI
- **GIVEN** 게임이 실행 중이고 `SaveAsync` 활발히 호출되는 상황
- **WHEN** 화면 전체 영역 관찰
- **THEN** "저장 중...", 플로피 디스크 아이콘, 스피너 등 세이브 진행 표시 UI 미표시. 에러 다이얼로그 없음
- **Evidence**: 저장 트리거 동안 화면 녹화 → UI 검수. `grep -r "SavingWidget\|SaveIndicator\|저장 중" Source/UI/` = 0건
- **Type**: CODE_REVIEW — **Source**: Pillar 1, Player Fantasy

### Criterion COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY (R2 NEW — D2 negative AC)
- **GIVEN** Day 13 compound event 시뮬: 3개 reason `SaveAsync` 호출이 짧은 시간 (≤ 50ms) 내에 순차 발생 — `ECardOffered` → `EDreamReceived` → `ENarrativeEmitted`
- **WHEN** 두 번째 호출 직전에 worker thread fault injection으로 `EDreamReceived` commit만 fail
- **THEN** 첫 번째 commit (`ECardOffered`) → disk 정상 반영. 두 번째 (`EDreamReceived`) → fail (silent retry, R3 fallback 동작). 세 번째 (`ENarrativeEmitted`) → 정상 commit. **결과**: disk에는 ECardOffered 후 ENarrativeEmitted 상태이지만 EDreamReceived는 미반영. **이는 의도된 동작** (Save/Load는 per-trigger atomic만 보장).
- **Evidence**: 3 reason 시뮬레이션 후 disk 슬롯 로드 → `LastSaveReason = "ENarrativeEmitted"` (마지막 성공 commit), Dream unlock 상태는 미반영. AC는 *이 결과가 발생함*을 명시적으로 검증 — sequence atomicity가 *보증되지 않음*을 캡처.
- **Type**: AUTOMATED (negative AC) — **Source**: §Detailed Design §8 (R2 NEW), R1 Disagreement 2 adjudication
- **Note**: Sequence atomicity가 필요한 경우 Game State Machine #5가 BeginCompoundEvent()/EndCompoundEvent() boundary로 in-memory mutation을 batch한 뒤 *마지막에 1회* SaveAsync 호출하는 방식으로 자연스럽게 1 commit 변환. 그것이 GSM의 책임 영역.

---

### Coverage 요약

| 분류 | Criterion 수 |
|---|---|
| Save procedure (atomic, ping-pong, WSN, CRC, rename) | 5 |
| Load procedure (slot select, validity, fallback limit, FreshStart signals) | 4 |
| Schema migration (future/old reject, sanity, exception rollback) | 4 |
| Cross-system atomicity (NarrativeCount cap-leak-free) | 1 |
| Engine archive round-trip (R3 fallback 경로) | 2 |
| Edge cases (E4, E14, E15, E19, E20, E22, E23) | 7 |
| Negative ACs (CODE_REVIEW) | 3 |
| **Total** | **26** (R3: ENGINE_ARCHIVE 2개 + COMPOUND_EVENT 1개 = R2 24개 + 2) |
| AUTOMATED | 19 (73.1%) |
| AUTOMATED (조건부 — 인프라 미준비 시 MANUAL) | 1 (E4_TEMP_CRASH_RECOVERY) |
| CODE_REVIEW | 3 (11.5%) |
| MANUAL (release-gate one-off) | 2 (ENGINE_ARCHIVE 2건) |
| Negative AC (AUTOMATED) | 1 (COMPOUND_EVENT) |

### Coverage Gaps

**Untestable (자동·수동 모두 고비용 또는 영역 외)**:

1. **실제 전원 차단 (E4, E5)** — NTFS write-back cache flush 보장은 실제 하드웨어 kill이 아니면 검증 불가. CI는 프로세스 kill로 시뮬레이션. 실기 검증은 별도 하드웨어 power strip 차단 (Manual one-off, Implementation 단계)
2. **WSN wrap-around (E3)** — `uint32` 상한 도달 비현실적. GDD 명시로 갈음
3. **NTFS atomic rename 보장 자체** — OS 계약, UE 레이어에서 검증 불가. Windows documentation 인용
4. **동시 인스턴스 충돌 (E18)** — Window & Display Management(#14) 책임. Save/Load 단독 AC 없음 (설계 의도)
5. **비 ASCII 경로 (E15)** — CI 환경에 한국어 사용자명 계정 생성 필요. 일반 CI 파이프라인 밖에서 별도 환경 구성

**Uncovered (다른 GDD·통합 테스트 범위)**:

| 항목 | 이유 |
|---|---|
| Growth/Dream/Farewell atomicity 패턴 준수 | 각 downstream GDD §Acceptance Criteria 책임 (Save/Load §5 일반화 contract) |
| Title&Settings UI의 "이끼 아이는 깊이 잠들었습니다" 분기 | Title&Settings UI GDD 책임 |
| Time과의 LastSaveReason round-trip durability | Time GDD 또는 Time-Save integration test |

### Test Infrastructure Needs (구현 전 준비)

| 인프라 | 우선순위 | 용도 | AC 매핑 |
|---|---|---|---|
| **`IMockPlatformFile`** (mock 파일시스템) | **BLOCKING** | 저장 실패 주입, 지연 주입, I/O 카운터 | E14, E22, E23, ATOMIC_RENAME |
| **`CorruptSlotPayload(SlotId, ByteOffset)` 헬퍼** | **BLOCKING** | 한 바이트 변조로 CRC mismatch 유발 | VALIDITY_CRC, FALLBACK_ONE_TIME |
| **`PatchSlotHeader(SlotId, FieldName, Value)` 헬퍼** | HIGH | WSN, Magic, SchemaVersion 강제 세팅 | SCHEMA_FUTURE, SCHEMA_TOO_OLD, SLOT_SELECT |
| **버그 있는 Migrator 주입 인터페이스** (DI) | HIGH | `MigrateFromVNToVN+1` 교체 가능 | MIGRATION_SANITY, MIGRATION_EXCEPTION |
| **`OnLoadComplete` Delegate Capture Helper** | HIGH | bFreshStart / bHadPreviousData 검증 | 로드 관련 6개 AC |
| **프로세스 kill 테스트 래퍼** | **BLOCKING (R2 — BLOCKER 6 격상)** | `.tmp` 단계에서 종료 시뮬레이션. AC E4_TEMP_CRASH_RECOVERY가 AUTOMATED 분류를 유지하려면 본 래퍼가 sprint 0에 준비되어야 함. 미준비 시 AC가 자동 MANUAL로 강등 (release-gate one-off) | E4_TEMP_CRASH_RECOVERY |
| **I/O 호출 카운터** | MEDIUM | SaveArrayToFile 실제 실행 횟수 측정 | E22_100X_COALESCE |

## Open Questions

GDD 작성 중 도출된 미결 항목.

| # | 질문 | 담당 | 해결 시점 |
|---|---|---|---|
| OQ-1 | **Steam Cloud 통합** — Vertical Slice 또는 Full 단계에서 Steam Cloud 도입 시, atomic rename + dual-slot 모델이 Cloud sync와 어떻게 상호작용하는가? Cloud overwrite로 직전 정상 슬롯이 손상될 위험이 있는가? Cloud는 슬롯 A·B 모두 sync해야 하는가? | `unreal-specialist` + `release-manager` (Steam 통합 시점) | VS 또는 Full 단계 ADR |
| OQ-2 | **`MinCompatibleSchemaVersion` bump 정책** — 마이그레이션 chain이 비대해질 때 언제 MinCompat을 올릴지 (예: 메이저 릴리스마다? 분기별? 사용자 base 분포 기반?). 너무 빨리 올리면 구버전 사용자 데이터 증발, 너무 늦게면 chain 부담 증가 | `release-manager` + `qa-lead` | 첫 schema 변경 시점 (ADR-XXXX) |
| OQ-3 | **`bSaveDegraded` 사용자 노출 정책** — 현재는 silent (Pillar 1). 그러나 영구 silent retry 실패 시 사용자가 "왜 이끼 아이가 어제 상태로 돌아갔지?"라고 의아할 수 있음. 다음 정상 startup에서 "지난 번 저장에 문제가 있어 직전 상태로 돌아갔어요" 같은 cozy 톤 알림을 검토할 가치가 있는가? | `narrative-director` + `ux-designer` | Title&Settings UI GDD 작성 시점 또는 Playtest 후 |
| OQ-4 | **Test Infrastructure 우선순위** — §AC Test Infrastructure Needs의 BLOCKING 2개 (`IMockPlatformFile`, `CorruptSlotPayload`)가 첫 AC 구현 전 준비되어야 함. 어느 sprint에 누가 만드는가? Time GDD의 `FMockClockSource`(BLOCKING)와 함께 묶을 수 있는가? | `qa-lead` + `tools-programmer` | Implementation 단계 sprint 0 |
| OQ-5 | **Network drive 정책** (E29) — 명시적으로 미지원 처리 (Title 화면에 "표준 위치 사용 권장" 경고)할지, 현재처럼 best-effort silent degraded만 할지? Steam roaming profile 환경 영향 검토 필요 | `release-manager` | Steam 출시 전 |
| OQ-6 | **Save 디렉터리 경로 변경 — 향후 호환성** — 현재 `MossBaby/SaveGames/`. 향후 게임 이름 변경·spinoff 시 기존 사용자 세이브 마이그레이션 경로? 현재는 변경 시 모든 사용자가 FreshStart로 시작 | `release-manager` | 게임 이름 확정 시점 |
| **OQ-7 (R2 NEW — D1 adjudication)** | **E10 corruption "새 시작" 감정 문법** — 양 슬롯 동시 Invalid 발생 시 `GetLastValidSlotMetadata()` infra hook을 어떻게 소비할지: (a) 여전히 "이끼 아이는 깊이 잠들었습니다" 새 시작 ritual / (b) "당신은 N일을 함께 했습니다" partial farewell ritual / (c) hybrid — 사용자 선택. 본 GDD는 hook만 제공, 결정은 **Farewell Archive GDD #17 + creative-director** territory | `creative-director` + Farewell Archive GDD #17 작성자 | Farewell Archive GDD 작성 시점 |
| ~~OQ-8~~ | ~~Compound event sequence atomicity 책임 분배~~ | — | — | **RESOLVED** (2026-04-19 by [ADR-0009](../../docs/architecture/adr-0009-compound-event-sequence-atomicity.md) — GSM `BeginCompoundEvent` / `EndCompoundEvent` boundary 채택. Save/Load coalesce 정책 활용 → 단일 disk commit. Per-trigger atomicity는 변경 없음) |
