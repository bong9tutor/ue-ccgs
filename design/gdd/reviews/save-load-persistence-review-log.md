# Review Log — `save-load-persistence.md`

이 파일은 `design/gdd/save-load-persistence.md`의 모든 `/design-review` 결과를 시간순으로 누적한다. 새 리뷰 시 가장 최근 항목 아래에 추가하고, 이전 항목은 보존한다.

---

## Review — 2026-04-17 — Verdict: MAJOR REVISION NEEDED

Scope signal: **XL** (cross-cutting concern, 9 downstream 의존, 최소 2개 신규 ADR 예상: `UE::Tasks` vs `TFuture<>`, Windows flush 트리거 조합)
Specialists: systems-designer, game-designer, qa-lead, unreal-specialist, performance-analyst, creative-director (senior synthesis)
Blocking items: **7** (synthesis 후 통합) | Recommended: **22** | Nice-to-have: **10**
Prior verdict resolved: First review
Specialists raw counts: 16 BLOCKING 수집 → creative-director가 7개로 통합·격상·강등

### Summary (creative-director synthesis)

> 이 GDD의 이론적 기반(atomic write + CRC + WSN + schema migration)은 교과서적으로 정확하고 Pillar 1("silent failure") 철학이 전반에 내재화되어 있다 — 이는 드물고 귀중한 강점. 그러나 플랫폼 불가지론적 사고로 작성된 문서가 Windows + UE 5.6 + 21일 resident runtime이라는 구체적 현실에 대입될 때 세 가지 load-bearing 가정이 무너진다: (1) `ApplicationWillEnterBackgroundDelegate`는 모바일 전용이라 primary flush 경로가 Windows에서 실질적 무효, (2) `USaveGame` 엔진 archive 버전 drift (5.6 → 5.6.1 핫픽스) 미처리, (3) `MaxPayloadBytes` 100 MB cap × 3 peak = 300 MB로 프로젝트의 1.5 GB 메모리 예산 위반. 이들은 폴리시 항목이 아니라 토대 가정이다. 나머지 blocker들(증거 rotting, WSN wrap, E4 AUTOMATED 분류, FMossSaveSnapshot semantic)은 중요하지만 2차적이다.

### Top 3 blockers

1. **Windows 데스크톱 lifecycle 재작성** (unreal-B1+B2 + performance-R1 수렴). `FCoreDelegates::ApplicationWillEnterBackgroundDelegate`는 모바일 전용 → Windows에서 발화하지 않음. primary flush 경로가 실질적 무효. 대체: `GameViewportClient::CloseRequested` + `FSlateApplication::OnApplicationActivationStateChanged` + `FCoreDelegates::OnExit` + `UGameInstance::Shutdown` 조합. 세 트리거 간 발화 순서·중복 진입 방어 명세 필수. `DeinitFlushTimeoutSec` 하한 1.0s → 2.0s 상향.

2. **`MaxPayloadBytes` 메모리 예산 위반** (performance-B2). 저장 피크 ≈ 3× payload (live UMossSaveData + FMossSaveSnapshot + OutBytes). 100 MB cap × 3 = 300 MB가 프로젝트 1.5 GB 한도와 Lumen HWRT·UE baseline 고려 시 초과. cap을 **≤ 10 MB**로 낮추거나 1.5 GB 예산 재협상 ADR 필요.

3. **`NARRATIVE_COUNT_ATOMIC_COMMIT` 증거 구조 강제화** (systems-B2 = qa-B1 수렴). "함수 호출 스택 로그로 동일 frame 내 두 호출 확인" 증거가 Release 빌드 인라인·심볼 제거로 무효화. 구조적 강제로 전환: `IncrementNarrativeCountAndSave()` 단일 public method + `IncrementNarrativeCount()`·`SaveAsync()` 각각 private화. AC Evidence를 static-analysis (private caller grep) 또는 단위 테스트 fixture로 재작성.

### Remaining blockers

