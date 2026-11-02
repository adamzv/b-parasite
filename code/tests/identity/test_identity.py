import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPTS = Path(__file__).resolve().parents[2] / "scripts"
BOARD = 'CONFIG_BOARD_TARGET="bparasite@2.0.0/nrf52840"\n'


class IdentityFixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "source"
        self.build = Path(self.temp.name) / "build"
        self.output = self.build / "zigbee" / "generated"
        self.config = self.build / "zigbee" / "zephyr" / ".config"
        self.write(self.root / "code/samples/zigbee/src/main.c", "int sample = 1;\n")
        self.write(self.root / "code/prstlib/include/prst/adc.h", "int adc(void);\n")
        self.write(self.root / "code/prstlib/boards/bparasite/board.overlay", "status = okay;\n")
        self.write(self.root / "code/samples/zigbee/CMakeLists.txt", "project(test)\n")
        self.write(self.root / "code/samples/zigbee/Kconfig", "config EXAMPLE\n")
        self.write(self.root / "code/samples/zigbee/prj.conf", "CONFIG_EXAMPLE=y\n")
        self.write(self.config, BOARD + "CONFIG_EXAMPLE=y\n")

    @staticmethod
    def write(path, value):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value)

    def identify(self, *extra):
        result = subprocess.run(
            [sys.executable, str(SCRIPTS / "firmware_identity.py"),
             "--source-root", str(self.root), "--config", str(self.config),
             "--output-dir", str(self.output), "--date", "20261028",
             "--metadata", "compiler=test-1.0", *extra],
            text=True, capture_output=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads((self.output / "bparasite_build_identity.json").read_text())


class IdentityTests(IdentityFixture):
    def test_basic_attributes_expose_generated_identity(self):
        self.identify()
        here = Path(__file__).resolve().parent
        source = SCRIPTS.parent / "samples/zigbee/src/prst_zb_attrs.c"
        binary = self.build / "test_attrs"
        compile_result = subprocess.run(
            [os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
             "-Wno-unused-parameter", "-I", str(here / "mocks"), "-I", str(self.output),
             "-include", str(here / "mocks/zboss_api_addons.h"),
             str(source), str(here / "test_attrs.c"), "-o", str(binary)],
            text=True, capture_output=True,
        )
        self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
        result = subprocess.run([str(binary)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_same_inputs_have_same_identity_and_do_not_rewrite_header(self):
        first = self.identify()
        header = self.output / "bparasite_build_identity.h"
        before = header.stat().st_mtime_ns
        self.assertEqual(self.identify(), first)
        self.assertEqual(header.stat().st_mtime_ns, before)
        self.assertRegex(first["firmware_id"], r"^bp-[0-9a-f]{12}$")
        self.assertLessEqual(len(first["firmware_id"]), 16)
        self.assertEqual(first["date_code"], "20261028")

    def test_firmware_header_board_and_config_changes_change_identity(self):
        previous = self.identify()["firmware_id"]
        for relative in (
            "code/samples/zigbee/src/main.c",
            "code/prstlib/include/prst/adc.h",
            "code/prstlib/boards/bparasite/board.overlay",
            "code/samples/zigbee/Kconfig",
            "code/samples/zigbee/prj.conf",
            "code/samples/zigbee/CMakeLists.txt",
        ):
            with self.subTest(path=relative):
                path = self.root / relative
                path.write_text(path.read_text() + "changed\n")
                current = self.identify()["firmware_id"]
                self.assertNotEqual(current, previous)
                previous = current
        self.config.write_text(self.config.read_text() + "CONFIG_OTHER=y\n")
        self.assertNotEqual(self.identify()["firmware_id"], previous)

    def test_new_firmware_source_changes_identity(self):
        first = self.identify()["firmware_id"]
        self.write(self.root / "code/prstlib/src/new_sensor.c", "int sensor;\n")
        self.assertNotEqual(self.identify()["firmware_id"], first)

    def test_mtimes_and_unrelated_files_do_not_change_identity(self):
        first = self.identify()
        os.utime(self.root / "code/samples/zigbee/src/main.c", (123, 123))
        self.write(self.root / "README.md", "new documentation")
        self.write(self.root / "code/prstlib/boards/bparasite/README.md", "board notes")
        self.write(self.root / "code/tests/irrelevant.c", "test only")
        self.assertEqual(self.identify(), first)

    def test_sdk_metadata_changes_identity(self):
        first = self.identify()["firmware_id"]
        self.assertNotEqual(self.identify("--metadata", "sdk=2.0")["firmware_id"], first)


def hex_record(address, kind, payload=b""):
    data = bytes([len(payload), address >> 8, address & 255, kind]) + payload
    return ":" + (data + bytes([-sum(data) & 255])).hex().upper() + "\n"


class PackageTests(IdentityFixture):
    def setUp(self):
        super().setUp()
        self.identity = self.identify()
        self.payload = b"\0" * 32 + self.identity["firmware_id"].encode() + b"\0"
        self.zephyr = self.config.parent
        (self.zephyr / "zephyr.bin").write_bytes(self.payload)
        (self.zephyr / "zephyr.elf").write_bytes(b"\x7fELF" + self.payload)
        self.hex = self.build / "merged.hex"
        self.hex.write_text(hex_record(0, 0, self.payload) + hex_record(0, 1))
        self.package_dir = Path(self.temp.name) / "package"

    def package(self):
        return subprocess.run(
            [sys.executable, str(SCRIPTS / "package_zigbee.py"),
             "--build-dir", str(self.build), "--output-dir", str(self.package_dir)],
            text=True, capture_output=True,
        )

    def test_package_copies_verified_artifacts_and_hashes(self):
        result = self.package()
        self.assertEqual(result.returncode, 0, result.stderr)
        manifest = json.loads((self.package_dir / "manifest.json").read_text())
        self.assertEqual(manifest["identity"], self.identity)
        self.assertEqual(manifest["hex_ranges"], [[0, len(self.payload)]])
        for name in ("app.hex", "app.bin", "app.elf", "bparasite.mjs", "build-identity.json", "FLASHING.md"):
            artifact = self.package_dir / name
            self.assertTrue(artifact.is_file())
            self.assertEqual(manifest["sha256"][name], hashlib.sha256(artifact.read_bytes()).hexdigest())
        checksums = dict(line.split("  ", 1)[::-1] for line in
                         (self.package_dir / "SHA256SUMS").read_text().splitlines())
        self.assertEqual(set(checksums), {path.name for path in self.package_dir.iterdir()
                                        if path.name != "SHA256SUMS"})
        for name, digest in checksums.items():
            self.assertEqual(digest, hashlib.sha256((self.package_dir / name).read_bytes()).hexdigest())
        self.assertEqual(json.loads((self.package_dir / "build-identity.json").read_text()), self.identity)
        converter = SCRIPTS.parent / "samples/zigbee/bparasite.mjs"
        self.assertEqual((self.package_dir / "bparasite.mjs").read_bytes(), converter.read_bytes())
        instructions = (self.package_dir / "FLASHING.md").read_text()
        self.assertIn("program app.hex verify reset exit", instructions)
        self.assertIn("bparasite@2.0.0/nrf52840", instructions)

    def test_rejects_nvram_littlefs_and_uicr_addresses_before_writing(self):
        for upper, lower in ((0xF, 0x1000), (0xF, 0xA000), (0x1000, 0x1000)):
            with self.subTest(upper=upper, lower=lower):
                self.hex.write_text(
                    hex_record(0, 4, upper.to_bytes(2, "big")) +
                    hex_record(lower, 0, b"bad") + hex_record(0, 1)
                )
                result = self.package()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("protected", result.stderr.lower())
                self.assertFalse(self.package_dir.exists())

    def test_rejects_corrupt_hex_checksum(self):
        self.hex.write_text(self.hex.read_text().replace(":30", ":31", 1))
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("hex", result.stderr.lower())

    def test_rejects_mismatched_bin_or_elf_identity(self):
        for name in ("zephyr.bin", "zephyr.elf"):
            with self.subTest(artifact=name):
                path = self.zephyr / name
                original = path.read_bytes()
                path.write_bytes(original.replace(self.identity["firmware_id"].encode(), b"bp-000000000000"))
                result = self.package()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("identity", result.stderr.lower())
                path.write_bytes(original)

    def test_rejects_hex_that_disagrees_with_bin(self):
        altered = b"\x01" + self.payload[1:]
        self.hex.write_text(hex_record(0, 0, altered) + hex_record(0, 1))
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("disagree", result.stderr.lower())

    def test_rejects_unknown_board_and_modified_config(self):
        self.config.write_text(BOARD.replace("2.0.0", "1.2.0"))
        self.identity = self.identify()
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("board", result.stderr.lower())
        self.config.write_text(BOARD)
        self.identity = self.identify()
        self.config.write_text(BOARD + "CONFIG_OTHER=y\n")
        result = self.package()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("config", result.stderr.lower())


if __name__ == "__main__":
    unittest.main()
