#!/usr/bin/env python3
"""Validate release inputs and assemble Windows/Linux assets without publishing."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = r"[0-9A-Za-z][0-9A-Za-z.+-]*"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def checksums(path):
    result = {}
    for line in path.read_text().splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  ([^/\\\s]+)", line)
        require(match is not None, f"Invalid checksum line in {path.name}")
        digest, name = match.groups()
        require(name not in (".", "..") and name not in result, "Duplicate or invalid checksum filename")
        result[name] = digest
    require(result, f"Empty checksum file: {path}")
    return result


def metadata(path, commit, version):
    value = json.loads(path.read_bytes())
    require(value.get("source_commit") == commit, f"Source commit mismatch: {path.name}")
    require(value.get("package_version") == version, f"Package version mismatch: {path.name}")
    require(value.get("source_dirty") is False, f"Dirty or unidentified source: {path.name}")
    return value


def release_identity(commit, ref, event, create_release, project_version):
    require(re.fullmatch(r"[0-9a-f]{40}", commit) is not None, "Invalid source commit")
    require(event in ("push", "workflow_dispatch"), "Unsupported release event")
    match = re.fullmatch(r"refs/tags/(v([0-9]+\.[0-9]+\.[0-9]+))", ref)
    tag = match.group(1) if match else ""
    release = event == "push" or create_release
    require(not release or tag, "A draft release requires an existing vX.Y.Z tag")
    if tag:
        require(match.group(2) == project_version, "The tag does not match the CMake project version")
    return {"commit": commit, "tag": tag, "version": tag[1:] if tag else "dev-" + commit[:12],
            "create_release": str(bool(release)).lower()}


def prepare():
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    require(commit == os.environ["GITHUB_SHA"], "Unexpected source checkout")
    project = re.search(r"project\(ArrowPuzzle VERSION ([0-9]+\.[0-9]+\.[0-9]+)",
                        (ROOT / "CMakeLists.txt").read_text())
    require(project is not None, "Cannot determine the CMake project version")
    identity = release_identity(commit, os.environ["GITHUB_REF"], os.environ["GITHUB_EVENT_NAME"],
                                os.environ.get("MANUAL_RELEASE") == "true", project.group(1))
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        for key, value in identity.items():
            output.write(f"{key}={value}\n")
    print(f"Validated {identity['version']} from {commit}; create draft: {identity['create_release']}")


def assemble(windows, linux, output, commit, version):
    require(re.fullmatch(r"[0-9a-f]{40}", commit) is not None, "Invalid source commit")
    require(re.fullmatch(VERSION, version) is not None, "Invalid package version")
    require(not output.exists(), "Refusing to overwrite an existing assembled release")
    win_name = f"ArrowPuzzle-{version}-windows-x64.zip"
    tar_name = f"ArrowPuzzle-{version}-linux-x64.tar.gz"
    image_name = f"ArrowPuzzle-{version}-linux-x64.AppImage"
    win_meta = metadata(windows / "build-info-windows-x64.json", commit, version)
    linux_meta = metadata(linux / "build-info-linux-x64.json", commit, version)
    image_meta = metadata(linux / "build-info-linux-appimage-x64.json", commit, version)
    require(linux_meta.get("architecture") == "x86_64", "Unexpected Linux architecture")
    require(image_meta.get("executable_sha256") == linux_meta.get("executable_sha256"),
            "The AppImage and tar have different executables")
    win_sums = checksums(windows / "SHA256SUMS-windows-x64.txt")
    linux_sums = checksums(linux / "SHA256SUMS-linux-x64.txt")
    require(set(win_sums) == {"ArrowPuzzle.exe", win_name}, "Unexpected Windows checksum subjects")
    require(set(linux_sums) == {"ArrowPuzzle", tar_name, image_name}, "Unexpected Linux checksum subjects")
    for directory, name, sums in ((windows, win_name, win_sums), (linux, tar_name, linux_sums),
                                  (linux, image_name, linux_sums)):
        require(sha256((directory / name).read_bytes()) == sums[name], f"Checksum mismatch: {name}")
    require(image_meta.get("source_archive_sha256") == linux_sums[tar_name], "AppImage used a different tar archive")
    require(image_meta.get("appimage_sha256") == linux_sums[image_name], "AppImage metadata checksum mismatch")
    with zipfile.ZipFile(windows / win_name) as archive:
        require(archive.namelist().count("ArrowPuzzle.exe") == 1, "Missing or duplicate Windows executable")
        require(archive.namelist().count("build-info.json") == 1, "Missing or duplicate Windows build metadata")
        win_exe = archive.read("ArrowPuzzle.exe")
        require(json.loads(archive.read("build-info.json")) == win_meta, "Windows inner/outer metadata mismatch")
    tar_root = f"ArrowPuzzle-{version}-linux-x64"
    with tarfile.open(linux / tar_name, "r:gz") as archive:
        names = archive.getnames()
        require(names.count(tar_root + "/ArrowPuzzle") == 1, "Missing or duplicate Linux executable")
        require(names.count(tar_root + "/build-info.json") == 1, "Missing or duplicate Linux build metadata")
        binary = archive.getmember(tar_root + "/ArrowPuzzle")
        require(binary.isfile() and binary.mode & 0o111, "Linux executable is not an executable regular file")
        linux_exe = archive.extractfile(binary).read()
        require(json.loads(archive.extractfile(tar_root + "/build-info.json").read()) == linux_meta,
                "Linux inner/outer metadata mismatch")
    require(sha256(win_exe) == win_sums["ArrowPuzzle.exe"] == win_meta.get("executable_sha256"),
            "Windows executable checksum mismatch")
    require(sha256(linux_exe) == linux_sums["ArrowPuzzle"] == linux_meta.get("executable_sha256"),
            "Linux executable checksum mismatch")
    # Crypto verification is performed by gh in the workflow before this step.
    copies = [(windows, win_name), (linux, tar_name), (linux, image_name),
              (windows, "build-info-windows-x64.json"), (linux, "build-info-linux-x64.json"),
              (linux, "build-info-linux-appimage-x64.json"),
              (windows, "attestation-windows-x64.sigstore.json"), (linux, "attestation-linux-x64.sigstore.json")]
    for directory, name in copies:
        require((directory / name).is_file(), f"Missing release asset: {name}")
    output.mkdir(parents=True)
    for directory, name in copies:
        shutil.copy2(directory / name, output / name)
    combined = win_sums | linux_sums
    (output / "SHA256SUMS.txt").write_text("".join(f"{digest}  {name}\n" for name, digest in sorted(combined.items())))
    print(f"Assembled {len(copies)} assets and {len(combined)} SHA-256 subjects from {commit}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("prepare")
    package = commands.add_parser("assemble")
    package.add_argument("--windows-dir", type=Path, required=True)
    package.add_argument("--linux-dir", type=Path, required=True)
    package.add_argument("--output-dir", type=Path, required=True)
    package.add_argument("--commit", required=True)
    package.add_argument("--version", required=True)
    args = parser.parse_args()
    if args.command == "prepare":
        prepare()
    else:
        assemble(args.windows_dir, args.linux_dir, args.output_dir, args.commit, args.version)


if __name__ == "__main__":
    main()