4. **`USaveGame` 엔진 archive 버전을 Validity 검사에 포함** (unreal-B4). 5.6 → 5.6.1 핫픽스가 archive 버전을 변경하면 CRC는 통과하지만 `LoadGameFromMemory` 내부 예외. Formula 3 Validity 판정에 엔진 archive 버전 검사 추가 또는 별도 failure mode로 분류. 5.6 → 5.6.1 round-trip AC 추가.

5. **Formula 2 WSN wrap-around 정책 명시화** (systems-B1). 21일 범위에서 발생 불가능하나 GDD가 장기 canonical reference임을 감안 시 "명시만 하고 처리 없음"은 불충분. wrap 후 Formula 1이 오래된 슬롯 선택. 택일: (a) `uint32 → uint64`, (b) wrap 감지 시 양 슬롯 동기화 + WSN=1 재설정, (c) wrap 영구 허용·동률 보호 의존 명문화.

6. **`E4_TEMP_CRASH_RECOVERY` AUTOMATED 분류 오류** (qa-B3). 프로세스 kill은 UE Automation Framework in-process에서 달성 불가. MANUAL 재분류 + release-gate one-off로 전환 OR "프로세스 kill 테스트 래퍼"를 Test Infrastructure Needs에서 MEDIUM → BLOCKING으로 격상.

7. **`FMossSaveSnapshot` 값 복사 계약 spec화** (systems-B3 + unreal-R5 수렴). `UMossSaveData`의 TMap/TArray가 shallow copy되면 worker thread에서 GC/rehash UAF 위험. `FMossSaveSnapshot` 필드 목록을 명시적 POD-only (no `UObject*`, no raw heap pointers)로 제약 또는 deep-copy semantic 명문화.

### Recommended (non-blocking, 22건 요약)

- **[qa-B2]** `MIGRATION_EXCEPTION_ROLLBACK` — UE C++ 예외 비활성화 충돌, `EMigrateResult::Failed` / bool 반환 기반으로 재설계.
- **[qa-B4]** `E15_NON_ASCII_PATH` AUTOMATED vs Manual 분류 이중성 해소.
- **[systems-R4]** Formula 3 short-circuit `NOT IOError`를 앞 순서로 이동 (진단 품질).
- **[systems-R5]** `SaveFailureRetryThreshold × DeinitFlushTimeoutSec` 안전 범위 표에 직접 표기 (prose만으로 부족).
- **[game-R1 = OQ-3]** `bSaveDegraded` 사용자 노출 정책을 Save/Load GDD 내에서 결정 (Day 21 Farewell degraded 상태 Pillar 4 위협).
- **[unreal-R1/R2/R3/R4]** `FFileHelper::SaveArrayToFile` `WriteFlags` / `FCrc::MemCrc32` seed / `IPlatformFile`↔`IFileManager` 혼용 / `UDeveloperSettings` Shipping 동작 명시.
- **[unreal-B3 → RECOMMENDED]** `Async(ThreadPool)+TFuture<>` vs `UE::Tasks` ADR (senior가 BLOCKING 대신 ADR로 처리 가능 판단).
- **[qa-R2]** `FALLBACK_ONE_TIME_LIMIT` → `GetFallbackCount()` accessor로 전환.
- **[qa-R4]** `.tmp` 부분 쓰기(header OK, payload truncated) 커버리지 갭 추가.
- **[qa-R5]** UPROPERTY 레이아웃 변경 silent mis-deserialization 커버리지 갭 + SchemaVersion bump 의무 체크리스트.
- **[performance-R2/R3]** E22 HDD 100ms 지연 & AV false-positive degraded 실기 측정 체크리스트화.
- **[performance-R4]** 21일 장기 실행 `TFuture` 메모리 leak AC 추가 ("30회 저장 후 베이스라인 +5% 이내").
- **[systems-R6]** Formula 4 k=0 분기를 Load procedure Step 5에 명시.
- **[systems-R8 + qa-R6 + game-R2]** Cross-GDD atomicity contract 정적 lint AC 추가 (현재 Save/Load 측에서 contract 위반 탐지 불가).
- **[game-R3]** Day 15-20 degraded 상태 → Day 21 Farewell 진입 시 처리 policy 명시.
- **[game-R4]** E10 "새 시작" 감정 문법이 Save/Load 영역 외로 명시적 scope (Title&Settings UI GDD 또는 Farewell Archive로 위임).

