# Volchay Cleans

[![Build Windows .exe](https://github.com/reideveloperr-jpg/disk-cleaner/actions/workflows/build.yml/badge.svg)](https://github.com/reideveloperr-jpg/disk-cleaner/actions/workflows/build.yml)

> Анализатор диска для Windows на C++/Qt 6. Только смотрит — ничего не удаляет.

Volchay Cleans помогает понять, чем занят диск. Программа сканирует выбранную папку или весь диск, строит дерево размеров и показывает все файлы в одной таблице с фильтрами по типу (Видео, Фото, Аудио, Документы, Архивы, Исполняемые, Код, Прочее), поиском, сортировкой и подробной информацией о каждом файле. **Ничего не удаляет** — все решения по очистке принимаешь сам, открывая файлы в проводнике.

## Возможности

- **Обзор дисков** — карточки всех смонтированных дисков с круговым индикатором заполненности.
- **Анализ папок** — дерево с размерами и долей в родительской папке, отсортированное по убыванию.
- **Файлы** — таблица всех найденных файлов с фильтром-чипсами по типу, поиском, сортировкой по любому столбцу и панелью деталей справа (имя, путь, размер, даты, MIME, атрибуты, MD5 по запросу).
- **Тёмная и светлая темы** с акцентом «Claude orange» (`#D97757`). Переключение по клику в сайдбаре.
- **Фоновое сканирование** в отдельном потоке с отменой и крутящимся круглым лоадером (никаких прогресс-баров).
- **Исключения** — список папок, которые пропускаются при сканировании (например, сетевые диски).
- **Открытие в проводнике** и копирование пути в один клик.

## Скриншоты

В тёмной теме приложение использует фон `#1B1B1F`, в светлой — `#FAFAF7`. Акцент во всех состояниях — `#D97757`. Скриншоты прилагаются к PR `feat/initial-app`.

## Сборка из исходников

### Требования

- **CMake** ≥ 3.16
- **Qt 6** (модули `Widgets`, `Svg`, `SvgWidgets`, `Concurrent`)
- Компилятор с поддержкой C++17:
  - **Windows:** MSVC 2019+ или MinGW-w64
  - **Linux:** g++ ≥ 11 (для отладки)

### Windows (MSVC + Qt 6)

```powershell
# 1. Установить Qt 6 через aqt или Qt Installer.
# 2. Открыть x64 Native Tools Command Prompt for VS 2022.
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.7.0/msvc2019_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
windeployqt build/Release/VolchayCleans.exe
```

`build/Release/VolchayCleans.exe` — готовый исполняемый файл. Папка `build/Release` после `windeployqt` содержит все нужные DLL и плагины Qt — её можно архивировать целиком и запускать на любом Windows 10/11.

### Linux (для разработки)

```bash
sudo apt-get install -y qt6-base-dev qt6-tools-dev libqt6svg6-dev \
                        libqt6svgwidgets6 cmake g++ libgl1-mesa-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/VolchayCleans
```

## Готовый .exe

Каждый push в `main` собирает Windows-бинарь через GitHub Actions:

1. Открыть [Actions](https://github.com/testdurnya02-alt/Volchay-Cleans/actions).
2. Выбрать последний запуск workflow **Build Windows .exe**.
3. Внизу страницы скачать артефакт **VolchayCleans-windows-x64**.
4. Распаковать ZIP и запустить `VolchayCleans.exe`.

## Архитектура

```
src/
├── main.cpp                     # точка входа, тема, шрифт, MainWindow
├── core/
│   ├── SizeFormatter            # форматирование размеров (КБ/МБ/ГБ)
│   ├── FileType                 # классификация по расширению
│   ├── ScanResult               # POD-структуры результата сканирования
│   ├── ScanWorker               # рекурсивное сканирование в QThread
│   ├── HashWorker               # MD5 по запросу
│   └── DriveInfo                # перечисление дисков
├── models/
│   ├── FileTableModel           # таблица файлов + прокси-фильтр
│   └── FolderTreeModel          # дерево папок с размерами
├── platform/
│   └── ShellOps                 # «Открыть в проводнике», буфер обмена
└── ui/
    ├── ThemeManager             # тёмная/светлая тема, QSS, палитра
    ├── SpinnerWidget            # крутящийся круглый лоадер
    ├── RingIndicator            # статичный круг для % заполненности
    ├── Sidebar / MainWindow     # навигация и каркас окна
    ├── OverviewView / DriveCard # карточки дисков
    ├── AnalyzerView             # дерево папок
    ├── FilesView / FilterChips / DetailsPanel
    └── SettingsView             # настройки
```

## Безопасность

- Программа никогда не удаляет файлы. Все операции — только чтение.
- Запускается без UAC (`asInvoker`). Системные папки, к которым нет прав чтения, тихо пропускаются.
- Симлинки не разворачиваются, чтобы не зацикливаться.
- MD5 считается только по явному нажатию кнопки и в фоновом потоке, чтобы не блокировать UI.

## Лицензия

[MIT](LICENSE)
