#include "arrow/Persistence.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#endif

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static unsigned long counter = 0;
        const auto stamp =
            std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("arrow-persistence-tests-" + std::to_string(stamp) + "-" +
             std::to_string(++counter));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}
    };
}

void writeText(
    const std::filesystem::path& path,
    std::string_view contents
) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << contents;
    require(static_cast<bool>(output), "Unable to prepare test file");
}

arrow::SavedGame sampleGame() {
    arrow::SavedGame result;
    result.columns = 4;
    result.rows = 3;
    result.lives = 2;
    result.arrows = {
        {
            {{3, 0}, {2, 0}, {1, 0}, {0, 0}},
            {1, 0}
        },
        {
            {{0, 1}, {1, 1}, {2, 1}, {3, 1}},
            {-1, 0}
        },
        {
            {{3, 2}, {2, 2}, {1, 2}, {0, 2}},
            {1, 0}
        }
    };
    result.present = {true, false, true};
    result.failed = {false, true, false};
    return result;
}

bool equal(const arrow::Arrow& lhs, const arrow::Arrow& rhs) {
    return lhs.direction == rhs.direction && lhs.cells == rhs.cells;
}

bool equal(const arrow::SavedGame& lhs, const arrow::SavedGame& rhs) {
    if (lhs.columns != rhs.columns || lhs.rows != rhs.rows ||
        lhs.lives != rhs.lives || lhs.present != rhs.present ||
        lhs.failed != rhs.failed ||
        lhs.arrows.size() != rhs.arrows.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.arrows.size(); ++index) {
        if (!equal(lhs.arrows[index], rhs.arrows[index])) {
            return false;
        }
    }
    return true;
}

void testSeparateDirectories() {
    TemporaryDirectory temporary;
    const auto config = temporary.path() / "config" / "arrow-puzzle";
    const auto data = temporary.path() / "data" / "arrow-puzzle";
    const arrow::Persistence persistence{config, data};
    auto settings = persistence.loadSettings();
    require(persistence.settingsPath() == config / "settings.txt", "Wrong settings path");
    require(persistence.saveGamePath() == data / "savegame.txt", "Wrong save path");
    require(std::filesystem::exists(persistence.settingsPath()), "Config directory was not created");
    require(persistence.setSounds(settings, false), "Separate settings write failed");
    const auto game = sampleGame();
    require(persistence.saveSavedGame(game), "Separate saved-game write failed");
    const arrow::Persistence reopened{config, data};
    require(!reopened.loadSettings().sounds, "Separate settings were not restored");
    const auto restored = reopened.loadSavedGame();
    require(restored.has_value() && equal(*restored, game), "Separate save was not restored");
    require(!std::filesystem::exists(config / "savegame.txt") &&
            !std::filesystem::exists(data / "settings.txt"), "Data leaked into the wrong directory");
    require(reopened.removeSavedGame() && std::filesystem::exists(reopened.settingsPath()),
            "Removing the save affected settings");
}

#if defined(__linux__)
class EnvironmentValue {
public:
    explicit EnvironmentValue(const char* name) : name_(name) {
        if (const char* value = std::getenv(name)) previous_ = value;
    }
    ~EnvironmentValue() {
        if (previous_) setenv(name_, previous_->c_str(), 1);
        else unsetenv(name_);
    }
    void set(const char* value) const {
        require((value ? setenv(name_, value, 1) : unsetenv(name_)) == 0,
                "Unable to set test environment");
    }
private:
    const char* name_;
    std::optional<std::string> previous_;
};
#endif

