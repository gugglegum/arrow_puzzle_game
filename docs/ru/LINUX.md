# Разработка нативной версии Linux

[English](../en/LINUX.md) | **Русский**

Windows остаётся основной платформой разработки. Используйте отдельные клоны
под Windows и Linux, передавайте готовые изменения через GitHub и не переносите
каталоги сборки. Первая цель Linux — x86-64 с X11 (либо XWayland в сессии
Wayland) и настольным OpenGL 3.3. Native Wayland, ARM64 и Android требуют
отдельной работы.

## Сборка

В Linux Mint 22.3 / Ubuntu 24.04 однократно установите зависимости:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git python3 \
    libasound2-dev libx11-dev libxrandr-dev libxi-dev \
    libgl1-mesa-dev libxcursor-dev libxinerama-dev
```

Нужен CMake 3.24+, в том числе для presets (schema 5). В старых дистрибутивах
может потребоваться обновить CMake из стандартного пакета. Первая конфигурация
скачивает зафиксированный commit raylib 5.5; системная raylib не требуется.

Из корня репозитория:

```bash
./tools/build-linux.sh
./build/linux/arrow_puzzle
```

Скрипт конфигурирует Release, собирает в два потока и запускает все пять
наборов CTest без окна. Дополнительные аргументы передаются конфигурации CMake.
`--debug` использует `build/linux-debug`; `CMAKE_BUILD_PARALLEL_LEVEL` задаёт
параллелизм. Эквивалентные команды:

```bash
cmake --preset linux
cmake --build --preset linux --parallel 2
ctest --preset linux
```

Графика, шрифт и звуковые эффекты встроены. Программа может запускаться из
другого каталога без файлов репозитория. raylib слинкована статически, однако
остаются зависимости от системных библиотек C/C++, X11, драйвера OpenGL и
необязательной службы звука.

## Настройки и сохранение

| Файл | Стандартный путь Linux | Абсолютное переопределение |
| --- | --- | --- |
| Настройки | `~/.config/arrow-puzzle/settings.txt` | `$XDG_CONFIG_HOME/arrow-puzzle/settings.txt` |
| Сохранение | `~/.local/share/arrow-puzzle/savegame.txt` | `$XDG_DATA_HOME/arrow-puzzle/savegame.txt` |

Незаданные, пустые и относительные XDG-пути заменяются стандартными. Если
`HOME` отсутствует или некорректен, домашний каталог определяется по учётной
записи пользователя. Папки создаются при записи; каталог программы может быть
доступен только для чтения. В Windows оба файла остаются рядом с EXE. Текстовые
форматы версии 1 не изменились: для переноса прогресса скопируйте оба файла
в соответствующие каталоги другой ОС при закрытой игре. Автоматического импорта
файлов из каталога Linux-программы нет.

Для QA задавайте временные абсолютные XDG-пути, сохраняя личный прогресс:

```bash
mkdir -p "$PWD/out/linux-smoke"
cd out/linux-smoke
XDG_CONFIG_HOME="$PWD/config" XDG_DATA_HOME="$PWD/data" \
    ARROWPUZZLE_SCREENSHOT=menu.png ARROWPUZZLE_SCREENSHOT_MENU=1 \
    ARROWPUZZLE_SCREENSHOT_FRAME=4 ../../build/linux/arrow_puzzle
```

raylib сохраняет скриншоты относительно начального рабочего каталога: задавайте
имя файла и запускайте игру в нужной папке. Полученное изображение нужно смотреть.

## Проверки рабочего стола

```bash
cmake --build build/linux --parallel 2 --target \
    arrow_render_tests arrow_ui_text_tests arrow_audio_tests
./build/linux/arrow_render_tests
./build/linux/arrow_ui_text_tests
./build/linux/arrow_audio_tests assets/sounds
```

Проверки рендера/текста требуют X11 и OpenGL 3.3. В CI их можно выполнять
через Xvfb с программным Mesa; это не замер производительности GPU. Аудиотесту
нужно устройство воспроизведения; он проверяет микшер игры и глушит выход.
Задержку динамиков или звукового тракта VMware он не измеряет.

Осмотрите главное меню, выбор сложности, настройки, обе темы поля, HUD и
результаты. Проверьте click/drag, колесо, Ctrl +/- / Ctrl+0, save/resume, F11,
Alt+Enter, Escape, Alt-Tab, изменение окна и потребление ресурсов в покое.
Полноэкранный режим Linux использует запрос EWMH к оконному менеджеру: он
восстанавливает окно, а разрешение экрана не меняется. В Windows сохраняется
существующая реализация borderless.

Проверка Mint/VMware не подтверждает производительность на реальном компьютере,
поведение других оконных менеджеров, XWayland, дробного масштабирования и
нескольких мониторов. Причина сообщённой задержки звука в Wine не установлена.

Источники: [зависимости raylib для Linux](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux),
[каталоги XDG](https://specifications.freedesktop.org/basedir/latest/),
[fullscreen EWMH](https://specifications.freedesktop.org/wm/latest/ar01s05.html).

## Архив разработки и CI

```bash
python3 tools/package-linux.py --version dev-local
cd out/linux-package
sha256sum --check --ignore-missing SHA256SUMS.txt
tar -xzf ArrowPuzzle-dev-local-linux-x64.tar.gz
cd ArrowPuzzle-dev-local-linux-x64
sha256sum --check --ignore-missing ../SHA256SUMS.txt
./ArrowPuzzle
```

Скрипт требует Release, обновляет сборку, устанавливает и strip-ит копию
(удаляя RPATH сборочного каталога) и упаковывает её с документацией, лицензиями
и `build-info.json`. Метаданные содержат компилятор, commit raylib, состояние
исходников, запуск Actions при наличии, зависимости ELF и версии символов.
Скрипт не перезаписывает пакет и метаданные; для следующего пакета используйте
`--output-dir`. Проверка скачанной программы:

```bash
python3 tools/smoke-linux.py /absolute/path/to/ArrowPuzzle
```

**Build and test** (`.github/workflows/build.yml`) проверяет push в ветки, PR
и ручные запуски на Windows/MSVC, Ubuntu 22.04/GCC и Ubuntu 24.04/Clang. Для
Linux выполняются проверки рендера/текста Mesa/Xvfb и распакованного пакета.
Пакеты, логи и скриншоты доступны в артефактах запуска 14 дней. Этот workflow
не создаёт теги и релизы; workflow релиза Windows остаётся отдельным.

Ubuntu 22.04/GCC — выбранная нижняя сборочная база CI, но не обещание работы
на любых старых системах. Смотрите `required_symbol_versions` конкретного
пакета: сборка Mint 22.3/GCC 13 требует более новых glibc/libstdc++, чем сборка
Ubuntu 22.04. Системные библиотеки и драйверы X11/OpenGL всё равно нужны.
AppImage — следующий этап после проверки tar. Ни один формат сам по себе не
подтверждает native Wayland или производительность другого GPU.
