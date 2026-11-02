# Building and testing

## Host tests

Install Node 22 or newer, Python 3, Bash and a C11 compiler, then run:

```sh
npm ci
npm test
```

The Node lockfile pins the converter dependencies. Python tests use the standard library. Nothing in the test suite connects to Home Assistant, MQTT or a physical device. See [code/tests](../code/tests/README.md) for the boundaries it checks.

## Docker build

From the repository root, with Docker running:

```sh
bash code/scripts/build-with-docker.sh zigbee nrf52840 2.0.0
```

The wrapper uses `ghcr.io/zephyrproject-rtos/ci:v0.28.8` (the `-arm64` image on Apple Silicon), which includes Zephyr SDK 0.17.4. The first build downloads the west projects into `code/`; allow several GB for the SDK workspace. Later builds reuse those projects. Each container installs the SDK's Python requirements, since the container is removed after the build.

Output is in `code/samples/zigbee/build_nrf52840_2.0.0/`. In a sysbuild, the application files are under `zigbee/zephyr/` and `merged.hex` is at the top of the build directory.

The pinned [west manifest](../code/prstlib/west.yml) selects Zigbee add-on commit `15854504b47cb8c84f4f07ddc716c9d77e4b9238`, which imports nRF Connect SDK v3.3.0. Run `west update` from `code/` when changing that manifest; the build script skips workspace setup once `.west/` exists.

## Local toolchain

With west, CMake, Ninja, the SDK's Python requirements and a compatible Zephyr SDK installed:

```sh
bash code/scripts/build.sh zigbee nrf52840 2.0.0
```

Use a Python virtual environment for this. The first run initializes west; each run checks and installs the SDK requirements in the active Python environment. The [Nordic installation guide](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/installation.html) covers toolchain setup.

`prj.conf` contains the release defaults: ten-minute readings, 30-second parent polling, logging disabled and channels 11–26. Use `CMAKE_EXTRA` for additional CMake options, for example:

```sh
CMAKE_EXTRA=-DEXTRA_CONF_FILE=prj_debug.conf \
  bash code/scripts/build-with-docker.sh zigbee nrf52840 2.0.0
```

A debug build has a different power profile. Don't use it to judge battery life.

## Package an image

```sh
python3 code/scripts/package_zigbee.py \
  --build-dir code/samples/zigbee/build_nrf52840_2.0.0 \
  --output-dir dist/zigbee-nrf52840-rev2

cd dist/zigbee-nrf52840-rev2
shasum -a 256 -c SHA256SUMS
```

The output directory must be empty. The packager verifies the board target and generated configuration, checks the build ID in BIN and ELF, checks Intel HEX records, rejects protected addresses and compares HEX with BIN. It includes the matching converter, identity and flashing instructions. Checksums cover every package file except the checksum list itself.

Keep the ELF and identity for any image you install. The Basic cluster reports `swBuildId` as `bp-` plus a content fingerprint. It depends on firmware, board and generated configuration inputs plus toolchain metadata. The UTC date is recorded separately; set `SOURCE_DATE_EPOCH` to fix it when rebuilding. The ID is not a promise of byte-for-byte reproducibility.

## CI and releases

Pull requests and pushes run host regressions, a format check and the rev 2.0.0/nRF52840 Zigbee build. CI packages and uploads the checked image as a workflow artifact. A `v*` tag also publishes a release with the firmware bundle, source snapshot and changelog. Tags containing a hyphen, such as `v0.1.0-rc.1`, are marked as prereleases.

Only the nRF52840 rev 2.0.0 Zigbee image is released here. Upstream's BLE firmware and other boards remain in the source tree, but aren't covered by this fork's release checks.
