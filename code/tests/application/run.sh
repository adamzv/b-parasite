#!/usr/bin/env bash
set -euo pipefail
TASK_DIR="$(cd "$(dirname "$0")" && pwd)"
TASK_ROOT="$(cd "$TASK_DIR/../.." && pwd)"
TASK_BUILD="$(mktemp -d)"
trap 'rm -rf "$TASK_BUILD"' EXIT
"${CC:-cc}" -std=c11 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -I"$TASK_DIR/include" -I"$TASK_ROOT/prstlib/include" "$TASK_DIR/test_application.c" -lm -o "$TASK_BUILD/test_application"
"$TASK_BUILD/test_application"
