#include "arrow/Game.hpp"

#if defined(_WIN32)
#include <windows.h>

int WINAPI WinMain(
    [[maybe_unused]] HINSTANCE instance,
    [[maybe_unused]] HINSTANCE previousInstance,
    [[maybe_unused]] LPSTR commandLine,
    [[maybe_unused]] int commandShow
) {
    return arrow::Game{}.run();
}
#else
int main() {
    return arrow::Game{}.run();
}
#endif
