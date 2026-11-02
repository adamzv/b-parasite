# Sensor regression verification — 2026-10-28

Run from the project root:

```sh
code/tests/sensors/run.sh
```

The runner compiles the actual `prstlib/src/shtc3.c` and `prstlib/src/adc.c` with host C11, warnings as errors, and hardware-only Zephyr mocks. ADC is compiled separately for phototransistor and LDR boards. Executables live in a temporary directory and are removed on exit.

The fault model treats failed enable/start writes as possibly applied, so cleanup is required even when those operations report failure. A failed disable leaves the simulated circuit on; tests require the shutdown attempt and the returned error, not an impossible guarantee that broken hardware switched off. Caller outputs are compared byte-for-byte after every failed acquisition, conversion, or shutdown.

## Red: before production edits

Command: `code/tests/sensors/run.sh`; exit status **1**. The success fixtures passed; cleanup, error precedence, and partial-output regressions failed:

```text
PASS SHTC3 success (result=0, sleeps=1, output_ok=1)
PASS SHTC3 not ready (result=1, sleeps=0, output_ok=1)
FAIL SHTC3 wake NACK still sleeps (result=-5, sleeps=0, output_ok=1)
FAIL SHTC3 measurement NACK still sleeps (result=-6, sleeps=0, output_ok=1)
FAIL SHTC3 read NACK still sleeps (result=-7, sleeps=0, output_ok=1)
PASS SHTC3 sleep failure preserves output (result=-8, sleeps=1, output_ok=1)
FAIL SHTC3 wake error wins over sleep error (result=-5, sleeps=0, output_ok=1)
FAIL SHTC3 measurement error wins over sleep error (result=-6, sleeps=0, output_ok=1)
FAIL SHTC3 read error wins over sleep error (result=-7, sleeps=0, output_ok=1)
PASS SHTC3 temperature CRC rejection (result=1, sleeps=1, output_ok=1)
PASS SHTC3 humidity CRC rejection (result=1, sleeps=1, output_ok=1)
FAIL SHTC3 CRC error wins over sleep error (result=-8, sleeps=1, output_ok=1)
PASS ADC-photo soil success (result=0, stops=1, disables=1, output_ok=1)
FAIL ADC-photo soil enable failure cleans up (result=-11, stops=0, disables=0, output_ok=1)
FAIL ADC-photo soil PWM start failure cleans up (result=-12, stops=0, disables=0, output_ok=1)
PASS ADC-photo soil ADC init failure (result=-21, stops=1, disables=1, output_ok=1)
FAIL ADC-photo soil ADC failure preserves output (result=-22, stops=1, disables=1, output_ok=0)
FAIL ADC-photo soil conversion failure preserves output (result=-23, stops=1, disables=1, output_ok=0)
FAIL ADC-photo soil final conversion failure preserves output (result=-23, stops=1, disables=1, output_ok=0)
FAIL ADC-photo soil PWM stop failure still disables discharge (result=-13, stops=1, disables=0, output_ok=0)
FAIL ADC-photo soil discharge disable failure preserves output (result=-14, stops=1, disables=1, output_ok=0)
FAIL ADC-photo soil ADC error wins over cleanup errors (result=-13, stops=1, disables=0, output_ok=0)
FAIL ADC-photo soil start error wins over cleanup errors (result=-12, stops=0, disables=0, output_ok=1)
FAIL ADC-photo soil first cleanup error wins (result=-13, stops=1, disables=0, output_ok=0)
PASS ADC-photo battery success (result=0, stops=0, disables=0, output_ok=1)
FAIL ADC-photo battery ADC failure preserves output (result=-22, stops=0, disables=0, output_ok=0)
FAIL ADC-photo battery conversion failure preserves output (result=-23, stops=0, disables=0, output_ok=0)
FAIL ADC-photo battery final conversion failure preserves output (result=-23, stops=0, disables=0, output_ok=0)
PASS ADC-photo light success (result=0, stops=0, disables=1, output_ok=1)
FAIL ADC-photo light enable failure powers off (result=-11, stops=0, disables=0, output_ok=1)
PASS ADC-photo light read failure powers off (result=-22, stops=0, disables=1, output_ok=1)
PASS ADC-photo light conversion failure powers off (result=-23, stops=0, disables=1, output_ok=1)
PASS ADC-photo light disable failure preserves output (result=-14, stops=0, disables=1, output_ok=1)
FAIL ADC-photo light read error wins over disable error (result=-14, stops=0, disables=1, output_ok=1)
PASS ADC-photo light setup failure powers off (result=-24, stops=0, disables=1, output_ok=1)
PASS ADC-photo light saturation retries lower gain (result=0, stops=0, disables=1, output_ok=1)
PASS ADC-photo light lower gain failure powers off (result=-24, stops=0, disables=1, output_ok=1)
PASS ADC-photo light lower gain read failure preserves output (result=-22, stops=0, disables=1, output_ok=1)
PASS ADC-LDR soil success (result=0, stops=1, disables=1, output_ok=1)
FAIL ADC-LDR soil enable failure cleans up (result=-11, stops=0, disables=0, output_ok=1)
FAIL ADC-LDR soil PWM start failure cleans up (result=-12, stops=0, disables=0, output_ok=1)
PASS ADC-LDR soil ADC init failure (result=-21, stops=1, disables=1, output_ok=1)
FAIL ADC-LDR soil ADC failure preserves output (result=-22, stops=1, disables=1, output_ok=0)
FAIL ADC-LDR soil conversion failure preserves output (result=-23, stops=1, disables=1, output_ok=0)
FAIL ADC-LDR soil final conversion failure preserves output (result=-23, stops=1, disables=1, output_ok=0)
FAIL ADC-LDR soil PWM stop failure still disables discharge (result=-13, stops=1, disables=0, output_ok=0)
FAIL ADC-LDR soil discharge disable failure preserves output (result=-14, stops=1, disables=1, output_ok=0)
FAIL ADC-LDR soil ADC error wins over cleanup errors (result=-13, stops=1, disables=0, output_ok=0)
FAIL ADC-LDR soil start error wins over cleanup errors (result=-12, stops=0, disables=0, output_ok=1)
FAIL ADC-LDR soil first cleanup error wins (result=-13, stops=1, disables=0, output_ok=0)
PASS ADC-LDR battery success (result=0, stops=0, disables=0, output_ok=1)
FAIL ADC-LDR battery ADC failure preserves output (result=-22, stops=0, disables=0, output_ok=0)
FAIL ADC-LDR battery conversion failure preserves output (result=-23, stops=0, disables=0, output_ok=0)
FAIL ADC-LDR battery final conversion failure preserves output (result=-23, stops=0, disables=0, output_ok=0)
PASS ADC-LDR light success (result=0, stops=0, disables=1, output_ok=1)
FAIL ADC-LDR light enable failure powers off (result=-11, stops=0, disables=0, output_ok=1)
FAIL ADC-LDR light read failure powers off (result=-22, stops=0, disables=0, output_ok=1)
FAIL ADC-LDR light conversion failure powers off (result=-23, stops=0, disables=0, output_ok=1)
FAIL ADC-LDR light disable failure preserves output (result=-14, stops=0, disables=1, output_ok=0)
FAIL ADC-LDR light read error wins over disable error (result=-22, stops=0, disables=0, output_ok=1)
```