void testDefaultDirectories() {
#if defined(__linux__)
    TemporaryDirectory temporary;
    EnvironmentValue home{"HOME"};
    EnvironmentValue config{"XDG_CONFIG_HOME"};
    EnvironmentValue data{"XDG_DATA_HOME"};
    home.set(temporary.path().c_str());
    for (const char* invalid : {static_cast<const char*>(nullptr), "", "relative/path"}) {
        config.set(invalid);
        data.set(invalid);
        const arrow::Persistence persistence;
        require(persistence.settingsPath() == temporary.path() / ".config/arrow-puzzle/settings.txt",
                "Unset/empty/relative config override did not use HOME");
        require(persistence.saveGamePath() == temporary.path() / ".local/share/arrow-puzzle/savegame.txt",
                "Unset/empty/relative data override did not use HOME");
    }
    const auto configRoot = temporary.path() / "custom config";
    const auto dataRoot = temporary.path() / "custom data";
    config.set(configRoot.c_str());
    data.set(dataRoot.c_str());
    const arrow::Persistence overridden;
    require(overridden.settingsPath() == configRoot / "arrow-puzzle/settings.txt" &&
            overridden.saveGamePath() == dataRoot / "arrow-puzzle/savegame.txt",
            "Absolute XDG overrides were not respected");
    require(!std::filesystem::exists(configRoot), "Path discovery wrote settings prematurely");
    data.set(nullptr);
    require(arrow::Persistence{}.settingsPath() == overridden.settingsPath() &&
            arrow::Persistence{}.saveGamePath() == temporary.path() / ".local/share/arrow-puzzle/savegame.txt",
            "One XDG override affected the other path");
    config.set(nullptr);
    for (const char* invalid : {static_cast<const char*>(nullptr), "", "relative/home"}) {
        home.set(invalid);
        const arrow::Persistence persistence;
        require(persistence.settingsPath().is_absolute() && persistence.saveGamePath().is_absolute(),
                "Missing HOME did not resolve the account home");
        require(persistence.directory() != arrow::Persistence::executableDirectory(),
                "Missing HOME fell back to the program directory");
    }
#elif defined(_WIN32)
    PWSTR raw = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw);
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> folder(raw, CoTaskMemFree);
    require(SUCCEEDED(result) && raw != nullptr, "Unable to read LocalAppData for the test");
    const arrow::Persistence persistence;
    const auto directory = std::filesystem::path{raw} / "ArrowPuzzle";
    require(persistence.settingsPath() == directory / "settings.txt" &&
            persistence.saveGamePath() == directory / "savegame.txt",
            "Ordinary Windows data must use LocalAppData");
    require(directory != arrow::Persistence::executableDirectory(),
            "Ordinary Windows data fell back to the executable directory");
#endif
}

void testPortableDirectories() {
    const auto executable = arrow::Persistence::executableDirectory();
#if defined(__linux__)
    TemporaryDirectory temporary;
    EnvironmentValue config{"XDG_CONFIG_HOME"};
    EnvironmentValue data{"XDG_DATA_HOME"};
    EnvironmentValue appDir{"APPDIR"};
    EnvironmentValue image{"APPIMAGE"};
    const auto wrong = temporary.path() / "unrelated app";
    const auto outer = temporary.path() / "portable image";
    std::filesystem::create_directories(wrong);
    std::filesystem::create_directories(outer);
    const auto imageFile = outer / "RenamedGame";
    writeText(imageFile, "runtime fixture");
    config.set(wrong.c_str());
    data.set(wrong.c_str());
    appDir.set(wrong.c_str());
    image.set(imageFile.c_str());
#endif
    const arrow::Persistence standalone{arrow::StorageMode::Portable};
    require(standalone.settingsPath() == executable / "settings.txt" &&
            standalone.saveGamePath() == executable / "savegame.txt",
            "Standalone portable data must stay beside the executable");
#if defined(__linux__)
    appDir.set(executable.c_str());
    const arrow::Persistence bundled{arrow::StorageMode::Portable};
    require(bundled.settingsPath() == outer / "settings.txt" &&
            bundled.saveGamePath() == outer / "savegame.txt",
            "Portable AppImage data must stay beside the outer image");
    image.set("relative.AppImage");
    require(arrow::Persistence{arrow::StorageMode::Portable}.directory() == executable,
            "A relative AppImage override redirected portable data");
    require(!std::filesystem::exists(outer / "settings.txt"),
            "Portable path discovery created files prematurely");
#endif
}

