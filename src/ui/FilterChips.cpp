#include "FilterChips.h"

#include "i18n/I18n.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLocale>
#include <QPushButton>

FilterChips::FilterChips(QWidget* parent) : QWidget(parent)
{
    setObjectName("FilterChips");

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(8);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);
    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        m_active = static_cast<FileCategory>(id);
        emit categoryChanged(m_active);
    });

    rebuild();

    connect(&I18n::instance(), &I18n::languageChanged, this, [this] { rebuild(); });
}

void FilterChips::rebuild()
{
    // Clear existing.
    for (auto* btn : m_buttons) {
        m_group->removeButton(btn);
        btn->deleteLater();
    }
    m_buttons.clear();
    QLayoutItem* item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        delete item;
    }

    for (FileCategory cat : FileType::all()) {
        auto* btn = new QPushButton(labelFor(cat), this);
        btn->setObjectName("Chip");
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFlat(true);
        if (cat == m_active) {
            btn->setChecked(true);
        }
        m_group->addButton(btn, static_cast<int>(cat));
        m_layout->addWidget(btn);
        m_buttons.insert(static_cast<int>(cat), btn);
    }
    m_layout->addStretch();
}

QString FilterChips::labelFor(FileCategory cat) const
{
    QString text = QString::fromUtf8("%1  %2")
                       .arg(FileType::glyph(cat), FileType::displayName(cat));
    if (m_counts.contains(cat) && cat != FileCategory::All) {
        text += QString::fromUtf8("  · %1").arg(QLocale().toString(m_counts.value(cat)));
    } else if (cat == FileCategory::All && m_counts.contains(FileCategory::All)) {
        text += QString::fromUtf8("  · %1").arg(QLocale().toString(m_counts.value(cat)));
    }
    return text;
}

void FilterChips::setActive(FileCategory cat)
{
    if (m_active == cat) {
        return;
    }
    m_active = cat;
    if (auto* btn = m_buttons.value(static_cast<int>(cat), nullptr)) {
        btn->setChecked(true);
    }
}

void FilterChips::setCounts(const QHash<FileCategory, int>& counts)
{
    m_counts = counts;
    for (auto it = m_buttons.constBegin(); it != m_buttons.constEnd(); ++it) {
        const FileCategory cat = static_cast<FileCategory>(it.key());
        it.value()->setText(labelFor(cat));
    }
}
