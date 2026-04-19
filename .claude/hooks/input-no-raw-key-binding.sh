#!/usr/bin/env bash
# Input Abstraction Rule 1: NO_RAW_KEY_BINDING CI grep
# Story 1-21 AC: raw key binding 금지
#   - BindKey / BindAxis (legacy Input API)
#   - EKeys::* 직접 참조
# Input 서브시스템 파일 (Source/MadeByClaudeCode/Input/) 자체는 제외
# Enhanced Input TestingInjectKey test helper는 Tests/ 에서 EKeys:: 참조 허용
#
# Usage:
#   bash .claude/hooks/input-no-raw-key-binding.sh
#   exit 0 (OK) or 1 (violation)

set -euo pipefail

readonly SOURCE_DIR="MadeByClaudeCode/Source/MadeByClaudeCode"
readonly PATTERN='BindKey\|BindAxis\|EKeys::'

# Input/ 디렉토리는 Enhanced Input Subsystem 자체 구현이므로 제외
# Tests/ 디렉토리의 MockKey/EKeys::는 mock injection 허용
MATCHES=$(grep -rEn "$PATTERN" "$SOURCE_DIR" \
    --include="*.cpp" \
    --include="*.h" \
    --exclude-dir="Input" \
    --exclude-dir="Tests" \
    2>/dev/null || true)

if [[ -n "$MATCHES" ]]; then
    echo "ERROR: NO_RAW_KEY_BINDING 위반 — raw key binding 검출:"
    echo "$MATCHES"
    echo ""
    echo "수정: UInputAction + UInputMappingContext (Enhanced Input) 사용하세요."
    echo "참고: docs/architecture/adr-0008-umg-drag-implementation.md"
    exit 1
fi

echo "OK: NO_RAW_KEY_BINDING 통과 (raw key binding 0건)"
exit 0
