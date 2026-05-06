#include "SettingsView.h"

#include "ThemeManager.h"
#include "ToggleSwitch.h"
#include "audio/SoundEngine.h"
#include "i18n/I18n.h"

#include <QApplication>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

SettingsView::SettingsView(QWidget* parent) : QWidget(parent)
{
    setObjectName("SettingsView");

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName("SettingsScroll");
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(m_scroll);
    content->setObjectName("SettingsContent");
    m_scroll->setWidget(content);
    outer->addWidget(m_scroll);

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(14);

    m_titleLabel = new QLabel(content);
    m_titleLabel->setObjectName("PageTitle");
    m_subtitleLabel = new QLabel(content);
    m_subtitleLabel->setObjectName("PageSubtitle");
    m_subtitleLabel->setWordWrap(true);
    root->addWidget(m_titleLabel);
    root->addWidget(m_subtitleLabel);

    // Theme group.
    m_themeBox = new QGroupBox(this);
    m_themeBox->setObjectName("Card");
    auto* themeLayout = new QHBoxLayout(m_themeBox);
    m_radioDark     = new QRadioButton(m_themeBox);
    m_radioLight    = new QRadioButton(m_themeBox);
    m_radioAmber    = new QRadioButton(m_themeBox);
    m_radioBlackout = new QRadioButton(m_themeBox);
    m_radioRgb      = new QRadioButton(m_themeBox);
    themeLayout->addWidget(m_radioDark);
    themeLayout->addWidget(m_radioLight);
    themeLayout->addWidget(m_radioAmber);
    themeLayout->addWidget(m_radioBlackout);
    themeLayout->addWidget(m_radioRgb);
    themeLayout->addStretch();
    root->addWidget(m_themeBox);

    connect(m_radioDark,     &QRadioButton::toggled, this, &SettingsView::onThemeRadioChanged);
    connect(m_radioLight,    &QRadioButton::toggled, this, &SettingsView::onThemeRadioChanged);
    connect(m_radioBlackout, &QRadioButton::toggled, this, &SettingsView::onThemeRadioChanged);
    connect(m_radioRgb,      &QRadioButton::toggled, this, &SettingsView::onThemeRadioChanged);
    connect(m_radioAmber,    &QRadioButton::toggled, this, &SettingsView::onThemeRadioChanged);

    // Scanner appearance group.
    m_scanBox = new QGroupBox(this);
    m_scanBox->setObjectName("Card");
    auto* radarLayout = new QVBoxLayout(m_scanBox);
    m_radarToggle = new ToggleSwitch(m_scanBox);
    m_radarHintLabel = new QLabel(m_scanBox);
    m_radarHintLabel->setObjectName("MutedLabel");
    m_radarHintLabel->setWordWrap(true);
    radarLayout->addWidget(m_radarToggle);
    radarLayout->addWidget(m_radarHintLabel);
    root->addWidget(m_scanBox);

    connect(m_radarToggle, &ToggleSwitch::toggled, this, [](bool on) {
        QSettings s;
        s.setValue(QStringLiteral("radarEnabled"), on);
    });

    // Sounds group.
    m_soundBox = new QGroupBox(this);
    m_soundBox->setObjectName("Card");
    auto* soundsLayout = new QVBoxLayout(m_soundBox);
    m_soundsToggle = new ToggleSwitch(m_soundBox);
    m_soundsHintLabel = new QLabel(m_soundBox);
    m_soundsHintLabel->setObjectName("MutedLabel");
    m_soundsHintLabel->setWordWrap(true);
    soundsLayout->addWidget(m_soundsToggle);
    soundsLayout->addWidget(m_soundsHintLabel);
    root->addWidget(m_soundBox);

    connect(m_soundsToggle, &ToggleSwitch::toggled, this, [](bool on) {
        SoundEngine::instance().setEnabled(on);
        if (on) SoundEngine::instance().play(SoundEngine::Click);
    });

    // Language group.
    m_langBox = new QGroupBox(this);
    m_langBox->setObjectName("Card");
    auto* langV = new QVBoxLayout(m_langBox);
    auto* langRow = new QHBoxLayout;
    m_radioEn = new QRadioButton(QStringLiteral("English"), m_langBox);
    m_radioUk = new QRadioButton(QStringLiteral("Українська"), m_langBox);
    langRow->addWidget(m_radioEn);
    langRow->addWidget(m_radioUk);
    langRow->addStretch();
    m_langHintLabel = new QLabel(m_langBox);
    m_langHintLabel->setObjectName("MutedLabel");
    m_langHintLabel->setWordWrap(true);
    langV->addLayout(langRow);
    langV->addWidget(m_langHintLabel);
    root->addWidget(m_langBox);

    connect(m_radioEn, &QRadioButton::toggled, this, [this](bool on) {
        if (on) I18n::instance().setLanguage(I18n::English);
    });
    connect(m_radioUk, &QRadioButton::toggled, this, [this](bool on) {
        if (on) I18n::instance().setLanguage(I18n::Ukrainian);
    });

    // Close-the-window-X behaviour group.
    m_closeBehaviorBox = new QGroupBox(this);
    m_closeBehaviorBox->setObjectName("Card");
    auto* closeV = new QVBoxLayout(m_closeBehaviorBox);
    auto* closeRow = new QHBoxLayout;
    m_radioCloseExit = new QRadioButton(m_closeBehaviorBox);
    m_radioCloseTray = new QRadioButton(m_closeBehaviorBox);
    closeRow->addWidget(m_radioCloseExit);
    closeRow->addWidget(m_radioCloseTray);
    closeRow->addStretch();
    m_closeBehaviorHint = new QLabel(m_closeBehaviorBox);
    m_closeBehaviorHint->setObjectName("MutedLabel");
    m_closeBehaviorHint->setWordWrap(true);
    closeV->addLayout(closeRow);
    closeV->addWidget(m_closeBehaviorHint);
    root->addWidget(m_closeBehaviorBox);

    connect(m_radioCloseExit, &QRadioButton::toggled, this, [](bool on) {
        if (on) {
            QSettings s;
            s.setValue(QStringLiteral("closeBehavior"),
                       QStringLiteral("exit"));
        }
    });
    connect(m_radioCloseTray, &QRadioButton::toggled, this, [](bool on) {
        if (on) {
            QSettings s;
            s.setValue(QStringLiteral("closeBehavior"),
                       QStringLiteral("tray"));
        }
    });

    // Overview-statistics group: which Overview stats blocks are visible.
    m_overviewBox = new QGroupBox(this);
    m_overviewBox->setObjectName("Card");
    auto* overviewLayout = new QVBoxLayout(m_overviewBox);

    // Order of toggles ↔ Overview block layout (top → bottom).
    m_overviewToggles = {
        { nullptr, QStringLiteral("showCombinedBar"),    "Combined usage bar",         true  },
        { nullptr, QStringLiteral("showPerDriveBars"),   "Per-drive usage bars",       true  },
        { nullptr, QStringLiteral("showFilesystems"),    "Filesystem breakdown",       true  },
        { nullptr, QStringLiteral("showMostUsedDrive"),  "Most used drive",            true  },
        { nullptr, QStringLiteral("showFullestDrive"),   "Fullest drive",              true  },
        { nullptr, QStringLiteral("showLowSpaceAlert"),  "Low free-space warning",     false },
        { nullptr, QStringLiteral("showLastScan"),       "Last scan summary",          true  },
        { nullptr, QStringLiteral("showTopCategories"),  "Top file categories",        true  },
        { nullptr, QStringLiteral("showLargestFiles"),     "Largest files",              true  },
        { nullptr, QStringLiteral("showFileSizeSummary"),  "File size summary (avg/median/min/max)", true  },
        { nullptr, QStringLiteral("showTopExtensions"),    "Top file types by size",     true  },
        { nullptr, QStringLiteral("showAgeHistogram"),     "File age histogram",         true  },
        { nullptr, QStringLiteral("showSizeBuckets"),      "File size buckets",          true  },
    };
    for (auto& t : m_overviewToggles) {
        t.widget = new ToggleSwitch(m_overviewBox);
        overviewLayout->addWidget(t.widget);
        connect(t.widget, &ToggleSwitch::toggled, this,
                [this, key = t.key](bool on) {
                    QSettings s;
                    s.setValue(QStringLiteral("overview/") + key, on);
                    emit overviewSettingsChanged();
                });
    }
    root->addWidget(m_overviewBox);

    // Exclusions group.
    m_excludedBox = new QGroupBox(this);
    m_excludedBox->setObjectName("Card");
    auto* excludedLayout = new QVBoxLayout(m_excludedBox);
    m_excludedHintLabel = new QLabel(m_excludedBox);
    m_excludedHintLabel->setObjectName("MutedLabel");
    m_excludedHintLabel->setWordWrap(true);
    excludedLayout->addWidget(m_excludedHintLabel);

    m_excludedList = new QListWidget(m_excludedBox);
    excludedLayout->addWidget(m_excludedList);

    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(m_excludedBox);
    m_removeBtn = new QPushButton(m_excludedBox);
    m_addBtn->setObjectName("PrimaryButton");
    m_removeBtn->setObjectName("SecondaryButton");
    connect(m_addBtn,    &QPushButton::clicked, this, &SettingsView::onAddExclusion);
    connect(m_removeBtn, &QPushButton::clicked, this, &SettingsView::onRemoveExclusion);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addStretch();
    excludedLayout->addLayout(btnRow);

    root->addWidget(m_excludedBox, 1);

    m_aboutLabel = new QLabel(this);
    m_aboutLabel->setObjectName("MutedLabel");
    root->addWidget(m_aboutLabel);

    loadFromSettings();
    retranslate();

    // Sync radio buttons when the theme is toggled from the sidebar.
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](ThemeManager::Theme t) {
                QSignalBlocker b1(m_radioDark), b2(m_radioLight),
                    b3(m_radioBlackout), b4(m_radioRgb), b5(m_radioAmber);
                m_radioDark->setChecked(t == ThemeManager::Dark);
                m_radioLight->setChecked(t == ThemeManager::Light);
                m_radioBlackout->setChecked(t == ThemeManager::Blackout);
                m_radioRgb->setChecked(t == ThemeManager::Rgb);
                m_radioAmber->setChecked(t == ThemeManager::Amber);
                retranslate();
            });

    connect(&I18n::instance(), &I18n::languageChanged, this, [this] { retranslate(); });
}