## Green: after production edits

Command: `code/tests/sensors/run.sh`; exit status **0**:

```text
PASS SHTC3 success (result=0, sleeps=1, output_ok=1)
PASS SHTC3 not ready (result=1, sleeps=0, output_ok=1)
PASS SHTC3 wake NACK still sleeps (result=-5, sleeps=1, output_ok=1)
PASS SHTC3 measurement NACK still sleeps (result=-6, sleeps=1, output_ok=1)
PASS SHTC3 read NACK still sleeps (result=-7, sleeps=1, output_ok=1)
PASS SHTC3 sleep failure preserves output (result=-8, sleeps=1, output_ok=1)
PASS SHTC3 wake error wins over sleep error (result=-5, sleeps=1, output_ok=1)
PASS SHTC3 measurement error wins over sleep error (result=-6, sleeps=1, output_ok=1)
PASS SHTC3 read error wins over sleep error (result=-7, sleeps=1, output_ok=1)
PASS SHTC3 temperature CRC rejection (result=1, sleeps=1, output_ok=1)
PASS SHTC3 humidity CRC rejection (result=1, sleeps=1, output_ok=1)
PASS SHTC3 CRC error wins over sleep error (result=1, sleeps=1, output_ok=1)
PASS ADC-photo soil success (result=0, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil enable failure cleans up (result=-11, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil PWM start failure cleans up (result=-12, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil ADC init failure (result=-21, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil ADC failure preserves output (result=-22, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil conversion failure preserves output (result=-23, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil final conversion failure preserves output (result=-23, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil PWM stop failure still disables discharge (result=-13, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil discharge disable failure preserves output (result=-14, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil ADC error wins over cleanup errors (result=-22, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil start error wins over cleanup errors (result=-12, stops=1, disables=1, output_ok=1)
PASS ADC-photo soil first cleanup error wins (result=-13, stops=1, disables=1, output_ok=1)
PASS ADC-photo battery success (result=0, stops=0, disables=0, output_ok=1)
PASS ADC-photo battery ADC failure preserves output (result=-22, stops=0, disables=0, output_ok=1)
PASS ADC-photo battery conversion failure preserves output (result=-23, stops=0, disables=0, output_ok=1)
PASS ADC-photo battery final conversion failure preserves output (result=-23, stops=0, disables=0, output_ok=1)
PASS ADC-photo light success (result=0, stops=0, disables=1, output_ok=1)
PASS ADC-photo light enable failure powers off (result=-11, stops=0, disables=1, output_ok=1)
PASS ADC-photo light read failure powers off (result=-22, stops=0, disables=1, output_ok=1)
PASS ADC-photo light conversion failure powers off (result=-23, stops=0, disables=1, output_ok=1)
PASS ADC-photo light disable failure preserves output (result=-14, stops=0, disables=1, output_ok=1)
PASS ADC-photo light read error wins over disable error (result=-22, stops=0, disables=1, output_ok=1)
PASS ADC-photo light setup failure powers off (result=-24, stops=0, disables=1, output_ok=1)
PASS ADC-photo light saturation retries lower gain (result=0, stops=0, disables=1, output_ok=1)
PASS ADC-photo light lower gain failure powers off (result=-24, stops=0, disables=1, output_ok=1)
PASS ADC-photo light lower gain read failure preserves output (result=-22, stops=0, disables=1, output_ok=1)
PASS ADC-LDR soil success (result=0, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil enable failure cleans up (result=-11, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil PWM start failure cleans up (result=-12, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil ADC init failure (result=-21, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil ADC failure preserves output (result=-22, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil conversion failure preserves output (result=-23, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil final conversion failure preserves output (result=-23, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil PWM stop failure still disables discharge (result=-13, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil discharge disable failure preserves output (result=-14, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil ADC error wins over cleanup errors (result=-22, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil start error wins over cleanup errors (result=-12, stops=1, disables=1, output_ok=1)
PASS ADC-LDR soil first cleanup error wins (result=-13, stops=1, disables=1, output_ok=1)
PASS ADC-LDR battery success (result=0, stops=0, disables=0, output_ok=1)
PASS ADC-LDR battery ADC failure preserves output (result=-22, stops=0, disables=0, output_ok=1)
PASS ADC-LDR battery conversion failure preserves output (result=-23, stops=0, disables=0, output_ok=1)
PASS ADC-LDR battery final conversion failure preserves output (result=-23, stops=0, disables=0, output_ok=1)
PASS ADC-LDR light success (result=0, stops=0, disables=1, output_ok=1)
PASS ADC-LDR light enable failure powers off (result=-11, stops=0, disables=1, output_ok=1)
PASS ADC-LDR light read failure powers off (result=-22, stops=0, disables=1, output_ok=1)
PASS ADC-LDR light conversion failure powers off (result=-23, stops=0, disables=1, output_ok=1)
PASS ADC-LDR light disable failure preserves output (result=-14, stops=0, disables=1, output_ok=1)
PASS ADC-LDR light read error wins over disable error (result=-22, stops=0, disables=1, output_ok=1)
```

No hardware, remote service, firmware storage, calibration curve, or board pin was changed by this test. Actual sensor power draw and electrical failure recovery remain hardware validation work.

## Wider host checks

- `python3 code/tests/recovery/run_tests.py`: exit **0**, `7/7 recovery tests passed`.
- `node --test code/tests/integration/test_converter.mjs`: exit **1**, prerequisite assertion `Set ZIGBEE_TEST_NODE_MODULES to real Zigbee2MQTT node_modules`.
- `python3 -m unittest discover -s code/tests/integration -p 'test_*.py'`: exit **1**, import failure `ModuleNotFoundError: No module named 'websocket'`.

The integration checks require the project's dependency environment; these direct system-interpreter runs do not establish integration behavior. The parent task owns integration and target firmware build verification.