### Nice-to-have (10건)

- [systems-N9/N10] Formula 1 동률 scenario 설명 + `HEADER_BYTE_LENGTH` 상수 기호화
- [unreal-N1/N2] `MoveFile→MoveFileExW` UE 소스 확인 + `PLATFORM_WINDOWS` compile guard AC
- [qa-N1/N2/N3] 하드웨어 kill MANUAL regression AC + 스모크 스위트 4개 지정 + `IMockPlatformFile` 범위 제한
- [game-N1/N2] 미래 대규모 migration blocking cozy-tone + cozy 어휘 오염 경고
- [performance-N1] UE 콜드 스타트 (2-5s) vs Pillar 1 tension OQ 추가

### Specialist Disagreements

**1. E10 "새 시작" Pillar 4 betrayal (game-designer BLOCKING vs creative-director RECOMMENDED)**
- game-designer: Day 13 corruption → 13일 침묵 증발 = Pillar 4 직접 위반, BLOCKING. 이 GDD가 Farewell Archive를 슬롯 레이어 위로 올리는 설계를 결정해야 함.
- creative-director: betrayal은 사실이나 "새 시작" 의미는 Farewell Archive GDD #17 + Creative Director territory. OQ-N + infra hook으로 scope 후 RECOMMENDED 강등.
- **사용자 adjudication 필요** (revision 시작 시).

**2. Compound-event 1-commit-loss guarantee (game-designer BLOCKING vs creative-director RECOMMENDED)**
- game-designer: Day 13 카드 + 꿈 + narrative 복합 이벤트에서 Growth commit / Dream miss 시나리오 가능 → "1 commit 손실" 보증 거짓, BLOCKING.
- creative-director: 보증을 *qualify*지 *expand* 아님. Save/Load는 per-trigger atomicity만; sequence atomicity는 GSM #5 책임. §Guarantees에 qualifier + negative AC 추가로 RECOMMENDED 해결.
- **사용자 adjudication 필요** (revision 시작 시).

### Deferrable to Downstream GDDs

Foundation Layer 특성상 다음 항목은 이번 revision에서 결정하지 않는 것이 바람직 — downstream GDD 작성 시 자연스럽게 해소됨:
- E10 감정 문법 → Farewell Archive GDD #17
- Compound-event sequence atomicity → GSM GDD #5
- NarrativeCount cap-enforce deserialization clamp → Dream Trigger GDD #9 (단 방어적 clamp는 이번 revision에 추가 가능)
- Atomicity contract static lint → GSM/Growth/Dream/Farewell 작성 완료 후

### Success criteria for R2

> (1) Windows 전원 off가 3초 flush 중 10초 지점에서 발생해도 재시작 후 ≤1 commit 손실이 결정적으로 재현됨
> (2) 1.5 GB 메모리 ceiling이 save-and-autosave 최악 동시 실행 상황에서 유지됨
> (3) `NARRATIVE_COUNT_ATOMIC_COMMIT`이 인라인 활성화된 Shipping 빌드에서 통과함

---

## R2 — 2026-04-17 — Status: BLOCKER + RECOMMENDED (D1·D2) 적용 완료, Re-review 대기

R2 작업: 7 BLOCKER 모두 해결 + 2 specialist disagreement 사용자 adjudication 결과 RECOMMENDED 적용. Specialist re-spawn 없이 R1 review-log의 합의 사항 직접 반영 (Time R2 선례 적용).

### R1 → R2 변경 요약

