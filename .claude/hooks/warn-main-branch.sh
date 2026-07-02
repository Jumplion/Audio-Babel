#!/usr/bin/env bash
# PreToolUse hook (Write|Edit): warns when editing files while on main/master.
# Never blocks the edit — always exits 0 with permissionDecision "allow".
set -uo pipefail

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$PROJECT_DIR" 2>/dev/null || exit 0

BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "")

if [[ "$BRANCH" == "main" || "$BRANCH" == "master" ]]; then
  REASON="You're about to edit files directly on '$BRANCH'. Consider creating a feature branch first, e.g.: git checkout -b feature/my-change"
  jq -n --arg reason "$REASON" '{
    systemMessage: ("⚠️  " + $reason),
    hookSpecificOutput: {
      hookEventName: "PreToolUse",
      permissionDecision: "allow",
      permissionDecisionReason: $reason,
      additionalContext: $reason
    }
  }'
fi

exit 0
