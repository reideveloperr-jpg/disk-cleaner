#include "ThemeManager.h"

#include "i18n/I18n.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QTextStream>

ThemeManager& ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

QString ThemeManager::themeName(Theme t)
{
    switch (t) {
        case Light:    return QStringLiteral("light");
        case Blackout: return QStringLiteral("blackout");
        case Rgb:      return QStringLiteral("rgb");
        case Amber:    return QStringLiteral("amber");
        case Dark:
        default:       return QStringLiteral("dark");
    }
}

QString ThemeManager::themeShortLabel(Theme t)
{
    switch (t) {
        case Light:    return I18n::instance().tr_s(QStringLiteral("☀  Light"));
        case Blackout: return I18n::instance().tr_s(QStringLiteral("⬤  Blackout"));
        case Rgb:      return I18n::instance().tr_s(QStringLiteral("✦  RGB"));
        case Amber:    return I18n::instance().tr_s(QStringLiteral("⚡  Amber"));
        case Dark:
        default:       return I18n::instance().tr_s(QStringLiteral("☾  Dark"));
    }
}

QString ThemeManager::loadStylesheet(Theme theme) const
{
    QString path;
    switch (theme) {
        case Light:    path = QStringLiteral(":/styles/light.qss"); break;
        case Blackout: path = QStringLiteral(":/styles/blackout.qss"); break;
        case Rgb:      path = QStringLiteral(":/styles/rgb.qss"); break;
        case Amber:    path = QStringLiteral(":/styles/amber.qss"); break;
        case Dark:
        default:       path = QStringLiteral(":/styles/dark.qss"); break;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream ts(&f);
    return ts.readAll();
}

void ThemeManager::apply(QApplication* app, Theme theme)
{
    if (!app) {
        return;
    }
    m_theme = theme;
    app->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    // Palette tweaks so native widgets respect the theme even before QSS loads.
    QPalette pal = app->palette();
    if (theme == Dark) {
        // Neutral charcoal (cool dark gray) — the amber stays just on the
        // Highlight role so the rest of native widgets don't feel orange.
        pal.setColor(QPalette::Window,        QColor(0x13, 0x13, 0x16));
        pal.setColor(QPalette::WindowText,    QColor(0xEC, 0xEB, 0xE6));
        pal.setColor(QPalette::Base,          QColor(0x13, 0x13, 0x16));
        pal.setColor(QPalette::AlternateBase, QColor(0x17, 0x17, 0x1B));
        pal.setColor(QPalette::Text,          QColor(0xEC, 0xEB, 0xE6));
        pal.setColor(QPalette::Button,        QColor(0x1A, 0x1A, 0x1E));
        pal.setColor(QPalette::ButtonText,    QColor(0xEC, 0xEB, 0xE6));
        pal.setColor(QPalette::Highlight,     QColor(0xF5, 0x9E, 0x0B));
        pal.setColor(QPalette::HighlightedText, QColor(0x12, 0x0A, 0x00));
        pal.setColor(QPalette::ToolTipBase,   QColor(0x1A, 0x1A, 0x1E));
        pal.setColor(QPalette::ToolTipText,   QColor(0xEC, 0xEB, 0xE6));
        pal.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x8A));
    } else if (theme == Blackout) {
        pal.setColor(QPalette::Window,        QColor(0x00, 0x00, 0x00));
        pal.setColor(QPalette::WindowText,    QColor(0xEC, 0xE6, 0xD6));
        pal.setColor(QPalette::Base,          QColor(0x00, 0x00, 0x00));
        pal.setColor(QPalette::AlternateBase, QColor(0x07, 0x06, 0x04));
        pal.setColor(QPalette::Text,          QColor(0xEC, 0xE6, 0xD6));
        pal.setColor(QPalette::Button,        QColor(0x0B, 0x0A, 0x07));
        pal.setColor(QPalette::ButtonText,    QColor(0xEC, 0xE6, 0xD6));
        pal.setColor(QPalette::Highlight,     QColor(0xF5, 0x9E, 0x0B));
        pal.setColor(QPalette::HighlightedText, QColor(0x00, 0x00, 0x00));
        pal.setColor(QPalette::ToolTipBase,   QColor(0x0B, 0x0A, 0x07));
        pal.setColor(QPalette::ToolTipText,   QColor(0xEC, 0xE6, 0xD6));
        pal.setColor(QPalette::PlaceholderText, QColor(0x6A, 0x65, 0x55));
    } else if (theme == Rgb) {
        // Dark base similar to Blackout, the colour comes from the moving
        // glow + RGB ribbon overlays painted on top.
        pal.setColor(QPalette::Window,        QColor(0x07, 0x07, 0x0E));
        pal.setColor(QPalette::WindowText,    QColor(0xEC, 0xEB, 0xF6));
        pal.setColor(QPalette::Base,          QColor(0x0B, 0x0B, 0x14));
        pal.setColor(QPalette::AlternateBase, QColor(0x12, 0x12, 0x1D));
        pal.setColor(QPalette::Text,          QColor(0xEC, 0xEB, 0xF6));
        pal.setColor(QPalette::Button,        QColor(0x14, 0x14, 0x21));
        pal.setColor(QPalette::ButtonText,    QColor(0xEC, 0xEB, 0xF6));
        pal.setColor(QPalette::Highlight,     QColor(0xF5, 0x9E, 0x0B));
        pal.setColor(QPalette::HighlightedText, QColor(0x07, 0x07, 0x0E));
        pal.setColor(QPalette::ToolTipBase,   QColor(0x14, 0x14, 0x21));
        pal.setColor(QPalette::ToolTipText,   QColor(0xEC, 0xEB, 0xF6));
        pal.setColor(QPalette::PlaceholderText, QColor(0x7E, 0x7C, 0x90));
    } else if (theme == Amber) {
        // Amber Acri — deep warm browns + saturated amber-orange accent.
        pal.setColor(QPalette::Window,        QColor(0x1A, 0x12, 0x07));
        pal.setColor(QPalette::WindowText,    QColor(0xFB, 0xEC, 0xCB));
        pal.setColor(QPalette::Base,          QColor(0x14, 0x0D, 0x05));
        pal.setColor(QPalette::AlternateBase, QColor(0x21, 0x16, 0x09));
        pal.setColor(QPalette::Text,          QColor(0xFB, 0xEC, 0xCB));
        pal.setColor(QPalette::Button,        QColor(0x29, 0x1B, 0x0B));
        pal.setColor(QPalette::ButtonText,    QColor(0xFB, 0xEC, 0xCB));
        pal.setColor(QPalette::Highlight,     QColor(0xFB, 0xBF, 0x24));
        pal.setColor(QPalette::HighlightedText, QColor(0x12, 0x09, 0x00));
        pal.setColor(QPalette::ToolTipBase,   QColor(0x29, 0x1B, 0x0B));
        pal.setColor(QPalette::ToolTipText,   QColor(0xFB, 0xEC, 0xCB));
        pal.setColor(QPalette::PlaceholderText, QColor(0x9D, 0x83, 0x57));
    } else {
        // Light theme — warm cream paper with amber-orange accents.
        pal.setColor(QPalette::Window,        QColor(0xFB, 0xF7, 0xEE));
        pal.setColor(QPalette::WindowText,    QColor(0x1F, 0x18, 0x10));
        pal.setColor(QPalette::Base,          QColor(0xFF, 0xFD, 0xF7));
        pal.setColor(QPalette::AlternateBase, QColor(0xF3, 0xEC, 0xDB));
        pal.setColor(QPalette::Text,          QColor(0x1F, 0x18, 0x10));
        pal.setColor(QPalette::Button,        QColor(0xF3, 0xEC, 0xDB));
        pal.setColor(QPalette::ButtonText,    QColor(0x1F, 0x18, 0x10));
        pal.setColor(QPalette::Highlight,     QColor(0xD9, 0x77, 0x06));
        pal.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
        pal.setColor(QPalette::ToolTipBase,   QColor(0xFF, 0xFD, 0xF7));
        pal.setColor(QPalette::ToolTipText,   QColor(0x1F, 0x18, 0x10));
        pal.setColor(QPalette::PlaceholderText, QColor(0xA0, 0x8C, 0x6E));
    }
    app->setPalette(pal);
    app->setStyleSheet(loadStylesheet(theme));

    QSettings s;
    s.setValue(QStringLiteral("theme"), themeName(theme));

    emit themeChanged(theme);
}

