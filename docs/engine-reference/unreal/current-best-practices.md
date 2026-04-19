# UE 5.6 Best Practices — Moss Baby 프로젝트 관점

*Last verified: 2026-04-16*
*Project pinned version: Unreal Engine 5.6*

> 이 문서는 UE 5.6 전반의 베스트 프랙티스 중 **이 프로젝트가 실제로 쓸 영역**에 초점을 둔 권장 사용법입니다. 에이전트가 구현 계획을 세울 때 참조.

---

## 1. Rendering — Terrarium Dusk 씬

### Lumen GI (사용 권장)
- 5.6의 HWRT Lumen은 CPU 병목이 완화되어 작은 씬의 황혼 조명 연출에 매우 적합
- **SkyLight + Directional Light (저각도)** 조합이 핵심 — 시간대별로 Directional Light 회전/색 보간
- **Lumen Scene Detail**: 작은 씬이므로 기본값 유지로 충분
- Hardware Ray Tracing 켜기 (DX12 + RT 지원 GPU 대상). 소프트웨어 Lumen도 차선책으로 사용 가능

### Nanite — **비권장**
- Moss Baby 에셋은 작고 단순함. Nanite 오버헤드 > 이득
- 대신 LOD 수동 or 단일 해상도 메시

### PSO Precaching (권장)
- 5.6의 PSO 점진 컴파일 특성으로 인한 런타임 히칭 완화용
- Project Settings → Rendering → PSO Precaching 활성화
- 릴리스 빌드에서 첫 실행 시 프리컴파일 단계 고려

### Post-process
- Bloom은 은은하게 (0.3 이하)
- Color Grading: LUT 기반 황혼 톤 맵 (이끼 녹 / 꿀빛 / 라벤더)
- Chromatic Aberration·Film Grain 극소량 (종이 질감 보강)

---

## 2. Materials — 스타일라이즈

### 이끼 아이 머티리얼
- Base: **Subsurface Scattering** (이끼의 내부 산란감)
- Params: 성장 단계별 `MaterialParameterCollection` 또는 `MaterialInstanceDynamic`으로 **감정 파라미터 보간**
- 절대 금지: Metallic·Plastic 느낌 (Pillar — "표면은 부드러움" 위반)

### 카드 머티리얼
- 종이/수채화 느낌: 노이즈 텍스처 + 약한 테셀레이션 or Parallax
- 카드 손에 집혔을 때 미세한 노멀 왜곡 (종이가 휘는 느낌)

---

## 3. Niagara — Particles

- 이끼 포자 (Day 1–5 탐색기): 매우 느린 중력, 긴 라이프타임
- 반딧불 (Day 15–20 징조기): 짧은 수명 + emissive 고요한 빛
- 선물 이펙트 (카드 건넴 순간): 한 번짜리 burst
- **파티클 수 상한**: 씬당 최대 200개 (데스크톱 메모리 제약)

---

## 4. UMG — Card UI & Dream Journal

- **UMG Widget Blueprints** 사용 (CommonUI는 이 스코프에 과함)
- **3장 카드 제시**: Horizontal Box + 카드 위젯 3개 인스턴스
- **건넴 제스처**: Drag & Drop (UMG 기본 지원) — 이끼 아이 근처로 놓으면 확정
- **꿈 일기**: Book-style UI. 페이지별 WidgetSwitcher + 좌우 Flip 애니메이션
- 텍스트: `Rich Text Block`으로 시적 포맷팅 (이탤릭·폰트 컬러) 가능
- **권장 폰트**: 한글 지원 — Nanum Myeongjo or Noto Serif KR (감성 톤)

---

## 5. Persistent State — 21일 실시간 처리

### 시간 추적
- **절대 `FTimerManager`로 21일 추적 금지** — 앱 종료 시 타이머 손실
- `FDateTime::UtcNow()` 저장 + 로드 시 경과 시간 계산
- 시스템 시간 조작 대응: 
  - 마지막 저장 시각 > 현재 시각 → 사용자 경고 (조용한 톤, Pillar 1 지키며)
  - 24h 이상 건너뜀 감지 시 "몇 날이 지났네요" 같은 내러티브 흡수

### 세이브 시스템
- `USaveGame` 상속 + 직렬화
- 매 카드 건넴 후 즉시 저장 (게임 종료 타이밍 예측 불가)
- 세이브 파일 위치: `FPlatformProcess::UserSettingsDir()` 하위

### 세이브 내용
- 시작 시각 (FDateTime)
- 건네진 카드 기록 (배열 — 카드 ID + 날짜)
- 획득한 꿈 인덱스 배열
- 현재 성장 단계 인덱스
- 이끼 아이 최종 형태 벡터 (태그 누적값)

