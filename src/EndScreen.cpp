#include "EndScreen.hpp"
#include "UiText.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace arrow {
namespace {

constexpr float kCelebrationDuration = 6.5F;
constexpr float kFadeDuration = 0.4F;
constexpr Color kLight{232, 240, 254, 255};
constexpr Color kQuiet{151, 174, 209, 255};
constexpr Color kGold{255, 218, 126, 255};
constexpr Color kCoral{255, 143, 157, 255};

float randomUnit(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return static_cast<float>(state & 0xFFFFFFU) / 16777216.0F;
}

void centered(UiText& text, const char* label, float x, float y, float size, float spacing, Color color,
              float maxWidth = 556.0F) {
    size = text.fitSize(label, size, spacing, maxWidth);
    const Vector2 measured = text.measure(label, size, spacing);
    text.draw(label, {std::round(x - measured.x * 0.5F), std::round(y)}, size, spacing, color);
}

void star(Vector2 center, float radius, Color color) {
    constexpr float turn = std::numbers::pi_v<float> / 5.0F;
    std::array<Vector2, 10> points{};
    for (std::size_t index = 0; index < points.size(); ++index) {
        const float angle = static_cast<float>(index) * turn - std::numbers::pi_v<float> * 0.5F;
        const float r = index % 2 == 0 ? radius : radius * 0.45F;
        points[index] = {center.x + std::cos(angle) * r, center.y + std::sin(angle) * r};
    }
    for (std::size_t index = 0; index < points.size(); ++index) {
        DrawTriangle(center, points[(index + 1) % points.size()], points[index], color);
    }
}

} // namespace

void EndScreen::reset() {
    outcome_ = Outcome::None;
    delayRemaining_ = 0;
    age_ = 0;
    nextBurst_ = 0;
}

void EndScreen::show(Outcome outcome, float delay) {
    reset();
    outcome_ = outcome;
    delayRemaining_ = std::isfinite(delay) ? std::max(0.0F, delay) : 0.0F;
    if (outcome != Outcome::Victory) return;
    constexpr std::array<float, 5> times{1.0F, 1.7F, 2.5F, 3.2F, 4.0F};
    constexpr std::array<Vector2, 5> origins{{{0.15F, 0.26F}, {0.84F, 0.22F},
        {0.50F, 0.11F}, {0.12F, 0.73F}, {0.89F, 0.70F}}};
    constexpr std::array<Color, 5> colors{{kGold, {115, 219, 255, 255},
        {255, 135, 206, 255}, {140, 247, 201, 255}, {192, 169, 255, 255}}};
    std::uint32_t state = 0xF17E2026U;
    for (std::size_t index = 0; index < bursts_.size(); ++index) {
        Burst& burst = bursts_[index];
        burst.time = times[index];
        burst.origin = origins[index];
        for (std::size_t sparkIndex = 0; sparkIndex < burst.sparks.size(); ++sparkIndex) {
            Spark& spark = burst.sparks[sparkIndex];
            const float angle = 2 * std::numbers::pi_v<float> *
                (static_cast<float>(sparkIndex) + randomUnit(state) * 0.65F) /
                static_cast<float>(burst.sparks.size());
            const float speed = 0.19F + randomUnit(state) * 0.25F;
            spark.velocity = {std::cos(angle) * speed, std::sin(angle) * speed};
            spark.lifetime = 1.25F + randomUnit(state) * 1.0F;
            spark.phase = randomUnit(state) * 6.28F;
            spark.color = sparkIndex % 7 == 0 ? kLight : colors[index];
        }
    }
}

EndScreen::Update EndScreen::update(float delta) {
    if (!active() || !std::isfinite(delta) || delta <= 0) return {};
    if (delayRemaining_ > 0.0F) {
        // The board stays unchanged during the delay; only its end needs a
        // new frame. Clamp stalled frames just like the result animation.
        delayRemaining_ = std::max(0.0F, delayRemaining_ - std::min(delta, 0.1F));
        return {visible(), 0};
    }
    const float duration = outcome_ == Outcome::Victory ? kCelebrationDuration : kFadeDuration;
    if (age_ >= duration) return {};
    age_ = std::min(duration, age_ + std::min(delta, 0.1F));
    int count = 0;
    if (outcome_ == Outcome::Victory) {
        while (nextBurst_ < static_cast<int>(bursts_.size()) && bursts_[nextBurst_].time <= age_) {
            ++nextBurst_;
            ++count;
        }
    }
    return {true, count};
}