void SettingsView::retranslate()
{
    m_titleLabel->setText(tr_("Settings"));
    m_subtitleLabel->setText(tr_("Tweak the theme, language, sounds, and scan exclusions."));

    m_themeBox->setTitle(tr_("Theme"));
    m_radioDark->setText(ThemeManager::themeShortLabel(ThemeManager::Dark));
    m_radioLight->setText(ThemeManager::themeShortLabel(ThemeManager::Light));
    m_radioAmber->setText(ThemeManager::themeShortLabel(ThemeManager::Amber));
    m_radioBlackout->setText(ThemeManager::themeShortLabel(ThemeManager::Blackout));
    m_radioRgb->setText(ThemeManager::themeShortLabel(ThemeManager::Rgb));

    m_scanBox->setTitle(tr_("During scanning"));
    m_radarToggle->setText(tr_("Show the radar with discovered files"));
    m_radarHintLabel->setText(
        tr_("The radar appears in the centre and visualises files as they are discovered. "
            "Turn it off if you prefer a calmer view."));

    m_soundBox->setTitle(tr_("Sounds"));
    m_soundsToggle->setText(tr_("Play sound effects"));
    m_soundsHintLabel->setText(
        tr_("Soft tones for scan start, scan complete, and other UI events."));

    m_langBox->setTitle(tr_("Language"));
    m_langHintLabel->setText(tr_("Switches the interface language instantly."));

    m_closeBehaviorBox->setTitle(tr_("When closing the window"));
    m_radioCloseExit->setText(tr_("Exit the application"));
    m_radioCloseTray->setText(tr_("Minimize to tray"));
    m_closeBehaviorHint->setText(
        tr_("Choose what should happen when you press the × button. "
            "Tray-mode keeps Volchay Cleans running in the system tray; "
            "right-click the tray icon for Open or Exit."));

    m_overviewBox->setTitle(tr_("Overview statistics"));
    for (auto& t : m_overviewToggles) {
        if (t.widget) {
            t.widget->setText(
                I18n::instance().tr_s(QString::fromUtf8(t.label)));
        }
    }

    m_excludedBox->setTitle(tr_("Excluded folders"));
    m_excludedHintLabel->setText(
        tr_("These paths are skipped during scans (e.g. network drives, backup folders)."));
    m_addBtn->setText(tr_("Add…"));
    m_removeBtn->setText(tr_("Remove"));

    m_aboutLabel->setText(
        tr_("Volchay Cleans · 0.2.0 · C++/Qt 6 · Analyses files locally; nothing is auto-deleted."));
}

