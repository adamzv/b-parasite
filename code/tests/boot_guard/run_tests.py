#!/usr/bin/env python3
"""Exercise the production boot guard with temporary files and injected FS errors."""
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1] / "samples" / "zigbee" / "src"
CASES = (
    "legacy_count_ignored", "first_crash", "crash_streak", "power_reset", "pin_reset",
    "read_error", "short_read", "seek_error", "write_error", "short_write", "close_error",
    "open_error", "clear_success", "clear_missing", "clear_retry", "submit_retry",
    "clear_coalesces",
)


def main():
    with tempfile.TemporaryDirectory(prefix="bparasite-boot-guard-build-") as temp:
        binary = str(Path(temp) / "test_boot_guard")
        subprocess.run([
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-parameter", "-D_POSIX_C_SOURCE=200809L", "-DCONFIG_LOG_DEFAULT_LEVEL=0",
            "-I", str(HERE / "mocks"), "-I", str(SOURCE),
            str(SOURCE / "boot_loop_guard.c"), str(HERE / "test_boot_guard.c"), "-o", binary,
        ], check=True)
        failures = 0
        for case in CASES:
            with tempfile.TemporaryDirectory(prefix="bparasite-boot-guard-files-") as files:
                result = subprocess.run([binary, case, files], capture_output=True, text=True, timeout=10)
            print(f"{case}: {(result.stdout + result.stderr).strip()}")
            failures += result.returncode != 0
        print(f"{len(CASES) - failures}/{len(CASES)} boot guard tests passed")
        return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
