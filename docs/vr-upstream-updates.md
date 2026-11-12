# Keeping the VR adaptation updateable

The mod is maintained as a source adaptation over a recorded Hookshot base, with
original VR code isolated under `vr/` and XR renderer helpers. It is not a DLL that
can safely be dropped into any future upstream installation. Upstream changes can
alter the host ABI, replay ownership, game hooks and camera conventions.

`vr/upstream.json` records the audited upstream commit, renderer ABI, full supported
game build IDs and integration files requiring review. Build scripts validate those
profiles. Private build receipts bind source fingerprints and executable/DLL hashes;
the launcher rejects stale or mixed builds. Game build IDs require separate review
when the game itself changes. A new upstream source release is not automatically a
new supported game revision.

## Prepare a candidate

Fetch upstream explicitly, choose the commit or tag being evaluated, then run:

```powershell
git fetch upstream
python -I -B vr/update_upstream.py audit --revision upstream/main
python -I -B vr/update_upstream.py prepare --revision upstream/main
```

Use the project's Builder Python if `python` is unavailable. The revision must be
already fetched and descend from the recorded base. Audit compares committed source;
uncommitted edits are reported, and prepare refuses them without discarding anything.
Prepare creates a detached worktree inside ignored `work/upstream-updates/`, applies
the source-only mod delta with three-way matching, and leaves conflicts plus a JSON
report for review. It does not fetch, build, install, promote or publish anything.
Every report remains `compatibility: unverified`, `release_ready: false`, including a
clean patch application. Reapplying to the recorded base tests the tooling only;
it does not prove compatibility with a newer upstream version.

Review conflicts and all integration changes: build pipeline, host/renderer table,
view/jitter hooks, render replay/present, queue synchronization and resource lifetime.
Bump the renderer ABI whenever its host-facing table changes. Build in the candidate's
own private work directory with current dependencies and a fresh lift when guest
hooks or lift inputs change. Never reuse stale receipts or a stable-installation DLL.

Run the source tests, VR tests, ordinary flat gameplay and bounded offline readback.
Then validate the real headset, tracking, image orientation, lifecycle and performance.
Only after those checks update the recorded base/profile and integrate the reviewed
candidate into the development branch, preserving both mod and upstream history.
Retain the previous working commit for rollback. A future automated compatibility
check may detect changes; it cannot guarantee that all future updates will work.

Only authored source, tests and documentation may be published. Candidate binaries,
game files, translated code, saves, captures and private receipts stay local.
