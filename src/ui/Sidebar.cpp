#include "Sidebar.h"

#include "ThemeManager.h"
#include "i18n/I18n.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>
#include <QSvgWidget>
#include <QToolButton>
#include <QVBoxLayout>

Sidebar::Sidebar(QWidget* parent) : QWidget(parent)
{
    setObjectName("Sidebar");
    setFixedWidth(232);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 22, 14, 18);
    root->setSpacing(12);

    // Brand row: logo + name.
    auto* brandRow = new QHBoxLayout;
    brandRow->setSpacing(10);
    auto* logo = new QSvgWidget(QStringLiteral(":/icons/logo.svg"), this);
    logo->setFixedSize(32, 32);
    auto* brand = new QLabel(QStringLiteral("Volchay Cleans"), this);
    brand->setObjectName("BrandLabel");
    brandRow->addWidget(logo);
    brandRow->addWidget(brand);
    brandRow->addStretch();
    root->addLayout(brandRow);

    auto* sectionLabel = new QLabel(tr_("Navigation"), this);
    sectionLabel->setObjectName("NavSectionLabel");
    m_navSectionLabel = sectionLabel;
    root->addSpacing(6);
    root->addWidget(sectionLabel);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        m_active = static_cast<SectionId>(id);
        emit sectionChanged(m_active);
    });

    m_navLayout = new QVBoxLayout;
    m_navLayout->setSpacing(4);
    m_navLayout->setContentsMargins(0, 0, 0, 0);
    root->addLayout(m_navLayout);

    m_btnOverview = makeNavButton(tr_("Overview"),
                                  QStringLiteral(":/icons/nav-overview.svg"), Overview);
    m_btnAnalyzer = makeNavButton(tr_("Analyzer"),
                                  QStringLiteral(":/icons/nav-analyzer.svg"), Analyzer);
    m_btnFiles    = makeNavButton(tr_("Files"),
                                  QStringLiteral(":/icons/nav-files.svg"), Files);
    m_btnRecent   = makeNavButton(tr_("Recent"),
                                  QStringLiteral(":/icons/nav-recent.svg"), Recent);
    m_btnSettings = makeNavButton(tr_("Settings"),
                                  QStringLiteral(":/icons/nav-settings.svg"), Settings);
    m_btnOverview->setChecked(true);

    root->addStretch();

    // Theme toggle pinned to the bottom.
    auto* footer = new QHBoxLayout;
    m_themeBtn = new QToolButton(this);
    m_themeBtn->setObjectName("ThemeToggle");
    m_themeBtn->setToolTip(tr_("Cycle theme (Dark → Light → Amber → Blackout → RGB)"));
    m_themeBtn->setText(ThemeManager::themeShortLabel(ThemeManager::instance().current()));
    m_themeBtn->setCursor(Qt::PointingHandCursor);
    m_themeBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);
    connect(m_themeBtn, &QToolButton::clicked, this, &Sidebar::themeToggleRequested);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](ThemeManager::Theme t) {
                m_themeBtn->setText(ThemeManager::themeShortLabel(t));
            });
    footer->addWidget(m_themeBtn);
    footer->addStretch();
    root->addLayout(footer);

    connect(&I18n::instance(), &I18n::languageChanged, this, [this] {
        m_navSectionLabel->setText(tr_("Navigation"));
        m_btnOverview->setText(tr_("Overview"));
        m_btnAnalyzer->setText(tr_("Analyzer"));
        m_btnFiles->setText(tr_("Files"));
        m_btnRecent->setText(tr_("Recent"));
        m_btnSettings->setText(tr_("Settings"));
        m_themeBtn->setText(ThemeManager::themeShortLabel(ThemeManager::instance().current()));
        m_themeBtn->setToolTip(tr_("Cycle theme (Dark → Light → Amber → Blackout → RGB)"));
    });
}

QPushButton* Sidebar::makeNavButton(const QString& title, const QString& iconPath, int id)
{
    auto* btn = new QPushButton(title, this);
    btn->setObjectName("NavButton");
    btn->setCheckable(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize({ 18, 18 });
    btn->setFlat(true);
    m_group->addButton(btn, id);
    m_navLayout->addWidget(btn);
    return btn;
}

void Sidebar::setActiveSection(SectionId id)
{
    if (m_active == id) {
        return;
    }
    m_active = id;
    if (auto* btn = m_group->button(static_cast<int>(id))) {
        btn->setChecked(true);
    }
}
