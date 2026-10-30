#include "arrow/Localization.hpp"

#include <array>
#include <cstdlib>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace arrow {
namespace {

constexpr std::array<const char*, 9> kCodes{"auto", "en", "es", "fr", "de", "pt", "zh", "ru", "uk"};
constexpr std::array<const char*, 9> kNames{
    "Auto", "English", "Español", "Français", "Deutsch", "Português", "简体中文", "Русский", "Українська"
};
struct Translation { Text id; std::array<const char*, 8> values; };
// English, Spanish, French, German, Portuguese, Simplified Chinese, Russian, Ukrainian.
// Kept in the executable; no external catalogs or runtime downloads.
constexpr auto kTranslations = std::to_array<Translation>({
    {Text::MainMenu, {"MAIN MENU", "MENÚ PRINCIPAL", "MENU PRINCIPAL", "HAUPTMENÜ", "MENU PRINCIPAL", "主菜单", "ГЛАВНОЕ МЕНЮ", "ГОЛОВНЕ МЕНЮ"}},
    {Text::Settings, {"Settings", "Ajustes", "Paramètres", "Einstellungen", "Definições", "设置", "Настройки", "Налаштування"}},
    {Text::NewGame, {"New game", "Nueva partida", "Nouvelle partie", "Neues Spiel", "Novo jogo", "新游戏", "Новая игра", "Нова гра"}},
    {Text::ResumeGame, {"Resume game", "Continuar partida", "Reprendre", "Spiel fortsetzen", "Continuar jogo", "继续游戏", "Продолжить игру", "Продовжити гру"}},
    {Text::Exit, {"Exit", "Salir", "Quitter", "Beenden", "Sair", "退出", "Выход", "Вихід"}},
    {Text::Back, {"Back", "Volver", "Retour", "Zurück", "Voltar", "返回", "Назад", "Назад"}},
    {Text::MainSubtitle, {"Find a way for every arrow.", "Encuentra una salida para cada flecha.", "Trouvez une sortie pour chaque flèche.", "Finde für jeden Pfeil einen Ausweg.", "Encontra uma saída para cada seta.", "为每支箭头找到出路。", "Найдите выход для каждой стрелки.", "Знайдіть вихід для кожної стрілки."}},
    {Text::SettingsSubtitle, {"Choose how you play.", "Elige cómo jugar.", "Jouez à votre façon.", "Spiele auf deine Art.", "Escolhe como jogar.", "选择你的玩法。", "Выберите, как играть.", "Виберіть, як грати."}},
    {Text::DifficultySubtitle, {"Choose your challenge.", "Elige tu desafío.", "Choisissez votre défi.", "Wähle deine Herausforderung.", "Escolhe o teu desafio.", "选择挑战难度。", "Выберите сложность.", "Виберіть складність."}},
    {Text::NewGameDetail, {"Choose your next challenge", "Elige tu próximo desafío", "Choisissez votre prochain défi", "Wähle deine nächste Herausforderung", "Escolhe o próximo desafio", "选择下一个挑战", "Выберите новое испытание", "Виберіть нове випробування"}},
    {Text::ResumeDetail, {"Continue your saved puzzle", "Continúa tu partida guardada", "Continuez votre partie sauvegardée", "Setze dein gespeichertes Rätsel fort", "Continua o jogo guardado", "继续已保存的谜题", "Вернитесь к сохранённой игре", "Поверніться до збереженої гри"}},
    {Text::SettingsDetail, {"Sound and gameplay preferences", "Opciones de sonido y juego", "Options de son et de jeu", "Ton und Spieleinstellungen", "Opções de som e de jogo", "声音与游戏偏好设置", "Звук, язык и правила игры", "Звук, мова та правила гри"}},
    {Text::ExitDetail, {"Close Arrow Puzzle", "Cerrar Arrow Puzzle", "Fermer Arrow Puzzle", "Arrow Puzzle schließen", "Fechar Arrow Puzzle", "关闭 Arrow Puzzle", "Закрыть Arrow Puzzle", "Закрити Arrow Puzzle"}},
    {Text::NoSavedPuzzle, {"No unfinished puzzle to resume", "No hay partidas pendientes", "Aucune partie en cours", "Kein gespeichertes Rätsel vorhanden", "Não há jogos por terminar", "没有可继续的谜题", "Нет незавершённой игры", "Немає незавершеної гри"}},
    {Text::Simple, {"Simple", "Fácil", "Facile", "Einfach", "Fácil", "简单", "Легко", "Легко"}},
    {Text::Middle, {"Middle", "Medio", "Moyen", "Mittel", "Médio", "中等", "Средне", "Середньо"}},
    {Text::Hard, {"Hard", "Difícil", "Difficile", "Schwer", "Difícil", "困难", "Сложно", "Складно"}},
    {Text::VeryHard, {"Very Hard", "Muy difícil", "Très difficile", "Sehr schwer", "Muito difícil", "极难", "Очень сложно", "Дуже складно"}},
    {Text::Cells20, {"20 x 20 cells", "20 x 20 casillas", "20 x 20 cases", "20 x 20 Felder", "20 x 20 células", "20 x 20 格", "20 x 20 клеток", "20 x 20 клітинок"}},
    {Text::Cells40, {"40 x 40 cells", "40 x 40 casillas", "40 x 40 cases", "40 x 40 Felder", "40 x 40 células", "40 x 40 格", "40 x 40 клеток", "40 x 40 клітинок"}},
    {Text::Cells80, {"80 x 80 cells", "80 x 80 casillas", "80 x 80 cases", "80 x 80 Felder", "80 x 80 células", "80 x 80 格", "80 x 80 клеток", "80 x 80 клітинок"}},
    {Text::Cells160, {"160 x 160 cells", "160 x 160 casillas", "160 x 160 cases", "160 x 160 Felder", "160 x 160 células", "160 x 160 格", "160 x 160 клеток", "160 x 160 клітинок"}},
    {Text::ArrowLoops, {"Arrow loops", "Bucles de flechas", "Boucles des flèches", "Pfeilschleifen", "Laços nas setas", "箭头回环", "Петли стрелок", "Петлі стрілок"}},
    {Text::FlyingCollisions, {"Flying arrow collisions", "Colisiones en movimiento", "Collisions en mouvement", "Kollisionen bewegter Pfeile", "Colisões em movimento", "移动箭头碰撞", "Столкновения в полёте", "Зіткнення в польоті"}},
    {Text::CrosshairGuides, {"Crosshair guides", "Guías de la cuadrícula", "Repères de la grille", "Rasterhilfslinien", "Guias da grelha", "十字辅助线", "Направляющие курсора", "Напрямні курсора"}},
    {Text::Sounds, {"Sounds", "Sonidos", "Sons", "Ton", "Sons", "声音", "Звуки", "Звуки"}},
    {Text::BoardTheme, {"Board theme", "Tema del tablero", "Thème du plateau", "Spielfeldthema", "Tema do tabuleiro", "棋盘主题", "Тема поля", "Тема поля"}},
    {Text::InterfaceLanguage, {"Language", "Idioma", "Langue", "Sprache", "Idioma", "语言", "Язык", "Мова"}},
    {Text::LoopsDetail, {"Allow loops in newly generated puzzles.", "Permitir bucles en los nuevos puzles.", "Autoriser les boucles dans les nouveaux puzzles.", "Schleifen in neuen Rätseln erlauben.", "Permitir laços nos novos puzzles.", "允许新谜题中的箭头形成回环。", "Разрешить петли в новых головоломках.", "Дозволити петлі в нових головоломках."}},
    {Text::CollisionsDetail, {"Moving arrows can cost a life on contact.", "Chocar en movimiento puede costar una vida.", "Un contact en mouvement peut coûter une vie.", "Kontakt bewegter Pfeile kostet ein Leben.", "O contacto entre setas pode custar uma vida.", "移动中的箭头相撞会损失生命。", "Контакт летящих стрелок отнимает жизнь.", "Контакт рухомих стрілок забирає життя."}},
    {Text::GuidesDetail, {"Highlight the cursor's row and column.", "Resaltar la fila y la columna del cursor.", "Surligner la ligne et la colonne du curseur.", "Zeile und Spalte des Zeigers hervorheben.", "Realçar a linha e a coluna do cursor.", "高亮光标所在的行和列。", "Подсвечивать строку и столбец курсора.", "Підсвічувати рядок і стовпець курсора."}},
    {Text::SoundsDetail, {"Play arrow, collision and celebration sounds.", "Sonidos de flechas, choques y celebraciones.", "Sons des flèches, des chocs et des victoires.", "Pfeil-, Kollisions- und Jubelklänge abspielen.", "Sons de setas, colisões e celebrações.", "播放箭头、碰撞和庆祝音效。", "Звуки стрелок, столкновений и победы.", "Звуки стрілок, зіткнень і перемоги."}},
    {Text::ThemeDetail, {"Light or dark board and top panel.", "Tablero y panel superior claros u oscuros.", "Plateau et bandeau supérieur clairs ou sombres.", "Helles oder dunkles Spielfeld und Kopfzeile.", "Tabuleiro e painel superior claros ou escuros.", "浅色或深色棋盘与顶部面板。", "Светлое или тёмное поле и верхняя панель.", "Світле або темне поле й верхня панель."}},
    {Text::LanguageDetail, {"Auto uses your system language.", "Auto usa el idioma del sistema.", "Auto utilise la langue du système.", "Auto verwendet die Systemsprache.", "Auto usa o idioma do sistema.", "Auto 使用系统语言。", "Auto использует язык системы.", "Auto використовує мову системи."}},
    {Text::On, {"ON", "SÍ", "OUI", "AN", "SIM", "开", "ВКЛ", "УВІМК"}},
    {Text::Off, {"OFF", "NO", "NON", "AUS", "NÃO", "关", "ВЫКЛ", "ВИМК"}},
    {Text::Light, {"LIGHT", "CLARO", "CLAIR", "HELL", "CLARO", "浅色", "СВЕТЛАЯ", "СВІТЛА"}},
    {Text::Dark, {"DARK", "OSCURO", "SOMBRE", "DUNKEL", "ESCURO", "深色", "ТЁМНАЯ", "ТЕМНА"}},
    {Text::MenuHint, {"Up / Down navigate    Enter select", "Arriba / Abajo: navegar    Enter: elegir", "Haut / Bas : naviguer    Entrée : choisir", "Auf / Ab: navigieren    Enter: wählen", "Cima / Baixo: navegar    Enter: escolher", "上 / 下：导航    Enter：选择", "Вверх / Вниз — выбор    Enter — открыть", "Вгору / Вниз — вибір    Enter — відкрити"}},
    {Text::DifficultyHint, {"Up / Down navigate    Enter select    Esc back", "Arriba / Abajo: navegar    Enter: elegir    Esc: volver", "Haut / Bas : naviguer    Entrée : choisir    Échap : retour", "Auf / Ab: navigieren    Enter: wählen    Esc: zurück", "Cima / Baixo: navegar    Enter: escolher    Esc: voltar", "上 / 下：导航    Enter：选择    Esc：返回", "Вверх / Вниз — выбор    Enter — начать    Esc — назад", "Вгору / Вниз — вибір    Enter — почати    Esc — назад"}},
    {Text::SettingsHint, {"Left / Right change    Enter / Space toggle    Auto-saved", "Izq. / Der.: cambiar    Enter / Espacio: activar    Autoguardado", "Gauche / Droite : modifier    Entrée / Espace : basculer    Sauvé", "Links / Rechts: ändern    Enter / Leertaste: umschalten    Gespeichert", "Esq. / Dir.: mudar    Enter / Espaço: alternar    Guardado", "左 / 右：切换    Enter / 空格：开关    自动保存", "Влево / Вправо — сменить    Enter / Пробел — вкл.    Автосохранение", "Ліворуч / Праворуч — змінити    Enter / Пробіл — увімк.    Автозбереження"}},
    {Text::BackgroundGeneration, {"Level generation is finishing in the background", "La generación termina en segundo plano", "La génération se termine en arrière-plan", "Das Rätsel wird im Hintergrund fertiggestellt", "A geração termina em segundo plano", "正在后台完成关卡生成", "Создание уровня завершается в фоне", "Створення рівня завершується у фоні"}},
    {Text::SoundsOn, {"Sounds: ON", "Sonido: SÍ", "Son : OUI", "Ton: AN", "Som: SIM", "声音：开", "Звуки: ВКЛ", "Звуки: УВІМК"}},
    {Text::SoundsOff, {"Sounds: OFF", "Sonido: NO", "Son : NON", "Ton: AUS", "Som: NÃO", "声音：关", "Звуки: ВЫКЛ", "Звуки: ВИМК"}},
    {Text::Help, {"Click: launch   LMB/MMB drag: pan   Wheel/Ctrl +/-: zoom   Ctrl 0: fit   F11: fullscreen   Esc: menu", "Clic: lanzar   Arrastrar izq./central: mover   Rueda/Ctrl +/-: zoom   Ctrl 0: ajustar   F11: pantalla completa   Esc: menú", "Clic : lancer   Glisser gauche/milieu : déplacer   Molette/Ctrl +/- : zoom   Ctrl 0 : cadrer   F11 : plein écran   Échap : menu", "Klick: starten   Links-/Mittelklick ziehen: verschieben   Rad/Ctrl +/-: Zoom   Ctrl 0: einpassen   F11: Vollbild   Esc: Menü", "Clique: lançar   Arrastar esq./central: mover   Roda/Ctrl +/-: zoom   Ctrl 0: ajustar   F11: ecrã inteiro   Esc: menu", "点击：发射    左键/中键拖动：平移    滚轮/Ctrl +/-：缩放    Ctrl 0：适应窗口    F11：全屏    Esc：菜单", "Клик: пуск   ЛКМ/СКМ: сдвиг   Колесо/Ctrl +/-: масштаб   Ctrl 0: вписать   F11: полный экран   Esc: меню", "Клік: пуск   ЛКМ/СКМ: зсув   Колесо/Ctrl +/-: масштаб   Ctrl 0: вмістити   F11: повний екран   Esc: меню"}},
    {Text::HelpCompact, {"Click: launch   Drag: pan   Wheel/Ctrl +/-: zoom   Ctrl 0: fit", "Clic: lanzar   Arrastrar: mover   Rueda/Ctrl +/-: zoom   Ctrl 0: ajustar", "Clic : lancer   Glisser : déplacer   Molette/Ctrl +/- : zoom   Ctrl 0 : cadrer", "Klick: starten   Ziehen: verschieben   Rad/Ctrl +/-: Zoom   Ctrl 0: einpassen", "Clique: lançar   Arrastar: mover   Roda/Ctrl +/-: zoom   Ctrl 0: ajustar", "点击：发射    拖动：平移    滚轮/Ctrl +/-：缩放    Ctrl 0：适应窗口", "Клик: пуск   Тянуть: сдвиг   Колесо/Ctrl +/-: масштаб   Ctrl 0: вписать", "Клік: пуск   Тягнути: зсув   Колесо/Ctrl +/-: масштаб   Ctrl 0: вмістити"}},
    {Text::Generating, {"GENERATING", "GENERANDO", "GÉNÉRATION", "ERSTELLUNG", "A GERAR", "正在生成", "СОЗДАНИЕ", "СТВОРЕННЯ"}},
    {Text::GenerationFailed, {"GENERATION FAILED", "ERROR DE GENERACIÓN", "ÉCHEC DE LA GÉNÉRATION", "ERSTELLUNG FEHLGESCHLAGEN", "FALHA NA GERAÇÃO", "生成失败", "НЕ УДАЛОСЬ СОЗДАТЬ УРОВЕН", "НЕ ВДАЛОСЯ СТВОРИТИ РІВЕНЬ"}},
    {Text::Arrows, {"ARROWS", "FLECHAS", "FLÈCHES", "PFEILE", "SETAS", "箭头", "СТРЕЛКИ", "СТРІЛКИ"}},
    {Text::GeneratingLevel, {"GENERATING LEVEL", "GENERANDO NIVEL", "GÉNÉRATION DU NIVEAU", "RÄTSEL WIRD ERSTELLT", "A GERAR NÍVEL", "正在生成关卡", "СОЗДАНИЕ УРОВНЯ", "СТВОРЕННЯ РІВНЯ"}},
    {Text::SearchingPuzzle, {"Searching for an interesting puzzle...", "Buscando un puzle interesante...", "Recherche d'un puzzle intéressant...", "Ein spannendes Rätsel wird gesucht...", "À procura de um puzzle interessante...", "正在寻找有趣的谜题...", "Ищем интересную головоломку...", "Шукаємо цікаву головоломку..."}},
    {Text::RetryGeneration, {"Press R to try another level  /  Esc for menu", "R: probar otro nivel  /  Esc: menú", "R : essayer un autre niveau  /  Échap : menu", "R: anderes Rätsel versuchen  /  Esc: Menü", "R: tentar outro nível  /  Esc: menu", "按 R 尝试其他关卡  /  Esc 返回菜单", "R — попробовать другой уровень  /  Esc — меню", "R — спробувати інший рівень  /  Esc — меню"}},
    {Text::GenerationErrorDetail, {"Unable to create a puzzle. Please try again.", "No se pudo crear el puzle. Inténtalo de nuevo.", "Impossible de créer un puzzle. Réessayez.", "Das Rätsel konnte nicht erstellt werden. Bitte erneut versuchen.", "Não foi possível criar o puzzle. Tenta novamente.", "无法创建谜题，请重试。", "Не удалось создать головоломку. Попробуйте ещё раз.", "Не вдалося створити головоломку. Спробуйте ще раз."}},
    {Text::FinishingGeneration, {"FINISHING LEVEL GENERATION", "TERMINANDO LA GENERACIÓN", "FIN DE LA GÉNÉRATION", "ERSTELLUNG WIRD ABGESCHLOSSEN", "A TERMINAR A GERAÇÃO", "正在完成关卡生成", "ЗАВЕРШЕНИЕ СОЗДАНИЯ УРОВНЯ", "ЗАВЕРШЕННЯ СТВОРЕННЯ РІВНЯ"}},
    {Text::ClosingGame, {"The game will close automatically...", "El juego se cerrará automáticamente...", "Le jeu va se fermer automatiquement...", "Das Spiel schließt sich automatisch...", "O jogo vai fechar automaticamente...", "游戏将自动关闭...", "Игра закроется автоматически...", "Гра закриється автоматично..."}},
    {Text::PuzzleComplete, {"PUZZLE COMPLETE", "PUZLE RESUELTO", "PUZZLE RÉSOLU", "RÄTSEL GELÖST", "PUZZLE RESOLVIDO", "谜题完成", "ГОЛОВОЛОМКА РЕШЕНА", "ГОЛОВОЛОМКУ РОЗВ'ЯЗАНО"}},
    {Text::OutOfLives, {"OUT OF LIVES", "SIN VIDAS", "PLUS DE VIES", "KEINE LEBEN MEHR", "SEM VIDAS", "生命耗尽", "ЖИЗНИ ЗАКОНЧИЛИСЬ", "ЖИТТЯ СКІНЧИЛИСЯ"}},
    {Text::YouWon, {"YOU WON!", "¡HAS GANADO!", "VICTOIRE !", "GEWONNEN!", "VITÓRIA!", "你赢了！", "ПОБЕДА!", "ПЕРЕМОГА!"}},
    {Text::GameOver, {"GAME OVER", "FIN DE LA PARTIDA", "PARTIE TERMINÉE", "SPIEL VORBEI", "FIM DO JOGO", "游戏结束", "ИГРА ОКОНЧЕНА", "ГРУ ЗАВЕРШЕНО"}},
    {Text::VictoryDetail, {"Every arrow found its way.", "Cada flecha encontró su salida.", "Chaque flèche a trouvé sa sortie.", "Jeder Pfeil hat seinen Ausweg gefunden.", "Todas as setas encontraram a saída.", "每支箭头都找到了出路。", "Каждая стрелка нашла выход.", "Кожна стрілка знайшла вихід."}},
    {Text::DefeatDetail, {"Take a breath. Try a new puzzle.", "Respira. Prueba un nuevo puzle.", "Soufflez. Essayez un nouveau puzzle.", "Atme durch. Versuche ein neues Rätsel.", "Respira. Experimenta um novo puzzle.", "休息一下，再挑战新的谜题。", "Передохните. Попробуйте новую головоломку.", "Перепочиньте. Спробуйте нову головоломку."}},
    {Text::PlayAgain, {"Play again", "Jugar de nuevo", "Rejouer", "Erneut spielen", "Jogar de novo", "再玩一次", "Играть ещё", "Грати ще"}},
    {Text::EndHint, {"R  new puzzle     Esc  main menu", "R  nuevo puzle     Esc  menú principal", "R  nouveau puzzle     Échap  menu principal", "R  neues Rätsel     Esc  Hauptmenü", "R  novo puzzle     Esc  menu principal", "R  新谜题     Esc  主菜单", "R  новая игра     Esc  главное меню", "R  нова гра     Esc  головне меню"}},
});
static_assert(kTranslations.size() == static_cast<std::size_t>(Text::Count));
static_assert([] {
    for (std::size_t i = 0; i < kTranslations.size(); ++i) {
        if (static_cast<std::size_t>(kTranslations[i].id) != i) return false;
        for (const char* value : kTranslations[i].values) if (!value || !*value) return false;
    }
    return true;
}(), "Every UI message needs all eight translations, in Text order");

#if !defined(_WIN32)
std::string_view environment(const char* key) noexcept {
    const char* value = std::getenv(key);
    return value ? value : "";
}
#endif

} // namespace

