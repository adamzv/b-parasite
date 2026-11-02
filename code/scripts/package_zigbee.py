#!/usr/bin/env python3
"""Validate and package the known nRF52840 rev2 application without flashing it."""

import argparse
import json
from pathlib import Path
import re
import shutil

from firmware_identity import sha256


PROTECTED_START = 0xF1000
KNOWN_BOARD = "bparasite@2.0.0/nrf52840"


def read_hex(path):
    image = {}
    base = 0
    ended = False
    for line in path.read_text().splitlines():
        if not line.strip():
            continue
        if ended or not line.startswith(":"):
            raise ValueError("invalid Intel HEX record sequence")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as error:
            raise ValueError("invalid Intel HEX encoding") from error
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) & 255:
            raise ValueError("invalid Intel HEX length or checksum")
        address = int.from_bytes(record[1:3], "big")
        kind = record[3]
        data = record[4:-1]
        if kind == 0:
            for offset, value in enumerate(data):
                absolute = base + address + offset
                if absolute >= PROTECTED_START:
                    raise ValueError(f"HEX programs protected address 0x{absolute:x}")
                if absolute in image and image[absolute] != value:
                    raise ValueError("conflicting Intel HEX records")
                image[absolute] = value
        elif kind == 1 and not data and address == 0:
            ended = True
        elif kind in (2, 4) and len(data) == 2 and address == 0:
            base = int.from_bytes(data, "big") << (4 if kind == 2 else 16)
        elif kind in (3, 5) and len(data) == 4 and address == 0:
            continue
        else:
            raise ValueError("unsupported Intel HEX record")
    if not ended or not image:
        raise ValueError("Intel HEX is empty or has no end record")
    ranges = []
    for address in sorted(image):
        if ranges and ranges[-1][1] == address:
            ranges[-1][1] += 1
        else:
            ranges.append([address, address + 1])
    return image, ranges


def unique_file(build, name):
    matches = list(build.rglob(name))
    if len(matches) != 1:
        raise ValueError(f"expected one {name}, found {len(matches)}")
    return matches[0]


def package(build, output):
    identity_path = unique_file(build, "bparasite_build_identity.json")
    identity = json.loads(identity_path.read_text())
    if identity["board"] != KNOWN_BOARD:
        raise ValueError(f"unsupported board: {identity['board']}")
    binary = unique_file(build, "zephyr.bin")
    elf = binary.with_name("zephyr.elf")
    config = binary.with_name(".config")
    if sha256(config.read_bytes()) != identity["inputs"]["generated/.config"]:
        raise ValueError("generated .config differs from build identity")
    firmware_id = identity["firmware_id"]
    if not re.fullmatch(r"bp-[0-9a-f]{12}", firmware_id):
        raise ValueError("invalid firmware identity")
    for artifact in (binary, elf):
        if firmware_id.encode() not in artifact.read_bytes():
            raise ValueError(f"firmware identity missing from {artifact.name}")
    merged_hex = build / "merged.hex"
    if not merged_hex.is_file():
        merged_hex = binary.with_name("zephyr.hex")
    image, ranges = read_hex(merged_hex)
    payload = binary.read_bytes()
    if min(image) != 0 or len(payload) != max(image) + 1 or any(
        value != image.get(address, 0xFF) for address, value in enumerate(payload)
    ):
        raise ValueError("HEX and BIN disagree")
    if output.exists() and any(output.iterdir()):
        raise ValueError("output directory must be empty")
    output.mkdir(parents=True, exist_ok=True)
    converter = Path(__file__).resolve().parents[1] / "samples/zigbee/bparasite.mjs"
    artifacts = {"app.hex": merged_hex, "app.bin": binary, "app.elf": elf,
                 "bparasite.mjs": converter, "build-identity.json": identity_path}
    for name, source in artifacts.items():
        shutil.copyfile(source, output / name)
    (output / "FLASHING.md").write_text(f"""# b-parasite application update

Target: **{KNOWN_BOARD}** only. Build ID: **{firmware_id}**.
Date code: {identity['date_code']}. No device has been flashed by this script.

1. Preserve a known-good backup of the device before programming.
2. From this directory, verify artifacts: `shasum -a 256 -c SHA256SUMS`.
3. Connect the SWD probe and use its OpenOCD interface configuration. For a
   CMSIS-DAP probe:

```sh
openocd -f interface/cmsis-dap.cfg -f target/nrf52.cfg \\
  -c "program app.hex verify reset exit"
```

Use the checked HEX, which specifies addresses. Do not run mass erase, recover,
`nrf5 mass_erase`, or a factory reset. The update programs application sectors
only. Its highest allowed address is 0xf0fff; Zigbee NVRAM starts at 0xf1000,
product configuration at 0xf9000 and LittleFS at 0xfa000. UICR is excluded.
Pairing, device identity and stored soil calibration remain in those untouched
regions. A BIN has no address information; do not substitute it in this command.

After reset, confirm new sensor reports, calibration values and network recovery.
Read Zigbee Basic cluster `swBuildId` ({firmware_id}) and `dateCode`
({identity['date_code']}) to identify the firmware. Hardware operation and
recovery are not established by package validation.

Install the included `bparasite.mjs` as a Zigbee2MQTT external converter before
updating. After updating, press SW1 four times and reconfigure the device in
Zigbee2MQTT during its 15-minute service window. Keep the original calibration
values and check that they are unchanged afterward.

These storage addresses apply to this fork's partition layout. When migrating
from another firmware, compare its layout first; preserving these sectors does
not migrate data stored at different addresses. For a first flash from BLE or a
blank board, follow the full guide:
https://github.com/adamzv/b-parasite/blob/main/docs/flashing.md
""")
    hashes = {path.name: sha256(path.read_bytes()) for path in sorted(output.iterdir())}
    manifest = {"identity": identity, "sha256": hashes, "hex_ranges": ranges,
                "protected_start": PROTECTED_START}
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    hashes = {**hashes, "manifest.json": sha256((output / "manifest.json").read_bytes())}
    (output / "SHA256SUMS").write_text("".join(f"{digest}  {name}\n" for name, digest in sorted(hashes.items())))
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        manifest = package(args.build_dir.resolve(), args.output_dir.resolve())
        print(f"Packaged {manifest['identity']['firmware_id']} into {args.output_dir}")
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"package: {error}\n")


if __name__ == "__main__":
    main()