**BLOCKER 1 — Windows 데스크톱 lifecycle 재작성** ✅:
- `ApplicationWillEnterBackgroundDelegate` (모바일 전용, Windows no-op) 제거
- 4개 트리거 조합으로 교체: `UGameViewportClient::CloseRequested` (T1) + `FSlateApplication::OnApplicationActivationStateChanged` (T2) + `FCoreDelegates::OnExit` (T3) + `UGameInstance::Shutdown` (T4)
- 발화 순서 deterministic 보증 없음을 명시 + 중복 진입 방어 (`bDeinitFlushInProgress` 플래그, 영구 차단)
- 단일 진입점 `FlushAndDeinit()` private method
- `DeinitFlushTimeoutSec` 하한 1.0s → **2.0s** 상향 (AV 검사 + I/O + dispatch 안전 마진)
- Section §States and Transitions §동시성 규칙 + §Edge Cases E21 + §Tuning Knobs + §Dependencies §UE 엔진 API 4곳 동기 갱신

**BLOCKER 2 — `MaxPayloadBytes` 메모리 예산 위반** ✅:
- `MaxPayloadBytes`: 100 MB → **10 MB** (Default), 안전 범위 [10⁵, 10⁹] → **[10⁵, 5×10⁷]** (50 MB)
- 메모리 peak ≈ 3× cap 명시 (live + snapshot + OutBytes)
- 10 MB × 3 = 30 MB → 1.5 GB의 2% (Lumen 150MB + UE 800MB + cap 30MB ≈ 1.0GB, 0.5GB 마진)
- 50 MB 초과 시 ADR 재예산 필수 명시
- §Formula Constants Summary + §Tuning Knobs + §노브 상호작용 동기 갱신

**BLOCKER 3 — `NARRATIVE_COUNT_ATOMIC_COMMIT` 증거 구조 강제화** ✅:
- "스택 로그로 동일 frame 내 두 호출 확인" 증거 *제거* (Release 빌드 인라인에서 무효)
- 단일 public method `IncrementNarrativeCountAndSave()` + private `IncrementNarrativeCount()`/`TriggerSaveForNarrative()`
- 3중 검증 메커니즘:
  1. 컴파일 타임 (private 가시성 강제)
  2. Static analysis CI gate (grep callsite count)
  3. Negative unit test (friend class로 wrapper 우회 시 leak 입증)
- §Detailed Design §5 + §AC NARRATIVE_COUNT_ATOMIC_COMMIT 동기 갱신

**BLOCKER 4 — `USaveGame` engine archive version Validity 추가** ✅:
- Formula 3에 `EngineArchiveCompatible(S.Payload)` 7번째 조건 추가
- short-circuit 평가 순서 권고 (R1 systems-R4 부분 적용): `NOT IOError → Exists → Magic → ...→ Crc32 → EngineArchiveCompatible`
- 신규 AC 2개:
  - `ENGINE_ARCHIVE_VERSION_MISMATCH` (5.6 vs 5.6.1 빌드 mismatch 탐지)
  - `ENGINE_ARCHIVE_5_6_TO_5_6_1_ROUND_TRIP` (4 분류 outcome — silent semantic drift 차단)

**BLOCKER 5 — Formula 2 WSN wrap-around policy 명시화** ✅:
- 옵션 (b) 채택 — wrap detection + WSN_RESET 절차 (uint32 유지, schema bump 없음)
- 21일 게임에서 도달 불가능 (1.4억년 연속 플레이 필요)이지만 GDD 장기 canonical reference로서 cold-path 절차 명문화
- WSN_RESET: 양 슬롯에 동일 페이로드 순차 commit (WSN=1 → WSN=2)
- Known race window (양 commit 사이 크래시) 명시 — cold path 한정
- 옵션 (a) uint64 / 옵션 (c) tiebreak 의존을 거부한 근거 명시
- §Formula 2 §Overflow Safety + §Edge Cases E3 동기 갱신

