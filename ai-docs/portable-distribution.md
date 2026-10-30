# Portable distribution and AppImage data locations

Discussion recorded on 2026-10-30 after pulling `master` from `83a5ddd` to
`3fd31be`. The user is considering explicitly labelled portable packages whose
persistent game files stay in one chosen directory. This is a preference and
design proposal, not an instruction to change storage behavior or release assets
yet. No portable mode was implemented in this discussion.

## Accepted screenshot policy

The user contacted the initial GPT-5.6 Sol chat and reports that it added F12
without a user request for debugging convenience. The user has now explicitly
requested removal: accidental presses should not create unwanted files, and
players can use their own screenshot tools. The later clarification requires
the remaining environment hooks to work **only in Debug builds**.

- Remove the game's F12 handler and README control entries.
- Disable raylib's independent `SUPPORT_SCREEN_CAPTURE` and
  `SUPPORT_GIF_RECORDING` handlers in every configuration; otherwise F12 and
  Control+F12 could still create PNG/GIF files after removing the game handler.
- `ARROWPUZZLE_SCREENSHOT_HOOKS=$<CONFIG:Debug>` conditionally compiles the
  environment parsing, deterministic seed/scenario setup, captures, and related
  save/exit behavior. Release and RelWithDebInfo contain none of those hooks.
- CI keeps the seven capture/save/resume scenarios on a separate Debug build.
  The real Release binary and its tar/AppImage packages are checked through
  external X11 input, with capture variables set and F12/Control+F12 pressed:
  they must create no image, ignore developer variables, and exit normally.
  A binary scan also rejects hook strings and raylib capture filename patterns.
- The existing 0.5.0 draft/tag/packages are historical artifacts and are not
  replaced by this source change. New builds receive the changed policy.

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

## User requirement: a portable executable must work alone

Clarified on 2026-10-30 after the marker proposal: the user explicitly rejects
`portable.flag` as obscure and unsuitable for the self-contained executable.
A player may extract only the executable from the portable archive. That file
must retain portable persistence without a sidecar, launcher, environment setup,
or undocumented convention. Do not re-propose a marker as the selected design.

Keep one codebase, but produce distinct executable variants if adopting both
data policies:

1. **Standard mode:** retain the current Linux XDG defaults, suitable for
   installed/read-only locations and the ordinary AppImage.
2. **Portable mode:** keep settings and saves directly in the chosen game
   directory, with this behavior built into the executable. Windows ZIP already
   supplies the basic adjacent-file behavior. A portable Linux archive must
   contain an executable that remains portable when extracted on its own.

A compile-time CMake option is the recommended implementation approach, not
implemented or tested yet. It can select the default persistence policy in the
same sources, producing a standard executable and a portable executable with
clear package names. Portable behavior must not depend on the executable's
filename either. There is no need for a separate codebase or extra runtime file.
Do not introduce a new non-portable Windows distribution as an incidental part
of this discussion; existing Windows adjacent-file behavior should be preserved.

A launcher-only approach also fails the executable-alone requirement if direct
launch bypasses it. AppImage's optional `.home`/`.config` directories remain
upstream functionality, but should not be the prerequisite that makes a package
labelled Portable honor its data policy. A future portable AppImage should carry
the portable executable and resolve the outer image directory for persistent
files. The current standard AppImage and tar still contain the same binary;
do not describe distinct standard/portable binaries as already available.

If implementing portable mode:

- Resolve the selected directory independently of the current working directory.
- Keep explicit portable storage local even when normal XDG overrides exist.
- Report an unwritable portable directory instead of silently falling back to
  the user's home; hidden fallback would defeat the user's stated preference.
- Do not silently import, move, overwrite, or delete existing XDG/adjacent saves.
- Audit other permanent game-owned outputs. F12 screenshots were removed by the
  accepted policy above; Debug-only QA captures are an explicit development
  action. OS-managed records and temporary AppImage mounting/extraction remain
  separate matters.
- Verify archive and AppImage launch paths, read-only failures, save/resume,
  different working directories, copying/renaming just the executable, and
  absence of writes to the user's normal config/data directories. Preserve
  Windows behavior and existing progress.

The earlier discussion referred to the then-existing **F12** command, which
wrote `arrow-puzzle-screenshot.png` relative to the initial working directory.
The user later explicitly requested its removal, as recorded above. The
separate `ARROWPUZZLE_SCREENSHOT` hooks remain for deliberate Debug-only QA.

The user then said they did not recall requesting screenshot functionality.
The complete, non-shallow Git history places both `KEY_F12` and
`TakeScreenshot("arrow-puzzle-screenshot.png")`, as well as the README control
entry, in root commit `d5ebdb911b857a8dcd5c5bb3dc4ff58038816245`
(`Initial commit`, 2026-07-28). This establishes when the feature entered the
repository, not who requested it or whether it was authorized. No user request
for this feature appears in the current conversation. Do not treat its existing
code/documentation as evidence of acceptance or as a requirement to retain it.
The explicit F12 product shortcut and automated QA hooks are distinct concerns;
the subsequent removal request supersedes the earlier unconfirmed status.

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
portable design using adjacent data must account for third-party
managers moving only the AppImage, rather than assuming the whole portable
directory moves with it. No manager settings or game behavior were changed.

The user subsequently confirmed the executable-bit explanation and integrated
the image: it disappeared from Downloads and appeared under Games in the menu.
Mint's generic Uninstall then reported no associated package and offered to
remove only the menu item. Prefer Gear Lever's own Remove action while its
desktop entry still exists: upstream removes the AppImage, desktop entry and
icon. ArrowPuzzle's XDG settings/save directories are independent and remain
unless separately deleted. This is removal guidance, not a deletion performed
by this chat.

## Sources checked

- [AppImage quickstart](https://docs.appimage.org/introduction/quickstart.html)
- [Mounting/extracting and optional desktop integration](https://docs.appimage.org/user-guide/run-appimages.html)
- [AppImage portable mode](https://docs.appimage.org/user-guide/portable-mode.html)
- [Outer image and inner mount environment variables](https://docs.appimage.org/packaging-guide/environment-variables.html)
- [Exact pinned runtime implementation](https://github.com/AppImage/type2-runtime/blob/caf24f9/src/runtime/runtime.c)
- [Gear Lever overview](https://gearlever.mijorus.it/docs/overview/)
- [Gear Lever launch button implementation](https://github.com/mijorus/gearlever/blob/master/src/AppDetails.py)
- [Gear Lever executable permissions and menu integration](https://github.com/mijorus/gearlever/blob/master/src/providers/AppImageProvider.py)
