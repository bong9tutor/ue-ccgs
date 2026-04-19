# Review Log — `data-pipeline.md`

이 파일은 `design/gdd/data-pipeline.md`의 모든 `/design-review` 결과를 시간순으로 누적한다. 새 리뷰 시 가장 최근 항목 아래에 추가하고, 이전 항목은 보존한다.

---

## Review — 2026-04-17 — Verdict: MAJOR REVISION NEEDED

Scope signal: **XL** (Foundation/infrastructure, 7 forward-declared deps, 2 ADR pending, 8 BLOCKING items)
Specialists: game-designer, systems-designer, unreal-specialist, qa-lead, performance-analyst, creative-director (senior synthesis)
Blocking items: **8** (senior synthesis 후 통합) | Recommended: **5** | Nice-to-have: **3**
Prior verdict resolved: First review
Independence note: 본 세션이 GDD 작성 세션과 동일. systems-designer 자문의 self-validation 위험 존재. Independence score 70%.

### Summary (creative-director synthesis)

> 이 GDD는 Foundation/infrastructure 시스템으로서의 책임 경계(read-only 카탈로그, C1·C2 Pillar 강제 contract, fail-close 정책)가 명확하고, Tuning Knobs의 상호작용 매트릭스와 default 값 근거 도출(§G.6–G.8)은 다른 Foundation GDD(Time, Save/Load)보다 체계적이다. 그러나 세 가지 load-bearing 가정이 무너진다: (1) `ensureMsgf`가 UE 5.6 Shipping에서 strip 확정 → G.7 단조성 검증과 AC-DP-13 pre-init guard가 출시 빌드에서 작동 안 함, (2) `DuplicateCardIdPolicy = DegradedFallback` default가 디자이너 실수 1건으로 21일 무입력 형태 수렴을 야기 → Pillar 4 ("끝이 있다") silent 위반, (3) D2 T_init budget이 SSD warm-load를 암묵 전제하여 HDD cold-start 사용자에서 3초 freeze 구조적 위반.

### Top 3 Blockers (synthesized)

1. **`ensureMsgf` Shipping strip → AC-DP-13/AC-DP-17 무효화** [unreal-specialist + qa-lead 합의]. `ensure*` = Shipping에서 strip 확정. G.7 단조성 검증, AC-DP-13 pre-init guard 모두 출시 빌드에서 작동 안 함. `check()` 또는 명시적 `if + UE_LOG(Fatal)` 재설계 필수.

2. **`DuplicateCardIdPolicy` default 변경 = WarnOnly + 첫 row 승리 규칙** [game-designer, senior 채택]. 현재 DegradedFallback은 카드 카탈로그 전체 무력화 → 21일 무입력 끝. Pillar 4 직접 위반. WarnOnly + 첫 row 승리 규칙 + 중복 Warning 로그로 디자이너 알림.

3. **OQ-E-7 결정 — 카탈로그 단독 실패 시 pending day + 다음 앱 복구** [game-designer]. Fatal = Pillar 1 위반, silent skip = Pillar 4 위반. game-concept.md "메마름 → 첫 카드 복구" 패턴 차용.

### Remaining Blockers (5)

4. **AssetManager PrimaryAssetType 등록 누락** [unreal-specialist]. Cooked 빌드 `GetPrimaryAssetIdList()` 빈 결과 위험. GDD에 등록 절차 + AC Cooked 검증 추가.

5. **Subsystem Initialize 의존성 순서 명시** [unreal-specialist]. `Collection.InitializeDependency<>()` 미사용. Subsystem 간 순서 비결정적.

6. **GC 안전성 — `UPROPERTY()` mark 명시** [unreal-specialist]. `UDreamDataAsset*` 캐시에 `UPROPERTY()` 없으면 GC pass에서 dangling pointer.

7. **D2/D3 boundary degenerate 처리 + naming** [systems-designer]. D3 음수 underflow 클램프 필요. "안전 범위" 오도 naming. N_path=N_card 고정 가정. S_card FText 오버헤드 미반영.

8. **HDD cold-load 명시 + 메모리 합산 표** [performance-analyst]. MaxInitTimeBudgetMs=50이 SSD 전제 미명시. 전체 메모리 합산 1115MB/1500MB(74%) 표 추가 필요.

### Specialist Disagreements (3건, senior adjudication 완료)

1. **DuplicateCardIdPolicy**: game-designer BLOCKING (WarnOnly) vs GDD 작성 시 systems-designer 채택 (DegradedFallback) → **senior: game-designer 채택** (Pillar > 시스템 일관성).
2. **Editor Module priority**: unreal-specialist BLOCKING vs senior RECOMMENDED 격하 → **senior: RECOMMENDED** (GDD 결함 아닌 인프라 prerequisite).
3. **Shipping strip BLOCKING vs RECOMMENDED**: unreal-specialist + qa-lead 양쪽 BLOCKING 합의 → **senior: BLOCKING 확정**.

### Pillar Alignment (creative-director 종합)

- P1 조용한 존재: **CONCERN** → P0-1 해결 시 PASS
- P2 선물의 시학: **CONCERN** (태그 최적화 경로 + Player Fantasy 과대 framing)
- P3 말할 때만 말한다: **PASS**
- P4 끝이 있다: **FAIL** → P0-2/3 해결 시 PASS

