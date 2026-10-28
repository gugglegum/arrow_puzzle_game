#pragma once

#include "arrow/Board.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace arrow {

struct Settings {
    bool arrowLoops{true};
    bool flyingArrowCollisions{true};
    bool crosshairGuides{false};
    bool sounds{false};

    auto operator<=>(const Settings&) const = default;
};

struct SavedGame {
    int columns{};
    int rows{};
    int lives{};
    std::vector<Arrow> arrows;
    std::vector<bool> present;
    std::vector<bool> failed;
};

[[nodiscard]] bool isValidSavedGame(const SavedGame& game) noexcept;

// Persistence never throws for file I/O. A failed settings load returns the
// defaults, while a missing or invalid saved game returns std::nullopt.
class Persistence {
public:
    explicit Persistence(std::filesystem::path directory);

    [[nodiscard]] static std::filesystem::path executableDirectory() noexcept;

    [[nodiscard]] const std::filesystem::path& directory() const noexcept;
    [[nodiscard]] std::filesystem::path settingsPath() const;
    [[nodiscard]] std::filesystem::path saveGamePath() const;

    // Creates a canonical settings.txt with default values when no valid
    // settings file exists.
    [[nodiscard]] Settings loadSettings() const noexcept;
    [[nodiscard]] bool saveSettings(const Settings& settings) const noexcept;

    // These helpers update the caller's value only after it has been saved.
    [[nodiscard]] bool setArrowLoops(
        Settings& settings,
        bool enabled
    ) const noexcept;
    [[nodiscard]] bool setFlyingArrowCollisions(
        Settings& settings,
        bool enabled
    ) const noexcept;
    [[nodiscard]] bool setCrosshairGuides(
        Settings& settings,
        bool enabled
    ) const noexcept;
    [[nodiscard]] bool setSounds(
        Settings& settings,
        bool enabled
    ) const noexcept;

    [[nodiscard]] std::optional<SavedGame> loadSavedGame() const noexcept;
    [[nodiscard]] bool saveSavedGame(const SavedGame& game) const noexcept;
    [[nodiscard]] bool removeSavedGame() const noexcept;

private:
    std::filesystem::path directory_;
};

} // namespace arrow
