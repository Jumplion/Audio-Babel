#!/usr/bin/env bash
# PostToolUse hook (Write|Edit): auto-runs tests after Claude writes test files.
# Never blocks — always exits 0. Results are fed back to Claude via
# hookSpecificOutput.additionalContext instead of failing the tool call.
set -uo pipefail

INPUT=$(cat)
FILE_PATH=$(printf '%s' "$INPUT" | jq -r '.tool_input.file_path // .tool_response.filePath // empty')

[[ -z "$FILE_PATH" ]] && exit 0
[[ -f "$FILE_PATH" ]] || exit 0

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}"
REL_PATH="${FILE_PATH#"$PROJECT_DIR"/}"

cd "$PROJECT_DIR" 2>/dev/null || exit 0

KIND=""
case "$REL_PATH" in
  cpp/tests/test_*.cpp) KIND="cpp" ;;
  *.test.js | *.spec.js | *__tests__*) KIND="js" ;;
esac

[[ -z "$KIND" ]] && exit 0

OUTPUT=""

if [[ "$KIND" == "cpp" ]]; then
  BUILD_DIR="$PROJECT_DIR/build"
  if [[ ! -d "$BUILD_DIR" ]]; then
    OUTPUT="ℹ️  No build/ directory found, skipped running C++ tests for $REL_PATH. Build first: ./tools/bash/build.sh Debug \"Unix Makefiles\" build"
  else
    JOBS=$(nproc 2>/dev/null || echo 1)
    if BUILD_RESULT=$(cmake --build "$BUILD_DIR" -j"$JOBS" --target tests_catch2 2>&1); then
      if TEST_RESULT=$(cd "$BUILD_DIR" && ./tests_catch2 -d yes 2>&1); then
        OUTPUT="✅ C++ unit tests passed (tests_catch2), triggered by $REL_PATH"$'\n'"$(printf '%s' "$TEST_RESULT" | tail -20)"
      else
        OUTPUT="❌ C++ unit tests failed (tests_catch2), triggered by $REL_PATH:"$'\n'"$TEST_RESULT"
      fi
    else
      OUTPUT="❌ Building tests_catch2 failed:"$'\n'"$BUILD_RESULT"
    fi
  fi
elif [[ "$KIND" == "js" ]]; then
  if jq -e '.scripts.test' package.json >/dev/null 2>&1; then
    if TEST_RESULT=$(npm test 2>&1); then
      OUTPUT="✅ JS tests passed, triggered by $REL_PATH"$'\n'"$TEST_RESULT"
    else
      OUTPUT="❌ JS tests failed, triggered by $REL_PATH:"$'\n'"$TEST_RESULT"
    fi
  else
    OUTPUT="ℹ️  No \"test\" script in package.json, skipped running JS tests for $REL_PATH"
  fi
fi

[[ -z "$OUTPUT" ]] && exit 0

# Cap output so a large test dump doesn't blow up context.
OUTPUT=$(printf '%s' "$OUTPUT" | head -c 4000)

jq -n --arg ctx "$OUTPUT" --arg file "$REL_PATH" '{
  hookSpecificOutput: {
    hookEventName: "PostToolUse",
    additionalContext: $ctx
  }
}'
exit 0
