# Релизы Windows, собранные в GitHub Actions

[English](../en/RELEASING.md) | **Русский**

Workflow **Windows release** собирает исполняемый файл на размещённом в GitHub
runner Windows 2022. Он запускается только после отправки тега `vX.Y.Z` или
ручного запуска владельцем через **Run workflow**. Для push в ветки и PR
используется отдельный workflow проверок, описанный ниже.

## Проверки до релиза

Отдельный workflow **Build and test** (`build.yml`) запускается при push в ветки,
для PR и вручную. Release и все пять штатных наборов проверяются на Windows
2022/MSVC, Ubuntu 22.04/GCC и Ubuntu 24.04/Clang. Linux дополнительно проверяет
рендер/текст через Mesa/Xvfb и запускает распакованный tar-архив: изолированные
XDG-каталоги, папка программы только для чтения, меню, обе темы и save/resume
с CRLF. Для проверки аудиомикшера по-прежнему нужно локальное устройство.

Ubuntu 22.04 также собирает AppImage после проверки tar и проверяет запуск
через распаковку без FUSE. Успешные задачи сохраняют ZIP, tar.gz и (на Ubuntu
22.04) AppImage с SHA-256 и метаданными на 14 дней. Файлы указывают проверенные
commit/run, но не имеют attestations релиза.
Логи и Linux-скриншоты сохраняются и при сбое. У workflow только права чтения
репозитория: он не создаёт теги, черновики и опубликованные релизы. Пакеты
скачиваются со страницы запуска Actions; упаковка Linux описана в [LINUX.md](LINUX.md).

## Предварительная сборка до создания тега

Для скриншотов будущего релиза используйте отдельный каталог Release-сборки,
пока релизного тега ещё нет. Из MSVC x64 Developer PowerShell:

```powershell
cmake -S . -B build/release-preview -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DARROWPUZZLE_RELEASE_PREVIEW=ON
cmake --build build/release-preview --parallel 2
ctest --test-dir build/release-preview --output-on-failure
.\tools\package-windows.ps1 -Version 0.4.0 `
    -BuildDirectory build/release-preview -OutputDirectory out/release-preview
```

`ARROWPUZZLE_RELEASE_PREVIEW` показывает версию проекта из CMake, например
`v0.4.0`, не создавая и не перемещая Git-теги. По умолчанию параметр выключен:
обычные сборки определяют версию по последнему релизному тегу и числу коммитов.
GitHub Actions его не включает: сборка тега получает версию из самого тега.
Локальная предварительная сборка не имеет подтверждения происхождения GitHub;
её `build-info.json` показывает, что сборка выполнена локально.

Завершите README и добавьте релизные скриншоты **до** создания тега. На этом
финальном этапе укажите дату соответствующего раздела changelog и используйте
его содержимое в описании релиза на GitHub.

## Подготовка релиза

1. Завершите changelog и документацию, затем закоммитьте и отправьте нужные
   исходники вместе с workflow. Версия тега должна совпадать с
   `project(... VERSION ...)` в `CMakeLists.txt`.
2. Создайте и отправьте стабильный тег, например `v0.4.0`, на этом коммите.
3. Откройте **Actions → Windows release** и дождитесь сборки и тестов.
4. Проверьте созданный черновик в **Releases**, добавьте соответствующий раздел
   changelog к сведениям о происхождении сборки и опубликуйте релиз.

Сборки тегов создают черновик от имени `github-actions[bot]`. Повторная сборка
может обновить существующий черновик; файлы уже опубликованного релиза она
никогда не заменяет. Workflow не требует персонального access token или
секрета для подписи.

Для ручной сборки выберите ветку или тег в **Run workflow**. Оставьте
**Create/update a draft release** выключенным, если нужны только скачиваемые
артефакты Actions. Для подготовки черновика выберите существующий тег
`vX.Y.Z` и включите эту опцию. Теги должны уже существовать: workflow их не
создаёт и не перемещает.

## Файлы и проверки

- `ArrowPuzzle-<version>-windows-x64.zip` содержит автономный EXE, оба README,
  changelog, техническую документацию и скриншоты, лицензии и `build-info.json`.
  Относительные ссылки README работают после распаковки.
- `SHA256SUMS.txt` содержит контрольные суммы ZIP и распакованного
  `ArrowPuzzle.exe`.
- `build-info.json` содержит коммит исходников, отображаемую версию,
  контрольную сумму EXE и ссылку на запуск Actions.
- `attestation.sigstore.json` — подписанный пакет подтверждения происхождения
  для EXE и ZIP.

Job использует тот же Release-скрипт, что и локальная разработка, запускает
полный headless-набор CTest и упаковывает только что собранный EXE. Проверки
GPU и звука, которым нужны дисплей и устройство воспроизведения, остаются
отдельными локальными проверками. raylib зафиксирована на точном коммите 5.5,
а actions — на полных хешах коммитов.

## Проверка скачанных файлов

С помощью актуальной [GitHub CLI](https://cli.github.com/) проверьте ZIP до
распаковки, подставив имя скачанной версии:

```powershell
gh attestation verify .\ArrowPuzzle-0.4.0-windows-x64.zip `
    --repo gugglegum/arrow_puzzle_game `
    --signer-workflow gugglegum/arrow_puzzle_game/.github/workflows/release.yml `
    --deny-self-hosted-runners
```

Распакованный исполняемый файл можно проверить отдельно:

```powershell
gh attestation verify .\ArrowPuzzle.exe `
    --repo gugglegum/arrow_puzzle_game `
    --signer-workflow gugglegum/arrow_puzzle_game/.github/workflows/release.yml `
    --deny-self-hosted-runners
```

Добавьте `--source-digest <full-commit-sha>`, чтобы потребовать конкретный коммит
исходников. Проверяются контрольная сумма файла и личность GitHub workflow,
выполнившего сборку. Это подтверждает происхождение сборки, но не означает
независимый аудит всего кода или побайтовое совпадение EXE при разных версиях
компилятора. Эта проверка также не связана с Windows Authenticode и репутацией
SmartScreen.

См. [документацию GitHub по attestations](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations).

## Локальная упаковка

После успешной Release-сборки тот же шаг упаковки доступен локально:

```powershell
.\tools\package-windows.ps1 -Version dev-local
```

Он сохраняет результат в `out/release`, не меняет входные файлы сборки и
отказывается перезаписывать уже существующий ZIP. Локальный пакет содержит
контрольные суммы и сведения о сборке, но подтверждение происхождения GitHub
для данного workflow может создать только запуск Actions.
