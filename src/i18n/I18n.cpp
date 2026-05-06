#include "I18n.h"

#include <QHash>
#include <QSettings>

namespace {

// English → Ukrainian lookup. Missing entries simply fall back to English.
const QHash<QString, QString>& uk_table()
{
    static const QHash<QString, QString> t = {
        // Sidebar / chrome
        { "Volchay Cleans",                     "Volchay Cleans" },
        { "Navigation",                         "Навігація" },
        { "Overview",                           "Огляд" },
        { "Analyzer",                           "Аналіз" },
        { "Files",                              "Файли" },
        { "Settings",                           "Налаштування" },
        { "Cycle theme (Dark → Light → Blackout → RGB)",
                                                "Зміна теми (Темна → Світла → Чорна → RGB)" },
        { "☾  Dark",                            "☾  Темна" },
        { "☀  Light",                           "☀  Світла" },
        { "⬤  Blackout",                        "⬤  Чорна" },
        { "✦  RGB",                             "✦  RGB" },

        // Top bar / scan
        { "Drive:",                             "Диск:" },
        { "Path to scan…",                      "Шлях для сканування…" },
        { "Browse…",                            "Огляд…" },
        { "Pick a folder",                      "Виберіть папку" },
        { "Scan",                               "Сканувати" },
        { "Cancel",                             "Скасувати" },
        { "Ready",                              "Готово" },
        { "Scanning…",                          "Сканування…" },
        { "Cancelling…",                        "Скасування…" },
        { "Scan cancelled",                     "Сканування скасовано" },
        { "Specify a path to scan",             "Вкажіть шлях для сканування" },
        { "Choose folder",                      "Виберіть папку" },
        { "Files: %1 · %2",                     "Файлів: %1 · %2" },
        { "Done: %1 files · %2",                "Готово: %1 файлів · %2" },
        { "Error: %1",                          "Помилка: %1" },

        // Overview
        { "Drives",                             "Диски" },
        { "Pick a drive to inspect its contents.",
                                                "Виберіть диск, щоб переглянути його вміст." },
        { "Refresh",                            "Оновити" },
        { "Analyze",                            "Аналізувати" },
        { "Used: %1 of %2",                     "Зайнято: %1 з %2" },
        { "Free: %1",                           "Вільно: %1" },
        { "No label",                           "Без мітки" },

        // Overview — stats card
        { "Storage at a glance",                "Загальна статистика" },
        { "No drives detected.",                "Дисків не знайдено." },
        { "Total",                              "Усього" },
        { "Used",                               "Зайнято" },
        { "Free",                               "Вільно" },
        { "Filesystems",                        "Файлові системи" },
        { "Most used drive",                    "Найбільш заповнений диск" },
        { "Fullest drive",                      "Найбільш заповнений (% )" },
        { "Combined usage",                     "Спільне використання" },
        { "Per-drive usage",                    "По кожному диску" },
        { "By type",                            "За типом" },
        { "Largest files",                      "Найбільші файли" },
        { "Unknown",                            "Невідомо" },
        { "Last scan",                          "Останнє сканування" },
        { "%1  ·  %2 files  ·  %3  ·  %4 folders",
                                                "%1  ·  %2 файлів  ·  %3  ·  %4 папок" },
        { "Finished: %1",                       "Завершено: %1" },
        { "Largest type: %1  ·  %2",            "Найбільший тип: %1  ·  %2" },

        // Analyzer
        { "Folder analysis",                    "Аналіз папок" },
        { "Folder tree with sizes appears here once a scan finishes.",
                                                "Дерево папок із розмірами з'явиться після сканування." },
        { "Search by name…",                    "Пошук за назвою…" },
        { "Open",                               "Відкрити" },
        { "Open the selected file or folder",   "Відкрити вибраний файл або папку" },
        { "Show in Windows Explorer",           "Показати у Провіднику Windows" },
        { "Root: %1 · Files: %2 · Size: %3 · Folders: %4",
                                                "Корінь: %1 · Файлів: %2 · Розмір: %3 · Папок: %4" },
        { "Folder",                             "Папка" },
        { "Size",                               "Розмір" },
        { "% of parent",                        "% від батька" },
        { "Files count",                        "Файлів" },

        // Files view
        { "Files",                              "Файли" },
        { "Run a scan to see the file list with type filters.",
                                                "Запустіть сканування, щоб побачити список файлів з фільтрами." },
        { "Search by name or path…",            "Пошук за назвою або шляхом…" },
        { "Files found: %1 · Total size: %2",   "Знайдено: %1 файлів · Загалом: %2" },
        { "Delete",                             "Видалити" },
        { "Click to move to Recycle Bin · Shift+Click to delete permanently",
                                                "Клік — у Кошик · Shift+Клік — видалити назавжди" },
        { "Move to Recycle Bin",                "Перемістити в Кошик" },
        { "Move this item to the Recycle Bin?\n\n%1",
                                                "Перемістити цей елемент в Кошик?\n\n%1" },
        { "Delete permanently",                 "Видалити назавжди" },
        { "Delete forever",                     "Видалити назавжди" },
        { "This will permanently delete:\n%1\n\nThis cannot be undone.",
                                                "Це назавжди видалить:\n%1\n\nЦю дію неможливо скасувати." },
        { "Don't ask again",                    "Більше не питати" },
        { "Could not delete:\n%1\n\n%2",        "Не вдалося видалити:\n%1\n\n%2" },

        // File categories (also used as chip labels)
        { "All",                                "Усі" },
        { "Video",                              "Відео" },
        { "Photo",                              "Фото" },
        { "Audio",                              "Аудіо" },
        { "Document",                           "Документи" },
        { "Documents",                          "Документи" },
        { "Archive",                            "Архіви" },
        { "Archives",                           "Архіви" },
        { "Executable",                         "Виконувані" },
        { "Executables",                        "Виконувані" },
        { "Code",                               "Код" },
        { "Other",                              "Інше" },

        // File table
        { "Name",                               "Назва" },
        { "Type",                               "Тип" },
        { "Modified",                           "Змінено" },
        { "Path",                               "Шлях" },

        // Details panel
        { "File details",                       "Деталі файла" },
        { "Select a file in the table on the left.",
                                                "Виберіть файл у таблиці зліва." },
        { "Copy path",                          "Копіювати шлях" },
        { "Reveal in Explorer",                 "Відкрити в Провіднику" },
        { "Compute MD5",                        "Обчислити MD5" },
        { "Computing MD5…",                     "Обчислюємо MD5…" },
        { "MD5: %1",                            "MD5: %1" },
        { "Type:",                              "Тип:" },
        { "MIME:",                              "MIME:" },
        { "Size:",                              "Розмір:" },
        { "Created:",                           "Створено:" },
        { "Accessed:",                          "Останній доступ:" },
        { "Attributes:",                        "Атрибути:" },
        { "Hidden",                             "Прихований" },
        { "Read-only",                          "Тільки читання" },
        { "System",                             "Системний" },

        // Settings
        { "Tweak the theme, language, sounds, and scan exclusions.",
                                                "Налаштуйте тему, мову, звуки та виключені папки." },
        { "Theme",                              "Тема" },
        { "During scanning",                    "Під час сканування" },
        { "Show the radar with discovered files",
                                                "Показувати радар зі знайденими файлами" },
        { "The radar appears in the centre and visualises files as they are discovered. Turn it off if you prefer a calmer view.",
                                                "Радар з'являється посередині та показує знайдені файли. Вимкніть, якщо хочете спокійніший вигляд." },
        { "Sounds",                             "Звуки" },
        { "Play sound effects",                 "Програвати звукові ефекти" },
        { "Soft tones for scan start, scan complete, and other UI events.",
                                                "М'які тони на старт сканування, завершення та інші події інтерфейсу." },
        { "Language",                           "Мова" },
        { "Switches the interface language instantly.",
                                                "Миттєво змінює мову інтерфейсу." },
        { "Excluded folders",                   "Виключені папки" },
        { "These paths are skipped during scans (e.g. network drives, backup folders).",
                                                "Ці шляхи пропускаються під час сканування (наприклад, мережеві диски або бекапи)." },
        { "Add…",                               "Додати…" },
        { "Remove",                             "Видалити" },
        { "Pick a folder to exclude",           "Виберіть папку для виключення" },
        { "Volchay Cleans · 0.2.0 · C++/Qt 6 · Analyses files locally; nothing is auto-deleted.",
                                                "Volchay Cleans · 0.2.0 · C++/Qt 6 · Аналізатор файлів; нічого не видаляється автоматично." },

        // Radar
        { "Hide radar",                         "Сховати радар" },
        { "Disable animation",                  "Вимкнути анімацію" },
        { "Root: %1",                           "Корінь: %1" },

        // Refresh / mark-deleted state
        { "Remove the rows that were marked as deleted from the tree",
                                                "Прибрати рядки, позначені як видалені, з дерева" },

        // Overview — configure / new histograms
        { "⚙ Configure",                        "⚙ Налаштувати" },
        { "Pick which statistics to show on the Overview page",
                                                "Виберіть, які блоки показувати на сторінці Огляд" },
        { "Overview statistics",                "Статистика огляду" },
        { "Combined usage bar",                 "Спільна шкала зайнятості" },
        { "Per-drive usage bars",               "Шкали по дисках" },
        { "Filesystem breakdown",               "Файлові системи" },
        { "Low free-space warning",             "Попередження про мало вільного місця" },
        { "Low space",                          "Мало вільного місця" },
        { "Last scan summary",                  "Підсумок останнього сканування" },
        { "Top file categories",                "Найбільші категорії файлів" },
        { "File age histogram",                 "Розподіл за віком файлів" },
        { "File size buckets",                  "Розподіл за розміром файлів" },
        { "File age",                           "Вік файлів" },
        { "File size",                          "Розмір файлів" },
        { "Last 7 days",                        "Останні 7 днів" },
        { "Last 30 days",                       "Останні 30 днів" },
        { "Last year",                          "Останній рік" },
        { "Older",                              "Старіші" },
        { "< 1 MB",                             "< 1 МБ" },
        { "1 MB – 100 MB",                      "1 МБ – 100 МБ" },
        { "100 MB – 1 GB",                      "100 МБ – 1 ГБ" },
        { "> 1 GB",                             "> 1 ГБ" },
    };
    return t;
}

}  // namespace

I18n& I18n::instance()
{
    static I18n inst;
    return inst;
}

void I18n::setLanguage(Lang lang)
{
    if (m_lang == lang) {
        return;
    }
    m_lang = lang;
    QSettings s;
    s.setValue(QStringLiteral("language"),
               lang == Ukrainian ? QStringLiteral("uk") : QStringLiteral("en"));
    emit languageChanged(lang);
}

QString I18n::tr_s(const QString& english) const
{
    if (m_lang == English) {
        return english;
    }
    const auto& t = uk_table();
    const auto it = t.constFind(english);
    return it != t.constEnd() ? it.value() : english;
}

QString I18n::languageName(Lang lang)
{
    return lang == Ukrainian ? QStringLiteral("Українська")
                             : QStringLiteral("English");
}

void I18n::loadFromSettings()
{
    QSettings s;
    const QString name = s.value(QStringLiteral("language"),
                                 QStringLiteral("en")).toString();
    m_lang = (name == QStringLiteral("uk")) ? Ukrainian : English;
}
