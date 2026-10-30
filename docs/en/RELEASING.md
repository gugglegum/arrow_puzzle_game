# Windows and Linux releases built by GitHub Actions

**English** | [Русский](../ru/RELEASING.md)

The **Release** workflow (`release.yml`) builds Windows x64 on Windows 2022/MSVC
and Linux x86-64 on Ubuntu 22.04/GCC. It validates both platforms and their
provenance before assembling one set of release files. A version-tag push or
an explicitly selected manual tag can prepare a **draft**; publication remains
manual. The workflow never creates/moves tags or publishes a release.

## Development checks before release

The separate **Build and test** workflow (`build.yml`) runs on branch pushes,
pull requests and manual dispatch. It builds Release and runs the six default
CTest suites on Windows/MSVC, Ubuntu 22.04/GCC and Ubuntu 24.04/Clang. It also
checks release assembly guards: mixed commits/versions, dirty metadata,
modified packages/binaries, missing signatures and invalid draft inputs.

Linux rendering/text checks use Mesa/Xvfb. A separate Debug executable
runs menus, themes, no-audio startup and CRLF save/resume capture scenarios.
The normal executable excludes those hooks; both unpacked tar and AppImage are
checked through external X11 input for startup, ignored QA variables, no captures
on F12/Control+F12 and clean exit, with isolated XDG data and a read-only program
directory for ordinary builds; portable checks require adjacent files and no
XDG writes. Both modes run Debug save/resume scenarios. Packaging accepts only
Release. Binary checks validate the baked storage mode and reject capture
hooks/hotkeys in production files. Interactive GPU/input and
audio-device checks still need a desktop. Development artifacts are retained
for 14 days and do not carry the release workflow's provenance attestations.

## Testing the complete release pipeline without a draft

Open **Actions → Release → Run workflow**, select the intended branch and
leave **Create/update a draft release** unchecked. This runs the same builds,
tests, packaging, per-platform provenance verification, common assembly and
checksum attestation as a tagged release. Download `release-<source-sha>` from
the run's artifacts; release artifacts are retained for 30 days. The draft job
is skipped, so no tag, draft or published release is created.

The combined files are assembled on every successful manual run, including
branch builds. This makes the complete delivery path testable before tagging.

## Preparing a release draft

1. Finish changelog, screenshots and documentation; commit and push the intended
   source and workflow. Set `project(... VERSION ...)` to the intended version.
2. With authorization to prepare that version, create/push a new stable
   `vX.Y.Z` tag on that commit. The workflow checks that the tag version matches
   CMake. Existing published tags/assets must not be reused or changed.
3. Open **Actions → Release**. Both platform jobs must succeed before common
   assembly and provenance verification can succeed. Only then can the draft
   job create/update the common Windows/Linux draft as `github-actions[bot]`.
4. Review the draft and the relevant changelog entry. Publish only when the
   maintainer explicitly chooses to do so.

A manual draft run requires selecting an existing matching `vX.Y.Z` tag and
checking **Create/update a draft release**. Selecting a branch with that option
fails validation before the builds. The workflow rechecks that the tag still
points to the built commit before writing assets. Reruns can update a draft;
they refuse to replace assets of an already published release. A failed platform
or verification never produces a partial common draft.

Historical Windows-only releases retain their original assets and workflow
revision. Rerunning an old published tag does not add Linux to that release.
No personal access token or signing secret is required by the release workflow.

## Publishing a verified draft

With the maintainer's request to publish the version, download the combined
artifact and independently check all six packages, ten checksum subjects,
six metadata records and eleven provenance subjects (including the manifest).
Inspect current screenshots, offline documentation links and unpacked startup.
Recheck that the tag still identifies the exact tested source and the release
is still a draft before publishing it as a stable Latest release:

```bash
gh release edit vX.Y.Z --draft=false --prerelease=false --latest
```

Then confirm public unauthenticated access to the page and every asset, the
Latest release API result, and unchanged identities/hashes for older releases.
Record publication evidence in a later documentation commit without moving the
published tag. Replacing an unpublished candidate requires the maintainer's
authorization and a fresh complete build; published tags/assets are immutable.

## Files and provenance

| Common draft asset | Contents / purpose |
| --- | --- |
| `ArrowPuzzle-<version>-windows-x64.zip` | Standalone `ArrowPuzzle.exe`, documentation, screenshots, licenses and internal `build-info.json` |
| `ArrowPuzzle-<version>-linux-x64.tar.gz` | Executable `ArrowPuzzle` with documentation, licenses and internal `build-info.json` |
| `ArrowPuzzle-<version>-linux-x64.AppImage` | The same Linux executable in a single-file container with icon, launcher, runtime notices and metadata |
| `ArrowPuzzle-<version>-windows-x64-portable.zip` | Standalone `ArrowPuzzle-portable.exe` with adjacent settings/save storage |
| `ArrowPuzzle-<version>-linux-x64-portable.tar.gz` | Standalone `ArrowPuzzle-portable` with adjacent settings/save storage |
| `ArrowPuzzle-<version>-linux-x64-portable.AppImage` | Portable Linux executable; data beside the outer AppImage |
| `SHA256SUMS.txt` | SHA-256 for six download packages and four unpacked executables |
| `build-info-windows-x64.json` | Windows source commit, version, executable hash and Actions run |
| `build-info-linux-x64.json` | Linux source/build identity, executable hash and required library symbol versions |
| `build-info-linux-appimage-x64.json` | AppImage hash, input tar hash, executable identity and pinned tool/runtime details |
| `attestation-windows-x64.sigstore.json` | Signed provenance bundle for the Windows EXE and ZIP |
| `attestation-linux-x64.sigstore.json` | Signed provenance bundle for the Linux executable, tar and AppImage |
| `attestation-checksums.sigstore.json` | Signed provenance bundle for the combined checksum manifest |

