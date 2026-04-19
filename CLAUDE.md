# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 기본 언어

사용자와의 대화 및 문서 작성의 기본 언어는 **한국어**입니다.

## 저장소 성격

이 저장소는 **3가지 역할을 동시에 수행**합니다:

1. **가이드 사이트** — `docs/ccgs-guide/` 의 정적 HTML로 작성된 한국어 튜토리얼 (빌드 없이 브라우저로 열어 확인)
2. **CCGS 프레임워크 통합** — `.claude/`, `design/`, `production/` 에 CCGS 에이전트·스킬·워크플로우 포함
3. **샘플 언리얼 프로젝트** — `MadeByClaudeCode/` 에 UE 5.6 Blank C++ 프로젝트

대상 주제: **Claude Code Game Studios(CCGS) 프레임워크 + Unreal Engine 5.6** 을 활용한 비전공자용 게임 개발 가이드.

## 구조

### 가이드 사이트
- `docs/ccgs-guide/index.html` — 진입점(랜딩). 5개 파트 카드로 이동.
- `docs/ccgs-guide/part1-setup.html` … `part5-production.html` — 파트별 본문.
- `docs/ccgs-guide/style.css` — 공유 스타일.
- `README.md` — 가이드 개요 및 목차.

5개 파트는 **순차적 학습 흐름**으로 설계되어 있습니다: 설치(Part 1) → 컨셉(Part 2) → 시스템 설계/GDD(Part 3) → 아키텍처·스토리 분해(Part 4) → 구현·릴리스(Part 5).

### CCGS 프레임워크 (루트에 통합)
- `.claude/` — 에이전트, 스킬, 훅, 규칙 정의
- `design/` — GDD, 시스템 설계 문서 저장 위치
- `production/` — 스프린트, 스테이지, 세션 상태 관리
- `_ccgs_upstream/` — CCGS 원본 (subtree, 업데이트 추적용. 실제 동작은 루트 파일 사용)
- `CCGS-LICENSE`, `CCGS-UPGRADING.md` — CCGS 라이선스/업그레이드 가이드

### 샘플 UE 프로젝트
- `MadeByClaudeCode/` — UE 5.6 Blank C++ 프로젝트 (표준 UE 구조 유지)
  - `MadeByClaudeCode.uproject`
  - `Config/` — 엔진·게임·에디터 설정
  - `Content/` — 에셋
  - `Source/MadeByClaudeCode/` — C++ 코드

## Technology Stack

- **Engine**: Unreal Engine 5.6
- **Language**: C++ (primary), Blueprint (gameplay prototyping)
- **Build System**: Unreal Build Tool (UBT)
- **Asset Pipeline**: Unreal Content Pipeline
- **Version Control**: Git with trunk-based development

## CCGS 문서 참조

@.claude/docs/directory-structure.md
@.claude/docs/technical-preferences.md
@.claude/docs/coordination-rules.md
@.claude/docs/coding-standards.md
@.claude/docs/context-management.md

## Engine Version Reference

@docs/engine-reference/unreal/VERSION.md

## Collaboration Protocol

**User-driven collaboration, not autonomous execution.**
Every task follows: **Question → Options → Decision → Draft → Approval**

- 에이전트는 Write/Edit 사용 전 "May I write this to [filepath]?" 로 확인해야 함
- 여러 파일 변경은 명시적 승인 필요
- 사용자 지시 없이는 커밋 금지

## CCGS 업데이트 동기화

CCGS 원본이 업데이트되면 다음 명령어로 `_ccgs_upstream/` 을 갱신할 수 있습니다:

```bash
git subtree pull --prefix=_ccgs_upstream ccgs main --squash
```

업데이트 후 루트의 `.claude/`, `design/`, `production/` 과 수동 비교·병합이 필요합니다.

## 편집 시 주의점

- HTML 가이드 파일들은 **공통 디자인 시스템**(색상 변수 `--accent`, `--claude`, `--ue`, `--green`, `--purple` 등, `Noto Sans KR`/`JetBrains Mono` 폰트)을 공유합니다.
- 파트 간 링크는 상대 경로(`part2-concept.html`)를 사용합니다.
- 외부 CCGS 리포 링크는 `https://github.com/bong9tutor/Claude-Code-Game-Studios` 입니다.
- 독자는 **프로그래밍 경험이 전혀 없다고 가정**하고 설명 난이도·용어를 유지하십시오.
- UE 프로젝트(`MadeByClaudeCode/`)는 UE 표준 구조를 따르며, CCGS 에이전트는 `Source/MadeByClaudeCode/` 하위에서 C++ 파일을 생성·수정합니다.
