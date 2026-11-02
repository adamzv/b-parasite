# Crash boot guard regression verification — 2026-10-28

Command from the project root:

```sh
python3 code/tests/boot_guard/run_tests.py
```

The runner compiles the actual `samples/zigbee/src/boot_loop_guard.c` as host C11 with warnings as errors. Filesystem calls operate on temporary host files, with failures injected at the FS boundary. Work is queued until the test runs it; atomic operations use host C atomics. Each scenario runs in a fresh process, matching static state reset at MCU boot.

The migration test seeds legacy `/lfs/boots_since_join` with 12, leaves the new crash-only file absent, and verifies that the first software crash boot returns zero, stores one in `/lfs/crash_boots`, and preserves the legacy value. Counter return values remain **before** the current boot, so the existing threshold comparison applies unchanged.

Error tests verify conservative zero results and no outstanding handles. A short/failed read does not write a new counter. Seek, write, and close failures cannot report a confirmed previous count. An unsuccessful close may still have written data; the API therefore returns zero rather than claiming the transaction persisted. The mock follows the actual Zephyr LittleFS constraint: `littlefs_close()` releases file data even if it returns an error, so the caller must close exactly once.

## Red: before production edits

Exit status **1**:

```text
legacy_count_ignored: FAIL line 117: prst_boot_loop_guard_register_boot(true) == 0
FAIL line 118: stored(crash_file, 1)
FAIL line 119: stored(legacy_file, 12)
first_crash: FAIL line 122: stored(crash_file, 1)
crash_streak: FAIL line 127: prst_boot_loop_guard_register_boot(true) == 4
FAIL line 128: stored(crash_file, 5)
FAIL line 129: prst_boot_loop_guard_register_boot(true) == 5
FAIL line 130: stored(crash_file, 6)
FAIL line 131: stored(legacy_file, 12)
power_reset: FAIL line 136: !exists(crash_file)
FAIL line 137: stored(legacy_file, 12)
FAIL line 139: stored(crash_file, 1)
pin_reset: FAIL line 136: !exists(crash_file)
FAIL line 137: stored(legacy_file, 12)
FAIL line 139: stored(crash_file, 1)
read_error: FAIL line 149: writes == 0
short_read: FAIL line 149: writes == 0
seek_error: FAIL line 144: prst_boot_loop_guard_register_boot(true) == 0
FAIL line 149: writes == 0
write_error: FAIL line 144: prst_boot_loop_guard_register_boot(true) == 0
short_write: FAIL line 144: prst_boot_loop_guard_register_boot(true) == 0
close_error: FAIL line 144: prst_boot_loop_guard_register_boot(true) == 0
open_error: PASS
clear_success: FAIL line 158: !exists(crash_file)
FAIL line 159: stored(legacy_file, 12)
clear_missing: FAIL line 159: stored(legacy_file, 12)
clear_retry: FAIL line 172: unlinks == 2
FAIL line 173: !exists(crash_file)
submit_retry: FAIL line 183: submissions == 2
FAIL line 184: !exists(crash_file)
clear_coalesces: FAIL line 193: !exists(crash_file)
FAIL line 194: stored(legacy_file, 12)
1/17 boot guard tests passed
```

## Green: after production edits

Exit status **0**:

```text
legacy_count_ignored: PASS
first_crash: PASS
crash_streak: PASS
power_reset: PASS
pin_reset: PASS
read_error: PASS
short_read: PASS
seek_error: PASS
write_error: PASS
short_write: PASS
close_error: PASS
open_error: PASS
clear_success: PASS
clear_missing: PASS
clear_retry: PASS
submit_retry: PASS
clear_coalesces: PASS
17/17 boot guard tests passed
```

Async tests verify deferred execution, successful/missing-file completion, coalescing while pending, retry after a failed unlink, and retry after a failed work submission. Success suppresses redundant clears; failures leave another request possible.

Physical flash failures and MCU reset-cause reporting remain hardware validation. No firmware was flashed and no legacy storage file was modified by the implementation work.