### Success Criteria for R2

> (1) AC-DP-13/AC-DP-17이 Shipping 빌드에서도 pre-init guard / 단조성 검증이 작동함을 검증 가능
> (2) DuplicateCardIdPolicy=WarnOnly default + 첫 row 승리 규칙이 명시되고, 21일 무입력 끝 경로가 차단됨
> (3) OQ-E-7이 "pending day + 다음 앱 복구" 정책으로 GDD에 반영됨
> (4) D3 공식에 `max(0, ...)` 클램프 적용되고, "안전 범위" naming이 정정됨
> (5) HDD cold-load 사용자 시나리오가 GDD에 명시되고, 대응 정책이 결정됨

---

## R2 — 2026-04-17 — Status: 8 BLOCKER 수정 완료, Re-review 대기

### R1 → R2 변경 요약

**BLOCKER 1 — `ensureMsgf` Shipping strip** ✅:
- G.7 `ensureMsgf` → `checkf` 또는 명시적 `if+UE_LOG Fatal` (Shipping-safe)
- AC-DP-17 재설계: Shipping 빌드 포함 모든 빌드에서 동작 보장

**BLOCKER 2 — `DuplicateCardIdPolicy` default** ✅:
- Default DegradedFallback → **WarnOnly + 첫 row 승리 규칙**
- E1, G.3, G.6(b), G.8, AC-DP-11 동기 갱신
- Pillar 4 위반 경로 차단

**BLOCKER 3 — OQ-E-7 Card 단독 실패 정책** ✅:
- **Pending day + 다음 앱 복구** 정책 채택 (game-concept "메마름 → 첫 카드 복구" 패턴)
- E10 재작성, OQ-E-7 Resolved

**BLOCKER 4 — AssetManager PrimaryAssetType 등록** ✅:
- R1에 DefaultEngine.ini PrimaryAssetType 등록 절차 추가
- Cooked 빌드 검증 AC-DP-18 신규 (미작성 — re-review 시 추가)

**BLOCKER 5 — Subsystem Initialize 의존성 순서** ✅:
- R1에 `Collection.InitializeDependency<UDataPipelineSubsystem>()` contract 명시

**BLOCKER 6 — GC 안전성 UPROPERTY()** ✅:
- R1에 `UObject*` 캐시 멤버 `UPROPERTY()` 필수 명시

**BLOCKER 7 — D3 boundary + naming** ✅:
- D3 공식에 `max()` 분모 0 클램프 + 음수 underflow 클램프 적용
- "안전 범위" → "권장 상한" naming 정정

**BLOCKER 8 — HDD cold-load + 메모리 합산** ✅:
- D2에 HDD cold-start C_asset 0.5–2.0ms 시나리오 + 계산 표 추가
- 전체 메모리 합산 표 (1080MB/1500MB = 72%, 여유 420MB)
- 50ms budget 유지, 초과 시 Async 전환 검토 플래그

### R2 → R3 권고

다음 fresh session에서:
```
/design-review design/gdd/data-pipeline.md --depth lean
```

R2 verdict 기대치: **APPROVED** 또는 **NEEDS REVISION (minor)**. 8 BLOCKER 모두 처리, R1 Success Criteria 5개 충족. AC-DP-18 (Cooked PrimaryAssetType 검증) 신규 AC 미작성이 유일한 잔여 사항.

---

## Review — 2026-04-17 — Verdict: APPROVED (R3 수정 후)
Scope signal: **M** (단일 Foundation 시스템, 3 formulas, 6 downstream dependents, ADR 2개 예정)
Specialists: [lean mode — single-session analysis, no specialist agents]
Blocking items: **0** (R3 수정 후) | Recommended: **5** (R1 deferred 유지) + E2 ensure() 일관성
Prior verdict resolved: **Yes** — R1의 8 BLOCKER 모두 해소, R2 인코딩 손상·AC-DP-18 갭도 R3에서 수정

### Summary

R1의 8 BLOCKER(ensureMsgf Shipping strip, DuplicateCardIdPolicy Pillar 4 위반, OQ-E-7 미결정, AssetManager 등록 누락, Subsystem 의존성 순서, GC UPROPERTY, D3 boundary degenerate, HDD cold-load)가 R2에서 모두 적절히 해소됨을 확인. R2 수정 과정에서 발생한 UTF-8 인코딩 손상 5곳과 AC-DP-18 미작성을 R3에서 동일 세션 내 즉시 수정. 책임 경계·Pillar 강제 contract·fail-close 정책이 명확하며, 프로그래머에게 전달 준비 완료.

### R3 수정 사항 (본 세션)

- 인코딩 손상 5곳 텍스트 복원 (G.7, AC-DP-17)
- AC-DP-18 신규 작성 (Cooked PrimaryAssetType 검증) + Coverage Matrix·AC count 갱신
- G.6(f) `ensureMsgf` → `checkf`/`if+Fatal` 일치
- G.11 OQ-E-1 resolved 텍스트 WarnOnly 수정
- D3 변수 표 "안전 상한" → "권장 상한" 수정
