# Changelog

## 0.1.0

First public release of this Zigbee fork, based on rbaron/b-parasite.

- Dry/wet calibration stored on the board, captured with SW1 or set through Zigbee2MQTT.
- One press takes a reading and opens a short receive window; four presses open the longer service mode.
- Retry backoff for network recovery, crash-loop protection and deferred diagnostic counter writes.
- Sensor cleanup after failed measurements and unknown values for failed readings.
- Battery millivolts, sensor error mask, soil validity, boot/reset diagnostics, uptime and firmware build identity.
- Zigbee2MQTT external converter for calibration, diagnostics and invalid-reading handling.
- Public defaults scan all Zigbee channels; the earlier home deployment scanned channel 11 only.
- Host regressions, pinned converter dependencies and checked release packages.

Release target: b-parasite rev 2.0.0 / nRF52840. Earlier firmware was flashed and observed through Zigbee2MQTT and Home Assistant on two boards. The public build still needs sleep-current and coordinator-outage checks on hardware. ZHA calibration and diagnostics are unverified.
