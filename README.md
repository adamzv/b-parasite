# b-parasite — Zigbee firmware

[![Firmware checks](https://github.com/adamzv/b-parasite/actions/workflows/b-parasite.yml/badge.svg)](https://github.com/adamzv/b-parasite/actions/workflows/b-parasite.yml)

This is my Zigbee-focused fork of [rbaron/b-parasite](https://github.com/rbaron/b-parasite), an open source soil moisture, temperature, humidity and light sensor. The hardware is Raphael Baron's work. This fork adds the firmware changes I use with Zigbee2MQTT and Home Assistant.

Most of the work is about living with the sensors: calibrating them in their pots, getting useful information when something goes wrong, and recovering after the coordinator disappears.

<img src="img/resized/b-parasite-2.0.0.jpg" width="512" alt="Front and back of a b-parasite board">

## What changed

- **Calibration on the board.** Set dry and wet points with SW1 or Zigbee2MQTT. The board saves them across restarts.
- **More useful diagnostics.** Exact battery voltage in mV, sensor errors, soil validity, boot count, reset cause, uptime and a build ID.
- **Recovery after network trouble.** Failed joins keep retrying with a capped backoff. Repeated error signals don't keep pushing the next attempt away.
- **Sensor failure handling.** Measurement circuits are shut down after errors, and failed readings reach Zigbee2MQTT as unknown instead of looking like fresh data.
- **Less time awake during normal use.** Logging is off, readings are ten minutes apart, and one button press opens a short receive window. A longer service mode is available when needed.

## Flash it

The release target is **b-parasite rev 2.0.0 with an nRF52840**. Check the marking on your module before choosing an image.

1. Download the firmware bundle from [Releases](https://github.com/adamzv/b-parasite/releases).
2. Install its `bparasite.mjs` converter in Zigbee2MQTT.
3. Back up the board, verify the bundle's checksums, and flash `app.hex` over SWD.
4. Pair it, or reconfigure the existing device if you're updating this firmware.

The [flashing guide](docs/flashing.md) covers wiring, first installation and updates. You need an SWD probe; these releases don't provide Zigbee OTA updates.

## The button

| SW1 | What it does |
| --- | --- |
| One press | Take a reading and listen for commands for 60 seconds |
| Two presses | Capture the dry calibration point |
| Three presses | Capture the wet calibration point |
| Four or more presses | Open a 15-minute service window for setup or calibration writes |
| Hold while powering up, then keep holding for 5 seconds | Forget the Zigbee network and pair again |

For calibration, leave the probe in its intended position in the pot. Capture the dry point when the soil is as dry as you want it to get, and the wet point after watering. The firmware waits for settling and takes a median; an invalid dry/wet pair is rejected. See the [Zigbee reference](code/samples/zigbee/README.md) for the details and LED signals.

## Home Assistant

Use **Zigbee2MQTT with the included external converter** for calibration and diagnostics. Enable Home Assistant discovery in Zigbee2MQTT and the entities appear there.

ZHA's standard measurements may work, but this fork's calibration and diagnostic features haven't been verified with ZHA. The plant-assistant used in my own setup is separate; the sensor doesn't need it.

## Building and testing

```sh
npm ci
npm test

bash code/scripts/build-with-docker.sh zigbee nrf52840 2.0.0
```

The build uses a pinned Zigbee add-on revision and nRF Connect SDK. The [build guide](docs/building.md) covers local toolchains, configuration and release packaging.

Earlier firmware from this work was flashed on two rev 2.0.0/nRF52840 boards, with calibration preserved and reports checked through Zigbee2MQTT and Home Assistant. The public build enables all Zigbee channels. Its current draw and recovery after a real coordinator outage still need hardware checks; host tests can't establish either.

## Upstream and hardware

The PCB designs, cases, BLE firmware and other samples are kept here from upstream. This fork's releases focus on the Zigbee target above; the other board and sample combinations aren't part of its release checks.

For ordering, assembly, hardware revisions and the original project documentation, use the [upstream wiki](https://github.com/rbaron/b-parasite/wiki) and [original README](README.upstream.md).

If you find a problem, include the board revision, module, Zigbee2MQTT version and Basic cluster `swBuildId` in an [issue](https://github.com/adamzv/b-parasite/issues). A description of what happened before the failure helps more than a screenshot of an unavailable entity.

Code is [MIT licensed](LICENSE). Hardware and design files retain upstream's [CC BY-SA 4.0 license](https://creativecommons.org/licenses/by-sa/4.0/).
