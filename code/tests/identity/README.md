# Firmware identity and package checks

```sh
python3 -m unittest discover -s code/tests/identity -v
```

The suite runs the real Python scripts on temporary source/build fixtures. It
also compiles the real `prst_zb_attrs.c` against a minimal ZCL boundary and checks
the emitted length-prefixed Basic attributes. Package fixtures are deliberately
small; they test validation rather than pretending to be executable firmware.

## Build contract

After Zephyr has generated `.config`, the sample CMake configuration invokes:

```sh
python3 code/scripts/firmware_identity.py \
  --source-root /path/to/b-parasite-main \
  --config /path/to/build/zigbee/zephyr/.config \
  --output-dir /path/to/build/zigbee/generated \
  --metadata compiler=GNU-12.2.0
```

The generated header defines `PRST_FIRMWARE_BUILD_ID` (`bp-` plus twelve hex
digits) and `PRST_FIRMWARE_BUILD_DATE` (`YYYYMMDD`). The companion JSON records the
complete input digest, individual content hashes, board and toolchain metadata.
The build ID hashes normalized input paths and contents plus metadata. The date
is separate build metadata; supply `--date YYYYMMDD` or `SOURCE_DATE_EPOCH` when
reproducing it. Otherwise generation uses the current UTC date. Source mtimes,
absolute checkout paths, Git and unrelated documentation are excluded.

CMake monitors source, board and configuration inputs with
`CMAKE_CONFIGURE_DEPENDS` and glob addition/removal checks. A source edit causes
reconfiguration before compilation. Identical generated files are not rewritten.

## Package contract

```sh
python3 code/scripts/package_zigbee.py \
  --build-dir /path/to/build \
  --output-dir /path/to/empty-release-directory
```

The package includes `app.hex`, `app.bin`, `app.elf`, `manifest.json`,
`SHA256SUMS` and `FLASHING.md`. The script checks:

- Known board `bparasite@2.0.0/nrf52840` and matching generated `.config` digest.
- Build identity present in both BIN and ELF.
- Intel HEX checksums and records, with no programmed address at or above
  `0xf1000`, including UICR.
- Complete HEX/BIN agreement, filling unprogrammed gaps with `0xff`.

The script never connects to hardware. The production build and subsequent radio
operation must be verified separately. Only HEX is used in the documented
OpenOCD update, with sector erasure and no mass erase or factory reset.