void testDefaultSettingsAreCreated() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};

    const arrow::Settings settings = persistence.loadSettings();
    require(settings.arrowLoops, "Arrow loops must default to on");
    require(
        settings.flyingArrowCollisions,
        "Flying-arrow collisions must default to on"
    );
    require(settings.crosshairGuides, "Crosshair guides must default to on");
    require(settings.sounds, "Sounds must default to on");
    require(settings.leftButtonDrag, "Left-button dragging must default to on");
    require(settings.boardTheme == arrow::BoardTheme::Dark, "Board theme must default to dark");
    require(settings.language == arrow::Language::Auto, "Language must default to Auto");
    require(
        std::filesystem::is_regular_file(persistence.settingsPath()),
        "Loading missing settings must create settings.txt"
    );

    const std::string text = readText(persistence.settingsPath());
    require(
        text.find("version=1") != std::string::npos,
        "Settings file must be versioned"
    );
    require(
        text.find("arrow_loops=on") != std::string::npos,
        "Default arrow-loop setting was not written"
    );
    require(
        text.find("flying_arrow_collisions=on") != std::string::npos,
        "Default flying-arrow collision setting was not written"
    );
    require(
        text.find("crosshair_guides=on") != std::string::npos,
        "Default crosshair guide setting was not written"
    );
    require(text.find("sounds=on") != std::string::npos, "Default sound setting was not written");
    require(text.find("left_button_drag=on") != std::string::npos, "Default left-button drag setting was not written");
    require(text.find("board_theme=dark") != std::string::npos, "Default board theme was not written");
    require(text.find("language=auto") != std::string::npos, "Default language was not written");
}

void testLanguageUpgradeAndRepair() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    const std::string legacy = "format=arrow-puzzle-settings\nversion=1\narrow_loops=off\n"
        "flying_arrow_collisions=off\ncrosshair_guides=off\nsounds=off\nboard_theme=light\n";
    for (const char* suffix : {"", "language=unknown\n", "language=\n"}) {
        writeText(persistence.settingsPath(), legacy + suffix);
        auto settings = persistence.loadSettings();
        require(settings.language == arrow::Language::Auto, "Missing/invalid language must use Auto");
        require(!settings.arrowLoops && !settings.flyingArrowCollisions && !settings.crosshairGuides && !settings.sounds &&
                settings.boardTheme == arrow::BoardTheme::Light, "Language migration changed other settings");
        require(readText(persistence.settingsPath()).find("language=auto\n") != std::string::npos,
                "Language was not repaired canonically");
        for (int i = 0; i < static_cast<int>(arrow::Language::Count); ++i) {
            const auto language = static_cast<arrow::Language>(i);
            require(persistence.setLanguage(settings, language), "Language setter failed");
            require(persistence.loadSettings() == settings, "Language was not restored");
        }
        const auto previous = settings;
        const auto bytes = readText(persistence.settingsPath());
        require(!persistence.setLanguage(settings, arrow::Language::Count) && settings == previous &&
                readText(persistence.settingsPath()) == bytes, "Invalid language damaged settings");
    }
    const arrow::Persistence blocked{temporary.path() / "not-a-directory"};
    writeText(blocked.directory(), "file blocks settings directory");
    arrow::Settings settings;
    require(!blocked.setLanguage(settings, arrow::Language::Russian) && settings.language == arrow::Language::Auto,
            "Failed write changed language in memory");
}

void testMissingSettingsUseDefaultsAndPreserveExplicitOff() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    constexpr std::array<std::string_view, 5> keys{{
        "arrow_loops", "flying_arrow_collisions", "crosshair_guides", "sounds", "left_button_drag"
    }};
    constexpr std::array<bool arrow::Settings::*, 5> members{{
        &arrow::Settings::arrowLoops, &arrow::Settings::flyingArrowCollisions,
        &arrow::Settings::crosshairGuides, &arrow::Settings::sounds, &arrow::Settings::leftButtonDrag
    }};

    for (unsigned missing = 0; missing < (1U << keys.size()); ++missing) {
        std::string contents = "format=arrow-puzzle-settings\nversion=1\nboard_theme=light\n";
        arrow::Settings expected;
        expected.boardTheme = arrow::BoardTheme::Light;
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if ((missing & (1U << index)) == 0) {
                contents += std::string(keys[index]) + "=off\n";
                expected.*members[index] = false;
            }
        }
        writeText(persistence.settingsPath(), contents);
        require(persistence.loadSettings() == expected,
                "Missing keys must default to on without changing explicit off values or theme");
        require(persistence.loadSettings() == expected,
                "Repaired settings did not retain defaults and explicit off values");
        const std::string repaired = readText(persistence.settingsPath());
        for (std::size_t index = 0; index < keys.size(); ++index) {
            const std::string assignment = std::string(keys[index]) +
                (expected.*members[index] ? "=on\n" : "=off\n");
            require(repaired.find(assignment) != std::string::npos,
                    "A missing or explicit setting was not saved canonically");
        }
    }
}