**BLOCKER 6 — `E4_TEMP_CRASH_RECOVERY` 분류** ✅:
- AUTOMATED 분류 유지 (강등 안 함)
- 조건부: §Test Infrastructure Needs의 *프로세스 kill 테스트 래퍼*를 MEDIUM → **BLOCKING으로 격상**
- 미준비 시 자동 MANUAL 강등 (release-gate one-off)으로 fallback 정책 명시

**BLOCKER 7 — `FMossSaveSnapshot` value copy contract spec화** ✅:
- POD-only USTRUCT로 명시
- 허용 타입: 기본 정수·부동소수점, FName/FString/FText/FGuid/FDateTime, TArray/TMap of POD, plain USTRUCT 재귀
- 금지 타입: UObject*, TWeakObjectPtr, raw heap, TSharedPtr, lambda
- Snapshot 생성 의무: `FromSaveData()` static + UObject 참조 필드 flatten (예: UDreamDataAsset* → FName DreamId)
- 3중 검증: 컴파일 타임 헤더 분리 / Static analysis grep / static_assert TIsTriviallyCopyable
- §Detailed Design Step 1 Snapshot 단계 spec 확장

### Disagreement Adjudication 결과

**D1 — E10 "새 시작" Pillar 4 betrayal**: **creative-director 입장 채택 → RECOMMENDED 격하**
- 감정 문법 결정은 Farewell Archive GDD #17 + creative-director territory로 위임
- 본 GDD는 infra hook만 제공: `GetLastValidSlotMetadata()` API + `FOnSaveDegradedReached` delegate
- §Detailed Design §7 R2 NEW + §Open Questions OQ-7 추가

**D2 — Compound-event 1-commit-loss guarantee**: **creative-director 입장 채택 → RECOMMENDED 격하**
- §Detailed Design §8 R2 NEW: per-trigger atomicity qualifier ("Save/Load는 per-trigger만; sequence는 GSM #5 책임")
- 신규 negative AC `COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY` (sequence atomicity 없음을 명시 검증)
- §Open Questions OQ-8 추가 (GSM이 BeginCompoundEvent/EndCompoundEvent batch 패턴 채택 여부)

### R2에서 적용 안 된 R1 RECOMMENDED 항목 (22건 중 deferrable)

향후 revision 또는 다른 GDD 작성 시점에 처리:
- [qa-B2] `MIGRATION_EXCEPTION_ROLLBACK` UE C++ 예외 비활성화 충돌 — 재설계 별도 sprint
- [qa-B4] `E15_NON_ASCII_PATH` 분류 이중성 해소
- [systems-R5] knob 안전 범위 표 명시 — 부분 적용 (DeinitFlushTimeout 안전 마진은 명시)
- [game-R1 = OQ-3] `bSaveDegraded` 사용자 노출 정책 — Title&Settings UI GDD까지 미해소
- [unreal-R1/R2/R3/R4] WriteFlags / FCrc seed / IPlatformFile vs IFileManager / UDeveloperSettings Shipping — 구현 단계 ADR
- [unreal-B3 → RECOMMENDED] `Async(ThreadPool)+TFuture<>` vs `UE::Tasks` ADR — 별도 ADR
- [qa-R2-R5, performance-R2-R4, systems-R6-R8, game-R2-R4] 각각 deferrable

### R2 → R3 단계 권고

다음 fresh session에서:
```
/design-review design/gdd/save-load-persistence.md --depth full
```

R2 verdict 기대치: **APPROVED** 또는 **CONCERNS (accepted)**. 7 BLOCKER + D1·D2 처리 완료로 R1 핵심 약점 해소. 추가 BLOCKING 발견 가능성은 R1 미처리 22 RECOMMENDED 중 일부가 BLOCKER로 escalate되는 시나리오만 남음.

### R2 Files Modified

- `design/gdd/save-load-persistence.md` (본 revision — 8 Edit calls)
- `design/gdd/reviews/save-load-persistence-review-log.md` (이 entry 추가)
- `design/gdd/systems-index.md` (Save/Load Status 갱신 예정)
- `production/session-state/active.md` (Foundation Layer Status 갱신 예정)

