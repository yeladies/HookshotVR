"""Audit/replay the source-only VR patch on an explicit upstream revision. Never installs or publishes."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import uuid


def git(root, *args, data=None, check=True):
    result = subprocess.run(["git", "-C", str(root), *args], input=data, capture_output=True)
    if check and result.returncode:
        raise RuntimeError(result.stderr.decode("utf-8", "replace").strip())
    return result


def resolve(root, revision):
    return git(root, "rev-parse", "--verify", revision + "^{commit}").stdout.decode().strip()


def allowed_patch(path):
    exact = {".gitignore", "readme.md", ".github/workflows/vr-bootstrap.yml", "host/hos_main.c", "host/rx_backend.c", "nx2/build.py", "nx2/test_timeline_modes.py", "gpu/vk/vk_api.h", "gpu/vk/vk_main.cpp", "gpu/vk/vk_replay.cpp", "gpu/vk/vk_draw.cpp", "tests/temporal_jitter_test.cpp", "tests/test_temporal_jitter.py", "tests/capture_slot_ownership_test.cpp", "tests/test_capture_slot_ownership.py"}
    prefixes = ("vr/", "docs/vr-", "gpu/vk/vk_xr", "gpu/vk/xr_", "tests/test_vr_")
    if path.startswith(("work/", "/")) or ".." in Path(path).parts:
        return False
    if path not in exact and not path.startswith(prefixes):
        return False
    return path == ".gitignore" or Path(path).suffix in {".md", ".ps1", ".py", ".cpp", ".c", ".h", ".json", ".yml", ".txt"}


def audit(root, manifest, revision):
    base = resolve(root, manifest["base_commit"])
    mod = resolve(root, "HEAD")
    candidate = resolve(root, revision)
    for tip in (mod, candidate):
        if git(root, "merge-base", "--is-ancestor", base, tip, check=False).returncode:
            raise RuntimeError("Recorded base must be an ancestor of both mod and candidate")
    paths = git(root, "diff", "--name-only", base, mod).stdout.decode().splitlines()
    forbidden = [p for p in paths if not allowed_patch(p)]
    if forbidden:
        raise RuntimeError("Patch contains paths outside the source allowlist: " + ", ".join(forbidden))
    files = []
    for path in manifest["integration_files"]:
        old = git(root, "rev-parse", base + ":" + path, check=False)
        new = git(root, "rev-parse", candidate + ":" + path, check=False)
        status = "missing" if new.returncode else "unchanged" if old.returncode == 0 and old.stdout == new.stdout else "changed"
        files.append({"path": path, "status": status})
    return {"base_commit": base, "mod_commit": mod, "candidate_commit": candidate,
            "integration_files": files, "patch_paths": paths,
            "requires_source_review": any(f["status"] != "unchanged" for f in files),
            "compatibility": "unverified", "release_ready": False,
            "working_tree_dirty": bool(git(root, "status", "--porcelain").stdout.strip()),
            "game_data_copied": False, "installation_changed": False}


def prepare(root, manifest, revision):
    root = Path(root).resolve()
    if git(root, "status", "--porcelain").stdout.strip():
        raise RuntimeError("Commit/stash source edits before preparing an isolated candidate; no edits were discarded")
    report = audit(root, manifest, revision)
    work = root / "work" / "upstream-updates"
    for path in (root / "work", work):
        if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
            raise RuntimeError("Candidate root must not be a link")
    if not work.resolve().is_relative_to(root):
        raise RuntimeError("Candidate path escapes the project")
    work.mkdir(parents=True, exist_ok=True)
    destination = work / ("candidate-" + report["candidate_commit"][:12] + "-" + uuid.uuid4().hex[:8])
    patch = git(root, "diff", "--binary", report["base_commit"], report["mod_commit"]).stdout
    git(root, "worktree", "add", "--detach", str(destination), report["candidate_commit"])
    applied = git(destination, "apply", "--3way", "--index", "-", data=patch, check=False) if patch else None
    report["candidate_directory"] = str(destination)
    report["patch_applied"] = applied is None or applied.returncode == 0
    report["conflicts"] = git(destination, "diff", "--name-only", "--diff-filter=U").stdout.decode().splitlines()
    report["apply_diagnostic"] = "" if applied is None else applied.stderr.decode("utf-8", "replace")
    report["next_step"] = "review source/conflicts, then build/test privately; never install an unverified candidate"
    (destination.parent / (destination.name + ".json")).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("audit", "prepare"))
    parser.add_argument("--revision", required=True, help="explicit already-fetched upstream commit/tag/ref")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root / "vr/upstream.json").read_text(encoding="utf-8"))
    try:
        report = (prepare if args.action == "prepare" else audit)(root, manifest, args.revision)
    except (RuntimeError, OSError) as error:
        parser.exit(2, str(error) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if report.get("patch_applied", True) else 1

if __name__ == "__main__":
    raise SystemExit(main())