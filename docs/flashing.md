# Flashing the Zigbee firmware

The release image is for **b-parasite rev 2.0.0 / nRF52840**. It is not an image for the nRF52833, SuperMini boards or a different PCB revision.

## What you need

- A release bundle from [GitHub Releases](https://github.com/adamzv/b-parasite/releases).
- An SWD probe supported by OpenOCD. A CMSIS-DAP probe works with the commands below.
- OpenOCD and a 3.3 V connection to the board. Follow the [upstream wiring guide](https://github.com/rbaron/b-parasite/wiki/Flashing). Don't connect 5 V to the board, and use one power source while programming.
- Zigbee2MQTT with external JavaScript enabled.

The release contains `app.hex`, `app.bin`, `app.elf`, `build-identity.json`, `bparasite.mjs`, `manifest.json`, `FLASHING.md` and `SHA256SUMS`. Flash the HEX; the BIN is kept for inspection and the ELF for debugging.

## Install the converter

Install the **converter from the same release** as the firmware. In Zigbee2MQTT, use Settings → Dev console → External converters to add `bparasite.mjs`, or put it in the `external_converters/` directory next to `configuration.yaml`.

New Zigbee2MQTT installations from 2.11 onward disable external JavaScript by default. Enable `enable_external_js` as described in the [Zigbee2MQTT documentation](https://www.zigbee2mqtt.io/advanced/more/external_converters.html). Remove an older external b-parasite converter so there is only one external definition for this device.

Home Assistant gets the standard measurements, calibration controls and diagnostics through Zigbee2MQTT discovery. This setup doesn't use a separate plant-assistant service.

## Back up the board

Connect the SWD probe and record the existing dry/wet calibration values if this is an update. For a CMSIS-DAP probe:

```sh
openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \
  -c "init; reset halt; dump_image flash-before.bin 0x0 0x100000; dump_image uicr-before.bin 0x10001000 0x1000; reset run; exit"
```

Keep both files privately. A full flash backup can contain Zigbee network credentials. If the chip is read protected, stop here: a recover/unlock operation erases it.

## Verify and program

Extract the release bundle and run these commands inside it:

```sh
# macOS; on Linux use: sha256sum -c SHA256SUMS
shasum -a 256 -c SHA256SUMS

openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \
  -c "program app.hex verify reset exit"
```

The package validator rejects an image that writes at or above `0xf1000`, including UICR. This leaves the fork's Zigbee storage, product configuration and LittleFS sectors alone. Use the command as shown; mass erase, recover and factory reset are not update steps.

That preserves pairing and calibration **when the installed firmware uses the same storage layout**. If you're migrating from another build, compare its partition map before relying on that. Leaving sectors intact cannot move data from an older layout.

## First installation

After flashing a blank board or replacing BLE firmware:

1. Allow joining in Zigbee2MQTT.
2. Power up the board. It looks for a coordinator and briefly blinks while joining.
3. If it retains an old Zigbee network, hold SW1 during power-up and keep holding it for five seconds. Five flashes confirm a network reset.
4. Press SW1 four times to keep it awake for the interview and configuration.
5. Set the dry and wet calibration points for the pot. The [button table](../README.md#the-button) covers the shortcuts.

The public configuration scans Zigbee channels 11–26. A failed join is retried with increasing pauses, so use the service window while setting up rather than waiting for the normal sampling cycle.

## Updating an existing board

Install the converter first, back up, verify checksums, then program the HEX without resetting the Zigbee network. After it boots:

1. Press SW1 four times.
2. Reconfigure the device in Zigbee2MQTT. Re-interview it first if the cluster information is stale.
3. Check that the device identity and `soil_dry` / `soil_wet` values are unchanged.
4. Read Basic cluster `swBuildId` and compare it with `build-identity.json`.
5. Check fresh sensor reports, `soil_valid` and `sensor_errors` in Zigbee2MQTT and Home Assistant.

Normal readings are ten minutes apart. One SW1 press requests a reading and gives commands a 60-second receive window; four presses give you 15 minutes for setup.

## Checks that need real hardware

After installation, check button behavior, battery sleep current and rejoining after a coordinator outage. A passing build and host test suite don't prove those behaviors. Earlier firmware was checked on two rev 2.0.0/nRF52840 boards, but that doesn't cover every coordinator or this public build's full-channel join configuration.

Battery percentage is an estimate from voltage. `battery_mv` is the measured value; neither is a battery capacity measurement.
