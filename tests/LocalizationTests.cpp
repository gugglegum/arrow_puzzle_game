#include "arrow/Localization.hpp"

#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void checkLocales() {
    using enum arrow::Language;
    using arrow::languageFromLocale;
    for (const char* tag : {"en", "en-US", "en_GB.UTF-8"}) require(languageFromLocale(tag) == English, "English locale");
    require(languageFromLocale("es_MX.UTF-8") == Spanish, "Spanish region");
    require(languageFromLocale("fr-CA") == French, "French region");
    require(languageFromLocale("DE_de.UTF-8@euro") == German, "Case and modifier");
    require(languageFromLocale("pt-BR") == Portuguese && languageFromLocale("pt_PT") == Portuguese, "Portuguese regions");
    for (const char* tag : {"zh-CN", "zh_TW.UTF-8", "zh-Hans", "zh-Hant-HK"})
        require(languageFromLocale(tag) == Chinese, "Chinese region or script");
    require(languageFromLocale("ru_RU.UTF-8") == Russian, "Russian locale");
    require(languageFromLocale("uk-UA") == Ukrainian, "Ukrainian locale");
    for (const char* tag : {"", "C", "C.UTF-8", "POSIX", "ja_JP.UTF-8", "russian", "english", "enough", "auto"})
        require(!languageFromLocale(tag), "Unknown locale accepted");
    using arrow::languageFromEnvironment;
    require(languageFromEnvironment("", "", "ru_RU.UTF-8", "") == Russian, "LANG");
    require(languageFromEnvironment("", "uk_UA", "ru_RU", "") == Ukrainian, "LC_MESSAGES precedence");
    require(languageFromEnvironment("de_DE", "uk_UA", "ru_RU", "") == German, "LC_ALL precedence");
    require(languageFromEnvironment("", "", "en_US.UTF-8", "ja:fr_CA:en") == French, "LANGUAGE preference chain");
    require(languageFromEnvironment("", "", "ru_RU", "ja:ko") == English, "Unsupported preferences fallback");
    for (const char* locale : {"", "C", "C.UTF-8", "POSIX"})
        require(languageFromEnvironment(locale, "", locale, "ru") == English, "C/POSIX must ignore LANGUAGE");
    require(languageFromEnvironment("ja_JP", "", "ru_RU", "") == English, "Unsupported system fallback");
    require(arrow::resolveLanguage(Auto, Russian) == Russian, "Auto resolution");
    require(arrow::resolveLanguage(German, Russian) == German, "Explicit selection wins");
    require(arrow::resolveLanguage(Auto, Auto) == English, "Unavailable system fallback");
    require(arrow::systemLanguage() > Auto && arrow::systemLanguage() < Count, "Live system resolution");
}
void checkCatalog() {
    using enum arrow::Language;
    auto choice = Auto;
    for (int i = 0; i < static_cast<int>(Count); ++i) {
        const auto language = static_cast<arrow::Language>(i);
        require(choice == language, "Forward cycle order");
        require(arrow::languageFromCode(arrow::languageCode(language)) == language, "Settings code round trip");
        require(!std::string_view{arrow::languageName(language)}.empty(), "Missing native name");
        choice = arrow::cycleLanguage(choice, 1);
        require(arrow::cycleLanguage(choice, -1) == language, "Backward cycle");
        for (int t = 0; t < static_cast<int>(arrow::Text::Count); ++t)
            require(*arrow::translate(static_cast<arrow::Text>(t), language) != '\0', "Missing translation");
    }
    require(choice == Auto && arrow::cycleLanguage(Auto, -1) == Ukrainian, "Cycle wraparound");
    require(!arrow::languageFromCode("xx") && !arrow::languageFromCode("en-US"), "Invalid persisted code");
    require(!arrow::isValidLanguage(static_cast<arrow::Language>(-1)), "Invalid enum");
    require(std::string_view{arrow::translate(arrow::Text::Settings, Russian)} == "Настройки", "Russian translation");
    require(std::string_view{arrow::translate(arrow::Text::Settings, Ukrainian)} == "Налаштування", "Ukrainian translation");
    require(std::string_view{arrow::translate(arrow::Text::Settings, Chinese)} == "设置", "Chinese translation");
}
} // namespace

int main() {
    try {
        checkLocales();
        checkCatalog();
        std::cout << "Localization tests passed: catalogs, native names, locale variants, precedence and cycling\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Localization test failure: " << error.what() << '\n';
        return 1;
    }
}
