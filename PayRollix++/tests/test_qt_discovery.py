#!/usr/bin/env python3
"""Exercise discovery with fake kit metadata, not a Windows build or real Qt."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "cmake" / "FindPayrollQt.cmake"
CMAKE = shutil.which("cmake")


@unittest.skipUnless(CMAKE, "CMake is required for discovery tests")
class QtDiscoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="payrollix qt discovery ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.result = self.root / "selected.txt"

    def kit(self, name="Qt/6.9.3/msvc2022_64", version="6.9.3", arch="x86_64"):
        root = self.root / name
        for relative in (
            "lib/cmake/Qt6/Qt6Config.cmake",
            "lib/cmake/Qt6Widgets/Qt6WidgetsConfig.cmake",
            "bin/windeployqt.exe", "bin/Qt6Core.dll", "bin/Qt6Gui.dll",
            "bin/Qt6Widgets.dll", "lib/Qt6Core.lib", "lib/Qt6Gui.lib",
            "lib/Qt6Widgets.lib", "plugins/platforms/qwindows.dll",
        ):
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        metadata = root / "mkspecs/qconfig.pri"
        metadata.parent.mkdir(parents=True)
        metadata.write_text(f"QT_ARCH = {arch}\nQT_VERSION = {version}\n")
        return root

    def discover(self, roots=None, explicit=None, environment=None):
        command = [CMAKE, f"-DPAYROLLIX_QT_RESULT={self.result}"]
        if roots is not None:
            command.append("-DPAYROLLIX_QT_SEARCH_ROOTS=" + ";".join(map(str, roots)))
        if explicit is not None:
            command.append(f"-DPAYROLLIX_QT_ROOT={explicit}")
        command.extend(["-P", str(SCRIPT)])
        env = os.environ.copy()
        for key in ("QTDIR", "QT_ROOT_DIR", "Qt6_DIR", "CMAKE_PREFIX_PATH"):
            env.pop(key, None)
        env.update(environment or {})
        return subprocess.run(command, capture_output=True, text=True, env=env)

    def selected(self):
        return Path(self.result.read_text())

    def test_finds_kit_in_version_tree_with_spaces(self):
        kit = self.kit()
        run = self.discover([self.root / "Qt"])
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(self.selected(), kit)

    def test_selects_newest_compatible_version_numerically(self):
        self.kit()
        newest = self.kit("Qt/6.10.2/msvc2022_64", "6.10.2")
        self.kit("Qt/6.11.0/arm64", "6.11.0", "arm64")
        run = self.discover([self.root / "Qt"])
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(self.selected(), newest)

    def test_explicit_kit_wins_over_newer_discovered_kit(self):
        kit = self.kit()
        self.kit("Qt/6.10.2/msvc2022_64", "6.10.2")
        run = self.discover([self.root / "Qt"], explicit=kit)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(self.selected(), kit)

    def test_invalid_override_fails_instead_of_silently_falling_back(self):
        self.kit()
        run = self.discover([self.root / "Qt"], explicit=self.root / "missing")
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("selected Qt folder", run.stderr)
        self.assertFalse(self.result.exists())

    def test_rejects_old_arm64_mingw_and_incomplete_kits(self):
        for name, version, arch, missing in (
            ("old", "6.8.3", "x86_64", None),
            ("arm", "6.9.3", "arm64", None),
            ("mingw", "6.9.3", "x86_64", "lib/Qt6Core.lib"),
            ("partial", "6.9.3", "x86_64", "bin/windeployqt.exe"),
        ):
            with self.subTest(name=name):
                kit = self.kit(name, version, arch)
                if missing:
                    (kit / missing).unlink()
                run = self.discover([kit])
                self.assertNotEqual(run.returncode, 0)
                self.assertIn("Qt was not found", run.stderr)
                self.assertFalse(self.result.exists())

    def test_missing_qt_explains_install_and_override(self):
        run = self.discover([self.root / "empty"])
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("Install Qt 6.9", run.stderr)
        self.assertIn("-QtRoot", run.stderr)
        self.assertIn("No software has been installed", run.stderr)

    def test_qtdir_and_package_directory_environment_hints(self):
        kit = self.kit()
        for key, value in (("QTDIR", kit), ("QT_ROOT_DIR", kit),
                           ("Qt6_DIR", kit / "lib/cmake/Qt6"),
                           ("CMAKE_PREFIX_PATH", kit)):
            with self.subTest(key=key):
                run = self.discover(environment={key: str(value)})
                self.assertEqual(run.returncode, 0, run.stderr)
                self.assertEqual(self.selected(), kit)

    def test_path_qt_tool_hint(self):
        kit = self.kit()
        run = self.discover(environment={"PATH": str(kit / "bin")})
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(self.selected(), kit)


if __name__ == "__main__":
    unittest.main()