Rectangle EndScreen::buttonRectangle(int index, int width, int height, float top) {
    const float centerX = static_cast<float>(width) * 0.5F;
    const float centerY = top + (static_cast<float>(height) - top) * 0.5F + 10.0F;
    return {centerX - 195.0F + index * 202.0F, centerY + 72.0F, 188.0F, 46.0F};
}

std::optional<int> EndScreen::buttonAt(Vector2 point, int width, int height, float top) const {
    if (visible()) {
        for (int index = 0; index < 2; ++index) {
            if (CheckCollisionPointRec(point, buttonRectangle(index, width, height, top))) return index;
        }
    }
    return std::nullopt;
}

void EndScreen::drawFireworks(int width, int height, float top) const {
    const float areaHeight = static_cast<float>(height) - top;
    const float scale = std::min(static_cast<float>(width), areaHeight);
    BeginBlendMode(BLEND_ADDITIVE);
    for (const Burst& burst : bursts_) {
        const Vector2 origin{burst.origin.x * width, top + burst.origin.y * areaHeight};
        const float time = age_ - burst.time;
        if (time >= -0.65F && time < 0) {
            const float progress = 1 + time / 0.65F;
            const Vector2 rocket{origin.x - 25.0F * (1 - progress),
                origin.y + (areaHeight + top - origin.y) * (1 - progress) * (1 - progress)};
            DrawLineEx(rocket, {rocket.x - 2.0F, rocket.y + 22.0F}, 1.4F, Fade(kGold, 0.45F));
            DrawCircleV(rocket, 2.0F, kGold);
        }
        if (time < 0 || time > 2.3F) continue;
        if (time < 0.14F) {
            DrawCircleGradient(static_cast<int>(origin.x), static_cast<int>(origin.y),
                25 + time * 140, Fade(kGold, (1 - time / 0.14F) * 0.35F), BLANK);
        }
        for (const Spark& spark : burst.sparks) {
            if (time >= spark.lifetime) continue;
            constexpr float drag = 1.45F;
            const float damping = std::exp(-drag * time);
            const float travel = (1 - damping) / drag;
            const float gravity = scale * 0.16F;
            const Vector2 point{origin.x + spark.velocity.x * scale * travel,
                origin.y + spark.velocity.y * scale * travel + gravity * (time - travel) / drag};
            const Vector2 velocity{spark.velocity.x * scale * damping,
                spark.velocity.y * scale * damping + gravity * travel};
            const float fade = std::pow(1 - time / spark.lifetime, 1.25F);
            const float shimmer = 0.8F + std::sin(time * 18 + spark.phase) * 0.2F;
            const Color ink = Fade(spark.color, fade * shimmer);
            const Vector2 tail{point.x - velocity.x * 0.08F, point.y - velocity.y * 0.08F};
            DrawLineEx(tail, point, 4.0F, Fade(spark.color, fade * 0.12F));
            DrawLineEx(tail, point, 1.7F, ink);
            DrawCircleV(point, 0.9F, Fade(kLight, fade * 0.65F));
        }
    }
    EndBlendMode();
}

