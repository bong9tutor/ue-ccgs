# Test Infrastructure — Moss Baby

**Engine**: Unreal Engine 5.6 (hotfix 5.6.1 권장)
**Test Framework**: UE Automation Framework (내장)
**CI**: `.github/workflows/tests.yml` (self-hosted runner 필요)
**Setup date**: 2026-04-19

## Directory Layout

```text
tests/                                              # 프로젝트 루트 — 테스트 인덱스·smoke·evidence
├── README.md                                       # 이 파일
├── unit/                                           # (참조용) Logic 테스트 카탈로그
├── integration/                                    # (참조용) Integration 테스트 카탈로그
├── smoke/
│   └── smoke-checklist.md                          # /smoke-check 게이트 (15분 수동 체크리스트)
└── evidence/                                       # Visual/UI 스토리의 수동 sign-off 기록 + screenshot

MadeByClaudeCode/Source/MadeByClaudeCode/Tests/     # 실제 C++ 테스트 소스 (UE 관례)
├── README.md                                       # UE Automation Framework 가이드
└── MossFormulaTests.cpp                            # 예시 — 14 GDD의 Formula 검증 test
```

> **경로 분리 이유**: UE Automation Framework는 `Source/[ModuleName]/` 하위 `.cpp` 파일을 자동 컴파일한다. 따라서 실제 C++ 테스트 소스는 `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/`에 배치. 이 `tests/` 디렉토리는 테스트 인덱스·smoke 체크리스트·수동 evidence만 보관 — 엔진에 직접 컴파일되지 않음.

## Test Categorization (Coding Standards §Test Evidence by Story Type)

| Story Type | Required Evidence | Location | Gate Level |
|---|---|---|---|
| **Logic** (formulas, AI, state machines) | Automated unit test — must pass | `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/` (category: `MossBaby.[System].Formula*`) | BLOCKING |
| **Integration** (multi-system) | Integration test OR documented playtest | `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/` (category: `MossBaby.[System].Integration*`) | BLOCKING |
| **Visual/Feel** (animation, VFX, feel) | Screenshot + lead sign-off | `tests/evidence/` | ADVISORY |
| **UI** (menus, HUD, screens) | Manual walkthrough doc OR interaction test | `tests/evidence/` | ADVISORY |
| **Config/Data** (balance tuning) | Smoke check pass | `tests/smoke/smoke-[date].md` | ADVISORY |

## Acceptance Criteria Types (Coding Standards §Acceptance Criteria Types)

| Type | 검증 시점 | Gate Level |
|---|---|---|
| **AUTOMATED** | CI/CD (매 push) — UE Automation `-nullrhi` 헤드리스 | BLOCKING |
| **INTEGRATION** | CI/CD + 통합 테스트 스위트 | BLOCKING |
| **CODE_REVIEW** | PR 코드 리뷰 + CI 정적 분석 (grep, static_assert) | BLOCKING |
| **MANUAL** | QA 수동 테스트 매트릭스 (드라이버 의존, CI 불가) | ADVISORY |

## Running Tests

### 에디터에서 (개발 중)

1. Editor 실행 → `Session Frontend` (창 메뉴) → `Automation` 탭
2. 테스트 트리에서 `MossBaby.*` 카테고리 선택
3. `Start Tests` 클릭
4. 실패한 테스트 클릭 → 상세 로그 확인

### 헤드리스 (CI 또는 로컬 명령줄)

```bash
# Windows
"$UE_EDITOR_PATH/UnrealEditor-Cmd.exe" \
  "$PROJECT_PATH/MadeByClaudeCode.uproject" \
  -ExecCmds="Automation RunTests MossBaby.; Quit" \
  -nullrhi -nosound -log -unattended
```

- `-nullrhi`: 렌더링 비활성 (로직 전용 테스트에 필수)
- `-nosound`: 오디오 비활성
- `-unattended`: 모달 다이얼로그 자동 Dismiss

## Determinism Rules (Coding Standards §Automated Test Rules)

모든 AUTOMATED 테스트는:
- **Naming**: `[system]_[feature]_test.cpp` 파일 / `test_[scenario]_[expected]` 함수 (UE 관례상 `F[SystemName]Test` 클래스 — `MossFormulaTests.cpp` 예시 참조)
- **Determinism**: 매 실행 동일 결과 (랜덤 시드 금지 — `FRandomStream` 고정 시드 권장)
- **Isolation**: 각 테스트가 자체 fixture setup/teardown
- **No hardcoded magic**: 상수는 registry 또는 fixture factory 경유 (boundary value 테스트는 예외 — 경계값 자체가 point)
- **Independence**: API/DB/I/O 호출 금지 — dependency injection (e.g., `IMossClockSource`)

## What NOT to Automate (Coding Standards §What NOT to Automate)

다음은 headless CI에서 검증 불가 — `tests/evidence/`에 수동 sign-off:
- Visual fidelity (shader output, VFX 외관, 애니메이션 커브)
- "Feel" qualities (입력 응답성, 체감 무게, 타이밍)
- 플랫폼별 렌더링 (target 하드웨어에서 테스트 — `-nullrhi` 헤드리스 불가)
- 풀 게임플레이 세션 (playtest 담당)

## CI Integration

`.github/workflows/tests.yml`은 push + PR 시 자동 실행. 단, UE는 Editor 설치가 필요한 self-hosted runner 전제 (무료 GitHub-hosted runner로는 UE 테스트 불가).

self-hosted runner 미설정 시:
- CI workflow는 참조용으로 유지
- Local 수동 실행 (위 "헤드리스" 섹션)으로 대체 가능
- Migration plan: 프로젝트가 Production 단계 진입 시 self-hosted runner 구성 검토

## Related Documents

- `.claude/docs/coding-standards.md` — Test Evidence / AC Types / Automated Test Rules / What NOT to Automate
- `docs/architecture/architecture.md` — 14 MVP 시스템 API contracts + AC 목록
- `design/gdd/` — 각 GDD의 §Acceptance Criteria (테스트 설계 원본)
- `MadeByClaudeCode/Source/MadeByClaudeCode/Tests/README.md` — UE-specific 상세 가이드
