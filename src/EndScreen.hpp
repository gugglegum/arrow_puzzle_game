#pragma once

#include <raylib.h>
#include "arrow/Localization.hpp"

#include <array>
#include <optional>

namespace arrow {

class UiText;

class EndScreen {
public:
    enum class Outcome { None, Victory, Defeat };
    struct Update {
        bool changed{};
        int fireworks{};
    };

    void reset();
    void show(Outcome outcome, float delay = 0.0F);
    Update update(float delta);
    void draw(UiText& text, int width, int height, float top,
              std::optional<int> hovered, std::optional<int> pressed,
              Language language = Language::English) const;

    // Gameplay ends immediately; the result may still be waiting to appear.
    [[nodiscard]] bool active() const { return outcome_ != Outcome::None; }
    [[nodiscard]] bool visible() const { return active() && delayRemaining_ <= 0.0F; }
    [[nodiscard]] Outcome outcome() const { return outcome_; }
    [[nodiscard]] static Rectangle buttonRectangle(int index, int width, int height, float top);
    [[nodiscard]] std::optional<int> buttonAt(Vector2 point, int width, int height, float top) const;

private:
    struct Spark {
        Vector2 velocity{};
        float lifetime{};
        float phase{};
        Color color{};
    };
    struct Burst {
        float time{};
        Vector2 origin{};
        std::array<Spark, 72> sparks{};
    };

    void drawFireworks(int width, int height, float top) const;

    Outcome outcome_{Outcome::None};
    float delayRemaining_{};
    float age_{};
    int nextBurst_{};
    std::array<Burst, 5> bursts_{};
};

} // namespace arrow