---

## Review — 2026-04-17 — Verdict: NEEDS REVISION
Scope signal: **M** (국소 수정, 아키텍처 변경 불필요)
Specialists: systems-designer, game-designer, qa-lead, unreal-specialist, performance-analyst, creative-director (senior synthesis)
Blocking items: **4** (R2 신규 도입 기술적 모순) | Recommended: **15** | Nice-to-have: **6**
Prior verdict resolved: R2 — R1의 7 BLOCKER 중 6개 적절 해소, 1개 부분 해소 (FMossSaveSnapshot)

### Summary (creative-director synthesis)

> R1의 7 BLOCKER 중 6개는 실질적으로 잘 해결되었으며, GDD의 이론적 기반(atomic write + CRC + WSN + schema migration)은 여전히 교과서적으로 정확하다. 그러나 R2 수정 과정에서 4개의 새로운 기술적 모순이 도입되었다: (1) `TIsTriviallyCopyable` static_assert가 GDD 자신의 허용 타입(FString, TArray 등)과 직접 모순, (2) `EngineArchiveCompatible` trial `LoadGameFromMemory()`가 순환 의존·2× 메모리·상태 오염 위험 야기, (3) T2 Alt+Tab이 `FlushAndDeinit()` 발동 → `bDeinitFlushInProgress` 영구 set → 이후 모든 종료 flush 차단 (Pillar 4 직접 위반, R2에서 가장 심각한 regression), (4) `FCrc::MemCrc32` seed 미지정 (1줄 수정). 모두 국소적 수정으로 해결 가능하므로 MAJOR REVISION NEEDED가 아닌 NEEDS REVISION.

### Top 4 CRITICAL (blocking)

1. **`TIsTriviallyCopyable<FMossSaveSnapshot>` 자기모순** [systems-designer + unreal-specialist 수렴] — static_assert가 허용 타입(FString, TArray)에 대해 false 반환. `HAS_VALUE_SEMANTIC<>`은 UE 비표준. 검증 2중으로 축소.
2. **`EngineArchiveCompatible` trial run 순환 의존** [4 agent 수렴: systems-designer, game-designer, performance-analyst, unreal-specialist] — Load fallback으로 대체.
3. **T2 Alt+Tab → FlushAndDeinit 영구 잠금** [3 agent 수렴: qa-lead, performance-analyst, systems-designer] — T2를 FlushOnly로 분리, 종료 경로(T1/T3/T4)와 독립.
4. **`FCrc::MemCrc32` seed 미지정** [unreal-specialist] — seed=0 명시.

### Specialist Disagreements

1. **TIsTriviallyCopyable — 1줄 삭제 vs contract 재서술**: unreal-specialist(1줄 삭제) vs systems-designer(BLOCKER). CD 판정: BLOCKER 유지 — "3중 검증" 기술이 실제 2중이면 contract가 거짓.
2. **MIGRATION_EXCEPTION_ROLLBACK — BLOCKER 격상 여부**: systems-designer+qa-lead(격상 권고) vs CD(RECOMMENDED 유지). CD 판정: migration chain 비어있으므로 RECOMMENDED + 첫 schema 변경 전 해결 deadline.

---

## R3 — 2026-04-17 — Status: 4 CRITICAL 수정 완료, Re-review 대기

### R2 → R3 변경 요약

**CRITICAL 1 — `TIsTriviallyCopyable` 모순 해소** ✅:
- static_assert 라인 제거
- "검증 메커니즘 (3중)" → "검증 메커니즘 (2중)" 재서술
- 2중 검증: 컴파일 타임 헤더 격리 + CI grep
- 제거 근거 명시: FString/TArray/TMap은 deep copy semantic이지만 trivially copyable이 아님

