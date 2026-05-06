#include "FilesView.h"

#include "DetailsPanel.h"
#include "FilterChips.h"
#include "audio/SoundEngine.h"
#include "core/SizeFormatter.h"
#include "i18n/I18n.h"
#include "models/FileTableModel.h"
#include "platform/ShellOps.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTableView>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Show the "permanent delete" confirmation. Returns true if the user
// confirmed. Honors the persisted "don't ask again" preference.
bool confirmPermanentDelete(QWidget* parent, const QString& path)
{
    QSettings s;
    if (s.value(QStringLiteral("permDeleteSkipPrompt"), false).toBool()) {
        return true;
    }

    QDialog dlg(parent);
    dlg.setWindowTitle(tr_("Delete permanently"));
    dlg.setModal(true);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 20, 20, 16);
    root->setSpacing(12);

    auto* msg = new QLabel(&dlg);
    msg->setWordWrap(true);
    msg->setText(tr_("This will permanently delete:\n%1\n\nThis cannot be undone.")
                     .arg(path));
    root->addWidget(msg);

    auto* dontAsk = new QCheckBox(tr_("Don't ask again"), &dlg);
    root->addWidget(dontAsk);

    auto* box = new QDialogButtonBox(&dlg);
    auto* del = box->addButton(tr_("Delete forever"), QDialogButtonBox::AcceptRole);
    box->addButton(QDialogButtonBox::Cancel)->setText(tr_("Cancel"));
    del->setObjectName("PrimaryButton");
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(box);

    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }
    if (dontAsk->isChecked()) {
        s.setValue(QStringLiteral("permDeleteSkipPrompt"), true);
    }
    return true;
}

bool confirmRecycle(QWidget* parent, const QString& path)
{
    QMessageBox box(parent);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr_("Move to Recycle Bin"));
    box.setText(tr_("Move this item to the Recycle Bin?\n\n%1").arg(path));
    auto* yes = box.addButton(tr_("Move to Recycle Bin"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel)->setText(tr_("Cancel"));
    box.setDefaultButton(yes);
    box.exec();
    return box.clickedButton() == yes;
}

}  // namespace

FilesView::FilesView(QWidget* parent) : QWidget(parent)
{
    setObjectName("FilesView");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("PageTitle");
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName("PageSubtitle");
    m_summaryLabel->setWordWrap(true);

    root->addWidget(m_titleLabel);
    root->addWidget(m_summaryLabel);

    auto* topRow = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setObjectName("SearchBox");
    m_search->setClearButtonEnabled(true);
    // Compact search box: keep the width tight, but let Qt pick the height
    // from the QSS padding + font so the placeholder/text never gets
    // clipped vertically.
    m_search->setMaximumWidth(220);
    m_search->setMinimumHeight(28);
    topRow->addWidget(m_search, 0);

    m_sortCombo = new QComboBox(this);
    m_sortCombo->setObjectName("SortCombo");
    m_sortCombo->setCursor(Qt::PointingHandCursor);
    m_sortCombo->setMinimumWidth(180);
    populateSortCombo();
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applySortFromCombo(); });
    topRow->addWidget(m_sortCombo);

    m_deleteBtn = new QPushButton(this);
    m_deleteBtn->setObjectName("DangerButton");
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &FilesView::onDeleteClicked);
    topRow->addWidget(m_deleteBtn);

    m_refreshBtn = new QPushButton(this);
    m_refreshBtn->setObjectName("SecondaryButton");
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setEnabled(false);
    connect(m_refreshBtn, &QPushButton::clicked, this, &FilesView::onRefreshClicked);
    topRow->addWidget(m_refreshBtn);

    topRow->addStretch();
    root->addLayout(topRow);

    m_chips = new FilterChips(this);
    root->addWidget(m_chips);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName("FilesSplitter");
    splitter->setChildrenCollapsible(false);

    auto* tableContainer = new QWidget(splitter);
    auto* tableLayout = new QVBoxLayout(tableContainer);
    tableLayout->setContentsMargins(0, 0, 0, 0);

    m_table = new QTableView(tableContainer);
    m_table->setObjectName("FilesTable");
    m_table->setSortingEnabled(true);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Drag selected rows out into Explorer / another app — see
    // FileTableModel::mimeData() for the text/uri-list payload.
    m_table->setDragEnabled(true);
    m_table->setDragDropMode(QAbstractItemView::DragOnly);
    m_table->setDefaultDropAction(Qt::CopyAction);
    m_table->setShowGrid(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    // Compact rows so the table fits more files on screen.
    m_table->verticalHeader()->setDefaultSectionSize(22);
    QFont tfont = m_table->font();
    tfont.setPointSizeF(tfont.pointSizeF() * 0.92);
    m_table->setFont(tfont);

    m_model = new FileTableModel(this);
    m_proxy = new FileFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_table->setModel(m_proxy);
    m_table->sortByColumn(FileTableModel::ColSize, Qt::DescendingOrder);

    // Mirror header clicks back into the dropdown so the two stay in sync.
    connect(m_table->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, [this](int col, Qt::SortOrder /*order*/) {
                if (!m_sortCombo) return;
                for (int i = 0; i < m_sortCombo->count(); ++i) {
                    if (m_sortCombo->itemData(i).toInt() == col) {
                        QSignalBlocker b(m_sortCombo);
                        m_sortCombo->setCurrentIndex(i);
                        return;
                    }
                }
            });
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableView::customContextMenuRequested,
            this, &FilesView::onTableContextMenu);

    tableLayout->addWidget(m_table);

    m_details = new DetailsPanel(splitter);

    splitter->addWidget(tableContainer);
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({ 720, 480 });

    root->addWidget(splitter, 1);

    connect(m_chips, &FilterChips::categoryChanged, this, [this](FileCategory c) {
        m_proxy->setCategory(c);
    });
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& s) {
        m_proxy->setSearchText(s);
    });
    connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &FilesView::onSelectionChanged);

    // Double-click opens the file with the OS default app.
    connect(m_table, &QTableView::doubleClicked, this,
            [this](const QModelIndex& idx) {
                if (!idx.isValid()) return;
                const QModelIndex src = m_proxy->mapToSource(idx);
                if (!src.isValid() || src.row() >= m_files.size()) return;
                const QString path = m_model->fileAt(src.row()).path;
                if (path.isEmpty()) return;
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            });

    connect(&I18n::instance(), &I18n::languageChanged, this, [this] { retranslate(); });

    retranslate();
}

