# Релизы Windows и Linux, собранные GitHub Actions

[English](../en/RELEASING.md) | **Русский**

Workflow **Release** (`release.yml`) собирает Windows x64 на Windows 2022/MSVC
и Linux x86-64 на Ubuntu 22.04/GCC. Обе платформы и происхождение файлов
проверяются до объединения в общий набор. Push тега версии либо явно выбранный
ручной запуск тега подготавливает **черновик**; публикация остаётся ручной.
Workflow не создаёт и не перемещает теги, не публикует релиз.

## Проверки до релиза

Отдельный workflow **Build and test** (`build.yml`) запускается при push в ветки,
для PR и вручную. Release и пять штатных наборов CTest проверяются на Windows/MSVC,
Ubuntu 22.04/GCC и Ubuntu 24.04/Clang. Проверяются и ограничения сборки релиза:
смешанные коммиты/версии, изменённые исходники, подмена пакетов/бинарников,
отсутствие подписей и некорректные параметры создания черновика.

Проверки рендера/текста Linux используют Mesa/Xvfb. Распакованный tar и AppImage
проходят меню, обе темы, запуск без звука и save/resume с CRLF, изолированными
XDG-путями и папкой программы только для чтения. Интерактивные проверки GPU/ввода
и аудиоустройства по-прежнему требуют рабочего стола. Артефакты разработки
хранятся 14 дней и не имеют attestations релизного workflow.

## Проверка всего релизного процесса без черновика

Откройте **Actions → Release → Run workflow**, выберите нужную ветку и оставьте
**Create/update a draft release** выключенным. Выполнятся те же сборки, тесты,
упаковка, проверка происхождения обеих платформ, объединение и attestation
контрольных сумм, что и для тега. Общий набор доступен в артефакте
`release-<source-sha>`; релизные артефакты хранятся 30 дней. Задача создания
черновика пропускается: тег, черновик и опубликованный релиз не создаются.

Общий набор собирается при каждом успешном ручном запуске, включая запуск
ветки. Это позволяет проверить доставку до создания тега.

## Подготовка черновика релиза

1. Завершите changelog, скриншоты и документацию, закоммитьте и отправьте нужные
   исходники и workflow. Задайте требуемую версию в `project(... VERSION ...)`.
2. При наличии разрешения на подготовку этой версии создайте и отправьте новый
   стабильный тег `vX.Y.Z` на этот коммит. Версия тега проверяется по CMake.
   Уже опубликованные теги и файлы нельзя переиспользовать или менять.
3. Откройте **Actions → Release**. Обе платформы должны успешно пройти проверки
   до общего объединения и проверки происхождения. Только после этого задача
   черновика может создать/обновить общий Windows/Linux-черновик от имени
   `github-actions[bot]`.
4. Проверьте черновик и нужный раздел changelog. Публикация выполняется только
   после отдельного решения владельца.

Для ручной подготовки черновика выберите существующий тег `vX.Y.Z`, совпадающий
с CMake, и включите **Create/update a draft release**. Ветка с включённой опцией
не пройдёт проверку ещё до сборки. Перед записью файлов workflow повторно
проверяет, что тег указывает на собранный коммит. Повторные запуски могут
обновлять черновик, но не заменяют файлы опубликованного релиза. Сбой платформы
или проверки не приводит к созданию частичного общего черновика.

Исторические релизы только для Windows сохраняют исходные файлы и версию
workflow. Повторный запуск старого опубликованного тега не добавляет в него
Linux. Персональный access token или секрет подписи workflow не требуется.

## Файлы и происхождение

| Файл общего черновика | Содержание / назначение |
| --- | --- |
| `ArrowPuzzle-<version>-windows-x64.zip` | Автономный `ArrowPuzzle.exe`, документация, скриншоты, лицензии и внутренний `build-info.json` |
| `ArrowPuzzle-<version>-linux-x64.tar.gz` | Исполняемый `ArrowPuzzle`, документация, лицензии и внутренний `build-info.json` |
| `ArrowPuzzle-<version>-linux-x64.AppImage` | Тот же Linux-бинарник в одном файле с иконкой, launcher, сведениями о runtime и метаданными |
| `SHA256SUMS.txt` | SHA-256 трёх скачиваемых пакетов и обоих распакованных бинарников |
| `build-info-windows-x64.json` | Коммит, версия, хэш EXE и запуск Actions для Windows |
| `build-info-linux-x64.json` | Исходники/окружение Linux, хэш бинарника и версии символов системных библиотек |
| `build-info-linux-appimage-x64.json` | Хэши AppImage и исходного tar, бинарник и зафиксированные инструмент/runtime |
| `attestation-windows-x64.sigstore.json` | Подписанное подтверждение происхождения EXE и ZIP |
| `attestation-linux-x64.sigstore.json` | Подписанное подтверждение происхождения Linux-бинарника, tar и AppImage |
| `attestation-checksums.sigstore.json` | Подписанное подтверждение происхождения общего файла контрольных сумм |

