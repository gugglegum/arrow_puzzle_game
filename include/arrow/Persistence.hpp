#pragma once

#include "arrow/Board.hpp"
#include "arrow/Localization.hpp"

#include <filesystem>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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

struct ManualSave {
    std::string id;
    std::string name;
    std::int64_t created{}; // Unix seconds, unchanged by renaming/loading.
    int columns{}, rows{}, lives{};
    std::size_t remaining{}, total{};
};

struct SaveCatalog {
    std::vector<ManualSave> entries; // Newest first; IDs break timestamp ties.
    std::size_t unreadable{};
};

// UTF-8, at most 80 characters, no controls or leading/trailing ASCII spaces.
[[nodiscard]] bool isValidSaveName(std::string_view name) noexcept;

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

    [[nodiscard]] std::filesystem::path manualSavesDirectory() const;
    [[nodiscard]] std::optional<SaveCatalog> listManualSaves() const noexcept;
    [[nodiscard]] std::optional<std::string> createManualSave(
        const SavedGame& game, std::string_view name) const noexcept;
    [[nodiscard]] std::optional<SavedGame> loadManualSave(std::string_view id) const noexcept;
    [[nodiscard]] bool renameManualSave(std::string_view id, std::string_view name) const noexcept;
    [[nodiscard]] bool removeManualSave(std::string_view id) const noexcept;

private:
    std::filesystem::path directory_;
    std::filesystem::path saveDirectory_;
};

} // namespace arrow
