# Recorded release evidence

For the release procedure, version labels, preview builds, packaging, and
provenance policy, use [docs/en/RELEASING.md](../docs/en/RELEASING.md) and the actual
[version script](../cmake/GenerateBuildVersion.cmake). This page stores evidence
from completed releases, not a second procedure or a claim about a future build.

## 0.5.0 draft (2026-10-30)

- The user authorized a common Windows/Linux draft and file/provenance
  verification, explicitly excluding publication. The draft exists as
  **Arrow Puzzle v0.5.0**, created by `github-actions[bot]`, release ID
  `428459388`. The authenticated release page still shows **Draft** after
  saving its release notes. No publication was performed.
- Current [draft link](https://github.com/gugglegum/arrow_puzzle_game/releases/tag/untagged-50194b5b61b957f06641).
  GitHub regenerated its temporary `untagged-*` URL when the notes were saved;
  use the repository Releases page if another save makes this link stale.
- Tag/source commit: `fe0f6973745a2d27a298865c6c709b7b4060e8c8`.
  Annotated v0.5.0 tag object: `44caae05b69507fa873e5510a9f0ce7c01b3816e`.
  CMake/package version is `0.5.0`; both executable labels are `v0.5.0`.
- [Release run 40965820527](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40965820527)
  succeeded in all five jobs, including common draft creation. Windows and
  Linux full headless suites passed, followed by Linux rendering/text, tar
  and FUSE-free AppImage smoke checks and attestation verification.
  [Development run 40965794922](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40965794922)
  also passed Windows/MSVC, Ubuntu 22.04/GCC and Ubuntu 24.04/Clang.
- Ten attached files were verified: the Windows ZIP, Linux tar.gz/AppImage,
  SHA256SUMS.txt, three platform-specific build-info JSON files and the Windows,
  Linux and checksum Sigstore bundles. GitHub additionally lists two automatic
  source archives, so the page's Assets counter is 12.
- The downloaded common Actions artifact (ID `12937529369`) has SHA-256
  `26324bc62102b05c30dc109e522572007ddccdcb8ee1a9a80a3f2aacd4013789`.
  All ten contained file hashes match the authenticated draft page's attached
  asset digests, including after the release notes were saved. This establishes
  that the verified workflow files are the draft's exact bytes.
- All five SHA256SUMS subjects passed: three packages and both unpacked
  executables. ZIP/tar internal metadata match the attached JSON; all metadata
  records the clean source commit, `refs/tags/v0.5.0`, version and run above.
  The AppImage records the same Linux binary and input tar archive.
- Independent `gh attestation verify --bundle` checks passed for the Windows
  ZIP/EXE, Linux tar/executable/AppImage and common checksum manifest. Each
  required the exact source commit, tag ref, repository, release.yml identity
  and GitHub-hosted runners. No Authenticode signing is implied.
- The user downloaded `ArrowPuzzle-0.5.0-linux-x64.AppImage` into Downloads.
  That actual download was separately hashed and compared byte-for-byte with
  the verified artifact. SHA-256:
  `bb2b360dba868bf5e9d423248b7175ebc2e669e153b5adfc27122e6ebd4602ab`.
  Linux tar SHA-256:
  `2e809588a07affed50ec3a9d71789111d8503146a3e73155a83a9e937474c310`.
  Windows ZIP SHA-256:
  `832431a0b89e44d71511b2d5489bccb4080916245faadc749ca205e884b85dfa`.
- The four published releases retain their original IDs, publication state,
  asset IDs, names, sizes and digests. Latest remains published 0.4.0.
  Evidence, inventory, signatures and verification results are ignored under
  `out/release-validation-v0.5.0`. The GitHub connector cannot see this draft;
  its presence/status/asset digests were checked in the authenticated browser.

## 0.4.0

- Published release: [v0.4.0](https://github.com/gugglegum/arrow_puzzle_game/releases/tag/v0.4.0),
  published at `2026-10-28T22:05:12Z` (2026-10-29 in Asia/Yekaterinburg).
- Tag/source commit: `b785f2303048fa53b3aa5d69ada04cde548e0c3e`.
  The annotated tag object is `bd7ee416a3805be890ad882c2c0fa31e28553d4d`.
- Author: `github-actions[bot]`; public, not a prerelease, and marked Latest
  when verified. Release ID: `427237595`.
- [Successful Actions run](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40776144507),
  triggered by the tag push. Both build and draft-release jobs succeeded.
- ZIP: `ArrowPuzzle-0.4.0-windows-x64.zip` (2,035,360 bytes).
- ZIP SHA-256: `feb92c8790072878e7570feee25264edbf273bf2ac36cf03fcdc07c97c3bd967`.
- EXE SHA-256: `fe52e4ef1424c144823d1046f21fa5c2be7083b90c227a942188716828415b57`.
- Assets also include SHA256SUMS.txt, build-info.json, attestation.sigstore.json.
  Attached and packaged metadata match: version `v0.4.0`, exact tag/commit,
  clean source tree, and the Actions run above.
- All four default test suites passed in both the clean local Release preview
  and Actions (56.78 seconds total in Actions). The local generator suite
  reported 1,333 boards; broader generator measurements are recorded in
  [generator evidence](generator-0.4.md).
- Downloaded ZIP verified: 26 expected files, seven screenshots, offline links,
  matching tagged documentation/assets, and both SHA-256 sums. Text comparison
  allowed only LF/CRLF conversion from Windows checkout; the local LICENSE had
  LF while the Actions package had CRLF, with identical content.
- Independent verification of both downloaded ZIP and EXE enforced the source
  commit, `refs/tags/v0.4.0`, `.github/workflows/release.yml`, and GitHub-hosted
  runners. Both attestation checks passed before publication.
- The downloaded EXE started successfully from a fresh extraction. The main
  menu displayed `v0.4.0`; Very Hard seed 747 generated 3,445 arrows and rendered
  successfully. Both screenshots were visually reviewed.
- After publication, the release page and ZIP returned HTTP 200 without login;
  GitHub confirmed Latest and retained all four verified assets.

This release includes the accepted frontier generator, the 1.2-second fatal
collision pause, bilingual developer guides, and refreshed release screenshots.
Subsequent release-evidence documentation commits do not move the tag or replace
the published package. Recheck live GitHub state when answering about Latest.

## 0.3.0

- Published release: [v0.3.0](https://github.com/gugglegum/arrow_puzzle_game/releases/tag/v0.3.0).
- Tag/source commit: `52182f1cdb9d93749a45b9099c17cca20b92773c`.
- Author: `github-actions[bot]`; it was published and marked Latest at the time.
- [Successful Actions run](https://github.com/gugglegum/arrow_puzzle_game/actions/runs/40753825791).
- ZIP: `ArrowPuzzle-0.3.0-windows-x64.zip`.
- ZIP SHA-256: `b7673fc47037e2630704bc61d7e5c679f12f7041d2d879ea48419f9c936ba0c3`.
- EXE SHA-256: `45769e7018be9672a4aa45e7c92700997ea34243b17ff2023a8b20e22304044d`.
- Assets also include SHA256SUMS.txt, build-info.json, attestation.sigstore.json.
- All four default test suites passed in Actions. Downloaded EXE startup,
  19 ZIP files, offline links, checksums, and both EXE/ZIP provenance policies
  were checked locally. Public page and ZIP returned HTTP 200 without login.

The attestations establish source/workflow provenance, not Windows Authenticode
signing or byte-identical output across compiler installations. Never replace
these published assets or move the tag to retrofit a fix.

The subsequent delayed Game Over fix in `8ebae99` is not in this release. Read
[status and decisions](status-and-decisions.md) and Git/CHANGELOG for changes
after the tag. Recheck live GitHub state before answering questions about the
latest release or using a downloaded artifact; historical evidence is not a
fresh verification.
