# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 기본 언어

사용자와의 대화 및 문서 작성의 기본 언어는 **한국어**입니다.

## 저장소 성격

이 저장소는 코드 프로젝트가 아니라 **정적 HTML로 작성된 한국어 튜토리얼 사이트**입니다. 빌드·테스트·린트 과정이 없으며, 브라우저로 HTML 파일을 직접 열어 확인합니다.

대상 주제: **Claude Code Game Studios(CCGS) 프레임워크 + Unreal Engine 5.6** 을 활용한 비전공자용 게임 개발 가이드.

## 구조

- `docs/ccgs-guide/index.html` — 진입점(랜딩). 5개 파트 카드로 이동.
- `docs/ccgs-guide/part1-setup.html` … `part5-production.html` — 파트별 본문.
- `docs/ccgs-guide/style.css` — 공유 스타일(각 HTML에 `<style>` 인라인이 함께 쓰일 수 있음).
- `README.md` — 가이드 개요 및 목차.

5개 파트는 **순차적 학습 흐름**으로 설계되어 있습니다: 설치(Part 1) → 컨셉(Part 2) → 시스템 설계/GDD(Part 3) → 아키텍처·스토리 분해(Part 4) → 구현·릴리스(Part 5). 각 파트는 해당 단계의 CCGS 슬래시 명령어(`/start`, `/design-system`, `/dev-story` 등)를 소개합니다.

## 편집 시 주의점

- HTML 파일들은 **공통 디자인 시스템**(색상 변수 `--accent`, `--claude`, `--ue`, `--green`, `--purple` 등, `Noto Sans KR`/`JetBrains Mono` 폰트)을 공유합니다. 새 페이지나 섹션을 추가할 때 기존 CSS 변수와 pill/tag/part-card 등의 클래스 네이밍을 따르십시오.
- 파트 간 링크는 상대 경로(`part2-concept.html`)를 사용합니다.
- 외부 리포 링크는 `https://github.com/bong9tutor/Claude-Code-Game-Studios` 입니다.
- 독자는 **프로그래밍 경험이 전혀 없다고 가정**하고 설명 난이도·용어를 유지하십시오.
