# Zigbee firmware reference

This fork builds on upstream's Zigbee sample, originally adapted from Nordic's `zigbee_template`. For installation, start with the [flashing guide](../../../docs/flashing.md). This page covers the firmware's controls and attributes.

## Clusters
These [clusters](https://en.wikipedia.org/wiki/Zigbee#Cluster_library) are defined in the sample:

|Cluster ID|Name|
|--------|---|
|0x0001|Power Configuration|
|0x0400|Illuminance Measurement|
|0x0402|Temperature Measurement|
|0x0405|Relative Humidity Measurement|
|0x0408|Soil Moisture Measurement|
|0xFC00|Soil calibration (private): `soil_raw`, `soil_dry`, `soil_wet`|

## Pairing Mode
After boot the board looks for a Zigbee coordinator. While it is actively joining (the
`CONFIG_ZIGBEE_DEV_REJOIN_TIMEOUT_MS` window) the LED gives a short blink every 2 seconds; outside that window it stays
off to save the coin cell, and the restart watchdog retries the join with a doubling pause. Once joined, the LED is off.

### Factory Reset
A factory reset makes b-parasite forget its network and start pairing again. The method is chosen with
`CONFIG_PRST_ZB_FACTORY_RESET_METHOD`.

#### SW1 (default, hardware 2.0.0+)
Hold SW1 while resetting or inserting the battery and keep holding it for 5 seconds. Five LED flashes confirm the reset.

#### Double reset
Two resets within 5 seconds (reset pin or battery out/in). Wait more than 1 second between them.

#### Reset pin
A reset-pin reset factory-resets; a power-up (battery swap) keeps the network. An accidental pin reset unpairs the
board, so this method is easy to trigger by mistake.

## SW1

Presses are counted until SW1 stays released for `CONFIG_PRST_ZB_BUTTON_MULTI_PRESS_GAP_MS`; each press blinks once.

* **1×**: measure once and receive coordinator commands for 60 seconds. Normal sampling then resumes.
* **2×** in dry soil / **3×** in freshly watered soil: settle for `CONFIG_PRST_ZB_CALIBRATION_SETTLE_SEC`, then store the median of `CONFIG_PRST_ZB_CALIBRATION_CAPTURE_SAMPLES` readings as dry/wet. Three flashes confirm a saved point; ten flashes mean the pair was rejected.
* **4× or more**: explicitly enter the 15-minute interactive service mode, with 10-second sampling and one-second parent polling. Use this before onboarding or editing calibration from Zigbee2MQTT.

A new sequence replaces a pending capture. A capture that reaches its deadline is cancelled without changing calibration, including when the sensor keeps failing. Writes that put dry at or above wet receive INVALID_VALUE. Interview, button and service receive windows can overlap; the longest deadline is preserved.

## Boot loop guard
After `CONFIG_PRST_ZB_BOOT_LOOP_LIMIT` crash resets without a stable 10-minute run in between, the board gives one long
LED pulse and waits `CONFIG_PRST_ZB_BOOT_LOOP_BACKOFF_SEC` with the radio off before starting Zigbee; pressing SW1 ends
the wait. Debug counters (`boot`, `reset_cause_*`, `boot_loop_backoff`, ...) live in LittleFS under `/lfs/debug_counters`. Crash streaks use `/lfs/crash_boots`; the old all-boot `/lfs/boots_since_join` file is ignored. A normal power cycle or reset pin starts a fresh crash streak.

## Identify
An Identify command from the coordinator flashes the LED 15 times.

## Configs
Available options in `Kconfig`; `prj.conf` sets the values used for the deployed boards. Notable options:
* `CONFIG_PRST_ZB_SLEEP_DURATION_SEC`: time between sensor readings
* `CONFIG_PRST_ZB_PARENT_POLL_INTERVAL_SEC`: time between parent polls
* `CONFIG_PRST_ZB_RESTART_WATCHDOG_TIMEOUT_SEC` / `..._MAX_TIMEOUT_SEC`: pause before retrying a failed join, doubling
  up to the maximum

## Home Assistant Integration
The supported setup for this fork is Zigbee2MQTT with Home Assistant discovery.

### Zigbee Home Automation (ZHA)
The standard measurement clusters may work with ZHA. This fork's calibration and diagnostics have not been verified there.

### Zigbee2MQTT & Home Assistant
Use [bparasite.mjs](bparasite.mjs) from the same release as the firmware. It adds the 0xFC00 calibration and diagnostics cluster. [b-parasite.js](b-parasite.js) is the older upstream converter without those features. Installation steps are in the [flashing guide](../../../docs/flashing.md#install-the-converter).

## Diagnostics and invalid readings

The existing private cluster also reports `soil_valid`, `sensor_errors`, `battery_mv`, `boot_count`, `reset_cause` and `uptime_sec`. Error mask bits are battery=1, soil=2, light=4 and SHTC3=8. `battery_mv` preserves the ADC's millivolt result; the standard voltage attribute has 100 mV resolution. Battery percentage remains a voltage-based estimate.

The Basic cluster exposes a content-derived `swBuildId` (`bp-` plus a fingerprint) and build date. Keep the matching ELF with each installed image for future SWD diagnosis.

A failed measurement reports an unknown value instead of retaining a valid-looking old value. Install the matching `bparasite.mjs` converter before updating. An application-only update preserves calibration and pairing when the installed firmware uses the same storage layout; never mass-erase for this upgrade.

## Verification

`bash code/tests/run.sh` runs host regressions; see `code/tests/README.md` for integration dependencies. The production build and packaged image checks complement these mocks. Real radio recovery and sleep current must be checked after flashing.

## Power measurements

The `media/power-profile/` images are measurements from the upstream sample. This fork changes sampling, polling and recovery behavior, so those figures do not establish its battery life. The public build still needs a sleep-current measurement on hardware.
