#!/usr/bin/env bash
# Serve the docs/ static site (the GitHub Pages app) locally.
#
# Usage:
#   ./tools/bash/serve-docs.sh [port]
#
# Examples:
#   ./tools/bash/serve-docs.sh
#   ./tools/bash/serve-docs.sh 8080
set -euo pipefail

PORT=${1:-3000}

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
DOCS_DIR="$REPO_ROOT/docs"

if [[ ! -d "$DOCS_DIR" ]]; then
  echo "docs/ folder not found at $DOCS_DIR" >&2
  exit 2
fi

echo "Serving static site from: $DOCS_DIR"

open_browser() {
  local url=$1
  if command -v xdg-open &>/dev/null; then
    xdg-open "$url" &>/dev/null &
  elif command -v open &>/dev/null; then
    open "$url" &>/dev/null &
  else
    echo "Open $url in your browser"
  fi
}

# Prefer python3, fall back to python, then npx http-server.
if command -v python3 &>/dev/null; then
  open_browser "http://localhost:$PORT"
  exec python3 -m http.server "$PORT" --directory "$DOCS_DIR"
elif command -v python &>/dev/null; then
  open_browser "http://localhost:$PORT"
  exec python -m http.server "$PORT" --directory "$DOCS_DIR"
elif command -v npx &>/dev/null; then
  open_browser "http://localhost:$PORT"
  exec npx http-server "$DOCS_DIR" -p "$PORT"
else
  echo "Unable to start a server: install Python 3 or Node (npx)." >&2
  exit 3
fi