void ThemeManager::applySaved(QApplication* app)
{
    QSettings s;
    const QString name = s.value(QStringLiteral("theme"),
                                 QStringLiteral("dark")).toString();
    Theme t = Dark;
    if (name == QStringLiteral("light")) {
        t = Light;
    } else if (name == QStringLiteral("blackout")) {
        t = Blackout;
    } else if (name == QStringLiteral("rgb")) {
        t = Rgb;
    } else if (name == QStringLiteral("amber")) {
        t = Amber;
    }
    apply(app, t);
}

void ThemeManager::toggle(QApplication* app)
{
    Theme next = Dark;
    switch (m_theme) {
        case Dark:     next = Light;    break;
        case Light:    next = Amber;    break;
        case Amber:    next = Blackout; break;
        case Blackout: next = Rgb;      break;
        case Rgb:      next = Dark;     break;
    }
    apply(app, next);
}

QColor ThemeManager::accent() const
{
    switch (m_theme) {
        case Light: return QColor(0xD9, 0x77, 0x06);  // deeper amber for legibility on cream
        case Amber: return QColor(0xFB, 0xBF, 0x24);  // saturated amber-yellow
        case Rgb:
        case Dark:
        case Blackout:
        default:    return QColor(0xF5, 0x9E, 0x0B);  // signature amber-orange
    }
}
QColor ThemeManager::background() const
{
    switch (m_theme) {
        case Blackout: return QColor(0x00, 0x00, 0x00);
        case Rgb:      return QColor(0x07, 0x07, 0x0E);
        case Amber:    return QColor(0x1A, 0x12, 0x07);
        case Dark:     return QColor(0x13, 0x13, 0x16);
        case Light:
        default:       return QColor(0xFB, 0xF7, 0xEE);
    }
}
QColor ThemeManager::surface() const
{
    switch (m_theme) {
        case Blackout: return QColor(0x07, 0x06, 0x04);
        case Rgb:      return QColor(0x12, 0x12, 0x1D);
        case Amber:    return QColor(0x29, 0x1B, 0x0B);
        case Dark:     return QColor(0x1A, 0x1A, 0x1E);
        case Light:
        default:       return QColor(0xFF, 0xFD, 0xF7);
    }
}
QColor ThemeManager::surfaceAlt() const
{
    switch (m_theme) {
        case Blackout: return QColor(0x10, 0x0E, 0x09);
        case Rgb:      return QColor(0x1A, 0x1A, 0x29);
        case Amber:    return QColor(0x33, 0x22, 0x0E);
        case Dark:     return QColor(0x22, 0x22, 0x28);
        case Light:
        default:       return QColor(0xF3, 0xEC, 0xDB);
    }
}
QColor ThemeManager::border() const
{
    switch (m_theme) {
        case Blackout: return QColor(0x1A, 0x14, 0x0A);
        case Rgb:      return QColor(0x2C, 0x2C, 0x44);
        case Amber:    return QColor(0x4A, 0x30, 0x14);
        case Dark:     return QColor(0x25, 0x25, 0x2B);
        case Light:
        default:       return QColor(0xE7, 0xDD, 0xC2);
    }
}
QColor ThemeManager::text() const
{
    switch (m_theme) {
        case Blackout: return QColor(0xEC, 0xE6, 0xD6);
        case Rgb:      return QColor(0xEC, 0xEB, 0xF6);
        case Amber:    return QColor(0xFB, 0xEC, 0xCB);
        case Dark:     return QColor(0xEC, 0xEB, 0xE6);
        case Light:
        default:       return QColor(0x1F, 0x18, 0x10);
    }
}
QColor ThemeManager::mutedText() const
{
    switch (m_theme) {
        case Blackout: return QColor(0x82, 0x78, 0x65);
        case Rgb:      return QColor(0x8E, 0x8C, 0xA4);
        case Amber:    return QColor(0xB0, 0x91, 0x60);
        case Dark:     return QColor(0x8A, 0x89, 0x90);
        case Light:
        default:       return QColor(0x76, 0x67, 0x55);
    }
}