Имена с платформой предотвращают перезапись метаданных и подписей. Скрипт
объединения проверяет коммиты, версии, отсутствие изменений исходников, хэши,
внутренние метаданные и бинарники архивов, исполняемость Linux-файла и связь
AppImage с tar до копирования файлов. `gh attestation verify` отдельно проверяет
криптографические пакеты, репозиторий, workflow, точный коммит и использование
GitHub-hosted runners. Общий файл сумм тоже получает attestation и проверяется
до запуска задачи черновика.

Обе платформы используют те же Release-скрипты сборки/упаковки, что и локальная
разработка. raylib, actions и инструмент/runtime AppImage зафиксированы.
Linux использует CMake 3.28.3 и сборочную базу Ubuntu 22.04; требования библиотек записаны в
метаданных пакета. Зависимости X11/OpenGL и запуск без FUSE описаны в
[LINUX.md](LINUX.md).

## Проверка скачанных файлов

Используйте актуальную [GitHub CLI](https://cli.github.com/). Замените `<version>`
ниже на версию скачанного пакета. Пример для Linux:

```bash
gh attestation verify "ArrowPuzzle-<version>-linux-x64.AppImage" \
    --repo gugglegum/arrow_puzzle_game \
    --signer-workflow gugglegum/arrow_puzzle_game/.github/workflows/release.yml \
    --deny-self-hosted-runners
```

Та же команда проверяет tar.gz или Windows ZIP при подстановке имени файла.
Распакованные `ArrowPuzzle` и `ArrowPuzzle.exe` можно проверять отдельно.
`--source-digest <full-commit-sha>` требует конкретный коммит. Для проверки
скачанного пакета подтверждения добавьте
`--bundle attestation-linux-x64.sigstore.json` либо файл Windows.

Сначала проверьте происхождение общего файла сумм, затем скачанные пакеты:

```bash
gh attestation verify SHA256SUMS.txt \
    --bundle attestation-checksums.sigstore.json \
    --repo gugglegum/arrow_puzzle_game \
    --signer-workflow gugglegum/arrow_puzzle_game/.github/workflows/release.yml \
    --deny-self-hosted-runners
sha256sum --check --ignore-missing SHA256SUMS.txt
```

Повторите проверку сумм после распаковки для проверки бинарника. В Windows
хэши вычисляются через `Get-FileHash -Algorithm SHA256`; команда
`gh attestation verify` принимает и Windows-пути. В старых релизах только для
Windows внешние файлы называются `build-info.json` и `attestation.sigstore.json`;
их EXE/ZIP по-прежнему проверяются с той же идентичностью workflow.

Проверки подтверждают идентичность файлов и происхождение сборки, но не
независимый аудит кода или совпадение байтов разных компиляторов. Windows
Authenticode и репутация SmartScreen — отдельные механизмы.
См. [документацию GitHub по attestations](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations)
и [параметры проверки](https://cli.github.com/manual/gh_attestation_verify).

## Локальная предварительная сборка и упаковка

`ARROWPUZZLE_RELEASE_PREVIEW=ON` показывает будущую версию CMake до создания
тега. Для обычных сборок и Actions оставляйте OFF. Локальная подготовка не
создаёт тегов и не имеет GitHub-attestation. Пример из MSVC x64 shell после
выбора требуемой версии CMake:

```powershell
cmake -S . -B build/release-preview -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DARROWPUZZLE_RELEASE_PREVIEW=ON
cmake --build build/release-preview --parallel 2
ctest --test-dir build/release-preview --output-on-failure
.\tools\package-windows.ps1 -Version dev-local `
    -BuildDirectory build/release-preview -OutputDirectory out/release-preview
```

Обычная локальная Release-упаковка использует `tools/package-windows.ps1`,
`tools/package-linux.py`, затем `tools/package-appimage.py`; полные команды
приведены в руководстве Linux. Готовые файлы не перезаписываются. Подтверждения
происхождения GitHub этого workflow может создавать только запуск Actions.
