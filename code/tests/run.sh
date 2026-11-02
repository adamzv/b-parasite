#!/usr/bin/env bash
set -euo pipefail
TASK_DIR="$(cd "$(dirname "$0")" && pwd)"
TASK_ROOT="$(cd "$TASK_DIR/../.." && pwd)"
cd "$TASK_ROOT"
bash code/tests/sensors/run.sh
python3 code/tests/boot_guard/run_tests.py
python3 code/tests/recovery/run_tests.py
bash code/tests/application/run.sh
python3 -m unittest discover -s code/tests/identity -p 'test_*.py' -v
python3 code/tests/counters/run_tests.py
python3 -m unittest discover -s code/tests/scripts -p 'test_*.py' -v
sh code/tests/integration/run.sh
