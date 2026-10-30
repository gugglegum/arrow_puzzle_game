#pragma once

#include "arrow/Board.hpp"
#include "arrow/Localization.hpp"

#include <filesystem>
#include <optional>
#include <vector>

namespace arrow {

enum class BoardTheme { Light, Dark };
enum class StorageMode { User, Portable };

struct Settings {
    bool arrowLoops{true};
    bool flyingArrowCollisions{true};
    bool crosshairGuides{true};
    bool sounds{true};
    bool leftButtonDrag{true};
    BoardTheme boardTheme{BoardTheme::Dark};
    Language language{Language::Auto};

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
    // User mode uses Windows LocalAppData or Linux XDG directories. Portable
    // mode uses the executable's directory, or the outer AppImage's directory.
    explicit Persistence(StorageMode mode = StorageMode::User);
    explicit Persistence(std::filesystem::path directory);
    Persistence(
        std::filesystem::path settingsDirectory,
        std::filesystem::path saveDirectory
    );

    [[nodiscard]] static std::filesystem::path executableDirectory() noexcept;

    // The settings directory; settings and saves can have separate locations.
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
    [[nodiscard]] bool setLeftButtonDrag(Settings& settings, bool enabled) const noexcept;
    [[nodiscard]] bool setBoardTheme(Settings& settings, BoardTheme theme) const noexcept;
    [[nodiscard]] bool setLanguage(Settings& settings, Language language) const noexcept;

    [[nodiscard]] std::optional<SavedGame> loadSavedGame() const noexcept;
    [[nodiscard]] bool saveSavedGame(const SavedGame& game) const noexcept;
    [[nodiscard]] bool removeSavedGame() const noexcept;

private:
    std::filesystem::path directory_;
    std::filesystem::path saveDirectory_;
};

} // namespace arrow
