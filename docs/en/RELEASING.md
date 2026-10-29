# Windows releases built by GitHub Actions

**English** | [Русский](../ru/RELEASING.md)

The **Windows release** workflow builds the executable on a GitHub-hosted
Windows 2022 runner. It runs only after a `vX.Y.Z` tag is pushed or when an
owner starts it with **Run workflow**. Branch pushes and pull requests use the
development workflow described below.

## Development checks before release

The separate **Build and test** workflow (`build.yml`) runs on branch pushes,
pull requests and manual dispatch. It builds Release and runs all five default
suites on Windows 2022/MSVC, Ubuntu 22.04/GCC and Ubuntu 24.04/Clang. Linux also
runs rendering/text checks with Mesa/Xvfb and launches the unpacked tar archive
with isolated XDG data, a read-only program directory, menus, both themes and
CRLF save/resume checks. The audio mixer still needs an explicit local device test.

Ubuntu 22.04 also builds an AppImage after tar validation and checks its
FUSE-free extraction path. Each successful job uploads development packages
(ZIP, tar.gz and, on Ubuntu 22.04, AppImage) with hashes and build
metadata for 14 days. These files identify the tested commit/run; they are not
release-provenance-attested assets. Test logs and Linux captures are uploaded
on failures too. This workflow has read-only repository permissions and does
not create tags, drafts or published releases. Download artifacts from its
Actions run; Linux packaging details are in [LINUX.md](LINUX.md).

## Previewing a release before tagging

Use a separate Release build directory when preparing screenshots before a
release tag exists. From an MSVC x64 developer shell:

```powershell
cmake -S . -B build/release-preview -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DARROWPUZZLE_RELEASE_PREVIEW=ON
cmake --build build/release-preview --parallel 2
ctest --test-dir build/release-preview --output-on-failure
.\tools\package-windows.ps1 -Version 0.4.0 `
    -BuildDirectory build/release-preview -OutputDirectory out/release-preview
```

`ARROWPUZZLE_RELEASE_PREVIEW` displays the CMake project version (for example,
`v0.4.0`) without creating or moving Git tags. It is off by default, so ordinary
builds still identify themselves using the last release tag and commit count.
GitHub Actions does not enable it: a tagged Actions build gets its version from
the tag. A local preview has no GitHub provenance attestation; its
`build-info.json` records that it was built locally.

Finish README changes and add release screenshots **before** creating the tag.
Set the release date on the corresponding changelog entry at that final step,
then use its contents for the GitHub release description.

## Preparing a release

1. Finish the changelog and documentation, then commit and push the intended
   release source, including the workflow. Check that the tag version matches
   `project(... VERSION ...)` in `CMakeLists.txt`.
2. Create and push a stable release tag such as `v0.4.0` on that commit.
3. Open **Actions → Windows release** and wait for the build and tests.
4. Review the generated draft under **Releases**, add the corresponding
   changelog entry to its build-provenance notes, and publish it.

Tag builds create a draft using `github-actions[bot]`. Repeating a build may
update an existing draft; it never replaces assets of an already published
release. No personal access token or signing secret is required by the workflow.

To build manually, select a branch or tag in **Run workflow**. Leave
**Create/update a draft release** unchecked for a build with downloadable
Actions artifacts only. Select an existing `vX.Y.Z` tag and check that option
to prepare its release draft. Tags must already exist; the workflow never
creates or moves a tag.

## Files and checks

- `ArrowPuzzle-<version>-windows-x64.zip` contains the standalone executable,
  README files, changelog, developer documentation and screenshots, license
  notices and `build-info.json`. Relative README links work after extraction.
- `SHA256SUMS.txt` identifies both the ZIP and its unpacked `ArrowPuzzle.exe`.
- `build-info.json` records the source commit, visible version, executable hash
  and Actions run URL.
- `attestation.sigstore.json` is the signed build-provenance bundle for both
  the executable and the ZIP.

The job uses the same Release build helper as local development, runs the full
headless CTest suite and packages the just-built executable. Interactive GPU
and playback-device tests remain explicit local checks. raylib is pinned to
the exact 5.5 commit; actions are pinned to full commit hashes.

## Verifying a download

With a recent [GitHub CLI](https://cli.github.com/), verify the ZIP before
extracting it (replace the filename with the downloaded version):

```powershell
gh attestation verify .\ArrowPuzzle-0.4.0-windows-x64.zip `
    --repo gugglegum/arrow_puzzle_game `
    --signer-workflow gugglegum/arrow_puzzle_game/.github/workflows/release.yml `
    --deny-self-hosted-runners
```

The unpacked binary can be checked independently:

```powershell
gh attestation verify .\ArrowPuzzle.exe `
    --repo gugglegum/arrow_puzzle_game `
    --signer-workflow gugglegum/arrow_puzzle_game/.github/workflows/release.yml `
    --deny-self-hosted-runners
```

Add `--source-digest <full-commit-sha>` to require a specific source commit.
The checks validate the file's digest and the identity of the GitHub workflow
that built it. This establishes build provenance; it does not claim that all
source code has been independently audited or that different compiler versions
produce byte-identical executables. This is also separate from Windows
Authenticode signing and SmartScreen reputation.

See GitHub's [artifact-attestation documentation](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations).

## Local packaging

After a successful Release build, the same packaging step is available locally:

```powershell
.\tools\package-windows.ps1 -Version dev-local
```

It writes to `out/release`, preserves build inputs and refuses to overwrite
an existing ZIP. A local package has hashes and build metadata, but only an
Actions run can create the workflow's GitHub provenance attestation.