void testSettingsRoundTripAndAutomaticSetters() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};

    arrow::Settings settings;
    settings.arrowLoops = false;
    settings.flyingArrowCollisions = false;
    require(
        persistence.saveSettings(settings),
        "Explicit settings save failed"
    );
    require(
        persistence.loadSettings() == settings,
        "Settings round trip changed values"
    );

    require(
        persistence.setArrowLoops(settings, true),
        "Arrow-loop auto-save failed"
    );
    require(settings.arrowLoops, "Arrow-loop setter did not update caller");
    require(
        persistence.loadSettings().arrowLoops,
        "Arrow-loop setter did not persist the value"
    );

    require(
        persistence.setFlyingArrowCollisions(settings, true),
        "Collision auto-save failed"
    );
    require(
        persistence.loadSettings() == arrow::Settings{},
        "Automatic setters did not persist both defaults"
    );
    require(persistence.setCrosshairGuides(settings, false), "Disabling crosshair guides failed");
    require(!settings.crosshairGuides && persistence.loadSettings() == settings,
            "Crosshair guides did not stay disabled");
    require(persistence.setCrosshairGuides(settings, true), "Crosshair guide auto-save failed");
    require(settings.crosshairGuides, "Crosshair guide setter did not update caller");
    require(persistence.loadSettings() == settings, "Crosshair guides were not persisted");
    require(persistence.setSounds(settings, false), "Disabling sounds failed");
    require(!settings.sounds && !persistence.loadSettings().sounds, "Sounds did not stay disabled");
    require(persistence.setSounds(settings, true), "Enabling sounds failed");
    require(settings.sounds && persistence.loadSettings().sounds, "Sounds did not stay enabled");
    require(persistence.setLeftButtonDrag(settings, false), "Disabling left-button dragging failed");
    require(!settings.leftButtonDrag && persistence.loadSettings() == settings,
            "Left-button dragging did not stay disabled");
    require(persistence.setLeftButtonDrag(settings, true), "Enabling left-button dragging failed");
    require(settings.leftButtonDrag && persistence.loadSettings() == settings,
            "Left-button dragging did not stay enabled");
    require(persistence.setBoardTheme(settings, arrow::BoardTheme::Light), "Choosing a light board failed");
    require(settings.boardTheme == arrow::BoardTheme::Light && persistence.loadSettings() == settings,
            "Light board theme did not persist");
    require(persistence.setBoardTheme(settings, arrow::BoardTheme::Dark), "Choosing a dark board failed");
    require(persistence.loadSettings() == arrow::Settings{}, "Dark board theme did not persist");
}

void testBoardThemeUpgradeAndRepair() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    const std::string legacy = "format=arrow-puzzle-settings\nversion=1\narrow_loops=off\n"
        "flying_arrow_collisions=off\ncrosshair_guides=on\nsounds=on\n";
    writeText(persistence.settingsPath(), legacy);
    const auto upgraded = persistence.loadSettings();
    require(!upgraded.arrowLoops && !upgraded.flyingArrowCollisions && upgraded.crosshairGuides && upgraded.sounds,
            "Adding board themes changed another preference");
    require(upgraded.boardTheme == arrow::BoardTheme::Dark &&
            readText(persistence.settingsPath()).find("board_theme=dark") != std::string::npos,
            "Legacy settings were not upgraded to a dark board");

    writeText(persistence.settingsPath(), legacy + "board_theme=light\n");
    auto expected = upgraded;
    expected.boardTheme = arrow::BoardTheme::Light;
    require(persistence.loadSettings() == expected, "A light board preference was not loaded");
    writeText(persistence.settingsPath(), legacy + "board_theme=invalid\n");
    require(persistence.loadSettings() == upgraded, "Repairing a board theme changed another preference");
    require(readText(persistence.settingsPath()).find("board_theme=dark") != std::string::npos,
            "Malformed board theme was not repaired");

    auto unchanged = upgraded;
    const auto before = readText(persistence.settingsPath());
    require(!persistence.setBoardTheme(unchanged, static_cast<arrow::BoardTheme>(99)) && unchanged == upgraded &&
            readText(persistence.settingsPath()) == before, "Invalid enum changed the active or saved theme");
    const auto blockedPath = temporary.path() / "not-a-directory";
    writeText(blockedPath, "keep");
    const arrow::Persistence blocked{blockedPath};
    require(!blocked.setBoardTheme(unchanged, arrow::BoardTheme::Light) && unchanged == upgraded,
            "Failed board theme write changed the active preference");
}

