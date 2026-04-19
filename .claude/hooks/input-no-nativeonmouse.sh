#!/usr/bin/env bash
# ADR-0008 Forbidden Approaches: NativeOnMouse* override 금지
# Story 1-21 AC: UMG raw mouse event override 차단
#   - NativeOnMouseButtonDown
#   - NativeOnMouseMove
#   - NativeOnMouseButtonUp
#
# 대안: Enhanced Input (IA_PointerMove + IA_OfferCard Hold subscription)
#
# Usage:
#   bash .claude/hooks/input-no-nativeonmouse.sh
#   exit 0 (OK) or 1 (violation)

set -euo pipefail

readonly SOURCE_DIR="MadeByClaudeCode/Source/MadeByClaudeCode"
readonly PATTERN='NativeOnMouseButtonDown\|NativeOnMouseMove\|NativeOnMouseButtonUp'

MATCHES=$(grep -rEn "$PATTERN" "$SOURCE_DIR" \
    --include="*.h" \
    --exclude-dir="Tests" \
    2>/dev/null || true)

if [[ -n "$MATCHES" ]]; then
    echo "ERROR: NO_NATIVEONMOUSE_OVERRIDE 위반 — UMG raw mouse event override 검출:"
    echo "$MATCHES"
    echo ""
    echo "수정: Enhanced Input subscription (IA_PointerMove/IA_OfferCard Hold) 사용하세요."
    echo "참고: docs/architecture/adr-0008-umg-drag-implementation.md §Forbidden Approaches"
    exit 1
fi

echo "OK: NO_NATIVEONMOUSE_OVERRIDE 통과 (NativeOnMouse* override 0건)"
exit 0