bool isValidLanguage(Language language) noexcept {
    return language >= Language::Auto && language < Language::Count;
}
const char* languageCode(Language language) noexcept {
    return kCodes[static_cast<std::size_t>(isValidLanguage(language) ? language : Language::Auto)];
}
const char* languageName(Language language) noexcept {
    return kNames[static_cast<std::size_t>(isValidLanguage(language) ? language : Language::Auto)];
}
std::optional<Language> languageFromCode(std::string_view code) noexcept {
    for (std::size_t i = 0; i < kCodes.size(); ++i) if (code == kCodes[i]) return static_cast<Language>(i);
    return std::nullopt;
}
Language cycleLanguage(Language language, int direction) noexcept {
    const int index = isValidLanguage(language) ? static_cast<int>(language) : 0;
    const int count = static_cast<int>(Language::Count);
    return static_cast<Language>((index + (direction < 0 ? -1 : direction > 0 ? 1 : 0) + count) % count);
}
std::optional<Language> languageFromLocale(std::string_view locale) noexcept {
    const auto base = locale.substr(0, locale.find_first_of("-_.@"));
    if (base.size() != 2) return std::nullopt;
    std::array<char, 2> lower{};
    for (std::size_t i = 0; i < 2; ++i) {
        const char c = base[i];
        lower[i] = c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
    }
    return languageFromCode(std::string_view{lower.data(), lower.size()});
}
Language languageFromEnvironment(std::string_view all, std::string_view messages,
                                 std::string_view lang, std::string_view preferences) noexcept {
    const auto locale = !all.empty() ? all : !messages.empty() ? messages : lang;
    if (locale.empty() || locale == "C" || locale == "POSIX" || locale.starts_with("C.") || locale.starts_with("POSIX."))
        return Language::English;
    if (!preferences.empty()) {
        while (!preferences.empty()) {
            const auto end = preferences.find(':');
            const auto item = preferences.substr(0, end);
            if (item == "C" || item == "POSIX") return Language::English;
            if (const auto found = languageFromLocale(item)) return *found;
            if (end == std::string_view::npos) break;
            preferences.remove_prefix(end + 1);
        }
        return Language::English;
    }
    return languageFromLocale(locale).value_or(Language::English);
}
Language systemLanguage() noexcept {
#if defined(_WIN32)
    // UI language, rather than keyboard layout or regional number/date format.
    switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
    case LANG_ENGLISH: return Language::English;
    case LANG_SPANISH: return Language::Spanish;
    case LANG_FRENCH: return Language::French;
    case LANG_GERMAN: return Language::German;
    case LANG_PORTUGUESE: return Language::Portuguese;
    case LANG_CHINESE: return Language::Chinese;
    case LANG_RUSSIAN: return Language::Russian;
    case LANG_UKRAINIAN: return Language::Ukrainian;
    default: return Language::English;
    }
#else
    return languageFromEnvironment(environment("LC_ALL"), environment("LC_MESSAGES"),
                                   environment("LANG"), environment("LANGUAGE"));
#endif
}
Language resolveLanguage(Language choice, Language system) noexcept {
    const auto result = choice == Language::Auto ? system : choice;
    return isValidLanguage(result) && result != Language::Auto ? result : Language::English;
}
const char* translate(Text text, Language language) noexcept {
    const auto index = static_cast<std::size_t>(text);
    if (index >= kTranslations.size()) return "";
    const auto column = static_cast<std::size_t>(resolveLanguage(language, Language::English)) - 1;
    return kTranslations[index].values[column];
}

} // namespace arrow