void testLeftButtonDragUpgradeAndRepair() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    const std::string legacy = "format=arrow-puzzle-settings\nversion=1\narrow_loops=off\n"
        "flying_arrow_collisions=off\ncrosshair_guides=off\nsounds=off\nboard_theme=light\nlanguage=ru\n";
    arrow::Settings expected;
    expected.arrowLoops = expected.flyingArrowCollisions = expected.crosshairGuides = expected.sounds = false;
    expected.boardTheme = arrow::BoardTheme::Light;
    expected.language = arrow::Language::Russian;
    for (const char* suffix : {"", "left_button_drag=invalid\n", "left_button_drag=\n"}) {
        writeText(persistence.settingsPath(), legacy + suffix);
        require(persistence.loadSettings() == expected,
                "Missing/invalid left-button dragging changed existing preferences");
        require(readText(persistence.settingsPath()).find("left_button_drag=on\n") != std::string::npos,
                "Left-button dragging was not repaired canonically");
    }
    const auto blockedPath = temporary.path() / "not-a-directory";
    writeText(blockedPath, "keep");
    const arrow::Persistence blocked{blockedPath};
    auto unchanged = expected;
    require(!blocked.setLeftButtonDrag(unchanged, false) && unchanged == expected && readText(blockedPath) == "keep",
            "Failed left-button drag write changed active settings or an unrelated file");
}

void testSoundsUpgradeAndRepair() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    writeText(persistence.settingsPath(),
        "format=arrow-puzzle-settings\nversion=1\narrow_loops=off\n"
        "flying_arrow_collisions=off\ncrosshair_guides=on\n");
    const auto upgraded = persistence.loadSettings();
    require(!upgraded.arrowLoops && !upgraded.flyingArrowCollisions && upgraded.crosshairGuides,
            "Adding sounds changed existing preferences");
    require(upgraded.sounds, "Missing sounds must default to on");
    require(readText(persistence.settingsPath()).find("sounds=on") != std::string::npos,
            "Legacy sound preference was not written");

    writeText(persistence.settingsPath(),
        "format=arrow-puzzle-settings\nversion=1\narrow_loops=off\n"
        "flying_arrow_collisions=on\ncrosshair_guides=on\nsounds=invalid\n");
    const auto repaired = persistence.loadSettings();
    require(repaired.sounds && !repaired.arrowLoops &&
            repaired.flyingArrowCollisions && repaired.crosshairGuides,
            "Repairing sounds changed another preference");
    require(readText(persistence.settingsPath()).find("sounds=on") != std::string::npos,
            "Malformed sound setting was not repaired");

    const auto blockedPath = temporary.path() / "not-a-directory";
    writeText(blockedPath, "keep");
    const arrow::Persistence blocked{blockedPath};
    arrow::Settings unchanged;
    require(!blocked.setSounds(unchanged, false) && unchanged.sounds,
            "Failed sound write changed the active preference");
}

void testCrosshairGuidesUpgradeAndRepair() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    writeText(persistence.settingsPath(),
        "format=arrow-puzzle-settings\nversion=1\n"
        "arrow_loops=off\nflying_arrow_collisions=off\n");
    const arrow::Settings upgraded = persistence.loadSettings();
    require(!upgraded.arrowLoops && !upgraded.flyingArrowCollisions,
            "Adding crosshair guides changed legacy preferences");
    require(upgraded.crosshairGuides, "Missing crosshair guides must default to on");
    require(readText(persistence.settingsPath()).find("crosshair_guides=on") != std::string::npos,
            "Legacy settings were not upgraded");

    writeText(persistence.settingsPath(),
        "format=arrow-puzzle-settings\nversion=1\n"
        "arrow_loops=off\nflying_arrow_collisions=on\ncrosshair_guides=invalid\n");
    const arrow::Settings repaired = persistence.loadSettings();
    require(!repaired.arrowLoops && repaired.flyingArrowCollisions && repaired.crosshairGuides,
            "Malformed crosshair guides changed other preferences");
    require(readText(persistence.settingsPath()).find("crosshair_guides=on") != std::string::npos,
            "Invalid crosshair guide preference was not repaired");

    const auto blockedPath = temporary.path() / "not-a-directory";
    writeText(blockedPath, "keep");
    const arrow::Persistence blocked{blockedPath};
    arrow::Settings unchanged;
    require(!blocked.setCrosshairGuides(unchanged, false), "Unwritable setting falsely succeeded");
    require(unchanged.crosshairGuides, "Failed write changed the active preference");
    require(readText(blockedPath) == "keep", "Failed settings write changed an unrelated file");
}

