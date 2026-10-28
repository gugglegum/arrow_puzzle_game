#include "EndScreen.hpp"
#include "UiText.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void checkTimeline(float step) {
    arrow::EndScreen screen;
    screen.show(arrow::EndScreen::Outcome::Victory);
    int bursts = 0;
    for (int frame = 0; frame < static_cast<int>(8.0F / step); ++frame) {
        const auto update = screen.update(step);
        bursts += update.fireworks;
    }
    require(bursts == 5, "Victory must emit exactly five firework reports");
    require(!screen.update(step).changed && screen.update(step).fireworks == 0,
            "Celebration must stop animating and emitting sound");
    screen.show(arrow::EndScreen::Outcome::Defeat);
    for (int frame = 0; frame < 100; ++frame) require(screen.update(step).fireworks == 0, "Defeat emitted fireworks");
    require(screen.outcome() == arrow::EndScreen::Outcome::Defeat, "Outcome did not reset");
    screen.reset();
    require(!screen.visible() && !screen.update(step).changed, "Reset left an active celebration");
}

void checkDelayedDefeat(float step) {
    arrow::EndScreen screen;
    screen.show(arrow::EndScreen::Outcome::Defeat, 1.2F);
    require(screen.active() && !screen.visible(), "Delayed defeat did not finish gameplay before covering it");
    require(!screen.buttonAt({300, 450}, 720, 600, 82), "Delayed result captured a hidden button");
    require(!screen.update(-1).changed && !screen.update(std::numeric_limits<float>::quiet_NaN()).changed,
            "Invalid delta advanced the defeat delay");
    float elapsed = 0;
    int reveals = 0;
    while (!screen.visible() && elapsed < 2) {
        const auto update = screen.update(step);
        elapsed += step;
        require(update.fireworks == 0, "Delayed defeat emitted fireworks");
        require(update.changed == screen.visible(), "Invisible timer caused a redraw or missed its reveal");
        reveals += update.changed ? 1 : 0;
    }
    require(elapsed >= 1.19F && elapsed <= 1.2F + step + 0.001F && reveals == 1,
            "Defeat delay depends on frame rate or revealed more than once");
    for (int frame = 0; frame < 100; ++frame) screen.update(step);
    require(!screen.update(step).changed, "Defeat kept redrawing after its fade");

    screen.show(arrow::EndScreen::Outcome::Defeat, 1.2F);
    screen.update(10000);
    require(!screen.visible(), "A stalled frame skipped the whole defeat delay");
    screen.reset();
    require(!screen.active() && !screen.visible() && !screen.update(10).changed,
            "Reset left a pending defeat");
    screen.show(arrow::EndScreen::Outcome::Defeat, 1.2F);
    screen.show(arrow::EndScreen::Outcome::Victory);
    require(screen.visible(), "Previous defeat delay affected a new victory");
}

void screenshots(const char* directory) {
    std::filesystem::create_directories(directory);
    const std::filesystem::path previous = std::filesystem::current_path();
    std::filesystem::current_path(directory);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN | FLAG_MSAA_4X_HINT);
    InitWindow(1100, 900, "End screen visual check");
    arrow::UiText text;
    for (int width : {1100, 720}) {
        const int height = width == 720 ? 600 : 900;
        SetWindowSize(width, height);
        PollInputEvents();
        for (bool won : {false, true}) {
            arrow::EndScreen screen;
            screen.show(won ? arrow::EndScreen::Outcome::Victory : arrow::EndScreen::Outcome::Defeat);
            for (int frame = 0; frame < 201; ++frame) screen.update(1.0F / 60.0F);
            BeginDrawing();
            ClearBackground(RAYWHITE);
            screen.draw(text, width, height, 82, std::nullopt, std::nullopt);
            EndDrawing();
            const std::string name = std::string(won ? "victory-" : "defeat-") + std::to_string(width) + ".png";
            TakeScreenshot(name.c_str());
        }
    }
    text.release();
    CloseWindow();
    std::filesystem::current_path(previous);
}

} // namespace

int main(int argc, char** argv) {
    try {
        static_assert(sizeof(arrow::EndScreen) < 16 * 1024, "Celebration state must stay small");
        checkTimeline(1.0F / 10.0F);
        checkTimeline(1.0F / 60.0F);
        checkTimeline(1.0F / 144.0F);
        checkDelayedDefeat(1.0F / 10.0F);
        checkDelayedDefeat(1.0F / 60.0F);
        checkDelayedDefeat(1.0F / 144.0F);
        arrow::EndScreen screen;
        require(!screen.buttonAt({400, 400}, 720, 600, 82), "Hidden screen captured input");
        screen.show(arrow::EndScreen::Outcome::Victory);
        require(!screen.update(-1).changed && !screen.update(std::numeric_limits<float>::quiet_NaN()).changed,
                "Invalid delta changed animation state");
        require(screen.update(10000).fireworks == 0, "A stalled frame emitted a backlog of reports");
        for (int width : {720, 1100, 3840}) {
            const int height = width == 720 ? 600 : width == 1100 ? 900 : 2160;
            for (int index = 0; index < 2; ++index) {
                const Rectangle r = arrow::EndScreen::buttonRectangle(index, width, height, 82);
                require(r.x >= 0 && r.y > 82 && r.x + r.width < width && r.y + r.height < height,
                        "Result buttons do not fit in the gameplay area");
                require(screen.buttonAt({r.x + r.width / 2, r.y + r.height / 2}, width, height, 82) == index,
                        "Result button hit test failed");
            }
            require(!screen.buttonAt({static_cast<float>(width) / 2, 40}, width, height, 82), "Result screen captured the HUD");
        }
        if (argc == 2) screenshots(argv[1]);
        std::cout << "End screen tests passed: delayed defeat, finite animation, five one-shot reports, reset, stalled frames, button layout, "
                  << sizeof(screen) << " bytes of fixed state.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "End screen test failure: " << e.what() << '\n';
        return 1;
    }
}
