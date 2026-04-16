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
