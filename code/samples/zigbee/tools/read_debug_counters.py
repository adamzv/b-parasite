#!/usr/bin/env python3
"""Print the debug counters (boots, reset causes, errors) from a dump of the board's littlefs partition.

  pyocd cmd -t nrf52840 -O connect_mode=attach -c "savemem 0xfa000 0x6000 lfs.bin"
  read_debug_counters.py lfs.bin

Attach mode leaves the running firmware alone; a reset or a battery swap would itself add to the counters.
"""
import struct, sys

from littlefs import LittleFS

# Zephyr's FS_LITTLEFS_DECLARE_DEFAULT_CONFIG on the nRF52840's 4 KiB flash pages.
BLOCK_SIZE = 4096
READ_SIZE = PROG_SIZE = 16


def main(path):
    image = open(path, "rb").read()
    fs = LittleFS(block_size=BLOCK_SIZE, block_count=len(image) // BLOCK_SIZE, read_size=READ_SIZE,
                  prog_size=PROG_SIZE, cache_size=64, lookahead_size=32, mount=False)
    fs.context.buffer = bytearray(image)
    fs.mount()
    for name in sorted(fs.listdir("/debug_counters")):
        with fs.open(f"/debug_counters/{name}", "rb") as f:
            print(f"{name:32} {struct.unpack('<I', f.read(4))[0]}")


if __name__ == "__main__":
    main(sys.argv[1])