void testMalformedSettingsAreRepaired() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    writeText(
        persistence.settingsPath(),
        "format=arrow-puzzle-settings\n"
        "version=1\n"
        "arrow_loops=not-a-boolean\n"
        "flying_arrow_collisions=off\n"
    );

    const arrow::Settings settings = persistence.loadSettings();
    require(
        settings.arrowLoops,
        "Invalid setting should fall back to its default"
    );
    require(
        !settings.flyingArrowCollisions,
        "A valid setting next to an invalid setting should be preserved"
    );
    const std::string repaired = readText(persistence.settingsPath());
    require(
        repaired.find("arrow_loops=on") != std::string::npos &&
            repaired.find("flying_arrow_collisions=off") !=
                std::string::npos,
        "Malformed settings were not rewritten canonically"
    );

    writeText(
        persistence.settingsPath(),
        "format=some-other-program\n"
        "version=99\n"
        "arrow_loops=off\n"
        "flying_arrow_collisions=off\n"
    );
    require(
        persistence.loadSettings() == arrow::Settings{},
        "Unknown settings formats must fall back to all defaults"
    );
}

void testSavedGameRoundTrip() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    const arrow::SavedGame original = sampleGame();

    require(arrow::isValidSavedGame(original), "Sample game is invalid");
    require(persistence.saveSavedGame(original), "Saved-game write failed");
    const std::optional<arrow::SavedGame> loaded =
        persistence.loadSavedGame();
    require(loaded.has_value(), "Saved-game read failed");
    require(equal(*loaded, original), "Saved-game round trip changed state");

    const std::string text = readText(persistence.saveGamePath());
    require(
        text.find("format=arrow-puzzle-save") != std::string::npos &&
            text.find("version=1") != std::string::npos,
        "Saved-game format must be identified and versioned"
    );
    require(
        text.find("seed") == std::string::npos,
        "Saved games must store geometry, not a generator seed"
    );
    require(
        text.find("arrow=1,0;4;3,0;2,0;1,0;0,0") !=
            std::string::npos,
        "Saved game does not contain complete arrow geometry"
    );
    require(
        !std::filesystem::exists(
            std::filesystem::path(persistence.saveGamePath().string() + ".tmp")
        ),
        "Successful atomic save left a temporary file"
    );
}

void testInvalidGamesAreRejectedWithoutDestroyingOldSave() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    const arrow::SavedGame valid = sampleGame();
    require(persistence.saveSavedGame(valid), "Initial save failed");
    const std::string originalText = readText(persistence.saveGamePath());

    arrow::SavedGame invalid = valid;
    invalid.arrows[1].direction = {1, 1};
    require(
        !arrow::isValidSavedGame(invalid),
        "Diagonal arrow direction was accepted"
    );
    require(
        !persistence.saveSavedGame(invalid),
        "Invalid game was written"
    );
    require(
        readText(persistence.saveGamePath()) == originalText,
        "Rejected save destroyed the previous valid save"
    );

    invalid = valid;
    invalid.arrows[2].cells[0] = invalid.arrows[0].cells[0];
    require(
        !arrow::isValidSavedGame(invalid),
        "Overlapping arrows were accepted"
    );

    invalid = valid;
    invalid.present.pop_back();
    require(
        !arrow::isValidSavedGame(invalid),
        "Mismatched presence flags were accepted"
    );

    invalid = valid;
    invalid.arrows[0].cells[0] = {-1, 0};
    require(
        !arrow::isValidSavedGame(invalid),
        "Out-of-board geometry was accepted"
    );

    invalid = valid;
    invalid.lives = 0;
    require(
        !arrow::isValidSavedGame(invalid),
        "A game with no remaining lives was accepted"
    );

    invalid = valid;
    invalid.present.assign(invalid.present.size(), false);
    require(
        !arrow::isValidSavedGame(invalid),
        "A completed game was accepted as resumable"
    );

    invalid = valid;
    invalid.arrows[0].cells.pop_back();
    require(
        !arrow::isValidSavedGame(invalid),
        "An incompletely covered board was accepted"
    );
}

