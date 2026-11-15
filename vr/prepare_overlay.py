"""Prepare a pinned source-only VR overlay locally. No fetch, build, install or publish."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import uuid

_spec = importlib.util.spec_from_file_location("vr_overlay_policy", Path(__file__).with_name("update_upstream.py"))
_policy = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_policy)


def load_overlay(root, version):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", version):
        raise RuntimeError("Choose a named overlay, such as v2.3.1")
    directory = Path(root) / "vr" / "overlays" / version
    if directory.is_symlink() or (hasattr(directory, "is_junction") and directory.is_junction()):
        raise RuntimeError("Overlay directory must not be a link")
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema") != 1 or manifest.get("patch") != "source.patch":
        raise RuntimeError("Unsupported source-overlay manifest")
    for field in ("upstream_commit", "source_tree"):
        if not re.fullmatch(r"[0-9a-f]{40}", manifest.get(field, "")):
            raise RuntimeError("Invalid pinned " + field)
    if not isinstance(manifest.get("abi"), int) or manifest["abi"] < 1:
        raise RuntimeError("Invalid overlay ABI")
    patch = (directory / "source.patch").read_bytes()
    if len(patch) > 8 * 1024 * 1024 or hashlib.sha256(patch).hexdigest() != manifest.get("patch_sha256"):
        raise RuntimeError("Overlay patch hash or size differs from its reviewed manifest")
    paths = _policy.review_source_patch(root, patch)
    return manifest, patch, paths


def prepare(root, version):
    root = Path(root).resolve()
    manifest, patch, paths = load_overlay(root, version)
    try:
        commit = _policy.resolve(root, manifest["upstream_commit"])
    except RuntimeError as error:
        raise RuntimeError("Pinned upstream source is not fetched. Fetch the commit listed in the overlay manifest first; no network action was performed.") from error
    work = root / "work"
    destination_root = work / "source-overlays"
    for directory in (work, destination_root):
        if directory.is_symlink() or (hasattr(directory, "is_junction") and directory.is_junction()):
            raise RuntimeError("Overlay work directory must not be a link")
        if not directory.resolve().is_relative_to(root):
            raise RuntimeError("Overlay work directory escapes this checkout")
    destination_root.mkdir(parents=True, exist_ok=True)
    destination = destination_root / (version + "-" + uuid.uuid4().hex[:8])
    _policy.git(root, "worktree", "add", "--detach", str(destination), commit)
    applied = _policy.git(destination, "apply", "--3way", "--index", "-", data=patch, check=False)
    report = {"schema": 1, "version": version, "candidate_directory": str(destination),
              "upstream_commit": commit, "abi": manifest["abi"], "source_paths": paths,
              "patch_applied": applied.returncode == 0, "source_tree_matches": False,
              "compatibility": "unverified", "release_ready": False,
              "game_launched": False, "installation_changed": False, "published": False}
    if applied.returncode == 0:
        tree = _policy.git(destination, "write-tree").stdout.decode().strip()
        report["source_tree"] = tree
        report["source_tree_matches"] = tree == manifest["source_tree"]
        header = (destination / "gpu/vk/vk_api.h").read_text(encoding="utf-8")
        abi = re.search(r"#define\s+MKWVK_API_VERSION\s+(\d+)", header)
        report["abi_matches"] = bool(abi and int(abi[1]) == manifest["abi"])
    else:
        report["apply_diagnostic"] = applied.stderr.decode("utf-8", "replace")
    report_path = destination_root / (destination.name + ".json")
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    if not report["patch_applied"] or not report["source_tree_matches"] or not report.get("abi_matches"):
        raise RuntimeError("Overlay verification failed; candidate retained for review: " + str(report_path))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", help="reviewed overlay directory, e.g. v2.3.1")
    args = parser.parse_args()
    try:
        report = prepare(Path(__file__).resolve().parents[1], args.version)
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(2, str(error) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
