# Portable distribution and AppImage data locations

## Accepted storage contract (0.5.0, 2026-10-30)

The user explicitly requested ordinary and portable builds on **both** OSes.
This supersedes the earlier discussion-only proposal and the old Windows ZIP's
implicit portable behavior. Published release tags and assets are immutable.

| Package family | Ordinary storage | Portable storage |
| --- | --- | --- |
| Windows ZIP | `%LOCALAPPDATA%\ArrowPuzzle\settings.txt` and `savegame.txt` | Beside the EXE |
| Linux tar.gz | XDG config/settings and XDG data/save | Beside the ELF |
| Linux AppImage | Same XDG paths | Beside the outer `.AppImage` |

There are **six packages**, but **four executable variants** (OS x storage
mode). Each Linux ELF is reused unchanged in its tar and AppImage. Portable
package/executable names have the suffix `-portable`; the name does not select
the behavior. CMake builds `arrow_puzzle` and `arrow_puzzle_portable` from shared
sources, defining `ARROWPUZZLE_PORTABLE=0/1` only for the GUI target.
`Game::run()` selects `Persistence(StorageMode::User/Portable)` at compilation.

Windows uses `SHGetKnownFolderPath(FOLDERID_LocalAppData)`, not concatenated
profile paths or a narrow environment value. Settings and the one small offline
save share one per-user, non-roaming folder. `Saved Games` is a legitimate
alternative if a future product requirement calls for it; it is not the chosen
path. Linux respects absolute XDG overrides and falls back to the account home
for invalid/unset ones. Serialized version-1 formats are unchanged.

A portable executable must work when extracted **alone**, including after
renaming and from another working directory. The user rejected `portable.flag`
as obscure. No sidecar, filename test, runtime switch, launcher or environment
setup may be the prerequisite for portable mode. Portable mode ignores XDG,
never silently falls back to user storage, and requires a writable directory.
No existing adjacent/XDG/LocalAppData files are automatically imported, moved,
overwritten or deleted. Copy settings/save manually with the game closed.

For AppImage, `/proc/self/exe` resolves the inner mounted/extracted ELF, which is
not the durable destination. Portable mode uses the absolute outer `APPIMAGE`
path only when the executable is inside the corresponding canonical `APPDIR`.
This also handles extraction-and-run. Unrelated APPIMAGE/APPDIR variables
inherited from an AppImage-hosted terminal or coding agent must not redirect a
standalone game. Renaming the outer image is supported.

## Packaging and checks

- `package-windows.ps1 -Portable` / `package-linux.py --portable` package the
  portable target. Use separate output directories per mode; each has its own
  build-info and checksum files. AppImage infers the mode from the verified tar.
- Package metadata includes a boolean `portable` and `executable_name`.
  `ARROWPUZZLE_STORAGE=user/portable` is a compiled diagnostic string, not an
  environment switch. Packaging and common release assembly verify the actual
  binary's marker, capture exclusion and hashes, not just labels/metadata.
- Six packages and four unpacked executables give ten SHA256SUMS subjects.
  Windows and Linux provenance bundles cover four and six subjects respectively;
  the common checksum manifest is an eleventh signed subject.
- `check-linux-desktop.sh` runs rendering/text and both Debug save/resume suites.
  `check-linux-packages.sh VERSION --appimage` packages and launches both tar
  variants and both AppImages. Ordinary smoke uses a read-only program dir and
  isolated XDG; portable smoke renames a lone executable and requires adjacent
  data with no XDG writes. All production smoke is external X11 input.
- Windows QA should copy the portable Debug/Release executable to an isolated
  scratch directory; ordinary Debug now uses personal LocalAppData. Unit tests
  check KnownFolder path discovery without writing the actual profile.
- The earlier draft-only task kept its three-package v0.5.0 candidate intact.
  The user's later 0.5.0 publication request authorizes refreshing that still
  unpublished candidate with this six-package layout. Published tags/assets
  remain immutable; see the current release evidence and status notes.
- Final 0.5.0 is public with all six packages; independent checks and identities
  are recorded in [release history](release-history.md#050-2026-10-30).

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

## AppImage installation and portable limits

An AppImage is downloaded, made executable and launched without a required
system installation or administrator action. Optional desktop integration can
add launchers/icons elsewhere. The runtime mounts its embedded filesystem
read-only or temporarily extracts it; this OS/runtime activity is separate
from the game's persistent files. Portable means the game's own settings/save
stay adjacent, not that third-party managers or the OS never create anything.

AppImage's `.home`/`.config` mechanisms are upstream options, not prerequisites
for ArrowPuzzle portable mode. `.config` alone does not redirect XDG data, and
changing only HOME can lose to existing absolute XDG overrides. The compiled
portable path deliberately routes both files. If a manager moves only the
AppImage, it does not also move the existing adjacent data; move the whole
portable folder to retain its profile.

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