void EndScreen::draw(UiText& text, int width, int height, float top,
                     std::optional<int> hovered, std::optional<int> pressed, Language language) const {
    if (!visible()) return;
    const auto tr = [&](Text id) { return translate(id, language); };
    const bool won = outcome_ == Outcome::Victory;
    const Color accent = won ? kGold : kCoral;
    const int y = static_cast<int>(top) + 1;
    BeginScissorMode(0, y, width, std::max(0, height - y));
    DrawRectangleGradientV(0, y, width, height - y,
        won ? Color{10, 20, 41, 255} : Color{27, 19, 34, 255},
        won ? Color{22, 40, 66, 255} : Color{44, 29, 47, 255});
    const float centerX = static_cast<float>(width) * 0.5F;
    const float centerY = top + (static_cast<float>(height) - top) * 0.5F + 10.0F;
    const float cardWidth = std::min(620.0F, static_cast<float>(width) - 64.0F);
    DrawCircleGradient(static_cast<int>(centerX), static_cast<int>(centerY - 120),
        cardWidth * 0.65F, Fade(accent, 0.075F), BLANK);
    std::uint32_t random = 0x51A75U;
    for (int index = 0; index < 48; ++index) {
        const float x = randomUnit(random) * width;
        const float starY = top + randomUnit(random) * (height - top);
        DrawCircleV({x, starY}, index % 5 == 0 ? 1.5F : 0.8F, Fade(kQuiet, 0.2F + (index % 3) * 0.1F));
    }
    if (won) drawFireworks(width, height, top);
    const float progress = std::clamp(age_ / kFadeDuration, 0.0F, 1.0F);
    const float eased = 1 - std::pow(1 - progress, 3.0F);
    const float cardY = centerY - 148 + (1 - eased) * 12;
    DrawRectangleRounded({centerX - cardWidth * 0.5F + 4, cardY + 8, cardWidth, 306},
        0.12F, 12, Fade(BLACK, 0.18F * progress));
    DrawRectangleRounded({centerX - cardWidth * 0.5F, cardY, cardWidth, 306},
        0.12F, 12, Fade(won ? Color{22, 37, 60, 255} : Color{43, 29, 44, 255}, progress));
    DrawRectangleRoundedLinesEx({centerX - cardWidth * 0.5F, cardY, cardWidth, 306},
        0.12F, 12, 1, Fade(accent, progress * 0.28F));
    const Vector2 badge{centerX, cardY};
    DrawCircleV(badge, 33, won ? Color{35, 47, 63, 255} : Color{63, 36, 47, 255});
    DrawRing(badge, 31, 32, 0, 360, 64, Fade(accent, progress * 0.7F));
    if (won) star(badge, 18, Fade(accent, progress));
    else {
        DrawLineEx({badge.x - 9, badge.y - 9}, {badge.x + 9, badge.y + 9}, 3.5F, Fade(accent, progress));
        DrawLineEx({badge.x + 9, badge.y - 9}, {badge.x - 9, badge.y + 9}, 3.5F, Fade(accent, progress));
    }
    const float offset = cardY + 148 - centerY;
    centered(text, tr(won ? Text::PuzzleComplete : Text::OutOfLives), centerX, centerY - 92 + offset,
        13, 3, Fade(kQuiet, progress));
    const char* title = tr(won ? Text::YouWon : Text::GameOver);
    const float titleSize = text.fitSize(title, std::floor(std::min(74.0F, cardWidth * 0.115F)), 2, cardWidth - 48);
    centered(text, title, centerX, centerY - 59 + offset + 3,
        titleSize, 2, Fade(BLACK, progress * 0.3F));
    centered(text, title, centerX, centerY - 59 + offset,
        titleSize, 2, Fade(accent, progress));
    centered(text, tr(won ? Text::VictoryDetail : Text::DefeatDetail),
        centerX, centerY + 25 + offset, 18, 0.2F, Fade(kLight, progress));
    for (int index = 0; index < 2; ++index) {
        const Rectangle bounds = buttonRectangle(index, width, height, top);
        const bool highlighted = hovered == index;
        const bool down = highlighted && pressed == index;
        const Color fill = index == 0 ? (down ? Fade(accent, 0.75F) : accent)
            : down ? Color{50, 66, 90, 255} : highlighted ? Color{40, 57, 81, 255} : Color{30, 46, 68, 255};
        DrawRectangleRounded(bounds, 0.25F, 8, Fade(fill, progress));
        if (index == 1) DrawRectangleRoundedLinesEx(bounds, 0.25F, 8, 1, Fade(kQuiet, highlighted ? 0.8F : 0.4F));
        centered(text, tr(index == 0 ? Text::PlayAgain : Text::MainMenu), bounds.x + bounds.width * 0.5F,
            bounds.y + 12, 17, 0.2F, Fade(index == 0 ? Color{28, 34, 48, 255} : kLight, progress), bounds.width - 24);
    }
    centered(text, tr(Text::EndHint), centerX, centerY + 135 + offset,
        12, 0.2F, Fade(kQuiet, progress * 0.7F));
    EndScissorMode();
}

} // namespace arrow
