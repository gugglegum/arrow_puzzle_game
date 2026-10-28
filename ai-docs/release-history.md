# Recorded release evidence

For the release procedure, version labels, preview builds, packaging, and
provenance policy, use [docs/en/RELEASING.md](../docs/en/RELEASING.md) and the actual
[version script](../cmake/GenerateBuildVersion.cmake). This page stores evidence
from completed releases, not a second procedure or a claim about a future build.

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
