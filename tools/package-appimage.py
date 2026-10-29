#!/usr/bin/env python3
"""Wrap a verified Linux tar.gz in an AppImage, without FUSE or publication."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
TOOL_VERSION = "1.9.1"
TOOL_URL = f"https://github.com/AppImage/appimagetool/releases/download/{TOOL_VERSION}/appimagetool-x86_64.AppImage"
TOOL_SHA256 = "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True, stderr=subprocess.STDOUT).strip()


def appimage_tool():
    cache = ROOT / "build/appimage-tools" / TOOL_VERSION
    cache.mkdir(parents=True, exist_ok=True)
    tool = cache / "appimagetool-x86_64.AppImage"
    if not tool.exists():
        with tempfile.NamedTemporaryFile(dir=cache, delete=False) as file:
            download = Path(file.name)
        try:
            subprocess.run(["curl", "--fail", "--location", "--silent", "--show-error",
                            "--retry", "3", TOOL_URL, "--output", str(download)], check=True)
            if digest(download) != TOOL_SHA256:
                raise RuntimeError("appimagetool checksum mismatch")
            download.replace(tool)
        finally:
            download.unlink(missing_ok=True)
    if digest(tool) != TOOL_SHA256:
        raise RuntimeError(f"Cached appimagetool checksum mismatch: {tool}")
    tool.chmod(0o755)
    # Reuse the exact runtime shipped in the checksum-pinned tool. Otherwise
    # appimagetool downloads a moving 'continuous' runtime during every build.
    offset = int(command(str(tool), "--appimage-offset"))
    runtime = cache / "runtime-x86_64"
    with tool.open("rb") as file:
        runtime.write_bytes(file.read(offset))
    runtime_version = command(str(tool), "--appimage-version")
    return tool, runtime, runtime_version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() not in ("x86_64", "amd64"):
        parser.error("This packaging helper targets native Linux x86-64")
    archive = args.archive.resolve()
    output = (args.output_dir or archive.parent).resolve()
    tool, runtime, runtime_version = appimage_tool()
    with tempfile.TemporaryDirectory(prefix="appimage-", dir=ROOT / "build") as temp:
        work = Path(temp)
        source = work / "source"
        with tarfile.open(archive, "r:gz") as tar:
            members = tar.getmembers()
            for member in members:
                path = PurePosixPath(member.name)
                if path.is_absolute() or ".." in path.parts or not (member.isfile() or member.isdir()):
                    raise RuntimeError("The source archive contains an unsafe path or link")
            tar.extractall(source)
        candidates = list(source.glob("*/build-info.json"))
        if len(candidates) != 1:
            raise RuntimeError("Expected one packaged game with build-info.json")
        stage = candidates[0].parent
        metadata = json.loads(candidates[0].read_text())
        if metadata["source_commit"] != command("git", "rev-parse", "HEAD"):
            raise RuntimeError("Rebuild the tar archive from the current source commit first")
        if digest(stage / "ArrowPuzzle") != metadata["executable_sha256"]:
            raise RuntimeError("The archived executable does not match its build metadata")
        version = metadata["package_version"]
        if not version or any(character not in "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.+-" for character in version):
            raise RuntimeError("Invalid package version")
        destination = output / f"ArrowPuzzle-{version}-linux-x64.AppImage"
        if destination.exists() or destination.with_suffix(".AppImage.build-info.json").exists():
            raise RuntimeError(f"Refusing to overwrite {destination}")
        appdir = work / "ArrowPuzzle.AppDir"
        appdir.mkdir()
        binary_dir = appdir / "usr/bin"
        binary_dir.mkdir(parents=True)
        (stage / "ArrowPuzzle").rename(binary_dir / "ArrowPuzzle")
        content = appdir / "usr/share/arrow-puzzle"
        content.parent.mkdir(parents=True, exist_ok=True)
        stage.rename(content)
        shutil.copy2(ROOT / "assets/icons/arrow-puzzle-256.png", appdir / "arrow-puzzle.png")
        shutil.copy2(ROOT / "tools/appimage/arrow-puzzle.desktop", appdir)
        shutil.copy2(ROOT / "tools/appimage/AppRun", appdir)
        (appdir / "AppRun").chmod(0o755)
        shutil.copy2(ROOT / "tools/appimage/RUNTIME-NOTICES.txt", content)
        build_info = metadata | {
            "format": "AppImage type 2",
            "source_archive_sha256": digest(archive),
            "appimagetool_version": TOOL_VERSION,
            "appimagetool_url": TOOL_URL,
            "appimagetool_sha256": TOOL_SHA256,
            "runtime_version": runtime_version,
            "runtime_sha256": digest(runtime),
            "bundled_game_shared_libraries": [],
        }
        (content / "appimage-build-info.json").write_text(json.dumps(build_info, indent=2) + "\n")
        output.mkdir(parents=True, exist_ok=True)
        env = os.environ | {"ARCH": "x86_64", "VERSION": version,
                            "SOURCE_DATE_EPOCH": command("git", "show", "-s", "--format=%ct", "HEAD")}
        subprocess.run([str(tool), "--appimage-extract-and-run", "--no-appstream",
                        "--runtime-file", str(runtime), "--mksquashfs-opt=-processors",
                        "--mksquashfs-opt=2", str(appdir), str(destination)], env=env, check=True)
        destination.chmod(0o755)
        build_info["appimage_sha256"] = digest(destination)
        destination.with_suffix(".AppImage.build-info.json").write_text(json.dumps(build_info, indent=2) + "\n")
        with (output / "SHA256SUMS.txt").open("a") as file:
            file.write(f"{build_info['appimage_sha256']}  {destination.name}\n")
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as file:
            file.write(f"appimage_path={destination}\n")
    print(f"Packaged {destination}")


if __name__ == "__main__":
    main()
