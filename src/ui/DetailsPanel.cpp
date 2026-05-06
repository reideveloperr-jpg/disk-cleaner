#include "DetailsPanel.h"

#include "SpinnerWidget.h"
#include "audio/SoundEngine.h"
#include "core/SizeFormatter.h"
#include "i18n/I18n.h"
#include "platform/ShellOps.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMimeDatabase>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

DetailsPanel::DetailsPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName("DetailsPanel");
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(8);

    m_titleLabel = new QLabel(tr_("File details"), this);
    m_titleLabel->setObjectName("PageTitle");
    m_titleLabel->setWordWrap(true);

    m_subtitleLabel = new QLabel(tr_("Select a file in the table on the left."), this);
    m_subtitleLabel->setObjectName("PageSubtitle");
    m_subtitleLabel->setWordWrap(true);
    m_subtitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_details = new QPlainTextEdit(this);
    m_details->setObjectName("DetailsBox");
    m_details->setReadOnly(true);

    auto* btnRow = new QHBoxLayout;
    m_copyBtn   = new QPushButton(tr_("Copy path"), this);
    m_revealBtn = new QPushButton(tr_("Reveal in Explorer"), this);
    m_md5Btn    = new QPushButton(tr_("Compute MD5"), this);
    m_copyBtn  ->setObjectName("SecondaryButton");
    m_revealBtn->setObjectName("SecondaryButton");
    m_md5Btn   ->setObjectName("PrimaryButton");
    m_copyBtn->setEnabled(false);
    m_revealBtn->setEnabled(false);
    m_md5Btn->setEnabled(false);
    connect(m_copyBtn,   &QPushButton::clicked, this, &DetailsPanel::onCopyPath);
    connect(m_revealBtn, &QPushButton::clicked, this, &DetailsPanel::onRevealRequested);
    connect(m_md5Btn,    &QPushButton::clicked, this, &DetailsPanel::onComputeMd5);

    btnRow->addWidget(m_copyBtn);
    btnRow->addWidget(m_revealBtn);
    btnRow->addWidget(m_md5Btn);
    btnRow->addStretch();

    auto* hashRow = new QHBoxLayout;
    m_spinner = new SpinnerWidget(this);
    m_spinner->setFixedSize(18, 18);
    m_spinner->setVisible(false);
    m_hashLabel = new QLabel(this);
    m_hashLabel->setObjectName("MutedLabel");
    m_hashLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_hashLabel->setWordWrap(true);
    hashRow->addWidget(m_spinner);
    hashRow->addWidget(m_hashLabel, 1);

    root->addWidget(m_titleLabel);
    root->addWidget(m_subtitleLabel);
    root->addWidget(m_details, 1);
    root->addLayout(btnRow);
    root->addLayout(hashRow);

    m_hash = new HashRunner(this);
    connect(m_hash, &HashRunner::finished, this, &DetailsPanel::onMd5Done);
    connect(m_hash, &HashRunner::failed,   this, &DetailsPanel::onMd5Failed);
}

void DetailsPanel::showFile(const FileEntry& f)
{
    m_current = f;
    m_hasCurrent = true;

    m_titleLabel->setText(f.name);
    m_subtitleLabel->setText(f.path);

    QMimeDatabase mdb;
    const QMimeType mt = mdb.mimeTypeForFile(f.path, QMimeDatabase::MatchExtension);

    QString details;
    QTextStream ts(&details);
    ts.setEncoding(QStringConverter::Utf8);
    ts << tr_("Type:")     << "     "
       << (mt.comment().isEmpty() ? mt.name() : mt.comment()) << "\n";
    ts << tr_("MIME:")     << "     " << mt.name() << "\n";
    ts << tr_("Size:")     << "     " << SizeFormatter::humanReadable(f.size)
       << "  (" << QLocale().toString(qlonglong(f.size)) << " B)\n";
    ts << tr_("Modified")  << ": "
       << f.modified.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";
    ts << tr_("Created:")  << "  "
       << f.created.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";
    ts << tr_("Accessed:") << " "
       << f.accessed.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "\n";

    QStringList attrs;
    if (f.isHidden)   attrs << tr_("Hidden");
    if (f.isReadOnly) attrs << tr_("Read-only");
    if (f.isSystem)   attrs << tr_("System");
    ts << tr_("Attributes:") << " "
       << (attrs.isEmpty() ? QStringLiteral("—") : attrs.join(QStringLiteral(", "))) << "\n";

    m_details->setPlainText(details);
    m_hashLabel->clear();
    m_spinner->stop();
    m_spinner->setVisible(false);
    m_copyBtn->setEnabled(true);
    m_revealBtn->setEnabled(true);
    m_md5Btn->setEnabled(true);
}

void DetailsPanel::clear()
{
    m_hasCurrent = false;
    m_titleLabel->setText(tr_("File details"));
    m_subtitleLabel->setText(tr_("Select a file in the table on the left."));
    m_details->clear();
    m_hashLabel->clear();
    m_spinner->stop();
    m_spinner->setVisible(false);
    m_copyBtn->setEnabled(false);
    m_revealBtn->setEnabled(false);
    m_md5Btn->setEnabled(false);
}

void DetailsPanel::onComputeMd5()
{
    if (!m_hasCurrent) {
        return;
    }
    m_md5Btn->setEnabled(false);
    m_spinner->setVisible(true);
    m_spinner->start();
    m_hashLabel->setText(tr_("Computing MD5…"));
    SoundEngine::instance().play(SoundEngine::Click);
    m_hash->compute(m_current.path);
}

void DetailsPanel::onMd5Done(const QString& hash)
{
    m_spinner->stop();
    m_spinner->setVisible(false);
    m_md5Btn->setEnabled(true);
    m_hashLabel->setText(tr_("MD5: %1").arg(hash));
}

void DetailsPanel::onMd5Failed(const QString& msg)
{
    m_spinner->stop();
    m_spinner->setVisible(false);
    m_md5Btn->setEnabled(true);
    m_hashLabel->setText(tr_("Error: %1").arg(msg));
    SoundEngine::instance().play(SoundEngine::Error);
}

void DetailsPanel::onCopyPath()
{
    if (m_hasCurrent) {
        ShellOps::copyToClipboard(m_current.path);
    }
}

void DetailsPanel::onRevealRequested()
{
    if (m_hasCurrent) {
        ShellOps::revealInFileManager(m_current.path);
    }
}
