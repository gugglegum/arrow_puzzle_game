# Application icon

`arrow-puzzle.svg` is the selected vector master: two symmetric one-bend
arrows on a 4-by-4 grid, six gray dots, and circularly rounded arrowheads.
`arrow-puzzle.png` is its 1024-pixel preview.

The committed 16, 24, 32, 48, 64, 128, and 256-pixel PNGs are embedded as C++
data by CMake. `AppIcon.cpp` decodes them in memory after `InitWindow`, converts
them to RGBA8, calls raylib's `SetWindowIcons`, then frees the temporary images.
No external image files are read at runtime.

`arrow-puzzle.ico` contains those same seven PNGs. On Windows, CMake compiles
`WindowsIcon.rc.in` into the EXE. The `GLFW_ICON` resource is also the initial
window-class icon; Explorer and ordinary shortcuts extract it from the EXE.
The ICO is an explicit build dependency, so changing it relinks the resource.

## Updating the artwork

Normal game builds need only the committed assets and the usual compiler/SDK.
To regenerate assets after editing the SVG, install Node.js and ImageMagick 7
on PATH, then run from the repository root:

```text
node tools/generate-icons.mjs
```

This updates the PNGs and ICO, with intermediate SVGs under ignored
`out/icon-generation`. Review the output and commit the assets together.
The tool strips PNG metadata to avoid timestamp-only changes.

The master stroke is 52 in a 1024-unit view box. The 16/24-pixel exports use
64/58 for legibility; dot diameter always equals stroke width and head-corner
radius is one quarter of it. Small dots use a slightly brighter gray. Keep
the `data-corners` vertices on both head paths: they describe the original
sharp triangle, allowing the generator to reconstruct each circular fillet
at the adjusted stroke weight. They do not change SVG rendering.
