#!/usr/bin/env python3
"""Execute a reviewed optical ELF from a sealed anonymous memory file."""
import fcntl
import os
from pathlib import PurePosixPath
import stat
import sys

MAX_EXECUTABLE = 512 * 1024 * 1024


def stage_executable(path):
    source = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    memory = None
    try:
        before = os.fstat(source)
        if (not stat.S_ISREG(before.st_mode) or not 20 <= before.st_size <= MAX_EXECUTABLE
                or not before.st_mode & 0o111):
            raise ValueError("The optical entry point is not a bounded executable file.")
        if not hasattr(os, "memfd_create"):
            raise ValueError("This kernel does not provide sealed memory-file execution.")
        memory = os.memfd_create("isolde-optical-game", os.MFD_CLOEXEC | os.MFD_ALLOW_SEALING)
        first = True
        remaining = before.st_size
        while remaining:
            data = os.read(source, min(1024 * 1024, remaining))
            if not data:
                raise ValueError("The optical executable became unreadable.")
            if first:
                if not data.startswith(b"\x7fELF"):
                    raise ValueError("The optical entry point is not a Linux ELF executable.")
                first = False
            view = memoryview(data)
            while view:
                written = os.write(memory, view)
                if written <= 0:
                    raise OSError("Could not stage the optical executable.")
                view = view[written:]
            remaining -= len(data)
        after = os.fstat(source)
        if (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns) != (
                before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns):
            raise ValueError("The optical executable changed while being read.")
        os.fchmod(memory, 0o500)
        fcntl.fcntl(memory, fcntl.F_ADD_SEALS,
                    fcntl.F_SEAL_WRITE | fcntl.F_SEAL_SHRINK | fcntl.F_SEAL_GROW | fcntl.F_SEAL_SEAL)
        os.set_inheritable(memory, True)
        result = memory
        memory = None
        return result
    finally:
        os.close(source)
        if memory is not None:
            os.close(memory)


def execute(path):
    descriptor = stage_executable(path)
    try:
        os.execve("/proc/self/fd/" + str(descriptor), [path], os.environ)
    finally:
        os.close(descriptor)


def main():
    if len(sys.argv) != 2:
        raise ValueError("Expected one reviewed optical executable.")
    target = sys.argv[1]
    path = PurePosixPath(target)
    if (not target.startswith("/media/games/") or str(path) != target
            or any(part in (".", "..") for part in path.parts)):
        raise ValueError("The optical executable is outside the reviewed disc.")
    execute(target)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as error:
        print("Optical game: " + str(error), file=sys.stderr)
        sys.exit(2)
