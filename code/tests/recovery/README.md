# Recovery regression tests

Run from the project root:

```sh
python3 code/tests/recovery/run_tests.py
```

The runner compiles the production `restart_handler.c` with host timer, workqueue,
mutex and Zigbee boundaries. Each case runs in a fresh process. Time and work
delivery are controlled separately, so expiry can race with success or a new
recovery cycle. The in-flight stop case uses real pthread mutexes and condition
variables, without timing-dependent sleeps.

Covered behavior:

- Duplicate failure signals preserve the current retry deadline.
- SDK no-ops and unsuccessful rejoin rounds leave another retry scheduled.
- Backoff reaches its cap and stays there.
- Explicit success discards pending work, including a late timer callback.
- A new recovery cycle starts at the initial interval; stale work cannot make it
  retry early.
- Stop waits for an in-flight SDK request and prevents subsequent requests.

The SDK input double models its `wait_for_user_input` gate. These tests do not
execute ZBOSS, radio traffic, Zephyr scheduling or Trust Center rejoin. The
production build and hardware recovery remain separate validation steps.

Before implementation, the suite compiled the unchanged production handler and
failed all seven cases. After implementation, the same cases passed.
