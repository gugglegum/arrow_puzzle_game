#pragma once

#include <optional>
#include <string_view>

namespace arrow {

// Stable settings values; Auto is resolved at startup, never written as a locale.
enum class Language { Auto, English, Spanish, French, German, Portuguese, Chinese, Russian, Ukrainian, Count };

enum class Text {
    MainMenu, Settings, NewGame, ResumeGame, Exit, Back,
    MainSubtitle, SettingsSubtitle, DifficultySubtitle,
    NewGameDetail, ResumeDetail, SettingsDetail, ExitDetail, NoSavedPuzzle,
    Simple, Middle, Hard, VeryHard, Cells20, Cells40, Cells80, Cells160,
    ArrowLoops, FlyingCollisions, CrosshairGuides, Sounds, LeftButtonDrag, BoardTheme, InterfaceLanguage,
    LoopsDetail, CollisionsDetail, GuidesDetail, SoundsDetail, LeftButtonDragDetail, ThemeDetail, LanguageDetail,
    On, Off, Light, Dark, MenuHint, DifficultyHint, SettingsHint, BackgroundGeneration,
    SoundsOn, SoundsOff, Hint, HintWaiting, HintSettling, Help, HelpCompact, HelpMiddleDrag, HelpMiddleDragCompact,
    Generating, GenerationFailed, Arrows,
    GeneratingLevel, SearchingPuzzle, RetryGeneration, GenerationErrorDetail,
    FinishingGeneration, ClosingGame, PuzzleComplete, OutOfLives, YouWon, GameOver,
    VictoryDetail, DefeatDetail, PlayAgain, EndHint,
    Saves, SavesDetail, SavesSubtitle, CreateSave, CreateSaveDetail, LoadSave, RenameSave,
    DeleteSave, DeleteSaveQuestion, DeleteSaveDetail, SaveName, SaveNameHint, Save, Cancel,
    PreviousPage, NextPage, EmptySaves, SaveSuccess, SaveError, SaveUnreadable, NameInvalid,
    LoadSaveDetail, Lives, PageNumber,
    Count
};

[[nodiscard]] bool isValidLanguage(Language language) noexcept;
[[nodiscard]] const char* languageCode(Language language) noexcept;
[[nodiscard]] const char* languageName(Language language) noexcept;
[[nodiscard]] std::optional<Language> languageFromCode(std::string_view code) noexcept;
[[nodiscard]] Language cycleLanguage(Language language, int direction) noexcept;

// Accept BCP 47 and POSIX names, including region, encoding and modifier suffixes.
[[nodiscard]] std::optional<Language> languageFromLocale(std::string_view locale) noexcept;
// Pure Linux/GNU precedence helper, also tested on Windows. C/POSIX force English.
[[nodiscard]] Language languageFromEnvironment(std::string_view all, std::string_view messages,
                                             std::string_view lang, std::string_view preferences) noexcept;
[[nodiscard]] Language systemLanguage() noexcept;
[[nodiscard]] Language resolveLanguage(Language choice, Language system) noexcept;
[[nodiscard]] const char* translate(Text text, Language language) noexcept;

} // namespace arrow
