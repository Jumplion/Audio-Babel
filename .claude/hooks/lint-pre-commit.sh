#!/usr/bin/env bash
# PreToolUse hook (Bash, filtered to `git commit*` via settings.json "if"):
# lints every staged file before the commit runs. Never blocks — always
# exits 0 with permissionDecision "allow"; issues are surfaced to Claude
# via additionalContext instead of failing the tool call.
set -uo pipefail

INPUT=$(cat)
COMMAND=$(printf '%s' "$INPUT" | jq -r '.tool_input.command // empty')

# Defense in depth beyond the settings.json "if" matcher.
case "$COMMAND" in
  *"git commit"*) ;;
  *) exit 0 ;;
esac

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
cd "$PROJECT_DIR" 2>/dev/null || exit 0

mapfile -t STAGED_FILES < <(git diff --cached --name-only --diff-filter=ACM 2>/dev/null)
[[ ${#STAGED_FILES[@]} -eq 0 ]] && exit 0

OUTPUT=""
RAN_TSC=false

run() {
  local label="$1"; shift
  local result
  if ! result=$("$@" 2>&1); then
    OUTPUT+="❌ $label:"$'\n'"$result"$'\n\n'
  fi
}

for FILE in "${STAGED_FILES[@]}"; do
  [[ -f "$FILE" ]] || continue
  EXT="${FILE##*.}"
  case "$EXT" in
    js|mjs|cjs)
      [[ -f eslint.config.js ]] && run "ESLint ($FILE)" npx --no-install eslint "$FILE"
      ;;
    ts|tsx)
      if [[ -f tsconfig.json && "$RAN_TSC" == false ]]; then
        run "TypeScript type-check" npx --no-install tsc --noEmit -p tsconfig.json
        RAN_TSC=true
      fi
      ;;
    html)
      [[ -f .htmlhintrc ]] && run "HTMLHint ($FILE)" npx --no-install htmlhint "$FILE"
      ;;
    css)
      [[ -f .stylelintrc.json ]] && run "Stylelint ($FILE)" npx --no-install stylelint "$FILE"
      ;;
    cpp|cc|cxx|h|hpp)
      [[ -f .clang-format ]] && run "clang-format ($FILE)" clang-format --dry-run --Werror "$FILE"
      [[ -f build/compile_commands.json ]] && run "clang-tidy ($FILE)" clang-tidy -p build "$FILE"
      ;;
  esac
done

[[ -z "$OUTPUT" ]] && exit 0

OUTPUT=$(printf '%s' "$OUTPUT" | head -c 4000)
REASON="Lint/type-check issues found in staged files before this commit:"$'\n\n'"$OUTPUT"

jq -n --arg reason "$REASON" '{
  systemMessage: "⚠️  Lint issues found in staged files before commit — see details.",
  hookSpecificOutput: {
    hookEventName: "PreToolUse",
    permissionDecision: "allow",
    permissionDecisionReason: $reason,
    additionalContext: $reason
  }
}'
exit 0