void FilesView::populateSortCombo()
{
    if (!m_sortCombo) return;
    QSignalBlocker b(m_sortCombo);
    m_sortCombo->clear();
    m_sortCombo->addItem(tr_("Size (largest first)"), int(FileTableModel::ColSize));
    m_sortCombo->addItem(tr_("Name (A–Z)"),           int(FileTableModel::ColName));
    m_sortCombo->addItem(tr_("Type"),                int(FileTableModel::ColCategory));
    m_sortCombo->addItem(tr_("Modified date"),       int(FileTableModel::ColModified));
    m_sortCombo->addItem(tr_("Created date"),        int(FileTableModel::ColCreated));
    m_sortCombo->addItem(tr_("Accessed date"),       int(FileTableModel::ColAccessed));
    m_sortCombo->addItem(tr_("Path (A–Z)"),           int(FileTableModel::ColPath));
}

void FilesView::applySortFromCombo()
{
    if (!m_table || !m_sortCombo) return;
    const int col = m_sortCombo->currentData().toInt();
    Qt::SortOrder order = Qt::DescendingOrder;
    if (col == FileTableModel::ColName ||
        col == FileTableModel::ColCategory ||
        col == FileTableModel::ColPath) {
        order = Qt::AscendingOrder;
    }
    m_table->sortByColumn(col, order);
}

bool FilesView::ensurePathExistsOrMark(const QString& path)
{
    if (path.isEmpty()) return false;
    if (QFileInfo::exists(path)) return true;
    if (m_model) m_model->markDeleted(path);
    QMessageBox::information(
        this, tr_("File not found"),
        tr_("%1\n\nThis file is no longer on disk — it was moved, renamed or "
            "deleted outside Volchay Cleans. Click Refresh to update the list.")
            .arg(path));
    return false;
}

