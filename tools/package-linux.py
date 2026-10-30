#!/usr/bin/env python3
"""Package a fresh Linux Release build without publishing a release."""

import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
import tempfile

from build_features import check_production

ROOT = Path(__file__).resolve().parents[1]


def command(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()


def digest(path):
    with path.open("rb") as file:
        return hashlib.file_digest(file, "sha256").hexdigest() if hasattr(hashlib, "file_digest") else hashlib.sha256(file.read()).hexdigest()


def elf_requirements(executable):
    dynamic = command("readelf", "--wide", "--dynamic", str(executable))
    if re.search(r"\((?:RPATH|RUNPATH)\)", dynamic):
        raise RuntimeError("The installed executable still has an RPATH/RUNPATH")
    dependencies = sorted(re.findall(r"\(NEEDED\).*?\[(.*?)\]", dynamic))
    linked = command("ldd", str(executable))
    if "not found" in linked:
        raise RuntimeError("A runtime library is missing:\n" + linked)
    symbols = command("objdump", "--dynamic-syms", str(executable))
    versions = {}
    for family in ("GLIBC", "GLIBCXX", "CXXABI", "GCC"):
        found = re.findall(r"\b" + family + r"_([0-9]+(?:\.[0-9]+)+)\b", symbols)
        if found:
            versions[family] = max(set(found), key=lambda value: tuple(map(int, value.split("."))))
    return dependencies, versions


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/linux")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "out/linux-package")
    parser.add_argument("--portable", action="store_true", help="Package the portable target")
    args = parser.parse_args()
    if platform.system() != "Linux" or platform.machine() not in ("x86_64", "amd64"):
        parser.error("This package targets native Linux x86-64")
    if not re.fullmatch(r"[0-9A-Za-z][0-9A-Za-z.+-]*", args.version):
        parser.error("Version must contain only letters, numbers, dots, + and -")
    build = args.build_dir.resolve()
    output = args.output_dir.resolve()
    cache = (build / "CMakeCache.txt").read_text()
    if not re.search(r"^CMAKE_BUILD_TYPE:STRING=Release$", cache, re.MULTILINE):
        parser.error("Packaging requires a configured Release build")
    suffix = "-portable" if args.portable else ""
    target = "arrow_puzzle_portable" if args.portable else "arrow_puzzle"
    executable_name = "ArrowPuzzle" + suffix
    package_name = f"ArrowPuzzle-{args.version}-linux-x64{suffix}"
    archive = output / (package_name + ".tar.gz")
    for name in (archive.name, "build-info.json", "SHA256SUMS.txt"):
        if (output / name).exists():
            parser.error(f"Refusing to overwrite {output / name}; choose a new output directory")
    # Refresh the binary and Git-derived version, including after a commit.
    subprocess.run(["cmake", "--build", str(build), "--target", target,
                    "--parallel", os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL", "2")], check=True)
    output.mkdir(parents=True, exist_ok=True)
    commit = command("git", "rev-parse", "HEAD")
    epoch = int(command("git", "show", "-s", "--format=%ct", "HEAD"))
    with tempfile.TemporaryDirectory(prefix="package-", dir=build) as temp:
        stage = Path(temp) / package_name
        subprocess.run(["cmake", "--install", str(build), "--prefix", str(stage),
                        "--component", "PortableRuntime" if args.portable else "Runtime", "--strip"], check=True)
        executable = stage / executable_name
        (stage / target).rename(executable)
        check_production(executable.read_bytes(), args.portable)
        header = executable.read_bytes()[:20]
        if header[:6] != b"\x7fELF\x02\x01" or int.from_bytes(header[18:20], "little") != 62:
            raise RuntimeError("The built executable is not x86-64 ELF")
        executable.chmod(0o755)
        dependencies, versions = elf_requirements(executable)
        for filename in ("README.md", "README_RU.md", "CHANGELOG.md", "LICENSE", "THIRD_PARTY_NOTICES.md"):
            shutil.copy2(ROOT / filename, stage / filename)
        shutil.copytree(ROOT / "docs", stage / "docs")
        font_licenses = stage / "assets/fonts"
        font_licenses.mkdir(parents=True)
        shutil.copy2(ROOT / "assets/fonts/Inter-LICENSE.txt", font_licenses)
        version_header = (build / "generated/BuildVersion.hpp").read_text()
        version_label = re.search(r'kVersionLabel\[\] = "([^"]+)"', version_header).group(1)
        compiler = re.search(r"^CMAKE_CXX_COMPILER:[^=]+=(.+)$", cache, re.MULTILINE).group(1)
        run_url = None
        if os.environ.get("GITHUB_ACTIONS") == "true":
            run_url = "{GITHUB_SERVER_URL}/{GITHUB_REPOSITORY}/actions/runs/{GITHUB_RUN_ID}".format(**os.environ)
        metadata = {
            "package_version": args.version,
            "portable": args.portable,
            "executable_name": executable_name,
            "executable_version": version_label,
            "source_commit": commit,
            "source_ref": os.environ.get("GITHUB_REF"),
            "source_dirty": bool(command("git", "status", "--porcelain")),
            "build_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "workflow_run": run_url,
            "architecture": "x86_64",
            "display_backend": "X11 (XWayland compatible; native Wayland disabled)",
            "compiler": command(compiler, "--version").splitlines()[0],
            "cmake": command("cmake", "--version").splitlines()[0],
            "build_os": platform.freedesktop_os_release().get("PRETTY_NAME", "Linux"),
            "raylib_commit": re.search(r"GIT_TAG ([0-9a-f]{40})", (ROOT / "CMakeLists.txt").read_text()).group(1),
            "needed_libraries": dependencies,
            "required_symbol_versions": versions,
            "runtime_notes": "Also requires host OpenGL 3.3 drivers and X11 libraries dynamically loaded by GLFW; audio is optional.",
            "executable_sha256": digest(executable),
        }
        metadata_text = json.dumps(metadata, indent=2, ensure_ascii=False) + "\n"
        (stage / "build-info.json").write_text(metadata_text)

        def normalize(info):
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            info.mtime = epoch
            info.mode = 0o755 if info.isdir() or info.name.endswith("/" + executable_name) else 0o644
            return info

        # Stable archive ordering, ownership and gzip headers. Toolchains and
        # build timestamps can still differ; this is not a bit-reproducible claim.
        with archive.open("xb") as file, gzip.GzipFile(filename="", mode="wb", fileobj=file, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as tar:
                tar.add(stage, arcname=package_name, filter=normalize)
        (output / "build-info.json").write_text(metadata_text)
        (output / "SHA256SUMS.txt").write_text(
            f"{metadata['executable_sha256']}  {executable_name}\n{digest(archive)}  {archive.name}\n")
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as file:
            file.write(f"archive_path={archive}\npackage_name={package_name}\n")
    print(f"Packaged {archive} from {commit}")
    print(f"Required symbol versions: {versions}")


if __name__ == "__main__":
    main()
