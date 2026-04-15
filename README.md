<div align="center">

# 🎮 CCGS + Unreal Engine 5.6 게임 만들기

### 비전공자를 위한 AI 게임 개발 완전 가이드

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

## 📚 목차

정적 HTML로 작성된 5개 파트의 튜토리얼 사이트입니다. GitHub Pages에서 렌더링된 페이지를 보거나, 저장소를 클론하여 `docs/ccgs-guide/index.html` 을 브라우저로 직접 열 수 있습니다.

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

### 옵션 1. 온라인으로 보기 (권장)

👉 **https://bong9tutor.github.io/ue-ccgs/ccgs-guide/**

### 옵션 2. 로컬에서 보기

```bash
git clone https://github.com/bong9tutor/ue-ccgs.git
cd ue-ccgs
# 브라우저로 열기
start docs/ccgs-guide/index.html   # Windows
open  docs/ccgs-guide/index.html   # macOS
```

---

## 🗂️ 저장소 구조

```
ue-ccgs/
└── docs/
    └── ccgs-guide/
        ├── index.html              # 메인 랜딩 페이지
        ├── part1-setup.html        # Part 1: 사전 준비
        ├── part2-concept.html      # Part 2: 게임 컨셉
        ├── part3-design.html       # Part 3: 시스템 설계
        ├── part4-architecture.html # Part 4: 기술 설정
        ├── part5-production.html   # Part 5: 프로덕션
        └── style.css               # 공통 스타일
```

---

## 🔗 관련 링크

- 🎯 **Claude Code Game Studios (CCGS)**: https://github.com/bong9tutor/Claude-Code-Game-Studios
- 🤖 **Claude Code**: https://claude.com/claude-code
- 🎮 **Unreal Engine 5.6**: https://www.unrealengine.com

---

<div align="center">

**Claude Code Game Studios** · Unreal Engine 5.6 · 2026

Made with ❤️ for beginners

</div>
