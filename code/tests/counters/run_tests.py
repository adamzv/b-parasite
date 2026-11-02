#!/usr/bin/env python3
"""Compile production counters against bounded queues and temporary host files."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1] / "samples" / "zigbee" / "src"
CASES = (
    "deferred", "copied_name", "multiple", "concurrent_producers", "queue_full",
    "name_limit", "bad_name", "submit_retry", "full_queue_retry",
    "open_error", "read_error", "seek_error", "write_error", "sync_error", "close_error", "sync_api",
)


def main():
    with tempfile.TemporaryDirectory(prefix="bparasite-counters-build-") as temp:
        binary = str(Path(temp) / "test_counters")
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-Wno-sign-compare", "-pthread",
            "-D_POSIX_C_SOURCE=200809L", "-DCONFIG_LOG_DEFAULT_LEVEL=0",
            "-I", str(HERE / "mocks"), "-I", str(SOURCE),
            "-I", str(HERE.parents[1] / "prstlib" / "include"),
            str(SOURCE / "debug_counters.c"), str(HERE / "test_counters.c"), "-o", binary,
        ], check=True)
        failures = 0
        for case in CASES:
            with tempfile.TemporaryDirectory(prefix="bparasite-counters-files-") as files:
                result = subprocess.run([binary, case, files], capture_output=True, text=True, timeout=10)
            output = (result.stdout + result.stderr).strip() or f"exit {result.returncode}"
            print(f"{case}: {output}")
            failures += result.returncode != 0
        print(f"{len(CASES) - failures}/{len(CASES)} counter tests passed")
        return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
