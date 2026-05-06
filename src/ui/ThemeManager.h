#pragma once

#include <QColor>
#include <QObject>
#include <QString>

class QApplication;

class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum Theme {
        Dark = 0,
        Light = 1,
        Blackout = 2,
        Rgb = 3,
        Amber = 4,
    };

    static ThemeManager& instance();

    Theme current() const { return m_theme; }
    bool isDark() const { return m_theme == Dark || m_theme == Blackout; }

    void applySaved(QApplication* app);
    void apply(QApplication* app, Theme theme);
    void toggle(QApplication* app);  // cycles Dark → Light → Amber → Blackout → RGB → Dark

    static QString themeName(Theme t);
    static QString themeShortLabel(Theme t);  // includes a glyph: ☾ ☀ ⬤ ✦ 

    // Palette helpers used by custom-painted widgets.
    QColor accent() const;
    QColor background() const;
    QColor surface() const;
    QColor surfaceAlt() const;
    QColor border() const;
    QColor text() const;
    QColor mutedText() const;

signals:
    void themeChanged(Theme theme);

private:
    ThemeManager() = default;
    QString loadStylesheet(Theme theme) const;

    Theme m_theme = Dark;
};
