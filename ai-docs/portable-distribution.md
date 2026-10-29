# Portable distribution and AppImage data locations

Discussion recorded on 2026-10-30 after pulling `master` from `83a5ddd` to
`3fd31be`. The user is considering explicitly labelled portable packages whose
persistent game files stay in one chosen directory. This is a preference and
design proposal, not an instruction to change storage behavior or release assets
yet. No portable mode was implemented in this discussion.

## Current implementation, checked after the Linux port

- `Game::run()` constructs the default `Persistence` object.
- `defaultDirectory()` in `Persistence.cpp` uses the executable directory outside Linux;
  the current Windows ZIP therefore already keeps settings and saves beside
  `ArrowPuzzle.exe`.
- Linux uses `$XDG_CONFIG_HOME/arrow-puzzle/settings.txt` and
  `$XDG_DATA_HOME/arrow-puzzle/savegame.txt`, falling back to the account's
  `.config` and `.local/share` directories. The default is the same in a source
  build, the tar.gz package, and the AppImage. Merely extracting the archive on
  the Desktop does not select portable persistence in the current Linux game.
- Linux's explicit-directory persistence constructors are currently used by
  tests; there is no player-facing portable marker or command-line switch.
- `tools/appimage/AppRun` only launches the embedded executable. It does not
  independently redirect either XDG location.
- The same game binary is used in the Linux archive and its AppImage wrapper.
  The wrapper uses the pinned `caf24f9` type-2 runtime. Current guide/evidence:
  [Linux guide](../docs/en/LINUX.md) and [Linux progress](linux-portability.md).

Writing next to an executable is suitable for a writable extracted/portable
directory on either OS. It is not inherently a portability defect. The earlier
concern applies to read-only system/program directories and the *inner*
executable directory in a normally mounted AppImage. Packaging format and the
application's data-location policy are separate choices.

## How AppImage starts

An AppImage is normally downloaded, made executable, and launched directly;
there is no required system installation or administrator action. Desktop-menu
integration is optional and can itself add launcher/icon files elsewhere.
The runtime normally mounts its embedded filesystem read-only at a temporary
location and runs the inner `AppRun` entry point. The outer AppImage file can
still live in the user's chosen writable directory.

Saving beside the *outer* AppImage is possible. Saving beside the inner ELF
executable targets the mounted image instead. A future portable implementation
must resolve the outer location using `APPIMAGE`, not just `/proc/self/exe`.
The extraction-and-run fallback uses temporary extracted content and normally
cleans it afterward; that is not a durable place for saves either.

AppImage's existing portable mechanisms use a writable adjacent directory named
like the full image filename plus `.home` or `.config`. The pinned runtime
sets `HOME` for the former and `XDG_CONFIG_HOME` for the latter, including in
extraction-and-run. This is supported infrastructure, not an ArrowPuzzle-specific
portable-mode acceptance test.

For this game, `.config` alone only redirects settings. Existing absolute
`XDG_DATA_HOME` remains in effect for saves, and an absolute `XDG_CONFIG_HOME`
can also take precedence over a changed `HOME`. A robust portable package must
route both persistent locations deliberately. Renaming a versioned AppImage
also changes the adjacent portable-directory name expected by the runtime.

## Proposed product direction

Keep one codebase and the same underlying executable, with clear data modes:

1. **Standard mode:** retain the current Linux XDG defaults, suitable for
   installed/read-only locations and the ordinary AppImage.
2. **Portable mode:** keep settings and saves directly in the chosen game
   directory (or a documented child directory if later preferred). Windows ZIP
   already supplies the basic adjacent-file behavior. Offer an explicitly
   labelled portable Linux archive if the user adopts this direction.

A packaged marker such as `portable.flag` could select portable mode without
separate builds or requiring players to type arguments. For AppImage, inspect
the outer image's directory. This is a proposal; the current game ignores such
a marker. A Linux launcher exporting both XDG variables to directories within
the package is a lower-impact alternative using existing persistence, but users
would have to start through that launcher.

If implementing portable mode:

- Resolve the selected directory independently of the current working directory.
- Keep explicit portable storage local even when normal XDG overrides exist.
- Report an unwritable portable directory instead of silently falling back to
  the user's home; hidden fallback would defeat the user's stated preference.
- Do not silently import, move, overwrite, or delete existing XDG/adjacent saves.
- Audit other game-owned outputs, especially F12 screenshots, which currently
  go to raylib's initial working directory. The promise concerns permanent game
  files; OS-managed records and temporary AppImage mounting/extraction are a
  separate matter.
- Verify archive and AppImage launch paths, read-only failures, save/resume,
  different working directories, and absence of writes to the user's normal
  config/data directories. Preserve Windows behavior and existing progress.

This discussion does not authorize changing the prepared 0.5.0 draft, its tag,
attested packages, or publication state. Treat any later implementation and
release packaging as a separate scoped change.

## Gear Lever desktop-launch observation (2026-10-30)

The user reports that the first double-click of the AppImage in Nemo opens a
Gear Lever window showing the correct Arrow Puzzle icon/name, with Launch and
Move to the app menu buttons. After Launch, subsequent Nemo double-clicks start
the game directly, while opening the search result in the desktop menu brings
back Gear Lever. The screenshot confirms the manager's two-button presentation;
it does not reveal the installed Gear Lever version or the search result type.

Upstream Gear Lever's current `AppDetails.on_secondary_action_button_clicked`
calls `set_trusted()` before running. `AppImageListElement.set_trusted()` sets
file permissions to `0755`. This supports the explanation that Nemo can execute
the file directly after the first managed launch. The menu-search route may
instead open a file/recent-document result through its MIME handler. That part
remains an inference until the actual result/launcher and command are inspected
inside the Linux session; do not diagnose a game or AppImage defect from it.

Gear Lever's integration copies the image into its managed location, creates a
user `.desktop` entry and icon files, and can delete the original depending on
the move setting. Launch and menu integration are distinct actions. Manager
files/settings are separate from ArrowPuzzle's own persistence policy. A future
portable design using adjacent data/markers must account for third-party
managers moving only the AppImage, rather than assuming the whole portable
directory moves with it. No manager settings or game behavior were changed.

## Sources checked

- [AppImage quickstart](https://docs.appimage.org/introduction/quickstart.html)
- [Mounting/extracting and optional desktop integration](https://docs.appimage.org/user-guide/run-appimages.html)
- [AppImage portable mode](https://docs.appimage.org/user-guide/portable-mode.html)
- [Outer image and inner mount environment variables](https://docs.appimage.org/packaging-guide/environment-variables.html)
- [Exact pinned runtime implementation](https://github.com/AppImage/type2-runtime/blob/caf24f9/src/runtime/runtime.c)
- [Gear Lever overview](https://gearlever.mijorus.it/docs/overview/)
- [Gear Lever launch button implementation](https://github.com/mijorus/gearlever/blob/master/src/AppDetails.py)
- [Gear Lever executable permissions and menu integration](https://github.com/mijorus/gearlever/blob/master/src/providers/AppImageProvider.py)
