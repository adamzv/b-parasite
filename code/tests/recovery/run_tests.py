#!/usr/bin/env python3
"""Compile the production recovery handler against controllable SDK boundaries."""

import os
from pathlib import Path
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1] / "samples" / "zigbee" / "src"
CASES = (
    "duplicate_start",
    "noop_and_failure",
    "capped_backoff",
    "pending_success",
    "late_timer",
    "restart_after_stop",
    "running_stop_race",
)


def main():
    with tempfile.TemporaryDirectory(prefix="bparasite-recovery-") as temp:
        binary = str(Path(temp) / "test_restart_handler")
        subprocess.run(
            [
                os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                "-Werror", "-Wno-unused-parameter", "-pthread",
                "-DCONFIG_LOG_DEFAULT_LEVEL=0",
                "-DCONFIG_PRST_ZB_RESTART_WATCHDOG_TIMEOUT_SEC=600",
                "-DCONFIG_PRST_ZB_RESTART_WATCHDOG_MAX_TIMEOUT_SEC=7200",
                "-I", str(HERE / "mocks"), "-I", str(SOURCE),
                str(HERE / "test_restart_handler.c"),
                str(SOURCE / "restart_handler.c"), "-o", binary,
            ],
            check=True,
        )
        failures = 0
        for case in CASES:
            result = subprocess.run([binary, case], capture_output=True, text=True, timeout=10)
            print(f"{case}: {(result.stdout + result.stderr).strip()}")
            failures += result.returncode != 0
        print(f"{len(CASES) - failures}/{len(CASES)} recovery tests passed")
        return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
