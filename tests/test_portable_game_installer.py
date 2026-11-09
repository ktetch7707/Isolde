import importlib.util
import json
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile

spec=importlib.util.spec_from_file_location("portable_game_installer",Path(__file__).parents[1]/"tools/portable-game-installer.py")
installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)


class PortableGameInstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.archive=self.root/"fixture.zip"
        self.manifest={"schemaVersion":1,"title":"Inert Linux fixture","platform":"PC","entrypoint":"bin/game"}

    def package(self,extra=(),entry=b"\x7fELF"+b"\x00"*32,manifest=None):
        with zipfile.ZipFile(self.archive,"w") as archive:
            archive.writestr(installer.MANIFEST,json.dumps(manifest or self.manifest))
            archive.writestr("bin/game",entry)
            for item in extra: archive.writestr(*item)

    def test_inspects_and_installs_without_executing_payload(self):
        self.package()
        result=installer.inspect(self.archive)
        self.assertEqual(result["title"],"Inert Linux fixture")
        destination=self.root/"private"/"games"/"id"/"install"
        installer.install(self.archive,destination)
        self.assertEqual((destination/"bin/game").read_bytes(),b"\x7fELF"+b"\x00"*32)
        self.assertTrue((destination/"bin/game").stat().st_mode & stat.S_IXUSR)
        self.assertTrue((destination/".isolde-install.json").is_file())
        with self.assertRaisesRegex(ValueError,"already exists"):installer.install(self.archive,destination)

    def test_rejects_traversal_symlinks_duplicates_non_elf_and_bad_manifest(self):
        self.package(extra=[("../escape",b"unsafe")])
        with self.assertRaises(ValueError):installer.inspect(self.archive)
        self.package(entry=b"#!/bin/sh\n")
        with self.assertRaisesRegex(ValueError,"ELF"):installer.inspect(self.archive)
        self.package(manifest={"schemaVersion":1,"title":"x","platform":"PC","entrypoint":"../outside"})
        with self.assertRaises(ValueError):installer.inspect(self.archive)
        with zipfile.ZipFile(self.archive,"w") as archive:
            info=zipfile.ZipInfo("link");info.external_attr=(stat.S_IFLNK|0o777)<<16;archive.writestr(info,"/etc")
        with self.assertRaisesRegex(ValueError,"unsafe"):installer.inspect(self.archive)

    def test_archive_byte_and_entry_limits(self):
        self.archive.write_bytes(b"x"*(installer.MAX_ARCHIVE+1))
        with self.assertRaisesRegex(ValueError,"512 MiB"):installer.inspect(self.archive)
        with zipfile.ZipFile(self.archive,"w") as archive:
            for index in range(installer.MAX_ENTRIES+1):archive.writestr(f"{index}",b"")
        with self.assertRaisesRegex(ValueError,"too many"):installer.inspect(self.archive)


if __name__=="__main__":unittest.main()
