<p align="center">
  <img src="src/disk_cleaner/assets/volchay_logo.svg" width="96" alt="Volchay" align="middle">
  <span style="font-size: 48px; font-weight: 600; vertical-align: middle;">&nbsp;Cleans</span>
</p>

<h1 align="center">Volchay Cleans</h1>

<p align="center">
  Кроссплатформенный анализатор и чистильщик диска на Python + PySide6.
</p>

## Возможности

- **Анализ диска**: рекурсивный обход с учётом hard-link'ов (дедуп по inode), отменяемое сканирование, агрегированные размеры по папкам.
- **Фильтр по типу файлов**: Видео / Фото / Аудио / Документы / Архивы / Код / Установщики / Прочее. Топ-N самых больших файлов выбранного типа.
- **Поиск «мусора»**: корзина, временные папки, кэши браузеров (Chrome/Chromium/Firefox), `Thumbs.db`/`.DS_Store`, старые большие файлы.
- **Поиск дубликатов**: двухпроходный алгоритм (по размеру → BLAKE2b).
- **Безопасное удаление**: через системную корзину (`send2trash`), dry-run по умолчанию, лог всех действий.
- **Темы**: светлая и тёмная, единый акцент `#CC785C`. Переключение в меню «Вид → Тема», сохраняется через `QSettings`.

## Запуск

```bash
pip install -e .
volchay-cleans            # GUI
volchay-cleans-cli ~/     # CLI: показать топ-папок
```

`disk-cleaner` и `disk-cleaner-cli` оставлены как алиасы.

## Разработка

```bash
pip install -e .[dev]
ruff check src tests
mypy
QT_QPA_PLATFORM=offscreen pytest -q
```

## Архитектура

- `disk_cleaner.scanner` — обход ФС, агрегация размеров, дедуп hard-link'ов.
- `disk_cleaner.classifier` — категоризация файлов по типу.
- `disk_cleaner.junk` — правила обнаружения «мусора» (декларативные).
- `disk_cleaner.duplicates` — поиск дубликатов.
- `disk_cleaner.actions` — удаление в корзину, dry-run, лог.
- `disk_cleaner.gui` — PySide6-окно (`gui.theme`, `gui.spinner`, `gui.workers`).
- `disk_cleaner.cli` — CLI-режим.

## Лицензия

MIT
