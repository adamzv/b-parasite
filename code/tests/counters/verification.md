# Asynchronous counter regression verification — 2026-10-28

Command from the project root:

```sh
python3 code/tests/counters/run_tests.py
```

The runner compiles the actual `samples/zigbee/src/debug_counters.c` as host C11 with warnings as errors. Filesystem calls operate on temporary files; the test records every FS call and whether it occurs inside the worker. A fixed-size queue mock copies whole messages under a pthread mutex, and concurrent producers use real host threads. Work remains deferred until the test runs it.

## Red

The first compile failed because `prst_debug_counters_increment_async` did not yet exist. After exposing the API as a forwarding call to the existing synchronous implementation, the behavioral tests produced exit status **1**:

```text
deferred: FAIL line 149: fs_calls == 0 && !exists("sensor")
copied_name: FAIL line 156: fs_calls == 0
multiple: FAIL line 163: fs_calls == 0
concurrent_producers: FAIL line 170: fs_calls == 0
queue_full: FAIL line 177: prst_debug_counters_increment_async("overflow") == -ENOMSG
name_limit: FAIL line 184: fs_calls == 0
bad_name: exit -11
submit_retry: FAIL line 197: prst_debug_counters_increment_async("retained") == -EIO
full_queue_retry: FAIL line 176: prst_debug_counters_increment_async("soil") == (retry ? -EIO : 0)
open_error: FAIL line 204: prst_debug_counters_increment_async("bad") == 0
seek_error: FAIL line 204: prst_debug_counters_increment_async("bad") == 0
write_error: FAIL line 205: fs_calls == 0
sync_error: FAIL line 204: prst_debug_counters_increment_async("bad") == 0
close_error: FAIL line 204: prst_debug_counters_increment_async("bad") == 0
sync_api: PASS
1/15 counter tests passed
```

This intermediate implementation demonstrates the defects the queue must prevent: synchronous FS access, borrowed caller data, lack of queue bounds, invalid-name crashes, and missing submission retry behavior.

## Green

After implementing the fixed queue and worker, the same behavioral cases passed. A read-error cleanup characterization case was then added without changing production behavior; the final run returned exit status **0**:

```text
deferred: PASS
copied_name: PASS
multiple: PASS
concurrent_producers: PASS
queue_full: PASS
name_limit: PASS
bad_name: PASS
submit_retry: PASS
full_queue_retry: PASS
open_error: PASS
read_error: PASS
seek_error: PASS
write_error: PASS
sync_error: PASS
close_error: PASS
sync_api: PASS
16/16 counter tests passed
```

The queue holds eight copied names of up to 64 bytes. Calls use `K_NO_WAIT` and perform no filesystem operation. Worker submission failure returns the error and retains the queued request; the next valid call attempts submission again, including when the queue is already full. Retrying the same increment after a submission error can therefore enqueue it twice; the API documents retention rather than promising that an error means nothing was queued.

Worker tests inject open/read/seek/write/sync/close errors, verify no leaked handles, and verify that another queued counter still completes. Existing synchronous counter read/write error semantics remain unchanged; in particular, its read helper treats read errors as first access, and its short/failed write logging does not by itself change the return code. These pre-existing behaviors are outside this scheduling-only fix.

No hardware, remote service, or firmware storage was touched. Target SDK workqueue behavior and physical flash timing remain part of the parent firmware build and hardware validation.
