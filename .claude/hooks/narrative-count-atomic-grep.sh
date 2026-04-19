#!/usr/bin/env bash
# NARRATIVE_COUNT_ATOMIC_COMMIT — CI static analysis
# Story 1-20 AC (2): IncrementNarrativeCount/TriggerSaveForNarrative 외부 callsite = 0
#
# 탐지 대상: 인스턴스를 통한 . / -> 접근으로 private 메서드를 호출하는 외부 callsite
#   위반 예: SomeTime->IncrementNarrativeCount()
#   위반 예: TimeRef.TriggerSaveForNarrative()
#
# 정상 패턴 (탐지 안 됨):
#   void UMossTimeSessionSubsystem::IncrementNarrativeCount() { ... }  (정의 — . / -> 없음)
#   IncrementNarrativeCount();   (self 직접 호출 — . / -> 없음)
#   TriggerSaveForNarrative();   (self 직접 호출 — . / -> 없음)
#
# Usage:
#   bash .claude/hooks/narrative-count-atomic-grep.sh
#   exit 0 (OK) or 1 (violation)
#
# CI 연동:
#   .github/workflows 또는 pre-commit hook에서 이 스크립트를 실행.
#   exit 1이면 빌드/commit 차단.

set -euo pipefail

# 저장소 루트 기준 소스 디렉터리
readonly SOURCE_DIR="MadeByClaudeCode/Source/MadeByClaudeCode"

# 각 private method의 인스턴스 접근 패턴(. 또는 ->)을 탐지
# 단, 이 grep 스크립트 자체 파일은 제외 (.sh 파일 제외 — *.cpp, *.h만 대상)
MATCHES=$(grep -rEn \
    "\.IncrementNarrativeCount\b|\.TriggerSaveForNarrative\b|->IncrementNarrativeCount\b|->TriggerSaveForNarrative\b" \
    "$SOURCE_DIR" \
    --include="*.cpp" \
    --include="*.h" \
    2>/dev/null || true)

# 빈 줄 제거 후 카운트
COUNT=$(echo "$MATCHES" | grep -cE "\S" || true)

if (( COUNT > 0 )); then
    echo "ERROR: NARRATIVE_COUNT_ATOMIC_COMMIT 위반 — 외부 callsite 발견 ($COUNT 건):"
    echo ""
    echo "$MATCHES"
    echo ""
    echo "수정 방법:"
    echo "  IncrementNarrativeCountAndSave() wrapper만 사용하세요."
    echo "  IncrementNarrativeCount() / TriggerSaveForNarrative() 는 private 메서드입니다."
    echo "  외부 코드는 반드시 IncrementNarrativeCountAndSave()를 통해야 합니다."
    exit 1
fi

echo "OK: NARRATIVE_COUNT_ATOMIC_COMMIT 통과 (외부 . / -> callsite 0건)"
exit 0
