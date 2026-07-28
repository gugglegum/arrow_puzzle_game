# Third-Party Notices

## Inter 4.1

Arrow Puzzle bundles the unchanged `Inter-Medium.ttf` font from the official
Inter 4.1 release.

- Project: <https://rsms.me/inter/>
- Source: <https://github.com/rsms/inter/releases/tag/v4.1>
- Copyright: Copyright (c) 2016 The Inter Project Authors
- License: SIL Open Font License, Version 1.1

The complete license is included at
[`assets/fonts/Inter-LICENSE.txt`](assets/fonts/Inter-LICENSE.txt).

## raylib 5.5

Arrow Puzzle downloads raylib 5.5 at configure time and links it statically.
raylib is not vendored in this repository.

- Project: <https://www.raylib.com/>
- Source: <https://github.com/raysan5/raylib/tree/5.5>
- License: unmodified zlib/libpng license

The following notice is reproduced from the raylib 5.5 `LICENSE` file:

> Copyright (c) 2013-2024 Ramon Santamaria (@raysan5)
>
> This software is provided "as-is", without any express or implied warranty.
> In no event will the authors be held liable for any damages arising from the
> use of this software.
>
> Permission is granted to anyone to use this software for any purpose,
> including commercial applications, and to alter it and redistribute it
> freely, subject to the following restrictions:
>
> 1. The origin of this software must not be misrepresented; you must not claim
>    that you wrote the original software. If you use this software in a
>    product, an acknowledgment in the product documentation would be
>    appreciated but is not required.
> 2. Altered source versions must be plainly marked as such, and must not be
>    misrepresented as being the original software.
> 3. This notice may not be removed or altered from any source distribution.

raylib contains third-party single-file libraries for platform access and file
formats. Their notices are retained in the raylib source downloaded by CMake.
The upstream project maintains the dependency license list in the
[raylib wiki](https://github.com/raysan5/raylib/wiki/raylib-dependencies).

This file documents the project's direct dependency. Before publishing a
binary release, the release package should also include the notices for every
raylib embedded dependency enabled in that build, taken from the exact fetched
raylib source revision.