void FilesView::retranslate()
{
    m_titleLabel->setText(tr_("Files"));
    if (m_files.isEmpty()) {
        m_summaryLabel->setText(
            tr_("Run a scan to see the file list with type filters."));
    } else {
        refreshSummary();
    }
    m_search->setPlaceholderText(tr_("Search by name or path…"));
    m_deleteBtn->setText(tr_("Delete"));
    m_deleteBtn->setToolTip(
        tr_("Click to move to Recycle Bin · Shift+Click to delete permanently"));
    m_refreshBtn->setText(tr_("Refresh"));
    m_refreshBtn->setToolTip(
        tr_("Reconcile against disk and remove rows that were deleted or moved"));
    if (m_sortCombo) {
        const int prev = m_sortCombo->currentIndex();
        populateSortCombo();
        if (prev >= 0 && prev < m_sortCombo->count()) {
            QSignalBlocker b(m_sortCombo);
            m_sortCombo->setCurrentIndex(prev);
        }
    }
    // Refresh column headers and the translated cells (e.g. file type names).
    if (m_model) {
        emit m_model->headerDataChanged(
            Qt::Horizontal, 0, FileTableModel::ColumnCount - 1);
        const int rows = m_model->rowCount();
        if (rows > 0) {
            emit m_model->dataChanged(
                m_model->index(0, 0),
                m_model->index(rows - 1, FileTableModel::ColumnCount - 1),
                { Qt::DisplayRole });
        }
    }
}

void FilesView::refreshSummary()
{
    qint64 total = 0;
    for (const FileEntry& f : m_files) total += f.size;
    m_summaryLabel->setText(
        tr_("Files found: %1 · Total size: %2")
            .arg(QLocale().toString(qlonglong(m_files.size())),
                 SizeFormatter::humanReadable(total)));
}

void FilesView::setScanResult(const ScanResult& r)
{
    m_files = r.files;
    m_model->setFiles(r.files);
    refreshSummary();

    QHash<FileCategory, int> counts;
    counts.insert(FileCategory::All, r.files.size());
    for (const FileEntry& f : r.files) {
        counts[f.category] = counts.value(f.category) + 1;
    }
    m_chips->setCounts(counts);

    m_table->resizeColumnsToContents();
    if (m_table->columnWidth(FileTableModel::ColName) < 320) {
        m_table->setColumnWidth(FileTableModel::ColName, 320);
    }
    m_details->clear();
    m_deleteBtn->setEnabled(false);
    m_refreshBtn->setEnabled(true);
}

void FilesView::clear()
{
    m_files.clear();
    m_model->clear();
    m_details->clear();
    m_deleteBtn->setEnabled(false);
    m_refreshBtn->setEnabled(false);
}

void FilesView::onSelectionChanged()
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid()) {
        m_details->clear();
        m_deleteBtn->setEnabled(false);
        return;
    }
    const QModelIndex src = m_proxy->mapToSource(idx);
    if (!src.isValid() || src.row() >= m_files.size()) {
        m_details->clear();
        m_deleteBtn->setEnabled(false);
        return;
    }
    m_details->showFile(m_model->fileAt(src.row()));
    m_deleteBtn->setEnabled(true);
}

QString FilesView::currentSelectedPath() const
{
    const QModelIndex idx = m_table->currentIndex();
    if (!idx.isValid()) return {};
    const QModelIndex src = m_proxy->mapToSource(idx);
    if (!src.isValid() || src.row() >= m_files.size()) return {};
    return m_model->fileAt(src.row()).path;
}

void FilesView::onDeleteClicked()
{
    const QString path = currentSelectedPath();
    if (!ensurePathExistsOrMark(path)) return;

    const bool shift =
        (QApplication::keyboardModifiers() & Qt::ShiftModifier) != 0;

    bool ok = false;
    QString err;
    if (shift) {
        if (!confirmPermanentDelete(this, path)) return;
        ok = ShellOps::permanentDelete(path, &err);
    } else {
        if (!confirmRecycle(this, path)) return;
        ok = ShellOps::moveToTrash(path, &err);
    }

    if (!ok) {
        SoundEngine::instance().play(SoundEngine::Error);
        QMessageBox::warning(this, tr_("Delete"),
                             tr_("Could not delete:\n%1\n\n%2").arg(path, err));
        return;
    }

    SoundEngine::instance().play(SoundEngine::Click);

    // Mark the row as deleted (red strikethrough).  The entry is only
    // removed from the table when the user clicks Refresh.
    m_model->markDeleted(path);
    emit filesDeleted();
}

