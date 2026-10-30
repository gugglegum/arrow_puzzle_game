#include "arrow/Persistence.hpp"

#include "arrow/Types.hpp"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#elif defined(__linux__)
#include <cerrno>
#include <pwd.h>
#include <unistd.h>
#endif

namespace arrow {
namespace {

constexpr std::string_view kSettingsFormat = "arrow-puzzle-settings";
constexpr std::string_view kSaveGameFormat = "arrow-puzzle-save";
constexpr int kSettingsVersion = 1;
constexpr int kSaveGameVersion = 1;
constexpr std::uint64_t kMaximumBoardCells = 100'000'000;
constexpr std::size_t kMaximumLineLength = 64U * 1024U * 1024U;

std::filesystem::path defaultDirectory(
    [[maybe_unused]] const char* variable,
    [[maybe_unused]] const char* fallback
) {
#if defined(__linux__)
    if (const char* value = std::getenv(variable); value != nullptr) {
        const std::filesystem::path path{value};
        if (path.is_absolute()) return path / "arrow-puzzle";
    }
    std::filesystem::path home;
    if (const char* value = std::getenv("HOME"); value != nullptr) home = value;
    if (!home.is_absolute()) {
        // Desktop launchers normally supply HOME. Resolve the account home if
        // it is missing/invalid instead of writing beside a read-only program.
        std::vector<char> buffer(16384);
        passwd account{};
        passwd* result = nullptr;
        int error = 0;
        do {
            error = getpwuid_r(getuid(), &account, buffer.data(), buffer.size(), &result);
            if (error == ERANGE) buffer.resize(buffer.size() * 2);
        } while (error == ERANGE && buffer.size() <= 1024 * 1024);
        if (error == 0 && result != nullptr && result->pw_dir != nullptr) {
            home = result->pw_dir;
        }
    }
    if (!home.is_absolute()) {
        throw std::runtime_error("Cannot determine the user home directory for saved data");
    }
    return home / fallback / "arrow-puzzle";
#elif defined(_WIN32)
    PWSTR raw = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &raw);
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> folder(raw, CoTaskMemFree);
    if (FAILED(result) || raw == nullptr) {
        throw std::runtime_error("Cannot determine LocalAppData for saved data");
    }
    return std::filesystem::path{raw} / "ArrowPuzzle";
#else
    return Persistence::executableDirectory();
#endif
}

std::filesystem::path portableDirectory() {
    const auto executable = Persistence::executableDirectory();
#if defined(__linux__)
    const char* imageValue = std::getenv("APPIMAGE");
    const char* appDirValue = std::getenv("APPDIR");
    if (imageValue != nullptr && appDirValue != nullptr) {
        const std::filesystem::path image{imageValue};
        const std::filesystem::path appDir{appDirValue};
        if (image.is_absolute() && appDir.is_absolute()) {
            std::error_code error;
            const auto root = std::filesystem::weakly_canonical(appDir, error);
            if (!error) {
                const auto program = std::filesystem::weakly_canonical(executable, error);
                if (!error) {
                    const auto relative = program.lexically_relative(root);
                    // An AppImage-hosted terminal/agent can export these variables
                    // to an unrelated standalone game. Only honor our own AppDir.
                    if (!relative.empty() && !relative.is_absolute() &&
                        *relative.begin() != "..") {
                        return image.parent_path();
                    }
                }
            }
        }
    }
#endif
    return executable;
}

std::string_view trim(std::string_view value) noexcept {
    while (!value.empty() &&
           (value.front() == ' ' || value.front() == '\t' ||
            value.front() == '\r' || value.front() == '\n')) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           (value.back() == ' ' || value.back() == '\t' ||
            value.back() == '\r' || value.back() == '\n')) {
        value.remove_suffix(1);
    }
    return value;
}

