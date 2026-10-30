# Разработка нативной версии Linux

[English](../en/LINUX.md) | **Русский**

Для проверки обоих режимов используются
`bash tools/check-linux-desktop.sh` (рендер/текст и Debug save/resume) и
`bash tools/check-linux-packages.sh dev-local --appimage` (оба tar и оба AppImage).
Проверки `smoke-linux.py` и `check-production-binary.py` принимают `--portable`
для проверки portable-бинарника. Упаковщик Windows принимает `-Portable`,
Linux tar — `--portable`; задавайте разные выходные каталоги для двух вариантов.
AppImage-упаковщик определяет режим по проверенному исходному tar.

Windows остаётся основной платформой разработки. Используйте отдельные клоны
под Windows и Linux, передавайте готовые изменения через GitHub и не переносите
каталоги сборки. Первая цель Linux — x86-64 с X11 (либо XWayland в сессии
Wayland) и настольным OpenGL 3.3. Native Wayland, ARM64 и Android требуют
отдельной работы.

## Сборка

В Linux Mint 22.3 / Ubuntu 24.04 однократно установите зависимости:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git python3 curl \
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

Скрипт конфигурирует Release, собирает в два потока и запускает все шесть
наборов CTest без окна. Дополнительные аргументы передаются конфигурации CMake.
Из общих исходников собираются `arrow_puzzle` (данные пользователя) и
`arrow_puzzle_portable` (данные рядом с программой).
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

Таблица относится к обычным сборкам tar.gz и AppImage:

| Файл | Стандартный путь Linux | Абсолютное переопределение |
| --- | --- | --- |
| Настройки | `~/.config/arrow-puzzle/settings.txt` | `$XDG_CONFIG_HOME/arrow-puzzle/settings.txt` |
| Сохранение | `~/.local/share/arrow-puzzle/savegame.txt` | `$XDG_DATA_HOME/arrow-puzzle/savegame.txt` |

Незаданные, пустые и относительные XDG-пути заменяются стандартными. Если
`HOME` отсутствует или некорректен, домашний каталог определяется по учётной
записи пользователя. Папки создаются при записи; каталог программы может быть
доступен только для чтения. Обычная Windows-сборка использует
`%LOCALAPPDATA%\ArrowPuzzle\`. Текстовые
форматы версии 1 не изменились: для переноса прогресса скопируйте оба файла
в соответствующие каталоги другой ОС при закрытой игре. Автоматического импорта
файлов из каталога Linux-программы нет.

Пакеты с окончанием `-portable.tar.gz` или `-portable.AppImage` хранят оба файла
рядом с программой или внешним AppImage. Режим встроен в `arrow_puzzle_portable`:
маркер и определённое имя файла не нужны. XDG игнорируется; при недоступности
записи portable не переключается на пользовательские каталоги. Папка должна
быть доступна для записи. Менеджер AppImage, перемещающий только образ, не
переносит соседние файлы данных.

Screenshot-хуки есть только в Debug. Release и RelWithDebInfo исключают их;
горячие клавиши создания снимков отключены во всех конфигурациях.
Для QA задавайте временные абсолютные XDG-пути, сохраняя личный прогресс:

```bash
cmake --preset linux-debug
cmake --build build/linux-debug --parallel 2 --target arrow_puzzle
mkdir -p "$PWD/out/linux-smoke"
cd out/linux-smoke
XDG_CONFIG_HOME="$PWD/config" XDG_DATA_HOME="$PWD/data" \
    ARROWPUZZLE_SCREENSHOT=menu.png ARROWPUZZLE_SCREENSHOT_MENU=1 \
    ARROWPUZZLE_SCREENSHOT_FRAME=4 ../../build/linux-debug/arrow_puzzle
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

Эта проверка обычной игры требует `xdotool` и X11 либо Xvfb. Внешний ввод
проверяет запуск, игнорирование QA-переменных, отсутствие файлов после F12 и
Control+F12 и штатное завершение по Escape. Внутриигровые хуки не используются.
Отдельные детерминированные QA-сценарии запускаются так:

```bash
python3 tools/smoke-linux.py build/linux-debug/arrow_puzzle --debug-hooks
python3 tools/check-production-binary.py build/linux/arrow_puzzle
```

**Build and test** (`.github/workflows/build.yml`) проверяет push в ветки, PR
и ручные запуски на Windows/MSVC, Ubuntu 22.04/GCC и Ubuntu 24.04/Clang. Для
Linux выполняются проверки рендера/текста Mesa/Xvfb и распакованного пакета.
Пакеты, логи и скриншоты доступны в артефактах запуска 14 дней. Этот workflow
не создаёт теги и релизы; общий workflow **Release** с attestations остаётся отдельным; см. [RELEASING.md](RELEASING.md).

Ubuntu 22.04/GCC — выбранная нижняя сборочная база CI, но не обещание работы
на любых старых системах. Смотрите `required_symbol_versions` конкретного
пакета: сборка Mint 22.3/GCC 13 требует более новых glibc/libstdc++, чем сборка
Ubuntu 22.04. Системные библиотеки и драйверы X11/OpenGL всё равно нужны. Ни один формат сам
по себе не подтверждает native Wayland или производительность другого GPU.

## AppImage после проверки tar

```bash
python3 tools/package-appimage.py out/linux-package/ArrowPuzzle-dev-local-linux-x64.tar.gz
chmod +x out/linux-package/ArrowPuzzle-dev-local-linux-x64.AppImage
./out/linux-package/ArrowPuzzle-dev-local-linux-x64.AppImage
```

Архив должен быть собран из текущего commit. Скрипт использует `curl` для
первого скачивания appimagetool 1.9.1 с зафиксированной SHA-256 и кэширует его
в `build/appimage-tools`. Через `--runtime-file` берётся ровно тот runtime,
который встроен в этот инструмент; плавающий runtime не скачивается. Включены
AppRun, desktop-файл, иконка, лицензия/ссылки на исходники runtime и метаданные.
Бинарник игры идентичен содержимому tar. Хэш AppImage дописывается в
`SHA256SUMS.txt`; перезапись готового файла запрещена.

Системные библиотеки игры и графические драйверы не включены: требования к
glibc/libstdc++ совпадают с исходным tar. Это контейнер в одном файле, который
не обеспечивает совместимость с любым старым дистрибутивом. Настройки и
сохранения обычной версии остаются вне образа в XDG-каталогах. Portable хранит
их рядом с внешним AppImage, в том числе при `--appimage-extract-and-run`, а не
во временном каталоге монтирования или распаковки.

Если FUSE недоступен (в том числе в текущей VMware VM), запуск не требует
монтирования или изменения системы:

```bash
./out/linux-package/ArrowPuzzle-dev-local-linux-x64.AppImage --appimage-extract-and-run
python3 tools/smoke-linux.py out/linux-package/ArrowPuzzle-dev-local-linux-x64.AppImage --appimage
```

CI создаёт AppImage только на Ubuntu 22.04/GCC после проверки распакованного
архива и проверяет запуск/ввод обычной игры через extraction-and-run. Семь
сценариев снимков/save/resume выполняются на отдельной Debug-сборке; релизная
упаковка принимает только Release. Перед упаковкой обе платформы также
проверяют обычный бинарник на отсутствие строк capture-хуков и горячих клавиш.
Обычное монтирование FUSE в текущей VM не проверено; extraction-and-run проверен.
Локально также проверены реальные X11-вводы Ctrl zoom/fit, drag от головы с
возвратом без запуска, неизменность прогресса, Alt+Enter/Escape и смена фокуса.