void SettingsView::loadFromSettings()
{
    QSettings s;
    const QString themeName = s.value(QStringLiteral("theme"),
                                      QStringLiteral("dark")).toString();
    {
        QSignalBlocker b1(m_radioDark), b2(m_radioLight), b3(m_radioBlackout),
            b4(m_radioRgb), b5(m_radioAmber);
        if (themeName == QStringLiteral("light")) {
            m_radioLight->setChecked(true);
        } else if (themeName == QStringLiteral("blackout")) {
            m_radioBlackout->setChecked(true);
        } else if (themeName == QStringLiteral("rgb")) {
            m_radioRgb->setChecked(true);
        } else if (themeName == QStringLiteral("amber")) {
            m_radioAmber->setChecked(true);
        } else {
            m_radioDark->setChecked(true);
        }
    }

    const bool radarOn = s.value(QStringLiteral("radarEnabled"), true).toBool();
    m_radarToggle->setCheckedSilent(radarOn);

    const bool soundsOn = s.value(QStringLiteral("soundsEnabled"), true).toBool();
    m_soundsToggle->setCheckedSilent(soundsOn);

    const QString lang = s.value(QStringLiteral("language"),
                                 QStringLiteral("en")).toString();
    {
        QSignalBlocker b1(m_radioEn), b2(m_radioUk);
        if (lang == QStringLiteral("uk")) {
            m_radioUk->setChecked(true);
        } else {
            m_radioEn->setChecked(true);
        }
    }

    const QString closeBehavior = s.value(QStringLiteral("closeBehavior"),
                                          QStringLiteral("tray")).toString();
    {
        QSignalBlocker b1(m_radioCloseExit), b2(m_radioCloseTray);
        if (closeBehavior == QStringLiteral("exit")) {
            m_radioCloseExit->setChecked(true);
        } else {
            m_radioCloseTray->setChecked(true);
        }
    }

    const QStringList excluded =
        s.value(QStringLiteral("excludedPaths")).toStringList();
    m_excludedList->clear();
    for (const QString& p : excluded) {
        m_excludedList->addItem(p);
    }

    for (auto& t : m_overviewToggles) {
        if (!t.widget) continue;
        const bool on =
            s.value(QStringLiteral("overview/") + t.key, t.defaultOn).toBool();
        t.widget->setCheckedSilent(on);
    }
}

