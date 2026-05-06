#pragma once

#include <QObject>
#include <QString>

// Tiny in-process translation system.
//
// English is treated as the primary key for every visible string. When the
// active language is English we just return the key unchanged. For Ukrainian
// we look the key up in a static table; missing entries fall back to English.
//
// Use the `tr_()` macro everywhere a user-facing string would otherwise be
// `QStringLiteral("…")`. Numeric arguments still flow through `arg()` etc.,
// only the *template* needs to be translatable.

#define tr_(s) ::I18n::instance().tr_s(QStringLiteral(s))

class I18n : public QObject {
    Q_OBJECT
public:
    enum Lang {
        English   = 0,
        Ukrainian = 1,
    };

    static I18n& instance();

    Lang current() const { return m_lang; }
    void setLanguage(Lang lang);  // persists to QSettings + emits languageChanged

    // Look up a translation. Returns the original string if there is no entry
    // for the active language.
    QString tr_s(const QString& english) const;

    static QString languageName(Lang lang);  // "English" / "Українська"

    void loadFromSettings();

signals:
    void languageChanged(Lang lang);

private:
    I18n() = default;
    Lang m_lang = English;
};
