#pragma once

namespace arrow {

// X11/XWayland desktop fullscreen is managed by the window manager. This
// preserves display resolution, stacking, and the previous window geometry.
[[nodiscard]] bool isLinuxDesktopFullscreen();
[[nodiscard]] bool toggleLinuxDesktopFullscreen();

} // namespace arrow
