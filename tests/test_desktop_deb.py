import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("desktop_deb", ROOT / "tools/build-desktop-deb.py")
desktop_deb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(desktop_deb)


@unittest.skipUnless(
    all(shutil.which(tool) for tool in ("dpkg", "dpkg-deb", "dpkg-shlibdeps")),
    "Debian package tools are required",
)
class DesktopPackageTests(unittest.TestCase):
    def setUp(self):
        """Create a fake build without installing anything on the host."""
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        self.build.mkdir()
        version = (ROOT / "VERSION").read_text().strip()
        (self.build / "CMakeCache.txt").write_text(
            f"CMAKE_PROJECT_VERSION:STATIC={version}\n"
            f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}\n"
        )
        self.output = self.root / "packages"
        self.real_run = subprocess.run
        self.incomplete = False
        self.add_symlink = False
        self.payload_marker = "first release"

    def run_with_fake_install(self, command, **kwargs):
        """Use real Debian tools while substituting small local ELF fixtures."""
        if command[0] != "cmake":
            return self.real_run(command, **kwargs)
        stage = Path(kwargs["env"]["DESTDIR"]) / "opt/isolde"
        for name in ("isolde", "isolde-native-host", "isolde-recovery"):
            if self.incomplete and name == "isolde-native-host":
                continue
            destination = stage / "bin" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy("/bin/true", destination)
        config = stage / "bin/config/native-session-bus.conf"
        config.parent.mkdir(parents=True, exist_ok=True)
        config.write_text(self.payload_marker)
        config.chmod(0o644)
        if self.add_symlink:
            (stage / "bin/link").symlink_to("/bin/true")
        return subprocess.CompletedProcess(command, 0)

    def package(self):
        """Exercise the actual packaging tools without claiming an app build."""
        with patch.object(desktop_deb, "check_host", return_value="amd64"), \
             patch.object(desktop_deb.subprocess, "run", side_effect=self.run_with_fake_install):
            return desktop_deb.build_package(self.build, self.output)

    def test_package_has_windowed_launcher_and_runtime_dependencies(self):
        """A desktop package must be discoverable and declare runtime needs."""
        package = self.package()
        fields = subprocess.check_output(
            ["dpkg-deb", "-f", str(package), "Package", "Architecture", "Depends"], text=True
        )
        self.assertIn("Package: isolde", fields)
        self.assertIn("Architecture: amd64", fields)
        self.assertIn("libc6", fields)
        self.assertIn("qml6-module-qtwebengine", fields)
        extracted = self.root / "extracted"
        subprocess.run(["dpkg-deb", "-x", str(package), str(extracted)], check=True)
        launcher = (extracted / "usr/share/applications/org.isolde.Isolde.Desktop.desktop").read_text()
        self.assertIn("Exec=/opt/isolde/bin/isolde --windowed", launcher)
        self.assertIn("Name=Isolde (Desktop)", launcher)
        self.assertTrue((extracted / "opt/isolde/bin/config/native-session-bus.conf").is_file())
        self.assertFalse((extracted / "etc").exists())

    def test_incomplete_build_does_not_publish(self):
        """Missing runtime pieces must never produce an installable package."""
        self.incomplete = True
        with self.assertRaisesRegex(RuntimeError, "Incomplete installed build"):
            self.package()
        self.assertEqual(list(self.output.glob("*.deb")), [])

    def test_symlinks_are_rejected(self):
        """Do not publish a payload that can reference files outside staging."""
        self.add_symlink = True
        with self.assertRaisesRegex(RuntimeError, "contains a symlink"):
            self.package()
        self.assertEqual(list(self.output.glob("*.deb")), [])

    def test_private_umask_does_not_make_package_directories_private(self):
        """Other users must be able to traverse the installed application."""
        previous = os.umask(0o077)
        try:
            package = self.package()
        finally:
            os.umask(previous)
        extracted = self.root / "extracted"
        subprocess.run(["dpkg-deb", "-x", str(package), str(extracted)], check=True)
        self.assertEqual((extracted / "opt/isolde/bin").stat().st_mode & 0o777, 0o755)
        self.assertEqual(package.stat().st_mode & 0o777, 0o644)

    def test_package_upgrade_preserves_user_files(self):
        """A package upgrade replaces installed files without resetting a home directory."""
        first = self.package()
        self.payload_marker = "second release"
        with patch.object(desktop_deb, "check_build", return_value="9.9.9.9"):
            second = self.package()

        system = self.root / "system"
        user_file = system / "home/test/.config/Isolde/profile.json"
        user_file.parent.mkdir(parents=True)
        user_file.write_text("user profile")
        for package in (first, second):
            result = subprocess.run(
                ["dpkg", f"--root={system}", "--force-not-root", "--force-depends",
                 "--install", str(package)],
                text=True, capture_output=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

        self.assertEqual(user_file.read_text(), "user profile")
        self.assertEqual(
            (system / "opt/isolde/bin/config/native-session-bus.conf").read_text(),
            "second release",
        )
        self.assertIn("Version: 9.9.9.9", (system / "var/lib/dpkg/status").read_text())

    def test_target_host_and_architecture_are_required(self):
        """Do not generate packages against a different operating system or ABI."""
        with patch.object(desktop_deb.Path, "read_text", return_value='ID=ubuntu\nVERSION_ID="24.04"\n'), \
             patch.object(desktop_deb.subprocess, "check_output") as query:
            with self.assertRaisesRegex(RuntimeError, "Ubuntu 26.04"):
                desktop_deb.check_host()
            query.assert_not_called()
        with patch.object(desktop_deb.Path, "read_text", return_value='ID=ubuntu\nVERSION_ID="26.04"\n'), \
             patch.object(desktop_deb.subprocess, "check_output", return_value="i386\n"):
            with self.assertRaisesRegex(RuntimeError, "Unsupported package architecture"):
                desktop_deb.check_host()

    def test_rejects_mismatched_build_and_existing_output(self):
        """Avoid packaging a stale build or replacing a previous artifact."""
        package = self.package()
        original = package.read_bytes()
        with self.assertRaisesRegex(RuntimeError, "Refusing to overwrite"):
            self.package()
        self.assertEqual(package.read_bytes(), original)
        (self.build / "CMakeCache.txt").write_text(
            f"CMAKE_PROJECT_VERSION:STATIC=9.9.9.9\nCMAKE_HOME_DIRECTORY:INTERNAL={ROOT}\n"
        )
        with self.assertRaisesRegex(RuntimeError, "does not match"):
            self.package()


if __name__ == "__main__":
    unittest.main()
