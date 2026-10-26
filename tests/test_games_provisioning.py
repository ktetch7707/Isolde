import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("games_provision", Path(__file__).parents[1]/"tools/provision-games.py")
games = importlib.util.module_from_spec(spec)
spec.loader.exec_module(games)


class GamesProvisioningTests(unittest.TestCase):
    def test_no_install_without_explicit_flag(self):
        state={"dolphin-emu":{"ready":False}}
        with patch.object(games,"platform"),patch.object(games,"inspect",return_value=state),patch.object(games,"run") as run:
            result=games.provision(["dolphin-emu"])
            self.assertFalse(result["ready"]);run.assert_not_called()

    def test_repeat_setup_is_idempotent(self):
        with patch.object(games,"platform"),patch.object(games,"inspect",return_value={"pcsx2":{"ready":True}}),patch.object(games,"run") as run:
            self.assertTrue(games.provision(["pcsx2"],True)["ready"]);run.assert_not_called()

    def test_install_is_pinned_and_failure_is_not_ready(self):
        states=[{"pcsx2":{"ready":False}},{"pcsx2":{"ready":False}}]
        with patch.object(games,"platform"),patch.object(games,"inspect",side_effect=states),patch.object(games.shutil,"disk_usage") as disk,patch.object(games,"run") as run:
            disk.return_value.free=8*1024**3
            with self.assertRaises(RuntimeError):games.provision(["pcsx2"],True)
            command=run.call_args.args
            self.assertIn("pcsx2="+games.PACKAGES["pcsx2"][0],command)
            self.assertIn("--no-remove",command)

    def test_unknown_package_and_redirected_receipt_refused(self):
        with patch.object(games,"platform"):
            with self.assertRaises(ValueError):games.provision(["arbitrary"])
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/"original").write_text("preserve")
            (root/"receipt").symlink_to(root/"original")
            with self.assertRaises(ValueError):games.atomic_receipt(root/"receipt",{})
            self.assertEqual((root/"original").read_text(),"preserve")


if __name__=="__main__":unittest.main()