void FilesView::onRefreshClicked()
{
    // Refresh has three responsibilities now:
    //   1. Rows whose file actually went to the recycle bin (or was deleted
    //      permanently) leave the cache and the model.
    //   2. Rows that were marked deleted but are now back on disk — the
    //      user restored them from the recycle bin in Explorer — lose
    //      their strikethrough and stay in the list.
    //   3. Rows whose file vanished outside Volchay Cleans (cloud sync,
    //      manual move, antivirus quarantine, …) get reconciled too — we
    //      stat() each surviving entry and drop the ones that are gone.
    QVector<FileEntry> kept;
    kept.reserve(m_files.size());
    QHash<FileCategory, int> counts;
    counts.insert(FileCategory::All, 0);
    for (int i = 0; i < m_files.size(); ++i) {
        const FileEntry& f = m_files.at(i);
        const QModelIndex idx = m_model->index(i, 0);
        const QVariant fontVar = m_model->data(idx, Qt::FontRole);
        const bool markedDeleted =
            fontVar.canConvert<QFont>() && fontVar.value<QFont>().strikeOut();
        const bool onDisk = QFileInfo::exists(f.path);
        if (markedDeleted && onDisk) {
            // Restored — keep & clear strikethrough below.
            kept.push_back(f);
            counts[f.category] = counts.value(f.category) + 1;
        } else if (!markedDeleted && !onDisk) {
            // Vanished outside the app: mark for purge in the model loop
            // below and drop from the local cache.
            m_model->markDeleted(f.path);
        } else if (!markedDeleted && onDisk) {
            kept.push_back(f);
            counts[f.category] = counts.value(f.category) + 1;
        }
        // (markedDeleted && !onDisk) → truly deleted, drop.
    }
    counts[FileCategory::All] = kept.size();
    m_files = kept;
    // Clear strikethrough on revived rows, then drop the rest.
    m_model->purgeDeletedIfMissing();
    refreshSummary();
    m_chips->setCounts(counts);
    SoundEngine::instance().play(SoundEngine::Click);
}

bool FilesView::isPathMarkedDeleted(const QString& path) const
{
    return !path.isEmpty() && m_model && m_model->isDeleted(path);
}

void FilesView::restoreSelectedFromTrash()
{
    const QString path = currentSelectedPath();
    if (path.isEmpty()) return;
    if (!isPathMarkedDeleted(path)) return;

    QString err;
    const bool ok = ShellOps::restoreFromTrash(path, &err);
    if (!ok) {
        SoundEngine::instance().play(SoundEngine::Error);
        QMessageBox::warning(
            this, tr_("Restore from Recycle Bin"),
            tr_("Could not restore:\n%1\n\n%2").arg(path, err));
        return;
    }

    // Item is back on disk — clear strikethrough so the row keeps showing
    // up in the table without the "deleted" styling.
    m_model->clearDeleted(path);
    SoundEngine::instance().play(SoundEngine::Click);
}

void FilesView::onTableContextMenu(const QPoint& pos)
{
    const QModelIndex proxyIdx = m_table->indexAt(pos);
    if (!proxyIdx.isValid()) return;
    // Make sure the row under the cursor is the current selection so
    // currentSelectedPath() returns the same row the menu acts on.
    m_table->setCurrentIndex(proxyIdx);

    const QString path = currentSelectedPath();
    if (path.isEmpty()) return;
    const bool deleted = isPathMarkedDeleted(path);

    QMenu menu(this);
    QAction* openAct = menu.addAction(tr_("Open"));
    openAct->setEnabled(!deleted);
    QAction* revealAct = menu.addAction(tr_("Reveal in Explorer"));
    revealAct->setEnabled(!deleted);
    QAction* copyAct = menu.addAction(tr_("Copy path"));

    QAction* restoreAct = nullptr;
    if (deleted) {
        menu.addSeparator();
        restoreAct = menu.addAction(tr_("Restore from Recycle Bin"));
    }

    menu.addSeparator();
    QAction* deleteAct = menu.addAction(tr_("Delete"));
    deleteAct->setEnabled(!deleted);

    QAction* chosen = menu.exec(m_table->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == openAct) {
        if (!ensurePathExistsOrMark(path)) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else if (chosen == revealAct) {
        if (!ensurePathExistsOrMark(path)) return;
        ShellOps::revealInFileManager(path);
    } else if (chosen == copyAct) {
        ShellOps::copyToClipboard(path);
    } else if (restoreAct && chosen == restoreAct) {
        restoreSelectedFromTrash();
    } else if (chosen == deleteAct) {
        onDeleteClicked();
    }
}
