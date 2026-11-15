"""Synthetic source-overlay tests. No network, game data, build or headset."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("prepare_overlay", ROOT / "vr/prepare_overlay.py")
overlay = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(overlay)


class OverlayTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="vr-source-overlay-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.git("init", "-b", "main")
        self.git("config", "user.name", "authored fixture")
        self.git("config", "user.email", "fixture@example.invalid")
        for name, text in {".gitignore": "work/\n", "gpu/vk/vk_api.h": "#define MKWVK_API_VERSION 23\n",
                           "host/hos_main.c": "int camera = 0;\n",
                           "assets/saves/example/progress.sav": "Authored placeholder, not a game save.\n"}.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        self.commit("synthetic upstream")
        self.base = self.git("rev-parse", "HEAD")
        (self.root / "host/hos_main.c").write_text("int camera = 1;\n")
        (self.root / "gpu/vk/vk_api.h").write_text("#define MKWVK_API_VERSION 24\n")
        self.commit("authored camera change")
        self.target = self.git("rev-parse", "HEAD^{tree}")
        patch = subprocess.check_output(["git", "-C", str(self.root), "diff", "--binary", self.base, "HEAD"])
        self.git("checkout", "--detach", self.base)
        self.directory = self.root / "vr/overlays/test-version"
        self.directory.mkdir(parents=True)
        self.manifest = {"schema": 1, "patch": "source.patch", "upstream_commit": self.base,
                         "source_tree": self.target, "abi": 24}
        self.write_overlay(patch)

    def git(self, *args):
        return subprocess.check_output(["git", "-C", str(self.root), *args], stderr=subprocess.DEVNULL, text=True).strip()

    def commit(self, message):
        self.git("add", ".")
        self.git("commit", "-m", message)

    def write_overlay(self, patch):
        self.manifest["patch_sha256"] = hashlib.sha256(patch).hexdigest()
        (self.directory / "source.patch").write_bytes(patch)
        self.write_manifest()

    def write_manifest(self):
        (self.directory / "manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")

    def test_exact_source_replay_preserves_original_and_omits_upstream_data_from_patch(self):
        original = (self.root / "host/hos_main.c").read_bytes()
        report = overlay.prepare(self.root, "test-version")
        self.assertTrue(report["source_tree_matches"] and report["abi_matches"])
        self.assertFalse(report["release_ready"] or report["game_launched"] or report["installation_changed"])
        self.assertEqual(self.git("rev-parse", "HEAD"), self.base)
        self.assertEqual((self.root / "host/hos_main.c").read_bytes(), original)
        self.assertNotIn(b"assets/saves", (self.directory / "source.patch").read_bytes())
        self.assertEqual((Path(report["candidate_directory"]) / "assets/saves/example/progress.sav").read_text(),
                         "Authored placeholder, not a game save.\n")

    def test_hash_mismatch_is_rejected_before_checkout(self):
        with (self.directory / "source.patch").open("ab") as stream:
            stream.write(b"tampered")
        with self.assertRaisesRegex(RuntimeError, "hash"):
            overlay.prepare(self.root, "test-version")
        self.assertFalse((self.root / "work").exists())

    def test_game_path_is_rejected_even_with_matching_digest(self):
        patch = b"diff --git a/user/progress.sav b/user/progress.sav\nnew file mode 100644\n--- /dev/null\n+++ b/user/progress.sav\n@@ -0,0 +1 @@\n+authored forbidden-path fixture\n"
        self.write_overlay(patch)
        with self.assertRaisesRegex(RuntimeError, "forbidden path"):
            overlay.prepare(self.root, "test-version")
        self.assertFalse((self.root / "work").exists())

    def test_missing_pin_does_not_fetch(self):
        self.manifest["upstream_commit"] = "0" * 40
        self.write_manifest()
        with self.assertRaisesRegex(RuntimeError, "no network action"):
            overlay.prepare(self.root, "test-version")
        self.assertFalse((self.root / "work").exists())

    def test_mismatched_tree_is_retained_and_unverified(self):
        self.manifest["source_tree"] = "0" * 40
        self.write_manifest()
        with self.assertRaisesRegex(RuntimeError, "retained for review"):
            overlay.prepare(self.root, "test-version")
        reports = list((self.root / "work/source-overlays").glob("*.json"))
        report = json.loads(reports[0].read_text())
        self.assertFalse(report["source_tree_matches"] or report["release_ready"])
        self.assertEqual(self.git("rev-parse", "HEAD"), self.base)

    def test_wrong_abi_does_not_become_a_valid_candidate(self):
        self.manifest["abi"] = 25
        self.write_manifest()
        with self.assertRaisesRegex(RuntimeError, "verification failed"):
            overlay.prepare(self.root, "test-version")
        report = json.loads(next((self.root / "work/source-overlays").glob("*.json")).read_text())
        self.assertFalse(report["abi_matches"] or report["release_ready"])

    def test_overlay_name_cannot_escape_its_directory(self):
        with self.assertRaisesRegex(RuntimeError, "named overlay"):
            overlay.prepare(self.root, "../outside")
        self.assertFalse((self.root / "work").exists())


if __name__ == "__main__":
    unittest.main()
