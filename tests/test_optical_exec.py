import fcntl
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import unittest
from unittest import mock
import uuid

HELPER = Path(__file__).parents[1] / "tools/optical-exec.py"
spec = importlib.util.spec_from_file_location("optical_exec", HELPER)
adapter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adapter)


class OpticalExecTests(unittest.TestCase):
    def setUp(self):
        self.parent = Path(__file__).parent / ".optical-exec-testdata"
        self.parent.mkdir(exist_ok=True)
        self.root = self.parent / str(uuid.uuid4())
        self.root.mkdir()
        self.addCleanup(self.cleanup)

    def cleanup(self):
        shutil.rmtree(self.root, ignore_errors=True)
        try:
            self.parent.rmdir()
        except OSError:
            pass

    def test_real_elf_executes_from_sealed_memory(self):
        code = ("import importlib.util; s=importlib.util.spec_from_file_location('adapter',"
                + repr(str(HELPER)) + "); m=importlib.util.module_from_spec(s);"
                " s.loader.exec_module(m); m.execute('/usr/bin/true')")
        result = subprocess.run([sys.executable, "-c", code], capture_output=True, timeout=15)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_memfd_is_sealed_and_contains_exact_bytes(self):
        executable = self.root / "game"
        data = b"\x7fELF" + b"\x00" * 100
        executable.write_bytes(data)
        executable.chmod(0o500)
        descriptor = adapter.stage_executable(executable)
        self.addCleanup(os.close, descriptor)
        self.assertEqual(os.pread(descriptor, len(data), 0), data)
        self.assertTrue(fcntl.fcntl(descriptor, fcntl.F_GET_SEALS) & fcntl.F_SEAL_WRITE)
        with self.assertRaises(OSError):
            os.pwrite(descriptor, b"x", 0)

    def test_refuses_symlink_script_nonexecutable_and_oversize(self):
        path = self.root / "game"
        path.write_bytes(b"#!/bin/sh\n" + b"x" * 30)
        path.chmod(0o500)
        with self.assertRaisesRegex(ValueError, "ELF"):
            adapter.stage_executable(path)
        path.write_bytes(b"\x7fELF" + b"x" * 30)
        path.chmod(0o400)
        with self.assertRaisesRegex(ValueError, "bounded executable"):
            adapter.stage_executable(path)
        path.chmod(0o500)
        with mock.patch.object(adapter, "MAX_EXECUTABLE", 20):
            with self.assertRaisesRegex(ValueError, "bounded executable"):
                adapter.stage_executable(path)
        link = self.root / "link"
        link.symlink_to(path)
        with self.assertRaises(OSError):
            adapter.stage_executable(link)


if __name__ == "__main__":
    unittest.main()