void SettingsView::focusOverviewSection()
{
    if (!m_overviewBox || !m_scroll) return;
    // Scroll on the next event-loop tick so layout has settled if the
    // page was just shown via a stack switch.
    QTimer::singleShot(0, this, [this] {
        m_scroll->ensureWidgetVisible(m_overviewBox, 0, 16);
        m_overviewBox->setFocus(Qt::OtherFocusReason);
    });
}

void SettingsView::saveExcludedToSettings()
{
    QStringList excluded;
    for (int i = 0; i < m_excludedList->count(); ++i) {
        excluded << m_excludedList->item(i)->text();
    }
    QSettings s;
    s.setValue(QStringLiteral("excludedPaths"), excluded);
}

void SettingsView::onAddExclusion()
{
    const QString d = QFileDialog::getExistingDirectory(this,
        tr_("Pick a folder to exclude"), QDir::homePath());
    if (d.isEmpty()) {
        return;
    }
    for (int i = 0; i < m_excludedList->count(); ++i) {
        if (m_excludedList->item(i)->text() == d) {
            return;
        }
    }
    m_excludedList->addItem(d);
    saveExcludedToSettings();
}

void SettingsView::onRemoveExclusion()
{
    const auto items = m_excludedList->selectedItems();
    for (auto* it : items) {
        delete m_excludedList->takeItem(m_excludedList->row(it));
    }
    saveExcludedToSettings();
}

void SettingsView::onThemeRadioChanged()
{
    auto* app = qobject_cast<QApplication*>(QApplication::instance());
    if (!app) {
        return;
    }
    ThemeManager::Theme target = ThemeManager::Dark;
    if (m_radioLight->isChecked()) {
        target = ThemeManager::Light;
    } else if (m_radioBlackout->isChecked()) {
        target = ThemeManager::Blackout;
    } else if (m_radioRgb->isChecked()) {
        target = ThemeManager::Rgb;
    } else if (m_radioAmber->isChecked()) {
        target = ThemeManager::Amber;
    }
    if (ThemeManager::instance().current() == target) {
        return;
    }
    ThemeManager::instance().apply(app, target);
}