void testCorruptAndUnsupportedSavesAreIgnored() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};

    writeText(
        persistence.saveGamePath(),
        "format=arrow-puzzle-save\n"
        "version=99\n"
    );
    require(
        !persistence.loadSavedGame().has_value(),
        "Unsupported saved-game version was accepted"
    );

    writeText(
        persistence.saveGamePath(),
        "format=arrow-puzzle-save\n"
        "version=1\n"
        "columns=4\n"
        "rows=3\n"
        "lives=2\n"
        "arrow_count=1\n"
        "arrow=1,0;2;1,0;0,0\n"
        "present=2\n"
        "failed=0\n"
        "end=1\n"
    );
    require(
        !persistence.loadSavedGame().has_value(),
        "Invalid saved-game flags were accepted"
    );

    writeText(
        persistence.saveGamePath(),
        "format=arrow-puzzle-save\n"
        "version=1\n"
        "columns=1000000000\n"
        "rows=1000000000\n"
        "lives=3\n"
        "arrow_count=0\n"
        "present=\n"
        "failed=\n"
        "end=1\n"
    );
    require(
        !persistence.loadSavedGame().has_value(),
        "Unreasonably large saved game was accepted"
    );
}

void testManualSavesAreIndependentSnapshots() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path() / "config", temporary.path() / "data"};
    const auto game = sampleGame();
    require(persistence.saveSavedGame(game), "Autosave setup failed");
    const auto autosave = readText(persistence.saveGamePath());
    require(persistence.manualSavesDirectory() == temporary.path() / "data/saves", "Manual save location is wrong");
    const auto empty = persistence.listManualSaves();
    require(empty && empty->entries.empty() && empty->unreadable == 0, "Missing directory is not an empty catalog");
    std::vector<std::string> ids;
    for (int i = 0; i < 6; ++i) {
        const auto id = persistence.createManualSave(game, "Попытка / café = 你好");
        require(id.has_value(), "Unicode save creation failed");
        require(std::ranges::find(ids, *id) == ids.end(), "Manual save overwrote a duplicate name");
        ids.push_back(*id);
        const auto loaded = persistence.loadManualSave(*id);
        require(loaded && equal(*loaded, game), "Manual save changed geometry, lives or flags");
        const auto path = persistence.manualSavesDirectory() / (*id + ".arrow");
        require(readText(path).find("version=2\nname=") != std::string::npos, "Manual metadata is not versioned");
        require(!std::filesystem::exists(path.string() + ".tmp"), "Temporary file left after manual save");
    }
    const auto catalog = persistence.listManualSaves();
    require(catalog && catalog->entries.size() == 6, "Catalog lost same-name saves");
    for (std::size_t i = 0; i < catalog->entries.size(); ++i) {
        const auto& entry = catalog->entries[i];
        require(entry.name == "Попытка / café = 你好" && entry.columns == 4 && entry.rows == 3 &&
            entry.lives == 2 && entry.remaining == 2 && entry.total == 3 && entry.created > 0,
            "Catalog summary differs from saved game");
        if (i > 0) require(catalog->entries[i-1].created >= entry.created, "Catalog is not newest first");
    }
    require(persistence.renameManualSave(ids.front(), "Новая попытка"), "Rename failed");
    require(!persistence.renameManualSave(ids.front(), "bad\nname"), "Invalid rename succeeded");
    const auto renamed = persistence.listManualSaves();
    const auto entry = std::ranges::find(renamed->entries, ids.front(), &arrow::ManualSave::id);
    require(entry != renamed->entries.end() && entry->name == "Новая попытка", "Rename lost title");
    const auto oldEntry = std::ranges::find(catalog->entries, ids.front(), &arrow::ManualSave::id);
    require(entry->created == oldEntry->created, "Rename changed creation time");
    auto loaded = persistence.loadManualSave(ids.front());
    require(loaded && equal(*loaded, game), "Rename changed snapshot");
    loaded->lives = 1;
    require(persistence.saveSavedGame(*loaded), "Loaded-copy autosave failed");
    require(equal(*persistence.loadManualSave(ids.front()), game), "Playing a loaded copy changed its snapshot");
    require(persistence.removeSavedGame() && persistence.loadManualSave(ids.front()).has_value(),
        "Clearing autosave removed manual saves");
    require(persistence.saveSavedGame(game), "Autosave recreation failed");
    require(persistence.removeManualSave(ids.front()) && persistence.removeManualSave(ids.front()), "Manual removal failed");
    require(!persistence.loadManualSave(ids.front()), "Deleted manual save remains loadable");
    require(readText(persistence.saveGamePath()) == autosave, "Manual operations changed autosave");
    require(persistence.listManualSaves()->entries.size() == 5, "Deleting one snapshot affected others");
}