---

## 6. Desktop Companion 특이 요소

### 작은 창 모드
- `UGameUserSettings::SetScreenResolution` 으로 800×600 이하 지원
- `r.setres` 콘솔 명령 사용 가능

### Always-on-top (선택적)
- UE 기본 미지원. Windows API (`SetWindowPos` with `HWND_TOPMOST`) 호출 C++ 모듈 필요
- Plugin 또는 Game Instance Subsystem에서 GameViewportClient의 HWND 핸들 취득
- **주의**: 이 기능은 MVP 이후에 검토

### 메모리
- Target 1.5 GB ceiling. Lumen + 소규모 씬 기준 여유 있음.
- 텍스처 스트리밍 풀 적절히 설정

---

## 7. Architecture Patterns (프로젝트 레벨)

### Actor / Component 구성
- `AMossWorldGameMode` — 글로벌 상태
- `AMossBaby : AActor` — 이끼 아이 개체. 루트에 StaticMesh + ParticleSystem + SkeletalMesh (블렌드 셰이프용)
- `UGiftCardSystemComponent : UActorComponent` — 카드 덱 관리
- `UGrowthAccumulator : UActorComponent` — 태그 벡터 누적 로직
- `UDreamTriggerSubsystem : UGameInstanceSubsystem` — 꿈 조건 판정

### Data-driven
- 카드: `UDataTable` with `FGiftCardRow` struct (태그 배열, 아이콘, 시즌 가중치)
- 꿈: `UDataAsset` per dream — 트리거 조건 + 텍스트
- 최종 형태: `UDataTable` — 태그 벡터 클러스터 → 형태 매핑
- **이유**: 디자이너(개발자 본인)가 엔진 재빌드 없이 밸런싱·튜닝

### Subsystem 우선
- Singleton 직접 구현 대신 `UGameInstanceSubsystem` 사용
- 테스트·모킹에 유리 (UE Automation Framework 호환)

---

## 8. Testing

### Functional Tests
- `AFunctionalTest` 상속으로 자동화 테스트 작성
- 핵심 검증:
  - 카드 건넴 1회 → 태그 벡터 증가 검증
  - 21일 경과 → 최종 형태 판정 검증
  - 세이브 → 로드 왕복 데이터 일치
  - 꿈 트리거 조건 엣지 케이스

### CI 실행
- Headless: `UnrealEditor-Cmd.exe MadeByClaudeCode.uproject -ExecCmds="Automation RunTests MossBaby" -TestExit="Automation Test Queue Empty" -NullRHI -NoSplash -Unattended`

---

## 9. Source Code Organization (UE 관례)

```
Source/MadeByClaudeCode/
├── MadeByClaudeCode.h / .cpp      (엔진 모듈 엔트리)
├── Gameplay/
│   ├── MossBaby.h / .cpp          (이끼 아이 액터)
│   ├── GiftCardSystemComponent.h/.cpp
│   ├── GrowthAccumulator.h/.cpp
│   └── MossWorldGameMode.h/.cpp
├── Data/
│   ├── GiftCardRow.h              (struct)
│   ├── DreamAsset.h / .cpp        (UDataAsset)
│   └── FinalFormRow.h             (struct)
├── Save/
│   └── MossSaveGame.h / .cpp      (USaveGame)
├── Subsystems/
│   └── DreamTriggerSubsystem.h / .cpp
└── UI/
    ├── CardHandWidget.h / .cpp    (UUserWidget)
    └── DreamJournalWidget.h / .cpp
```

---

## 10. 피해야 할 UE 안티패턴 (이 프로젝트 특별 주의)

- **TickFunction 남용**: 대부분 이벤트/타이머 기반. Tick은 렌더 관련에만
- **Blueprint에서 무거운 로직**: 카드 태그 누적·꿈 판정은 **반드시 C++**
- **Hardcoded 문자열 경로**: `FSoftObjectPath` + DataTable 참조로 이관
- **Editor-only 기능 런타임 호출**: `#if WITH_EDITOR` 가드 필수
- **GC 없는 Raw 포인터**: 항상 `UPROPERTY()` + `TObjectPtr<>`

---

## Sources

- [UE 5.6 Release Notes](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-6-release-notes)
- [UE 5.6 Performance Highlights — Tom Looman](https://tomlooman.com/unreal-engine-5-6-performance-highlights/)
- [UE 5.6 Release Announcement](https://www.unrealengine.com/news/unreal-engine-5-6-is-now-available)
