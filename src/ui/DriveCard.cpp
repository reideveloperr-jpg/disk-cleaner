#include "DriveCard.h"

#include "RingIndicator.h"
#include "core/SizeFormatter.h"
#include "i18n/I18n.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

DriveCard::DriveCard(const DriveInfo& info, QWidget* parent)
    : QWidget(parent), m_info(info)
{
    setObjectName("DriveCard");
    setAttribute(Qt::WA_StyledBackground, true);
    setMinimumWidth(300);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(16);

    m_ring = new RingIndicator(this);
    m_ring->setFixedSize(96, 96);
    m_ring->setLineWidth(8);
    const int percent = int(info.usedFraction() * 100.0 + 0.5);
    m_ring->setFraction(info.usedFraction());
    m_ring->setCenterText(QString::fromUtf8("%1%").arg(percent));
    root->addWidget(m_ring);

    auto* col = new QVBoxLayout;
    col->setSpacing(2);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("CardTitle");
    m_titleLabel->setText(info.rootPath);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName("CardSubtitle");
    QString subtitle = info.label.isEmpty()
        ? tr_("No label")
        : info.label;
    if (!info.fileSystem.isEmpty()) {
        subtitle = QString::fromUtf8("%1 · %2").arg(subtitle, info.fileSystem.toUpper());
    }
    m_subtitleLabel->setText(subtitle);

    m_usedLabel = new QLabel(this);
    m_usedLabel->setObjectName("CardStat");
    m_usedLabel->setText(tr_("Used: %1 of %2")
                             .arg(SizeFormatter::humanReadable(info.usedBytes()),
                                  SizeFormatter::humanReadable(info.totalBytes)));

    m_freeLabel = new QLabel(this);
    m_freeLabel->setObjectName("CardStatMuted");
    m_freeLabel->setText(tr_("Free: %1")
                             .arg(SizeFormatter::humanReadable(info.freeBytes)));

    auto* btnRow = new QHBoxLayout;
    m_analyzeBtn = new QPushButton(tr_("Analyze"), this);
    m_analyzeBtn->setObjectName("PrimaryButton");
    m_analyzeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_analyzeBtn, &QPushButton::clicked, this, [this] {
        emit analyzeRequested(m_info.rootPath);
    });
    btnRow->addWidget(m_analyzeBtn);
    btnRow->addStretch();

    col->addWidget(m_titleLabel);
    col->addWidget(m_subtitleLabel);
    col->addSpacing(6);
    col->addWidget(m_usedLabel);
    col->addWidget(m_freeLabel);
    col->addSpacing(8);
    col->addLayout(btnRow);
    col->addStretch();

    root->addLayout(col, 1);
}
