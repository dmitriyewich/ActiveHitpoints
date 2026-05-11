![Анимация](https://github.com/user-attachments/assets/e50e9c25-5ff1-4680-be31-e108d21db8b9)
# ActiveHitpoints

`ActiveHitpoints` показывает цвет треугольника над целью в `GTA San Andreas / SA:MP` в зависимости от здоровья игрока.

Плагин распознаёт клиент `SA:MP` по `AddressOfEntryPoint` у `samp.dll` и поддерживает сборки **0.3.7 R1, R2, R3, R3-1, R4, R4-2, R5-1** и **0.3.DL R1**.

В репозитории лежат:

- исходники `ASI`-плагина на `C++`
- проект `Visual Studio` для сборки
- версия скрипта для `MoonLoader`

## Структура

- `source/main.cpp` — исходный код `ASI`-плагина
- `ActiveHitpoints.vcxproj` — проект `Visual Studio`
- `ActiveHitpoints.sln` — solution для сборки
- `moonloader/ActiveHitpoints.lua` — версия скрипта для `MoonLoader`

## Сборка

Сборка рассчитана на `Win32` и `Visual Studio`.

Основная конфигурация:

- `Release | Win32`

После сборки получается файл (путь по умолчанию в проекте):

- `build/Release/Win32/ActiveHitpoints.asi`

## CI

Ветка `main`: workflow **Build ActiveHitpoints Release Win32** (см. `.github/workflows/`) собирает `Release|Win32` с `PlatformToolset=v143` и выкладывает артефакт `.asi`. При публикации **Release** на GitHub тот же файл прикрепляется к релизу.

## Примечание

Релизы и готовые бинарники публикуются отдельно.

В индекс `git` не следует добавлять каталоги **`build/`**, **`.cursor/`**, **`.claude/`** и прочие локальные артефакты IDE.
