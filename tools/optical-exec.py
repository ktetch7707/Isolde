#!/usr/bin/env python3
"""Execute a reviewed optical ELF from a private temporary staging tree."""
import fcntl
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import tempfile

MAX_EXECUTABLE = 512 * 1024 * 1024
MAX_DIRECTORY_ENTRIES = 4096
MAX_PATH_DEPTH = 32


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


def execute(path, disc_root="/media/games"):
    root = Path(disc_root)
    target = Path(path)
    if not root.is_absolute() or not target.is_absolute():
        raise ValueError("The optical executable and disc root must be absolute.")
    try:
        parts = target.relative_to(root).parts
    except ValueError:
        raise ValueError("The optical executable is outside the reviewed disc.") from None
    if (not parts or len(parts) > MAX_PATH_DEPTH or ".." in parts
            or os.path.realpath(root) != str(root) or os.path.realpath(target) != str(target)):
        raise ValueError("The optical executable path is not a canonical disc path.")

    descriptor = stage_executable(path)
    staging = None
    try:
        staging = tempfile.mkdtemp(prefix="isolde-optical-")
        source = root
        destination = Path(staging)
        remaining_entries = MAX_DIRECTORY_ENTRIES
        for index, component in enumerate(parts):
            with os.scandir(source) as entries:
                for entry in entries:
                    remaining_entries -= 1
                    if remaining_entries < 0:
                        raise ValueError("The optical game directory contains too many entries to stage safely.")
                    if entry.name != component:
                        os.symlink(str(source / entry.name), destination / entry.name)
            source = source / component
            destination = destination / component
            if index < len(parts) - 1:
                os.mkdir(destination, 0o700)

        with os.fdopen(os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o500), "wb") as staged:
            os.lseek(descriptor, 0, os.SEEK_SET)
            with os.fdopen(os.dup(descriptor), "rb") as sealed:
                shutil.copyfileobj(sealed, staged, 1024 * 1024)
        os.close(descriptor)
        descriptor = None
        os.execve(str(destination), [path], os.environ)
    finally:
        if descriptor is not None:
            os.close(descriptor)
        if staging is not None:
            shutil.rmtree(staging)


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