void testManualSaveFailuresAndValidation() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    const auto game = sampleGame();
    for (const auto name : {"", " ", " leading", "trailing ", "a\nb", "a\rb", "a\tb", "\xC0\xAF", "\xED\xA0\x80"}) {
        require(!arrow::isValidSaveName(name) && !persistence.createManualSave(game, name), "Invalid name accepted");
    }
    require(arrow::isValidSaveName(std::string(80, 'a')) && !arrow::isValidSaveName(std::string(81, 'a')),
        "Name limit is incorrect");
    std::string unicode;
    for (int i = 0; i < 80; ++i) unicode += "я";
    require(arrow::isValidSaveName(unicode) && !arrow::isValidSaveName(unicode + "я"), "UTF-8 character limit is incorrect");
    auto invalid = game; invalid.lives = -1;
    require(!persistence.createManualSave(invalid, "invalid"), "Invalid game accepted as manual save");
    const auto id = *persistence.createManualSave(game, "good");
    for (const auto unsafe : {"../savegame", "../1", "1/2", "1\\2", "/1", "", "1.arrow"}) {
        require(!persistence.loadManualSave(unsafe) && !persistence.renameManualSave(unsafe, "bad") &&
            !persistence.removeManualSave(unsafe), "Unsafe ID reached the filesystem");
    }
    writeText(persistence.manualSavesDirectory() / "123-1.arrow", "corrupt");
    writeText(persistence.manualSavesDirectory() / "123-2.arrow.tmp", "interrupted write");
    const auto catalog = persistence.listManualSaves();
    require(catalog && catalog->entries.size() == 1 && catalog->unreadable == 1, "Corrupt save broke valid catalog entries");
    require(!persistence.loadManualSave("123-1") && !persistence.renameManualSave("123-1", "bad"),
        "Corrupt manual file accepted");
    // A failed atomic replacement must leave the old title and geometry intact.
    const auto path = persistence.manualSavesDirectory() / (id + ".arrow");
    const auto before = readText(path);
    std::filesystem::create_directory(path.string() + ".tmp");
    writeText(std::filesystem::path(path.string() + ".tmp") / "blocker", "keep nonempty");
    require(!persistence.renameManualSave(id, "replacement") && readText(path) == before,
        "Failed rename damaged existing snapshot");
    TemporaryDirectory blocked;
    const arrow::Persistence noDirectory{blocked.path()};
    writeText(noDirectory.manualSavesDirectory(), "blocks directory creation");
    require(!noDirectory.listManualSaves() && !noDirectory.createManualSave(game, "blocked"),
        "Filesystem failure was reported as success");
}

void testRemoveSavedGameIsIdempotent() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    require(
        persistence.removeSavedGame(),
        "Removing a missing saved game should succeed"
    );
    require(
        persistence.saveSavedGame(sampleGame()),
        "Saved-game setup failed"
    );
    require(persistence.removeSavedGame(), "Saved-game removal failed");
    require(
        !persistence.loadSavedGame().has_value(),
        "Removed saved game is still loadable"
    );
    require(
        persistence.removeSavedGame(),
        "Repeated saved-game removal should succeed"
    );
}

} // namespace

int main() {
    try {
        testDefaultDirectories();
        testPortableDirectories();
        testSeparateDirectories();
        testDefaultSettingsAreCreated();
        testLanguageUpgradeAndRepair();
        testMissingSettingsUseDefaultsAndPreserveExplicitOff();
        testSettingsRoundTripAndAutomaticSetters();
        testBoardThemeUpgradeAndRepair();
        testLeftButtonDragUpgradeAndRepair();
        testSoundsUpgradeAndRepair();
        testCrosshairGuidesUpgradeAndRepair();
        testMalformedSettingsAreRepaired();
        testSavedGameRoundTrip();
        testInvalidGamesAreRejectedWithoutDestroyingOldSave();
        testCorruptAndUnsupportedSavesAreIgnored();
        testRemoveSavedGameIsIdempotent();
        testManualSavesAreIndependentSnapshots();
        testManualSaveFailuresAndValidation();
        std::cout << "Persistence tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Persistence test failure: "
                  << exception.what() << '\n';
        return 1;
    }
}
