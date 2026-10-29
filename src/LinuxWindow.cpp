#include "LinuxWindow.hpp"

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <X11/Xatom.h>

namespace arrow {
namespace {

GLFWwindow* x11Window() {
    return glfwGetPlatform() == GLFW_PLATFORM_X11 ? glfwGetCurrentContext() : nullptr;
}

} // namespace

bool isLinuxDesktopFullscreen() {
    GLFWwindow* window = x11Window();
    if (window == nullptr) return false;
    Display* display = glfwGetX11Display();
    const Atom fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    Atom type{};
    int format{};
    unsigned long count{}, remaining{};
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(
        display, glfwGetX11Window(window), XInternAtom(display, "_NET_WM_STATE", False),
        0, 1024, False, XA_ATOM, &type, &format, &count, &remaining, &data
    );
    bool enabled = false;
    if (status == Success && type == XA_ATOM && format == 32 && data != nullptr) {
        const auto* atoms = reinterpret_cast<const Atom*>(data);
        for (unsigned long index = 0; index < count; ++index) enabled |= atoms[index] == fullscreen;
    }
    if (data != nullptr) XFree(data);
    return enabled;
}

bool toggleLinuxDesktopFullscreen() {
    GLFWwindow* window = x11Window();
    if (window == nullptr) return false;
    Display* display = glfwGetX11Display();
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.window = glfwGetX11Window(window);
    event.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = 2; // _NET_WM_STATE_TOGGLE
    event.xclient.data.l[1] = static_cast<long>(XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False));
    event.xclient.data.l[3] = 1; // Normal application, as specified by EWMH.
    const bool sent = XSendEvent(
        display, DefaultRootWindow(display), False,
        SubstructureNotifyMask | SubstructureRedirectMask, &event
    ) != 0;
    XFlush(display);
    return sent;
}

} // namespace arrow
