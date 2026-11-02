import json
import os
from pathlib import Path
import subprocess
import shutil
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]


class DockerBuildTests(unittest.TestCase):
    def test_container_can_access_the_full_repository_for_build_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            docker = Path(directory) / "docker"
            docker.write_text(f"#!{sys.executable}\nimport json, sys\nprint(json.dumps(sys.argv[1:]))\n")
            docker.chmod(0o755)
            env = {**os.environ, "PATH": directory + os.pathsep + os.environ["PATH"]}
            result = subprocess.run(
                ["bash", str(ROOT / "code/scripts/build-with-docker.sh"), "zigbee"],
                env=env, text=True, capture_output=True, check=True,
            )
            args = json.loads(result.stdout.splitlines()[-1])
            self.assertEqual(args[args.index("-v") + 1], f"{ROOT}:/workspace")
            self.assertEqual(args[args.index("-w") + 1], "/workspace")
            self.assertEqual(args[-2:], ["code/scripts/build.sh", "zigbee"])

    def test_build_passes_the_board_module_to_sysbuild(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            scripts = root / "code/scripts"
            scripts.mkdir(parents=True)
            (root / "code/.west").mkdir()
            (root / "code/samples/zigbee").mkdir(parents=True)
            shutil.copyfile(ROOT / "code/scripts/build.sh", scripts / "build.sh")
            pip = root / "pip"
            pip.write_text(f"#!{sys.executable}\nimport pathlib\npathlib.Path({str(root / 'requirements-installed')!r}).touch()\n")
            pip.chmod(0o755)
            west = root / "west"
            west.write_text(f"#!{sys.executable}\nimport json, sys\nprint(json.dumps(sys.argv[1:]))\n")
            west.chmod(0o755)
            env = {**os.environ, "PATH": directory + os.pathsep + os.environ["PATH"]}
            result = subprocess.run(["bash", str(scripts / "build.sh"), "zigbee"],
                                    env=env, text=True, capture_output=True, check=True)
            args = json.loads(result.stdout.splitlines()[-1])
            self.assertTrue((root / "requirements-installed").exists(),
                            "SDK requirements must be installed even when .west already exists")
            self.assertIn("--sysbuild", args)
            self.assertIn(f"-DZEPHYR_EXTRA_MODULES={root}/code/prstlib", args)

    def test_application_tests_use_the_selected_c_compiler(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            compiler = root / "selected-cc"
            marker = root / "compiler-used"
            compiler.write_text(
                f"#!{sys.executable}\nimport os, pathlib, sys\n"
                f"pathlib.Path({str(marker)!r}).touch()\n"
                "os.execvp('cc', ['cc', *sys.argv[1:]])\n"
            )
            compiler.chmod(0o755)
            subprocess.run(["bash", str(ROOT / "code/tests/application/run.sh")],
                           env={**os.environ, "CC": str(compiler)},
                           text=True, capture_output=True, check=True)
            self.assertTrue(marker.exists(), "Application tests must honor CC")


if __name__ == "__main__":
    unittest.main()
