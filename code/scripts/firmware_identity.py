#!/usr/bin/env python3
"""Generate a content-based Zigbee build identity without requiring Git."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def firmware_inputs(root):
    paths = set()
    for directory in (
        "code/samples/zigbee/src", "code/prstlib/src", "code/prstlib/include",
    ):
        paths.update(path for path in (root / directory).rglob("*")
                     if path.is_file() and path.suffix in (".c", ".h"))
    for directory in ("code/prstlib/boards", "code/prstlib/dts", "code/prstlib/zephyr"):
        paths.update(path for path in (root / directory).rglob("*") if path.is_file() and (
            path.suffix in (".dts", ".dtsi", ".overlay", ".conf", ".yaml", ".yml",
                            ".h", ".c", ".cmake") or
            path.name.startswith("Kconfig") or path.name.endswith("defconfig") or
            path.name == "CMakeLists.txt"
        ))
    for directory in ("code/samples/zigbee", "code/prstlib"):
        for pattern in ("Kconfig*", "CMakeLists.txt", "prj*.conf", "*.overlay"):
            paths.update(path for path in (root / directory).glob(pattern) if path.is_file())
    return {path.relative_to(root).as_posix(): sha256(path.read_bytes())
            for path in sorted(paths)}


def make_identity(root, config, date_code, metadata):
    datetime.strptime(date_code, "%Y%m%d")
    inputs = firmware_inputs(root)
    if not inputs:
        raise ValueError("no firmware inputs found")
    inputs["generated/.config"] = sha256(config.read_bytes())
    board = re.search(r'^CONFIG_BOARD_TARGET="([^"]+)"$', config.read_text(), re.MULTILINE)
    if not board:
        raise ValueError("generated .config has no CONFIG_BOARD_TARGET")
    fingerprint = sha256(json.dumps(
        {"inputs": inputs, "metadata": metadata}, sort_keys=True, separators=(",", ":")
    ).encode())
    return {
        "schema": 1,
        "firmware_id": f"bp-{fingerprint[:12]}",
        "source_sha256": fingerprint,
        "date_code": date_code,
        "board": board.group(1),
        "inputs": inputs,
        "metadata": metadata,
    }


def write_if_changed(path, content):
    if not path.exists() or path.read_text() != content:
        path.write_text(content)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--date", help="YYYYMMDD; otherwise UTC or SOURCE_DATE_EPOCH")
    parser.add_argument("--metadata", action="append", default=[], metavar="NAME=VALUE")
    args = parser.parse_args()
    try:
        epoch = os.environ.get("SOURCE_DATE_EPOCH")
        build_date = args.date or (
            datetime.fromtimestamp(int(epoch), timezone.utc) if epoch else datetime.now(timezone.utc)
        ).strftime("%Y%m%d")
        metadata = dict(value.split("=", 1) for value in args.metadata)
        identity = make_identity(args.source_root.resolve(), args.config, build_date, metadata)
        args.output_dir.mkdir(parents=True, exist_ok=True)
        header = (
            "#ifndef BPARASITE_BUILD_IDENTITY_H\n#define BPARASITE_BUILD_IDENTITY_H\n"
            f'#define PRST_FIRMWARE_BUILD_ID "{identity["firmware_id"]}"\n'
            f'#define PRST_FIRMWARE_BUILD_DATE "{identity["date_code"]}"\n'
            "#endif\n"
        )
        write_if_changed(args.output_dir / "bparasite_build_identity.h", header)
        write_if_changed(args.output_dir / "bparasite_build_identity.json",
                         json.dumps(identity, indent=2, sort_keys=True) + "\n")
        print(identity["firmware_id"])
    except (OSError, ValueError) as error:
        parser.exit(1, f"identity: {error}\n")


if __name__ == "__main__":
    main()
