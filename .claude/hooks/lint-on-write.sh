#!/usr/bin/env bash
# PreToolUse or PostToolUse hook (Write|Edit): runs type-check/lint for
# JS/TS, C++, HTML, CSS. Pass the hook event name as $1 ("PreToolUse" or
# "PostToolUse") so the emitted JSON matches what that event expects.
# On PreToolUse this lints the file's state BEFORE the edit is applied
# (useful to surface pre-existing issues); on PostToolUse it lints the
# file AFTER the edit. Never blocks — always exits 0.
set -uo pipefail

HOOK_EVENT="${1:-PostToolUse}"

INPUT=$(cat)
FILE_PATH=$(printf '%s' "$INPUT" | jq -r '.tool_input.file_path // .tool_response.filePath // empty')

[[ -z "$FILE_PATH" ]] && exit 0
[[ -f "$FILE_PATH" ]] || exit 0

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
REL_PATH="${FILE_PATH#"$PROJECT_DIR"/}"
EXT="${FILE_PATH##*.}"

cd "$PROJECT_DIR" 2>/dev/null || exit 0

OUTPUT=""

run() {
  local label="$1"; shift
  local result
  if result=$("$@" 2>&1); then
    OUTPUT+="✅ $label: no issues"$'\n'
  else
    OUTPUT+="❌ $label found issues:"$'\n'"$result"$'\n\n'
  fi
}

case "$EXT" in
  js|mjs|cjs)
    [[ -f eslint.config.js ]] && run "ESLint" npx --no-install eslint "$FILE_PATH"
    ;;
  ts|tsx)
    if [[ -f tsconfig.json ]]; then
      run "TypeScript type-check" npx --no-install tsc --noEmit -p tsconfig.json
    else
      OUTPUT+="ℹ️  No tsconfig.json found, skipped TypeScript type-check for $REL_PATH"$'\n'
    fi
    ;;
  html)
    [[ -f .htmlhintrc ]] && run "HTMLHint" npx --no-install htmlhint "$FILE_PATH"
    ;;
  css)
    [[ -f .stylelintrc.json ]] && run "Stylelint" npx --no-install stylelint "$FILE_PATH"
    ;;
  cpp|cc|cxx|h|hpp)
    [[ -f .clang-format ]] && run "clang-format check" clang-format --dry-run --Werror "$FILE_PATH"
    if [[ -f build/compile_commands.json ]]; then
      run "clang-tidy" clang-tidy -p build "$FILE_PATH"
    else
      OUTPUT+="ℹ️  build/compile_commands.json not found, skipped clang-tidy for $REL_PATH (run: cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)"$'\n'
    fi
    ;;
  *)
    exit 0
    ;;
esac

[[ -z "$OUTPUT" ]] && exit 0

# Cap output so a large lint dump doesn't blow up context.
OUTPUT=$(printf '%s' "$OUTPUT" | head -c 4000)
PREFIX="Lint/type-check results for $REL_PATH"
[[ "$HOOK_EVENT" == "PreToolUse" ]] && PREFIX+=" (before this edit is applied)"
CONTEXT="$PREFIX:"$'\n'"$OUTPUT"

if [[ "$HOOK_EVENT" == "PreToolUse" ]]; then
  jq -n --arg ctx "$CONTEXT" '{
    hookSpecificOutput: {
      hookEventName: "PreToolUse",
      permissionDecision: "allow",
      permissionDecisionReason: $ctx,
      additionalContext: $ctx
    }
  }'
else
  jq -n --arg ctx "$CONTEXT" '{
    hookSpecificOutput: {
      hookEventName: "PostToolUse",
      additionalContext: $ctx
    }
  }'
fi
exit 0
