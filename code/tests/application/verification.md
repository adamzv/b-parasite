# Application regression evidence

Before application edits, the actual main.c compiled with controlled Zephyr/ZBOSS boundary doubles failed all five initial cases:

```
FAIL single_press_is_one_measurement: single press entered long calibration
FAIL identify_never_blocks_buttons: identify slept on system workqueue
FAIL failed_capture_expires_without_saving: capture remained pending after expiry
FAIL invalid_soil_clears_the_reported_value: old soil remains valid
FAIL trust_center_success_stops_retries: TC rejoin success left retries active
application: 0/5 passed
```

Run `bash code/tests/application/run.sh`. Later cases additionally cover successful calibration, cancellation, delayed deadline dispatch, service mode expiry, failed air/light measurements, exact battery units and finite LED pulses. These tests execute application callbacks against mocked hardware/scheduling boundaries; they do not replace the target firmware build or a hardware radio test.

Independent review found overlapping poll windows. Three added regression cases failed before the window fix (`application: 12/15 passed`): a quick measurement shortened interview, rejoin shortened service, and capture completion shortened interview. The fix preserves the longest outstanding receive deadline.

A sensor error transition was verified to call synchronous persistence on the Zigbee thread (`application: 15/16 passed`). The application now queues those increments; its regression asserts the same fault is queued once per episode.
