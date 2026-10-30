# Third-Party Notices

## Inter 4.1

Arrow Puzzle bundles the unchanged `Inter-Medium.ttf` font from the official
Inter 4.1 release.

- Project: <https://rsms.me/inter/>
- Source: <https://github.com/rsms/inter/releases/tag/v4.1>
- Copyright: Copyright (c) 2016 The Inter Project Authors
- License: SIL Open Font License, Version 1.1

The complete license is included at
[`assets/fonts/Inter-LICENSE.txt`](assets/fonts/Inter-LICENSE.txt) in both the
source tree and binary release archives.

## Noto Sans SC / Arrow Puzzle CJK

Arrow Puzzle embeds a modified weight-500 UI subset of Noto Sans SC, renamed
Arrow Puzzle CJK. It provides Simplified Chinese glyphs on Windows and Linux.

- [Pinned Google Fonts source](https://github.com/google/fonts/tree/a85815a42757630ce188fdad368c2dfc444d4773/ofl/notosanssc)
- Copyright 2014-2021 Adobe (http://www.adobe.com/), with Reserved Font Name 'Source'
- License: SIL Open Font License, Version 1.1
- Complete license: [`assets/fonts/NotoSansSC-LICENSE.txt`](assets/fonts/NotoSansSC-LICENSE.txt), included in binary archives.
- Reproduction and source digest: [`assets/fonts/README.md`](assets/fonts/README.md).

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

### Components embedded by the Windows build

The Windows build uses raylib 5.5 source revision
`c1ab645ca298a2801097931d1079b10ff7eb9df8`. Its linked core, texture, and text
modules embed the following permissively licensed components:

| Component | Copyright / authors | License |
| --- | --- | --- |
| GLFW 3.4 | Marcus Geelnard; Camilla Löwy and contributors | zlib/libpng |
| msf_gif 2.2 | Copyright (c) 2021 Miles Fogle | MIT or public domain |
| sdefl and sinfl 1.0 | Copyright (c) 2020-2023 Micha Mettke | MIT or public domain |
| rprand 1.0 | Copyright (c) 2023 Ramon Santamaria | zlib/libpng |
| dirent for Win32 | Copyright Kevlin Henney, 1997, 2003 | Custom permissive license |
| stb_image 2.30 | Copyright (c) 2017 Sean Barrett | MIT or public domain |
| stb_image_write 1.16 | Copyright (c) 2017 Sean Barrett | MIT or public domain |
| stb_image_resize2 2.18 | Copyright (c) 2017 Sean Barrett | MIT or public domain |
| stb_perlin 0.5 | Copyright (c) 2017 Sean Barrett | MIT or public domain |
| stb_truetype 1.26 | Copyright (c) 2017 Sean Barrett | MIT or public domain |
| stb_rect_pack 1.01 | Copyright (c) 2017 Sean Barrett | MIT or public domain |
| QOI | Copyright (c) 2021 Dominic Szablewski | MIT |
| rl_gputex 1.0 | Copyright (c) 2013-2022 Ramon Santamaria | zlib/libpng |

The upstream raylib project documents its dependency set in the
[raylib dependency list](https://github.com/raysan5/raylib/wiki/raylib-dependencies).
The versions above were verified against the exact 5.5 source fetched by this
project.

#### MIT-licensed embedded components

Arrow Puzzle uses the MIT option for msf_gif, sdefl, sinfl, the stb libraries,
and QOI.

> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to
> deal in the Software without restriction, including without limitation the
> rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
> sell copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in
> all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
> THE SOFTWARE.

#### zlib/libpng-licensed embedded components

GLFW, rprand, and rl_gputex use the same zlib/libpng terms reproduced above
for raylib, with their respective copyright notices from the table.

#### dirent for Win32

> Permission to use, copy, modify, and distribute this software and its
> documentation for any purpose is hereby granted without fee, provided that
> this copyright and permissions notice appear in all copies and derivatives.
>
> This software is supplied "as is" without express or implied warranty.

## Optional Linux AppImage container

AppImage packages add the upstream type-2 runtime from checksum-pinned
appimagetool 1.9.1. Its MIT license, statically linked component references and
source/build links are included as `RUNTIME-NOTICES.txt` beside these notices
inside the AppImage (`usr/share/arrow-puzzle` after extraction). The source
checkout keeps the same text in `tools/appimage/RUNTIME-NOTICES.txt`. No system
shared libraries for the game are bundled by this packaging step.
