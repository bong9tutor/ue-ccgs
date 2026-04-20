<div align="center">

# 🎮 CCGS + Unreal Engine 5.6 게임 만들기

### 비전공자를 위한 AI 게임 개발 완전 가이드 + 샘플 프로젝트

Claude Code Game Studios(CCGS) 프레임워크와 Claude AI를 활용하여<br>
언리얼 엔진 5.6에서 게임을 만드는 실행 중심 Step-by-Step 가이드

![CCGS](https://img.shields.io/badge/CCGS-Framework-f97316)
![Claude Code](https://img.shields.io/badge/Claude%20Code-AI-d4a574)
![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.6-4da3ff)
![Language](https://img.shields.io/badge/Language-한국어-22c55e)

**프로그래밍 경험 0 · 터미널 처음 · 게임 개발 처음**

[📖 가이드 바로가기](https://bong9tutor.github.io/ue-ccgs/ccgs-guide/) · [🐙 CCGS 프로젝트](https://github.com/bong9tutor/Claude-Code-Game-Studios)

</div>

---

## 📦 저장소 구성

이 저장소는 **3가지 역할**을 동시에 수행합니다:

| 역할 | 위치 | 설명 |
|---|---|---|
| 📖 **가이드 사이트** | `docs/ccgs-guide/` | 5개 파트의 한국어 튜토리얼 (정적 HTML) |
| 🧠 **CCGS 프레임워크** | `.claude/` · `design/` · `production/` | 바로 실행 가능한 CCGS 통합 환경 |
| 🎮 **샘플 UE 프로젝트** | `MadeByClaudeCode/` | UE 5.6 Blank C++ 스켈레톤 |

저장소를 클론한 즉시 `claude` 명령어로 CCGS 워크플로우를 시작해 샘플 프로젝트를 발전시킬 수 있습니다.

---

## 📚 가이드 목차

정적 HTML 5개 파트의 튜토리얼 사이트입니다. GitHub Pages에서 보거나, 저장소를 클론하여 `docs/ccgs-guide/index.html` 을 브라우저로 직접 열 수 있습니다.

| # | 파트 | 주요 내용 | AI 명령어 |
|:-:|---|---|---|
| 1 | [**사전 준비**](https://bong9tutor.github.io/ue-ccgs/ccgs-guide/part1-setup.html) — 프로그램 설치하기 | Git, Node.js, Claude Code, UE 5.6 설치 및 CCGS 클론 | 설치 · 환경 설정 |
| 2 | [**게임 컨셉 만들기**](https://bong9tutor.github.io/ue-ccgs/ccgs-guide/part2-concept.html) — 아이디어에서 설계까지 | Claude와 대화하며 아이디어 발전, 시스템 정리 | `/start` `/brainstorm` `/setup-engine` `/map-systems` |
| 3 | [**시스템 설계**](https://bong9tutor.github.io/ue-ccgs/ccgs-guide/part3-design.html) — 게임의 뼈대 만들기 | 시스템별 GDD 작성, 설계 일관성 검증 | `/design-system` `/design-review` `/review-all-gdds` `/art-bible` |
| 4 | [**기술 설정 & 사전 제작**](https://bong9tutor.github.io/ue-ccgs/ccgs-guide/part4-architecture.html) — 구현 준비 | 아키텍처 결정, 에픽/스토리 분해, 스프린트 계획 | `/create-architecture` `/create-epics` `/create-stories` `/sprint-plan` |
| 5 | [**프로덕션 & 릴리스**](https://bong9tutor.github.io/ue-ccgs/ccgs-guide/part5-production.html) — 게임 완성하기 | 구현, 테스트, 빌드 및 출시 | `/dev-story` `/code-review` `/story-done` `/perf-profile` `/launch-checklist` |

> 📁 저장소 내 파일로 보기: [`docs/ccgs-guide/`](docs/ccgs-guide/)

---

## 📊 가이드 규모

<div align="center">

| 파트 | AI 명령어 | AI 에이전트 | 개발 단계 |
|:-:|:-:|:-:|:-:|
| **5** | **72** | **49** | **7** |

</div>

---

## 🧪 샘플 프로젝트 진행 상황

`MadeByClaudeCode/` 샘플 프로젝트(작업명 **Moss Baby** — 데스크톱 cozy 씬, UE 5.6 Blank C++) 를 CCGS 워크플로우로 실제 구축하며 가이드 내용을 검증하고 있습니다.

<div align="center">

| Sprint | 기간 | 완료율 | 상태 |
|:-:|:-:|:-:|:-:|
| **Sprint 01** | 2026-04 | **20 / 21 (95%)** | ✅ COMPLETE · QA 사인오프 완료 |

</div>

### Sprint 01 세부

| 분류 | 완료 | 비고 |
|---|---|---|
| Must Have | 12 / 13 | Story 1-11 Input 에셋: `awaiting-editor` (TD-008) |
| Should Have | 4 / 4 | AC-DP-18 Cooked 빌드 실측: `awaiting-cooked` (TD-009) |
| Nice to Have | 4 / 4 | ALL DONE |

### Sprint 01 산출물

- 🧪 **QA Plan** — [production/qa/qa-plan-sprint-01-2026-04-19.md](production/qa/qa-plan-sprint-01-2026-04-19.md)
- 🔥 **Smoke Report** — [production/qa/smoke-2026-04-19.md](production/qa/smoke-2026-04-19.md)
- ✅ **QA Sign-off** — [production/qa/qa-signoff-sprint-01-2026-04-19.md](production/qa/qa-signoff-sprint-01-2026-04-19.md)
- 🗒️ **Retrospective** — 커밋 [`431b3bb`](../../commit/431b3bb)
- 📋 **Sprint 문서** — [production/sprints/sprint-01.md](production/sprints/sprint-01.md)

### 열린 Tech Debt (Sprint 01 잔여)

- `TD-008` — Story 1-11 Input 에셋 수동 생성 (UE Editor)
- `TD-009` — AC-DP-18 Cooked 빌드 실측 (`[5.6-VERIFY]`)
- `TD-011` — Offer Hold boundary PIE integration
- `TD-015` — Input MANUAL 체크리스트 (Card Hand UI epic 후)

---

## 🚀 시작하기

### 옵션 1. 온라인 가이드 보기 (권장)

👉 **https://bong9tutor.github.io/ue-ccgs/ccgs-guide/**

### 옵션 2. 로컬에서 가이드 보기

```bash
git clone https://github.com/bong9tutor/ue-ccgs.git
cd ue-ccgs
# 브라우저로 열기
start docs/ccgs-guide/index.html   # Windows
open  docs/ccgs-guide/index.html   # macOS
```

### 옵션 3. CCGS로 샘플 프로젝트 실습하기

가이드를 읽으며 실제로 CCGS를 돌려보려면:

```bash
git clone https://github.com/bong9tutor/ue-ccgs.git
cd ue-ccgs

# Claude Code 실행 → CCGS 명령어 자동 로드됨
claude

# 세션 안에서:
/start
/setup-engine unreal 5.6
```

> Claude Code · UE 5.6 · Git 설치는 [Part 1](https://bong9tutor.github.io/ue-ccgs/ccgs-guide/part1-setup.html) 참고.

---

## 🗂️ 저장소 구조

```
ue-ccgs/
├── docs/
│   └── ccgs-guide/                ← 📖 가이드 사이트
│       ├── index.html             # 메인 랜딩
│       ├── part1-setup.html       # Part 1: 사전 준비
│       ├── part2-concept.html     # Part 2: 게임 컨셉
│       ├── part3-design.html      # Part 3: 시스템 설계
│       ├── part4-architecture.html# Part 4: 기술 설정
│       ├── part5-production.html  # Part 5: 프로덕션
│       └── style.css              # 공통 스타일
│
├── .claude/                       ← 🧠 CCGS 에이전트·스킬·훅
├── design/                        ← CCGS GDD·디자인 문서 저장 위치
├── production/                    ← CCGS 스프린트·세션 상태
├── _ccgs_upstream/                ← CCGS 원본 (subtree, 업데이트 추적용)
├── CCGS-LICENSE                   # CCGS MIT 라이선스
├── CCGS-UPGRADING.md              # CCGS 업그레이드 가이드
│
├── MadeByClaudeCode/              ← 🎮 샘플 UE 5.6 프로젝트
│   ├── MadeByClaudeCode.uproject
│   ├── Config/                    # 엔진·게임·에디터 설정
│   ├── Content/                   # 에셋
│   └── Source/
│       └── MadeByClaudeCode/      # C++ 코드
│
├── CLAUDE.md                      # 저장소 지침 (Claude Code가 자동 참조)
├── README.md
└── .gitignore                     # UE + CCGS 병합
```

---

## 🔄 CCGS 업데이트 받기

CCGS 원본 저장소가 업데이트되면 다음 명령어로 `_ccgs_upstream/` 을 동기화할 수 있습니다:

```bash
git subtree pull --prefix=_ccgs_upstream ccgs main --squash
```

이후 루트의 `.claude/`, `design/`, `production/` 과 수동 비교·병합하세요.

---

## ⚠️ CCGS + Unreal Engine 조합의 한계점

CCGS 프레임워크는 Godot · Unity · Unreal 3개 엔진을 공통 워크플로우로 커버하지만, **Unreal에서는 "에디터 의존 작업의 자동화"가 구조적으로 비어있습니다.** Sprint 01 을 실제로 돌려보며 확인한 한계를 솔직히 기록합니다. (2026-04 기준 · UE 5.6)

### 1. `.uasset` 자동 생성 경로가 프레임워크에 없음

- CCGS가 공식 문서화한 UE CLI 호출은 **헤드리스 테스트 실행** (`UnrealEditor -nullrhi -ExecCmds="Automation RunTests ..."`) 단 하나뿐입니다.
- `.uasset` 을 CLI로 생성하는 경로 — `PythonScriptPlugin`, `commandlet`, `AssetTools` 등 — 는 CCGS의 에이전트 · 스킬 · 훅 · 엔진 레퍼런스 어디에도 없습니다. 키워드 검색 시 0건.
- **결과**: Input Action · Input Mapping Context · Widget Blueprint · Data Asset 는 여전히 UE 에디터에서 **사람이 직접** 만들어야 하고, 스토리는 `awaiting-editor` 상태로 끝납니다. Sprint 01의 `TD-008 / TD-009 / TD-011 / TD-015` 가 이 패턴의 누적 증거입니다.

### 2. Blueprint 이벤트 그래프는 Python으로 자동화가 **근본적으로 불가능**

- UE 5.6 Python API 는 BP · Widget BP **스켈레톤 생성**, Input/DataAsset 에셋 생성, 프로퍼티 설정까지는 지원합니다.
- 그러나 **이벤트 그래프 노드 추가 API 가 Python 바인딩에 노출되어 있지 않습니다** (`EdGraph` · `EdGraphNode` 미노출). 커뮤니티 공통 한계이며, 우회하려면 노드 빌더를 노출하는 **C++ 플러그인** 을 직접 작성해야 합니다.
- **실무 결론**: 게임 로직은 **C++ 로**, Blueprint 는 **설정 셸(thin child)** 로 유지하는 전략이 현실적입니다. 이 저장소도 그 방향을 택했습니다 ([.claude/docs/coding-standards.md](.claude/docs/coding-standards.md) 의 "Gameplay values must be data-driven" 조항 참고).

### 3. MCP-for-Unreal 서버들은 **아직 프로덕션용이 아님**

- 공개된 MCP 서버 — `chongdashu/unreal-mcp`, `flopperam/unreal-engine-mcp`, `Natfii/UnrealClaude`, `kvick-games/UnrealMCP` 등 — 은 모두 **실험 단계 · UE 5.5 타깃 · solo-maintainer** 입니다.
- `chongdashu/unreal-mcp` 는 현재 **UE 5.6 호환성이 오픈 이슈** 상태입니다.
- 상용 옵션 **CLAUDIUS** (유료 $59.99) 는 UE 5.4-5.7 을 커버하지만 폐쇄 소스 · 단일 벤더 · FAB 마켓플레이스 종속입니다.
- 또한 MCP 의 "비동의식 에디터 제어" 방식은 이 저장소가 채택한 CCGS **"May I write this to [filepath]?" 승인 프로토콜**과 구조적으로 충돌합니다.

### 4. CCGS 기본 UE 에이전트의 **권한 비대칭**

- `ue-blueprint-specialist` 에이전트는 `disallowedTools: Bash` — **쉘 스크립트를 실행할 수 없습니다.**
- 즉, Blueprint 전담 에이전트가 Blueprint 생성 자동화 스크립트를 **실행할 수 없는 구조**입니다.
- 사용자가 `ue-asset-automation-specialist` 같은 신규 에이전트를 직접 정의해야 합니다 (이 저장소의 TODO 항목).

### 5. `.uasset` 바이너리 머지 리스크

- Blueprint 와 특히 **Widget Blueprint** 는 UE가 공식 제공하는 mergetool 에서도 **지원 범위 밖**입니다.
- Git에서 `.uasset` 충돌이 잦아질수록 복구 비용이 급증합니다.
- **완화책**: 핵심 데이터를 DataTable · DataAsset · JSON/YAML 로 외부화하여 **BP 자체의 diff 면적**을 최소화하는 데이터 중심 설계.

### 6. 이 저장소가 채택한 현재 워크어라운드

| 영역 | 전략 |
|---|---|
| 게임 로직 | **C++ 우선** — `Source/MadeByClaudeCode/` |
| 게임플레이 값 | `UDataAsset` + config 로 분리, 하드코딩 금지 ([coding-standards.md](.claude/docs/coding-standards.md)) |
| 에디터 의존 작업 | `MANUAL` AC 타입으로 명시 후 QA 매트릭스에서 실기 검증 |
| MANUAL 증거 | placeholder 문서로 `production/qa/evidence/` 에 추적 |
| CI | 헤드리스 UE Automation Framework (`-nullrhi`) 만 사용, 에셋 생성은 CI 밖 |

### 7. 향후 개선 로드맵

- [ ] `.claude/agents/ue-asset-automation-specialist.md` — Bash 실행 가능한 신규 에이전트 정의
- [ ] `tools/ue-automation/run-ue-python.ps1` — `UnrealEditor-Cmd.exe -run=pythonscript` 래퍼
- [ ] `MadeByClaudeCode/Content/Python/` — idempotent 에셋 생성기 (`create_input_assets.py` 등)
- [ ] `.claude/docs/coding-standards.md` AC 타입 표에 **`AUTOMATED_EDITOR`** 추가 (로그 + 파일 존재 검증 기준)
- [ ] `.claude/hooks/validate-uasset-generated.sh` — PostToolUse 훅으로 스토리 종료 시 에셋 존재 검증
- [ ] `docs/engine-reference/unreal/automation-python.md` — UE 5.6 Python API 로컬 레퍼런스
- [ ] `chongdashu/unreal-mcp` UE 5.6 호환성을 2–3개월 후 재평가 → 안정되면 MCP 파일럿

> 💬 이 한계점들은 CCGS 프레임워크의 결함이라기보다는, **Unreal Engine 자체가 에디터 중심 워크플로우로 설계된 데에서 기인**합니다. Godot (`.tscn`/`.tres` = 텍스트) 나 Unity (YAML 프리팹) 와 달리 UE는 `.uasset` 이 바이너리이기 때문에 AI 주도 자동화의 결이 다릅니다. 이 저장소는 그 간극을 메우는 시도를 공개 기록으로 남기는 것도 목표입니다.

---

## 🔗 관련 링크

- 🎯 **Claude Code Game Studios (CCGS)**: https://github.com/bong9tutor/Claude-Code-Game-Studios
- 🤖 **Claude Code**: https://claude.com/claude-code
- 🎮 **Unreal Engine 5.6**: https://www.unrealengine.com

---

## 📄 라이선스

- 이 저장소의 가이드 콘텐츠: 별도 명시 전까지 저작자(봉구튜터)에게 귀속
- `.claude/` · `design/` · `production/` · `_ccgs_upstream/` : [CCGS MIT 라이선스](./CCGS-LICENSE)

---

<div align="center">

**Claude Code Game Studios** · Unreal Engine 5.6 · 2026

Made with ❤️ for beginners

</div>
