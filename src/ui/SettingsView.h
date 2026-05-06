#pragma once

#include <QVector>
#include <QWidget>

class QListWidget;
class QPushButton;
class QLabel;
class QRadioButton;
class QGroupBox;
class QScrollArea;
class ToggleSwitch;

class SettingsView : public QWidget {
    Q_OBJECT
public:
    explicit SettingsView(QWidget* parent = nullptr);

    // Scrolls the Settings page so the user can immediately edit which
    // statistics show on the Overview page.  Used by OverviewView's
    // ⚙ Configure button.
    void focusOverviewSection();

signals:
    // Emitted when the user flips one of the Overview-statistics toggles
    // so the host can ask OverviewView to re-render.
    void overviewSettingsChanged();

private slots:
    void onAddExclusion();
    void onRemoveExclusion();
    void onThemeRadioChanged();

private:
    void loadFromSettings();
    void saveExcludedToSettings();
    void retranslate();

    QGroupBox* m_themeBox = nullptr;
    QGroupBox* m_scanBox = nullptr;
    QGroupBox* m_soundBox = nullptr;
    QGroupBox* m_langBox = nullptr;
    QGroupBox* m_excludedBox = nullptr;
    QGroupBox* m_overviewBox = nullptr;
    QGroupBox* m_closeBehaviorBox = nullptr;
    QScrollArea* m_scroll = nullptr;

    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_radarHintLabel = nullptr;
    QLabel* m_soundsHintLabel = nullptr;
    QLabel* m_langHintLabel = nullptr;
    QLabel* m_excludedHintLabel = nullptr;
    QLabel* m_aboutLabel = nullptr;

    QRadioButton* m_radioDark = nullptr;
    QRadioButton* m_radioLight = nullptr;
    QRadioButton* m_radioBlackout = nullptr;
    QRadioButton* m_radioRgb = nullptr;
    QRadioButton* m_radioAmber = nullptr;

    ToggleSwitch* m_radarToggle = nullptr;
    ToggleSwitch* m_soundsToggle = nullptr;

    QRadioButton* m_radioEn = nullptr;
    QRadioButton* m_radioUk = nullptr;

    // Close-the-window-X behaviour.
    QRadioButton* m_radioCloseExit = nullptr;
    QRadioButton* m_radioCloseTray = nullptr;
    QLabel* m_closeBehaviorHint = nullptr;

    QListWidget* m_excludedList = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_removeBtn = nullptr;

    // Map of QSettings key → toggle widget; used both to load default
    // states and to relabel them on language change.
    struct OverviewToggle {
        ToggleSwitch* widget;
        QString key;        // QSettings key (without "overview/" prefix)
        QByteArray label;   // English label, fed to tr_() at runtime
        bool defaultOn;
    };
    QVector<OverviewToggle> m_overviewToggles;
};