bool parseInt(std::string_view text, int& result) noexcept {
    text = trim(text);
    if (text.empty()) {
        return false;
    }
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool parseSize(std::string_view text, std::size_t& result) noexcept {
    text = trim(text);
    if (text.empty()) {
        return false;
    }
    std::uint64_t parsedValue{};
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const auto parsed = std::from_chars(begin, end, parsedValue);
    if (parsed.ec != std::errc{} || parsed.ptr != end ||
        parsedValue > std::numeric_limits<std::size_t>::max()) {
        return false;
    }
    result = static_cast<std::size_t>(parsedValue);
    return true;
}

bool parseBool(std::string_view text, bool& result) noexcept {
    text = trim(text);
    if (text == "1" || text == "true" || text == "on") {
        result = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "off") {
        result = false;
        return true;
    }
    return false;
}

bool splitAssignment(
    std::string_view line,
    std::string_view& key,
    std::string_view& value
) noexcept {
    const std::size_t separator = line.find('=');
    if (separator == std::string_view::npos) {
        return false;
    }
    key = trim(line.substr(0, separator));
    value = trim(line.substr(separator + 1));
    return !key.empty();
}

bool replaceFile(
    const std::filesystem::path& source,
    const std::filesystem::path& destination
) noexcept {
#if defined(_WIN32)
    return MoveFileExW(
               source.c_str(),
               destination.c_str(),
               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
           ) != FALSE;
#else
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    return !error;
#endif
}

bool writeAtomically(
    const std::filesystem::path& path,
    const std::function<bool(std::ostream&)>& writer
) noexcept {
    try {
        std::error_code error;
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path(), error);
            if (error) {
                return false;
            }
        }

        std::filesystem::path temporary = path;
        temporary += ".tmp";
        std::filesystem::remove(temporary, error);
        error.clear();

        {
            std::ofstream output(
                temporary,
                std::ios::binary | std::ios::trunc
            );
            if (!output || !writer(output)) {
                output.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
            output.flush();
            if (!output) {
                output.close();
                std::filesystem::remove(temporary, error);
                return false;
            }
        }

        if (!replaceFile(temporary, path)) {
            std::filesystem::remove(temporary, error);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool readLine(std::istream& input, std::string& line) {
    if (!std::getline(input, line) || line.size() > kMaximumLineLength) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

std::vector<std::string_view> split(
    std::string_view value,
    char delimiter
) {
    std::vector<std::string_view> parts;
    while (true) {
        const std::size_t separator = value.find(delimiter);
        parts.push_back(value.substr(0, separator));
        if (separator == std::string_view::npos) {
            break;
        }
        value.remove_prefix(separator + 1);
    }
    return parts;
}

bool parsePair(std::string_view text, int& first, int& second) noexcept {
    const std::size_t separator = text.find(',');
    if (separator == std::string_view::npos ||
        text.find(',', separator + 1) != std::string_view::npos) {
        return false;
    }
    return parseInt(text.substr(0, separator), first) &&
           parseInt(text.substr(separator + 1), second);
}

bool parseArrow(std::string_view text, Arrow& result) {
    const std::vector<std::string_view> parts = split(text, ';');
    if (parts.size() < 3) {
        return false;
    }

    int directionX{};
    int directionY{};
    std::size_t cellCount{};
    if (!parsePair(parts[0], directionX, directionY) ||
        !parseSize(parts[1], cellCount) ||
        cellCount == 0 || cellCount > parts.size() - 2 ||
        parts.size() != cellCount + 2) {
        return false;
    }

    result.direction = {directionX, directionY};
    result.cells.clear();
    result.cells.reserve(cellCount);
    for (std::size_t index = 0; index < cellCount; ++index) {
        int x{};
        int y{};
        if (!parsePair(parts[index + 2], x, y)) {
            return false;
        }
        result.cells.push_back({x, y});
    }
    return true;
}

bool parseFlags(
    std::string_view text,
    std::size_t expectedCount,
    std::vector<bool>& result
) {
    text = trim(text);
    if (text.size() != expectedCount) {
        return false;
    }
    result.clear();
    result.reserve(expectedCount);
    for (char value : text) {
        if (value != '0' && value != '1') {
            return false;
        }
        result.push_back(value == '1');
    }
    return true;
}

bool writeFlags(std::ostream& output, const std::vector<bool>& flags) {
    for (bool value : flags) {
        output.put(value ? '1' : '0');
    }
    output.put('\n');
    return static_cast<bool>(output);
}

} // namespace

bool isValidSavedGame(const SavedGame& game) noexcept {
    try {
        if (game.columns <= 0 || game.rows <= 0 ||
            game.lives <= 0 || game.lives > 3 ||
            game.arrows.empty() ||
            game.arrows.size() != game.present.size() ||
            game.arrows.size() != game.failed.size()) {
            return false;
        }

        const std::uint64_t area =
            static_cast<std::uint64_t>(game.columns) *
            static_cast<std::uint64_t>(game.rows);
        if (area == 0 || area > kMaximumBoardCells ||
            game.arrows.size() > area) {
            return false;
        }

        // A byte per board cell is substantially smaller than a hash set for
        // large boards (for example, 2000x2000 needs about four MiB here).
        std::vector<unsigned char> occupied(
            static_cast<std::size_t>(area),
            0
        );
        for (const Arrow& value : game.arrows) {
            if (value.cells.empty() || !isCardinal(value.direction)) {
                return false;
            }
            if (value.cells.size() > 1 &&
                value.cells[0] - value.cells[1] != value.direction) {
                return false;
            }
            for (std::size_t index = 0; index < value.cells.size(); ++index) {
                const Cell cell = value.cells[index];
                if (cell.x < 0 || cell.x >= game.columns ||
                    cell.y < 0 || cell.y >= game.rows) {
                    return false;
                }
                if (index > 0 &&
                    !isCardinal(cell - value.cells[index - 1])) {
                    return false;
                }

                const std::uint64_t position =
                    static_cast<std::uint64_t>(cell.y) *
                        static_cast<std::uint64_t>(game.columns) +
                    static_cast<std::uint64_t>(cell.x);
                if (occupied[static_cast<std::size_t>(position)] != 0) {
                    return false;
                }
                occupied[static_cast<std::size_t>(position)] = 1;
            }
        }
        return std::ranges::all_of(
                   occupied,
                   [](unsigned char value) {
                       return value != 0;
                   }
               ) &&
               std::ranges::any_of(
                   game.present,
                   [](bool value) {
                       return value;
                   }
               );
    } catch (...) {
        return false;
    }
}

Persistence::Persistence(StorageMode mode)
    : directory_(mode == StorageMode::Portable ? portableDirectory()
          : defaultDirectory("XDG_CONFIG_HOME", ".config")),
      saveDirectory_(mode == StorageMode::Portable ? directory_
          : defaultDirectory("XDG_DATA_HOME", ".local/share")) {}

Persistence::Persistence(std::filesystem::path directory)
    : Persistence(directory, directory) {}

Persistence::Persistence(
    std::filesystem::path settingsDirectory,
    std::filesystem::path saveDirectory
) : directory_(std::move(settingsDirectory)), saveDirectory_(std::move(saveDirectory)) {
    if (directory_.empty()) {
        directory_ = ".";
    }
    if (saveDirectory_.empty()) {
        saveDirectory_ = ".";
    }
}

std::filesystem::path Persistence::executableDirectory() noexcept {
    try {
#if defined(_WIN32)
        std::wstring buffer(512, L'\0');
        while (buffer.size() <= 32768) {
            const DWORD length = GetModuleFileNameW(
                nullptr,
                buffer.data(),
                static_cast<DWORD>(buffer.size())
            );
            if (length == 0) {
                break;
            }
            if (length < buffer.size()) {
                buffer.resize(length);
                return std::filesystem::path(buffer).parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
#elif defined(__linux__)
        std::error_code linkError;
        const std::filesystem::path executable =
            std::filesystem::read_symlink("/proc/self/exe", linkError);
        if (!linkError && !executable.empty()) {
            return executable.parent_path();
        }
#endif
        std::error_code currentError;
        const std::filesystem::path current =
            std::filesystem::current_path(currentError);
        return currentError ? std::filesystem::path{"."} : current;
    } catch (...) {
        return ".";
    }
}

const std::filesystem::path& Persistence::directory() const noexcept {
    return directory_;
}

std::filesystem::path Persistence::settingsPath() const {
    return directory_ / "settings.txt";
}

std::filesystem::path Persistence::saveGamePath() const {
    return saveDirectory_ / "savegame.txt";
}

Settings Persistence::loadSettings() const noexcept {
    Settings settings;
    bool canonical = true;
    try {
        std::ifstream input(settingsPath(), std::ios::binary);
        if (!input) {
            (void)saveSettings(settings);
            return settings;
        }

        bool formatSeen = false;
        bool versionSeen = false;
        bool arrowLoopsSeen = false;
        bool collisionsSeen = false;
        bool crosshairGuidesSeen = false;
        bool soundsSeen = false;
        bool boardThemeSeen = false;
        std::string line;
        while (std::getline(input, line)) {
            if (line.size() > kMaximumLineLength) {
                canonical = false;
                break;
            }
            const std::string_view stripped = trim(line);
            if (stripped.empty() || stripped.front() == '#') {
                continue;
            }

            std::string_view key;
            std::string_view value;
            if (!splitAssignment(stripped, key, value)) {
                canonical = false;
                continue;
            }
            if (key == "format") {
                formatSeen = value == kSettingsFormat;
                canonical = canonical && formatSeen;
            } else if (key == "version") {
                int version{};
                versionSeen =
                    parseInt(value, version) && version == kSettingsVersion;
                canonical = canonical && versionSeen;
            } else if (key == "arrow_loops") {
                arrowLoopsSeen = parseBool(value, settings.arrowLoops);
                canonical = canonical && arrowLoopsSeen;
            } else if (key == "flying_arrow_collisions") {
                collisionsSeen =
                    parseBool(value, settings.flyingArrowCollisions);
                canonical = canonical && collisionsSeen;
            } else if (key == "crosshair_guides") {
                crosshairGuidesSeen = parseBool(value, settings.crosshairGuides);
                canonical = canonical && crosshairGuidesSeen;
            } else if (key == "sounds") {
                soundsSeen = parseBool(value, settings.sounds);
                canonical = canonical && soundsSeen;
            } else if (key == "board_theme") {
                boardThemeSeen = value == "light" || value == "dark";
                if (boardThemeSeen) settings.boardTheme = value == "light" ? BoardTheme::Light : BoardTheme::Dark;
                canonical = canonical && boardThemeSeen;
            }
        }
        if (!input.eof()) {
            canonical = false;
        }
        canonical = canonical && formatSeen && versionSeen &&
                    arrowLoopsSeen && collisionsSeen && crosshairGuidesSeen && soundsSeen && boardThemeSeen;
        if (!formatSeen || !versionSeen) {
            settings = {};
        }
    } catch (...) {
        canonical = false;
        settings = {};
    }

    if (!canonical) {
        (void)saveSettings(settings);
    }
    return settings;
}

bool Persistence::saveSettings(const Settings& settings) const noexcept {
    if (settings.boardTheme != BoardTheme::Light && settings.boardTheme != BoardTheme::Dark) return false;
    return writeAtomically(settingsPath(), [&settings](std::ostream& output) {
        output << "format=" << kSettingsFormat << '\n'
               << "version=" << kSettingsVersion << '\n'
               << "arrow_loops="
               << (settings.arrowLoops ? "on" : "off") << '\n'
               << "flying_arrow_collisions="
               << (settings.flyingArrowCollisions ? "on" : "off") << '\n'
               << "crosshair_guides="
               << (settings.crosshairGuides ? "on" : "off") << '\n'
               << "sounds=" << (settings.sounds ? "on" : "off") << '\n'
               << "board_theme=" << (settings.boardTheme == BoardTheme::Dark ? "dark" : "light") << '\n';
        return static_cast<bool>(output);
    });
}

bool Persistence::setArrowLoops(
    Settings& settings,
    bool enabled
) const noexcept {
    Settings changed = settings;
    changed.arrowLoops = enabled;
    if (!saveSettings(changed)) {
        return false;
    }
    settings = changed;
    return true;
}

bool Persistence::setFlyingArrowCollisions(
    Settings& settings,
    bool enabled
) const noexcept {
    Settings changed = settings;
    changed.flyingArrowCollisions = enabled;
    if (!saveSettings(changed)) {
        return false;
    }
    settings = changed;
    return true;
}

bool Persistence::setCrosshairGuides(
    Settings& settings,
    bool enabled
) const noexcept {
    Settings changed = settings;
    changed.crosshairGuides = enabled;
    if (!saveSettings(changed)) {
        return false;
    }
    settings = changed;
    return true;
}

bool Persistence::setSounds(Settings& settings, bool enabled) const noexcept {
    Settings changed = settings;
    changed.sounds = enabled;
    if (!saveSettings(changed)) {
        return false;
    }
    settings = changed;
    return true;
}

bool Persistence::setBoardTheme(Settings& settings, BoardTheme theme) const noexcept {
    Settings changed = settings;
    changed.boardTheme = theme;
    if (!saveSettings(changed)) return false;
    settings = changed;
    return true;
}

std::optional<SavedGame> Persistence::loadSavedGame() const noexcept {
    try {
        std::ifstream input(saveGamePath(), std::ios::binary);
        if (!input) {
            return std::nullopt;
        }

        SavedGame game;
        std::string line;
        std::string_view key;
        std::string_view value;

        if (!readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "format" || value != kSaveGameFormat ||
            !readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "version") {
            return std::nullopt;
        }
        int version{};
        if (!parseInt(value, version) || version != kSaveGameVersion) {
            return std::nullopt;
        }

        if (!readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "columns" || !parseInt(value, game.columns) ||
            !readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "rows" || !parseInt(value, game.rows) ||
            !readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "lives" || !parseInt(value, game.lives)) {
            return std::nullopt;
        }

        if (game.columns <= 0 || game.rows <= 0) {
            return std::nullopt;
        }
        const std::uint64_t area =
            static_cast<std::uint64_t>(game.columns) *
            static_cast<std::uint64_t>(game.rows);
        if (area > kMaximumBoardCells) {
            return std::nullopt;
        }

        std::size_t arrowCount{};
        if (!readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "arrow_count" || !parseSize(value, arrowCount) ||
            arrowCount > area) {
            return std::nullopt;
        }
        game.arrows.reserve(arrowCount);
        for (std::size_t index = 0; index < arrowCount; ++index) {
            if (!readLine(input, line) ||
                !splitAssignment(line, key, value) ||
                key != "arrow") {
                return std::nullopt;
            }
            Arrow parsed;
            if (!parseArrow(value, parsed)) {
                return std::nullopt;
            }
            game.arrows.push_back(std::move(parsed));
        }

        if (!readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "present" ||
            !parseFlags(value, arrowCount, game.present) ||
            !readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "failed" ||
            !parseFlags(value, arrowCount, game.failed) ||
            !readLine(input, line) ||
            !splitAssignment(line, key, value) ||
            key != "end" || value != "1") {
            return std::nullopt;
        }

        while (std::getline(input, line)) {
            if (!trim(line).empty()) {
                return std::nullopt;
            }
        }
        if (!input.eof()) {
            return std::nullopt;
        }
        return isValidSavedGame(game)
            ? std::optional<SavedGame>{std::move(game)}
            : std::nullopt;
    } catch (...) {
        return std::nullopt;
    }
}

bool Persistence::saveSavedGame(const SavedGame& game) const noexcept {
    if (!isValidSavedGame(game)) {
        return false;
    }

    return writeAtomically(saveGamePath(), [&game](std::ostream& output) {
        output << "format=" << kSaveGameFormat << '\n'
               << "version=" << kSaveGameVersion << '\n'
               << "columns=" << game.columns << '\n'
               << "rows=" << game.rows << '\n'
               << "lives=" << game.lives << '\n'
               << "arrow_count=" << game.arrows.size() << '\n';
        for (const Arrow& value : game.arrows) {
            output << "arrow="
                   << value.direction.x << ',' << value.direction.y
                   << ';' << value.cells.size();
            for (Cell cell : value.cells) {
                output << ';' << cell.x << ',' << cell.y;
            }
            output << '\n';
        }
        output << "present=";
        if (!writeFlags(output, game.present)) {
            return false;
        }
        output << "failed=";
        if (!writeFlags(output, game.failed)) {
            return false;
        }
        output << "end=1\n";
        return static_cast<bool>(output);
    });
}

bool Persistence::removeSavedGame() const noexcept {
    try {
        std::error_code error;
        std::filesystem::remove(saveGamePath(), error);
        if (error) {
            return false;
        }
        std::filesystem::path temporary = saveGamePath();
        temporary += ".tmp";
        std::filesystem::remove(temporary, error);
        return !error;
    } catch (...) {
        return false;
    }
}

} // namespace arrow
