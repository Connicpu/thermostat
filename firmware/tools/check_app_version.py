#!/usr/bin/env python3
"""Fail unless each image's esp_app_desc version is a firmware version.

Accepted forms:
  X.Y.Z-g<hash>       hash is at least 7 hex digits
  X.Y.Z-N-g<hash>
  X.Y.Z               only when HEAD is exactly the tag fw-vX.Y.Z

card-, -dirty, and -nogit are rejected. Prints the boot-log line for each image.
"""
import os
import pathlib
import re
import struct
import subprocess
import sys

FW = pathlib.Path(__file__).resolve().parents[1]
HASHED = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+-g[0-9a-f]{7,}$")
AHEAD = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+-[0-9]+-g[0-9a-f]{7,}$")
BARE = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+$")


def app_version(path):
    data = pathlib.Path(path).read_bytes()
    magic = struct.pack("<I", 0xABCD5432)
    i = data.find(magic)
    if i < 0 or i + 80 > len(data):
        sys.exit(f"::error::esp_app_desc not found in {path}")
    ver = data[i + 16 : i + 48].split(b"\x00", 1)[0].decode("utf-8", "replace")
    name = data[i + 48 : i + 80].split(b"\x00", 1)[0].decode("utf-8", "replace")
    return ver, name


def exact_fw_tag():
    """Stripped fw-v* tag if HEAD is exactly that tag, else None."""
    try:
        out = subprocess.run(
            ["git", "describe", "--tags", "--exact-match", "--match", "fw-v*"],
            cwd=FW,
            check=False,
            capture_output=True,
            text=True,
        )
    except OSError as exc:
        print(f"::error::git describe failed: {exc}", file=sys.stderr)
        return None
    if out.returncode != 0:
        return None
    tag = out.stdout.strip()
    if not tag.startswith("fw-v"):
        return None
    return tag[len("fw-v") :]


def accept(ver, on_tag):
    if "card-" in ver or "-dirty" in ver or ver.endswith("-nogit") or "-nogit" in ver:
        return False
    if HASHED.fullmatch(ver) or AHEAD.fullmatch(ver):
        return True
    if BARE.fullmatch(ver) and on_tag == ver:
        return True
    return False


def main(argv):
    if len(argv) < 2:
        sys.exit("usage: check_app_version.py IMAGE.bin [IMAGE.bin ...]")
    on_tag = exact_fw_tag()
    failed = False
    for path in argv[1:]:
        ver, name = app_version(path)
        print(f"App version: {ver} ({path})")
        if name != "thermostat_v3":
            print(f"::error::unexpected project name {name!r} in {path}", file=sys.stderr)
            failed = True
            continue
        if not accept(ver, on_tag):
            print(
                f"::error::App version {ver!r} in {path} is not X.Y.Z-g<hash>, "
                f"X.Y.Z-N-g<hash>, or a bare X.Y.Z on tag fw-v{ver}",
                file=sys.stderr,
            )
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