**CRITICAL 2 — `EngineArchiveCompatible` trial run 제거** ✅:
- Formula 3에서 7번째 조건 제거 (7→6개 조건)
- Load Step 6 fallback이 archive mismatch를 implicit 처리하는 경로 명시
- AC ENGINE_ARCHIVE_VERSION_MISMATCH, ENGINE_ARCHIVE_5_6_TO_5_6_1_ROUND_TRIP를 fallback 경로 기반으로 재구성
- AC Type: AUTOMATED+INTEGRATION → MANUAL (release-gate one-off)
- Coverage Summary 재계산 (AUTOMATED 19, 조건부 1, CODE_REVIEW 3, MANUAL 2, Negative 1)

**CRITICAL 3 — T2 Alt+Tab FlushOnly 분리** ✅:
- T2는 `FlushOnly()` 호출 (일반 SaveAsync 1회, bDeinitFlushInProgress 미설정)
- `FlushAndDeinit()`은 T1/T3/T4 종료 경로 전용
- `bDeinitFlushInProgress: bool` → `FThreadSafeBool` (T3 등 비-게임 스레드 발화 대응)
- T2 delegate `RemoveAll(this)` 명세 추가
- E21 edge case 갱신 (T2 FlushOnly 경로 명시)
- T2 분리 근거 명시: 데스크톱 상주형 앱에서 Alt+Tab은 정상 사용 흐름

**CRITICAL 4 — `FCrc::MemCrc32` seed 명시** ✅:
- Step 5: `FCrc::MemCrc32(OutBytes.GetData(), OutBytes.Num(), 0)` seed=0 고정
- Formula 3: `FCrc::MemCrc32(S.Payload, 0)` seed=0

### R3 Files Modified

- `design/gdd/save-load-persistence.md` (R3 revision — 7 Edit calls)
- `design/gdd/reviews/save-load-persistence-review-log.md` (이 entry 추가)
- `design/gdd/systems-index.md` (Save/Load Status R3 갱신)

### R3 → R4 권고

다음 fresh session에서:
```
/design-review design/gdd/save-load-persistence.md --depth lean
```

R3 verdict 기대치: **APPROVED**. 4 CRITICAL 해결 + R1의 7 BLOCKER 모두 처리 완료. lean depth로 충분 — 15 RECOMMENDED는 구현 단계 또는 downstream GDD 작성 시 자연 해소 예정.

---

## Review — 2026-04-17 — Verdict: APPROVED
Scope signal: **M+** (2 upstream API, 4 formulas, 9 downstream dependents, ADR 1–2개 예상)
Specialists: [lean mode — single-session analysis, no specialist agents]
Blocking items: **0** | Recommended: **3** (신규) + 15 (R1 deferred 유지)
Prior verdict resolved: **Yes** — R2 review의 4 CRITICAL 모두 적절 해소 확인

### Summary

R3 revision이 R2 review의 4 CRITICAL(TIsTriviallyCopyable 모순, EngineArchiveCompatible trial run 순환, T2 Alt+Tab FlushAndDeinit 영구 잠금, FCrc seed 미지정)을 모두 적절히 해소. 이론적 기반(atomic write + CRC + WSN + schema migration)이 교과서적으로 정확하고, Pillar 1/3/4 철학이 전반에 내재화됨. 새로운 blocking issue 미발견. 프로그래머에게 전달 준비 완료.

### Recommended (신규 3건, 구현/downstream 이관)

1. **Coverage summary 산술 오류**: 27 ACs 나열되나 Coverage 요약 표는 26으로 집계. COMPOUND_EVENT_NO_SEQUENCE_ATOMICITY 미포함.
2. **MIGRATION_EXCEPTION_ROLLBACK + E7 UE 예외 모순**: UE 기본 bEnableExceptions=false 환경에서 throw 기반 롤백 불가. R1 [qa-B2] 이래 지속. 첫 schema bump(V=2) 전 해결 필수.
3. **R1 deferred RECOMMENDED 15건**: Async vs UE::Tasks ADR, WriteFlags 명시, .tmp 부분쓰기 커버리지 등 — 구현 sprint 착수 시 확인 필요.
