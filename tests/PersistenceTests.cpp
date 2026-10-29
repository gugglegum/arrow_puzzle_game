#include "arrow/Persistence.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
#else
    const arrow::Persistence persistence;
    const auto directory = arrow::Persistence::executableDirectory();
    require(persistence.settingsPath() == directory / "settings.txt" &&
            persistence.saveGamePath() == directory / "savegame.txt",
            "Portable persistence paths changed");
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
    require(settings.boardTheme == arrow::BoardTheme::Dark, "Board theme must default to dark");
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
    require(text.find("board_theme=dark") != std::string::npos, "Default board theme was not written");
}

void testMissingSettingsUseDefaultsAndPreserveExplicitOff() {
    TemporaryDirectory temporary;
    const arrow::Persistence persistence{temporary.path()};
    constexpr std::array<std::string_view, 4> keys{{
        "arrow_loops", "flying_arrow_collisions", "crosshair_guides", "sounds"
    }};
    constexpr std::array<bool arrow::Settings::*, 4> members{{
        &arrow::Settings::arrowLoops, &arrow::Settings::flyingArrowCollisions,
        &arrow::Settings::crosshairGuides, &arrow::Settings::sounds
    }};

    for (unsigned missing = 0; missing < 16; ++missing) {
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
        testSeparateDirectories();
        testDefaultSettingsAreCreated();
        testMissingSettingsUseDefaultsAndPreserveExplicitOff();
        testSettingsRoundTripAndAutomaticSetters();
        testBoardThemeUpgradeAndRepair();
        testSoundsUpgradeAndRepair();
        testCrosshairGuidesUpgradeAndRepair();
        testMalformedSettingsAreRepaired();
        testSavedGameRoundTrip();
        testInvalidGamesAreRejectedWithoutDestroyingOldSave();
        testCorruptAndUnsupportedSavesAreIgnored();
        testRemoveSavedGameIsIdempotent();
        std::cout << "Persistence tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Persistence test failure: "
                  << exception.what() << '\n';
        return 1;
    }
}
