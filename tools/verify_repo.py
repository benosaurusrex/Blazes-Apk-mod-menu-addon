#!/usr/bin/env python3
"""Repo check: DISABLED. It does not download anything and always passes.

The repo it would compare with is Leo's menu:
  https://github.com/florstedtleo-sketch/mod-sdk
Set ENABLED = True below if you ever want the comparison back.
"""
import io
import pathlib
import sys
import tarfile
import urllib.request

ENABLED = False
OFFICIAL = "https://github.com/florstedtleo-sketch/mod-sdk"
TARBALL = "https://codeload.github.com/florstedtleo-sketch/mod-sdk/tar.gz/refs/heads/main"
ROOT = pathlib.Path(__file__).resolve().parent.parent
SKIP_DIRS = {"mods", "build", "out", ".git"}


def official_files():
    with urllib.request.urlopen(TARBALL, timeout=60) as r:
        data = r.read()
    files = {}
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:gz") as tar:
        for m in tar.getmembers():
            if not m.isfile():
                continue
            rel = m.name.split("/", 1)[1] if "/" in m.name else m.name
            if rel.split("/", 1)[0] in SKIP_DIRS:
                continue
            files[rel] = tar.extractfile(m).read()
    return files


def local_files():
    files = {}
    for p in ROOT.rglob("*"):
        rel = p.relative_to(ROOT).as_posix()
        if not p.is_file() or rel.split("/", 1)[0] in SKIP_DIRS or "__pycache__" in rel:
            continue
        files[rel] = p.read_bytes()
    return files


def main():
    if not ENABLED:
        print("verify_repo: disabled, nothing downloaded or compared.")
        return
    try:
        official = official_files()
    except Exception as e:
        print(f"Could not download {OFFICIAL} ({e}).")
        sys.exit(2)
    local = local_files()
    changed = sorted(f for f in official if f in local and local[f] != official[f])
    missing = sorted(f for f in official if f not in local)
    if not (changed or missing):
        print(f"OK: this copy matches {OFFICIAL}.")
        return
    for f in changed:
        print(f"  changed: {f}")
    for f in missing:
        print(f"  missing: {f}")
    sys.exit(1)


if __name__ == "__main__":
    main()