Platform-specific names prevent metadata and signature bundles from overwriting
each other. The assembly helper checks source commits, package versions, clean
source metadata, package hashes, archive-internal metadata/binaries, Linux
executable permissions and the tar/AppImage relationship before copying files.
`gh attestation verify` separately verifies the supplied cryptographic bundles,
repository, workflow identity, exact source commit and GitHub-hosted runners.
The common manifest is itself attested and verified before the draft job runs.

Each build-info file also has a `-portable.json` counterpart. Metadata records
`portable` and `executable_name`; assembly checks the actual binary's baked
mode too. The Windows bundle covers both ZIPs and both EXEs; the Linux bundle
covers both tar archives, both ELFs and both AppImages. There are four executable
variants and six packages. Ordinary Windows storage is
`%LOCALAPPDATA%\ArrowPuzzle\`; ordinary Linux storage uses XDG. Release 0.5.0
uses this six-package layout: sixteen attached files in total, comprising six
packages, six metadata JSON files, three signature bundles and one manifest.

Each platform uses the same Release build/packaging helpers as local development.
raylib, actions and AppImage tooling/runtime are pinned. Linux uses CMake
3.28.3 and Ubuntu 22.04 as its build baseline; check the package's recorded library requirements.
See [LINUX.md](LINUX.md) for X11/OpenGL prerequisites and startup without FUSE.

## Verifying a download

Use a recent [GitHub CLI](https://cli.github.com/). Replace `<version>` below
with the actual downloaded package version. Linux example:

```bash
gh attestation verify "ArrowPuzzle-<version>-linux-x64.AppImage" \
    --repo gugglegum/arrow_puzzle_game \
    --signer-workflow gugglegum/arrow_puzzle_game/.github/workflows/release.yml \
    --deny-self-hosted-runners
```

The same command verifies the tar.gz or Windows ZIP when passed its filename.
The unpacked `ArrowPuzzle` and `ArrowPuzzle.exe` can also be verified separately.
Add `--source-digest <full-commit-sha>` to require a specific source commit.
To verify the downloaded bundle, also pass
`--bundle attestation-linux-x64.sigstore.json` (or the Windows bundle).

Verify the common checksum manifest's provenance, then check downloaded packages:

```bash
gh attestation verify SHA256SUMS.txt \
    --bundle attestation-checksums.sigstore.json \
    --repo gugglegum/arrow_puzzle_game \
    --signer-workflow gugglegum/arrow_puzzle_game/.github/workflows/release.yml \
    --deny-self-hosted-runners
sha256sum --check --ignore-missing SHA256SUMS.txt
```

Run the checksum command again after unpacking to verify the executable too.
On Windows, `Get-FileHash -Algorithm SHA256` computes the corresponding digests;
`gh attestation verify` accepts Windows paths as well. Older Windows-only
releases use `build-info.json` and `attestation.sigstore.json` outside the ZIP;
their EXE/ZIP attestations remain verifiable with the same workflow identity.

These checks establish file identity and build provenance. They do not assert
independent source auditing or identical bytes from different toolchains.
Windows Authenticode signing and SmartScreen reputation are separate.
See GitHub's [artifact-attestation documentation](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations)
and [verification options](https://cli.github.com/manual/gh_attestation_verify).

## Local preview and packaging

`ARROWPUZZLE_RELEASE_PREVIEW=ON` can display the intended CMake version before a
tag exists. Keep it OFF for normal and Actions builds. A local preview creates
no tags and has no GitHub provenance attestation. For example, from an MSVC x64
shell after setting the intended CMake version:

```powershell
cmake -S . -B build/release-preview -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DARROWPUZZLE_RELEASE_PREVIEW=ON
cmake --build build/release-preview --parallel 2
ctest --test-dir build/release-preview --output-on-failure
.\tools\package-windows.ps1 -Version dev-local `
    -BuildDirectory build/release-preview -OutputDirectory out/release-preview
```

Ordinary local Release packaging uses `tools/package-windows.ps1`,
`tools/package-linux.py` and then `tools/package-appimage.py`; see the Linux
guide for full commands. Packaging refuses to overwrite existing outputs.
Only an Actions run can produce this workflow's GitHub provenance attestations.

Add `-Portable` to the Windows packager or `--portable` to the Linux tar
packager, using a separate output directory for each mode. The AppImage helper
infers the mode from the verified input tar. No runtime marker/flag or filename
test selects the game's storage policy.
