"""Synthetic Git-only update tests; no network, runtime, game or user installation."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
spec = importlib.util.spec_from_file_location("update_upstream", Path(__file__).parents[1] / "update_upstream.py")
update = importlib.util.module_from_spec(spec)
spec.loader.exec_module(update)

class UpdateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="hookshot-vr-update-")
        self.root = Path(self.tmp.name)
        self.cmd("init", "-b", "mod")
        self.cmd("config", "user.name", "VR test fixture")
        self.cmd("config", "user.email", "fixture@example.invalid")
        (self.root / ".gitignore").write_text("work/\n")
        (self.root / "host").mkdir()
        (self.root / "host/hos_main.c").write_text("camera = 0;\n")
        (self.root / "readme.md").write_text("base\n")
        self.commit("base")
        self.base = self.cmd("rev-parse", "HEAD")
        (self.root / "vr").mkdir()
        (self.root / "vr/mod.py").write_text("# authored mod\n")
        self.commit("mod")
        self.manifest = {"base_commit": self.base, "integration_files": ["host/hos_main.c"]}
    def tearDown(self):
        self.tmp.cleanup()
    def cmd(self, *args):
        return subprocess.run(["git", "-C", str(self.root), *args], check=True, capture_output=True, text=True).stdout.strip()
    def commit(self, message):
        self.cmd("add", "."); self.cmd("commit", "-m", message)
    def candidate(self, conflict=False):
        self.cmd("switch", "-c", "upstream", self.base)
        (self.root / ("host/hos_main.c" if conflict else "readme.md")).write_text("upstream = 2;\n")
        self.commit("upstream")
        candidate = self.cmd("rev-parse", "HEAD")
        self.cmd("switch", "mod")
        return candidate
    def test_clean_replay_is_isolated_and_unverified(self):
        candidate = self.candidate()
        head = self.cmd("rev-parse", "HEAD")
        report = update.prepare(self.root, self.manifest, candidate)
        self.assertTrue(report["patch_applied"])
        self.assertFalse(report["release_ready"])
        self.assertEqual(report["compatibility"], "unverified")
        self.assertTrue((Path(report["candidate_directory"]) / "vr/mod.py").exists())
        self.assertEqual(self.cmd("rev-parse", "HEAD"), head)
        self.assertEqual((self.root / "readme.md").read_text(), "base\n")
    def test_conflict_is_retained_for_review(self):
        (self.root / "host/hos_main.c").write_text("mod = 1;\n"); self.commit("mod integration")
        report = update.prepare(self.root, self.manifest, self.candidate(True))
        self.assertFalse(report["patch_applied"])
        self.assertEqual(report["conflicts"], ["host/hos_main.c"])
        self.assertTrue(report["requires_source_review"])
        self.assertEqual((self.root / "host/hos_main.c").read_text(), "mod = 1;\n")
    def test_dirty_source_is_not_discarded(self):
        (self.root / "readme.md").write_text("user edit\n")
        with self.assertRaises(RuntimeError): update.prepare(self.root, self.manifest, self.base)
        self.assertEqual((self.root / "readme.md").read_text(), "user edit\n")
    def test_authored_shader_reapplies(self):
        shader=self.root/"vr/shaders/eye_address_copy.comp"
        shader.parent.mkdir(parents=True)
        shader.write_text("// authored synthetic shader source\n")
        self.commit("add authored shader")
        report=update.prepare(self.root,self.manifest,self.base)
        self.assertTrue(report["patch_applied"])
        self.assertEqual((Path(report["candidate_directory"])/"vr/shaders/eye_address_copy.comp").read_text(),shader.read_text())
        self.assertEqual(report["compatibility"],"unverified")
    def test_patch_allowlist(self):
        for path in ("work/image.bin", "vr/game.exe", "../host/hos_main.c", "user/save.sav", "vr/shaders/game.spv", "vr/shaders/unknown.comp", "work/capture.comp"):
            self.assertFalse(update.allowed_patch(path))
        self.assertTrue(update.allowed_patch("vr/src/mod.cpp"))
        self.assertTrue(update.allowed_patch("vr/shaders/eye_address_copy.comp"))

if __name__ == "__main__": unittest.main()