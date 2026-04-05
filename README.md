![Анимация](https://github.com/user-attachments/assets/e50e9c25-5ff1-4680-be31-e108d21db8b9)
# ActiveHitpoints

`ActiveHitpoints` показывает цвет треугольника над целью в `GTA San Andreas / SA:MP` в зависимости от здоровья игрока.

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

После сборки получается файл:

- `ActiveHitpoints.asi`

## Примечание

Релизы и готовые бинарники публикуются отдельно.
