# Recorded release evidence

For the release procedure, version labels, preview builds, packaging, and
provenance policy, use [docs/en/RELEASING.md](../docs/en/RELEASING.md) and the actual
[version script](../cmake/GenerateBuildVersion.cmake). This page stores evidence
from completed releases, not a second procedure or a claim about a future build.

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
