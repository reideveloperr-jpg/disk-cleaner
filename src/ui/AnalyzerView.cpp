#include "AnalyzerView.h"

#include "FilterChips.h"
#include "audio/SoundEngine.h"
#include "core/SizeFormatter.h"
#include "i18n/I18n.h"
#include "models/FolderFilterProxyModel.h"
#include "models/FolderTreeModel.h"
#include "platform/ShellOps.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

namespace {

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
    if (dlg.exec() != QDialog::Accepted) return false;
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

AnalyzerView::AnalyzerView(QWidget* parent) : QWidget(parent)
{
    setObjectName("AnalyzerView");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("PageTitle");

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName("PageSubtitle");
    m_summaryLabel->setWordWrap(true);

    root->addWidget(m_titleLabel);
    root->addWidget(m_summaryLabel);

    auto* topRow = new QHBoxLayout;
    topRow->setSpacing(8);

    m_search = new QLineEdit(this);
    m_search->setObjectName("SearchBox");
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(220);
    m_search->setMinimumHeight(28);
    topRow->addWidget(m_search, 0);

    // Sort selector: lets the user pick the active sort column without
    // hunting for the right header to click. Click toggles direction.
    m_sortCombo = new QComboBox(this);
    m_sortCombo->setObjectName("SortCombo");
    m_sortCombo->setCursor(Qt::PointingHandCursor);
    m_sortCombo->setMinimumWidth(180);
    populateSortCombo();
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { applySortFromCombo(); });
    topRow->addWidget(m_sortCombo);

    m_openBtn = new QPushButton(this);
    m_openBtn->setObjectName("SecondaryButton");
    m_openBtn->setCursor(Qt::PointingHandCursor);
    m_openBtn->setEnabled(false);
    connect(m_openBtn, &QPushButton::clicked, this, &AnalyzerView::onOpenClicked);
    topRow->addWidget(m_openBtn);

    m_revealBtn = new QPushButton(this);
    m_revealBtn->setObjectName("SecondaryButton");
    m_revealBtn->setCursor(Qt::PointingHandCursor);
    m_revealBtn->setEnabled(false);
    connect(m_revealBtn, &QPushButton::clicked, this, &AnalyzerView::onRevealClicked);
    topRow->addWidget(m_revealBtn);

    m_deleteBtn = new QPushButton(this);
    m_deleteBtn->setObjectName("DangerButton");
    m_deleteBtn->setCursor(Qt::PointingHandCursor);
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &AnalyzerView::onDeleteClicked);
    topRow->addWidget(m_deleteBtn);

    m_refreshBtn = new QPushButton(this);
    m_refreshBtn->setObjectName("SecondaryButton");
    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_refreshBtn->setEnabled(false);
    connect(m_refreshBtn, &QPushButton::clicked, this, &AnalyzerView::onRefreshClicked);
    topRow->addWidget(m_refreshBtn);

    topRow->addStretch();
    root->addLayout(topRow);

    m_chips = new FilterChips(this);
    root->addWidget(m_chips);

    m_tree = new QTreeView(this);
    m_tree->setObjectName("AnalyzerTree");
    m_tree->setUniformRowHeights(true);
    m_tree->setSortingEnabled(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setExpandsOnDoubleClick(true);
    m_tree->setAnimated(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Drag rows out into Explorer / other apps. The model emits
    // text/uri-list with the row's absolute path; Qt picks Copy by default
    // and Move when the user holds Shift on Windows.
    m_tree->setDragEnabled(true);
    m_tree->setDragDropMode(QAbstractItemView::DragOnly);
    m_tree->setDefaultDropAction(Qt::CopyAction);

    m_model = new FolderTreeModel(this);
    m_proxy = new FolderFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setSortRole(Qt::UserRole);
    m_tree->setModel(m_proxy);
    m_tree->sortByColumn(FolderTreeModel::ColSize, Qt::DescendingOrder);

    // Keep the dropdown in sync when the user clicks the header directly,
    // so both inputs always agree on the current sort column / direction.
    connect(m_tree->header(), &QHeaderView::sortIndicatorChanged, this,
            [this](int col, Qt::SortOrder /*order*/) {
                if (!m_sortCombo) return;
                for (int i = 0; i < m_sortCombo->count(); ++i) {
                    if (m_sortCombo->itemData(i).toInt() == col) {
                        QSignalBlocker b(m_sortCombo);
                        m_sortCombo->setCurrentIndex(i);
                        return;
                    }
                }
            });

    // The model recomputes aggregates whenever rows are pruned. Keep the
    // summary label honest by rebuilding it from the model's current state
    // every time those numbers change — fixes the bug where the totals
    // stayed at the pre-deletion values until the user re-scanned.
    connect(m_model, &FolderTreeModel::totalsChanged, this,
            [this](qint64, int) { rebuildSummary(); });
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeView::customContextMenuRequested,
            this, &AnalyzerView::onTreeContextMenu);

    m_tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    m_tree->header()->setStretchLastSection(false);

    connect(m_chips, &FilterChips::categoryChanged, this, [this](FileCategory c) {
        m_proxy->setCategory(c);
        m_model->setActiveCategory(c);
    });
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& s) {
        m_proxy->setSearchText(s);
    });

    connect(m_tree->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &AnalyzerView::onSelectionChanged);

    // Double-click on a file leaf opens it; folders keep default expand.
    connect(m_tree, &QTreeView::doubleClicked, this, [this](const QModelIndex& proxyIdx) {
        const QModelIndex src = m_proxy->mapToSource(proxyIdx);
        if (!m_model->isFile(src)) return;
        const QString p = m_model->pathAt(src);
        if (!p.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(p));
    });

    connect(&I18n::instance(), &I18n::languageChanged, this, [this] { retranslate(); });

    root->addWidget(m_tree, 1);

    retranslate();
}

void AnalyzerView::populateSortCombo()
{
    if (!m_sortCombo) return;
    QSignalBlocker b(m_sortCombo);
    m_sortCombo->clear();
    // Each item carries the source column index; ascending/descending is
    // implied by the natural order of each metric (size/files DESC, name/
    // dates ASC) for a sensible default.
    m_sortCombo->addItem(tr_("Size (largest first)"), int(FolderTreeModel::ColSize));
    m_sortCombo->addItem(tr_("Name (A–Z)"),           int(FolderTreeModel::ColName));
    m_sortCombo->addItem(tr_("% of parent"),         int(FolderTreeModel::ColPercent));
    m_sortCombo->addItem(tr_("Files count"),         int(FolderTreeModel::ColFiles));
    m_sortCombo->addItem(tr_("Modified date"),       int(FolderTreeModel::ColModified));
    m_sortCombo->addItem(tr_("Created date"),        int(FolderTreeModel::ColCreated));
    m_sortCombo->addItem(tr_("Accessed date"),       int(FolderTreeModel::ColAccessed));
}

void AnalyzerView::applySortFromCombo()
{
    if (!m_tree || !m_sortCombo) return;
    const int col = m_sortCombo->currentData().toInt();
    Qt::SortOrder order = Qt::DescendingOrder;
    // Names read most naturally A–Z; dates default to newest-first.
    if (col == FolderTreeModel::ColName) order = Qt::AscendingOrder;
    if (col == FolderTreeModel::ColModified ||
        col == FolderTreeModel::ColCreated ||
        col == FolderTreeModel::ColAccessed) {
        order = Qt::DescendingOrder;
    }
    m_tree->sortByColumn(col, order);
}

void AnalyzerView::rebuildSummary()
{
    if (!m_model) return;
    if (m_rootPath.isEmpty()) return;
    m_summaryLabel->setText(
        tr_("Root: %1 · Files: %2 · Size: %3 · Folders: %4")
            .arg(m_rootPath,
                 QLocale().toString(qlonglong(m_model->totalFiles())),
                 SizeFormatter::humanReadable(m_model->totalSize()),
                 QLocale().toString(qlonglong(m_lastFolderCount))));
    m_summaryLabel->setProperty("hasResults", true);
}

bool AnalyzerView::ensurePathExistsOrMark(const QString& path)
{
    if (path.isEmpty()) return false;
    if (QFileInfo::exists(path)) return true;
    // Stale cache — the file/folder went away outside Volchay Cleans (cloud
    // sync, manual move, etc.). Mark the row as deleted so Refresh removes
    // it, and tell the user instead of letting Windows return a raw error.
    if (m_model) m_model->markDeleted(path);
    QMessageBox::information(
        this, tr_("Item not found"),
        tr_("%1\n\nThis item is no longer on disk — it was moved, renamed or "
            "deleted outside Volchay Cleans. Click Refresh to update the tree.")
            .arg(path));
    return false;
}

void AnalyzerView::retranslate()
{
    m_titleLabel->setText(tr_("Folder analysis"));
    if (m_summaryLabel->property("hasResults").toBool()) {
        rebuildSummary();
    } else {
        m_summaryLabel->setText(
            tr_("Folder tree with sizes appears here once a scan finishes."));
    }
    m_search->setPlaceholderText(tr_("Search by name…"));
    m_openBtn->setText(tr_("Open"));
    m_openBtn->setToolTip(tr_("Open the selected file or folder"));
    m_revealBtn->setText(tr_("Reveal in Explorer"));
    m_revealBtn->setToolTip(tr_("Show in Windows Explorer"));
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
    if (m_model) {
        emit m_model->headerDataChanged(
            Qt::Horizontal, 0, FolderTreeModel::ColumnCount - 1);
    }
}

void AnalyzerView::setScanResult(const ScanResult& r)
{
    m_rootPath = r.rootPath;
    m_lastFolderCount = r.totalDirs;
    m_model->setRoot(r.root, r.files);
    rebuildSummary();
    m_tree->expandToDepth(0);
    for (int c = 0; c < FolderTreeModel::ColumnCount; ++c) {
        m_tree->resizeColumnToContents(c);
    }
    if (m_tree->columnWidth(FolderTreeModel::ColName) < 360) {
        m_tree->setColumnWidth(FolderTreeModel::ColName, 360);
    }

    QHash<FileCategory, int> counts;
    counts.insert(FileCategory::All, r.files.size());
    for (const FileEntry& f : r.files) {
        counts[f.category] = counts.value(f.category) + 1;
    }
    m_chips->setCounts(counts);

    m_openBtn->setEnabled(false);
    m_revealBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_refreshBtn->setEnabled(true);
}

void AnalyzerView::clear()
{
    m_rootPath.clear();
    m_lastFolderCount = 0;
    m_model->clear();
    m_summaryLabel->setText(
        tr_("Folder tree with sizes appears here once a scan finishes."));
    m_summaryLabel->setProperty("hasResults", false);
    m_openBtn->setEnabled(false);
    m_revealBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_refreshBtn->setEnabled(false);
}

QString AnalyzerView::currentSelectedPath() const
{
    const QModelIndex idx = m_tree->currentIndex();
    if (!idx.isValid()) return {};
    const QModelIndex src = m_proxy->mapToSource(idx);
    if (!src.isValid()) return {};
    return m_model->pathAt(src);
}

void AnalyzerView::onSelectionChanged()
{
    const QString p = currentSelectedPath();
    const bool has = !p.isEmpty();
    m_openBtn->setEnabled(has);
    m_revealBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has);
}

void AnalyzerView::onOpenClicked()
{
    const QString p = currentSelectedPath();
    if (!ensurePathExistsOrMark(p)) return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(p));
}

void AnalyzerView::onRevealClicked()
{
    const QString p = currentSelectedPath();
    if (!ensurePathExistsOrMark(p)) return;
    ShellOps::revealInFileManager(p);
}

void AnalyzerView::onDeleteClicked()
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
    // Visually mark the row(s) as deleted (red strikethrough) — the user
    // can clear them from the tree by clicking Refresh.
    m_model->markDeleted(path);
    emit pathDeleted();
}

void AnalyzerView::onRefreshClicked()
{
    // Drop deleted rows — but only the ones whose underlying file/folder
    // is genuinely gone from disk. Anything that came back (e.g. user
    // restored it from the recycle bin) loses the strikethrough and stays
    // visible in the tree.
    m_model->purgeDeletedIfMissing();
    SoundEngine::instance().play(SoundEngine::Click);
}

bool AnalyzerView::isPathMarkedDeleted(const QString& path) const
{
    return !path.isEmpty() && m_model && m_model->isDeleted(path);
}

void AnalyzerView::restoreSelectedFromTrash()
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
    m_model->clearDeleted(path);
    SoundEngine::instance().play(SoundEngine::Click);
}

void AnalyzerView::onTreeContextMenu(const QPoint& pos)
{
    const QModelIndex proxyIdx = m_tree->indexAt(pos);
    if (!proxyIdx.isValid()) return;
    m_tree->setCurrentIndex(proxyIdx);

    const QString path = currentSelectedPath();
    if (path.isEmpty()) return;
    const bool deleted = isPathMarkedDeleted(path);

    QMenu menu(this);
    QAction* openAct = menu.addAction(tr_("Open"));
    openAct->setEnabled(!deleted);
    QAction* revealAct = menu.addAction(tr_("Reveal in Explorer"));
    revealAct->setEnabled(!deleted);

    QAction* restoreAct = nullptr;
    if (deleted) {
        menu.addSeparator();
        restoreAct = menu.addAction(tr_("Restore from Recycle Bin"));
    }

    menu.addSeparator();
    QAction* deleteAct = menu.addAction(tr_("Delete"));
    deleteAct->setEnabled(!deleted);

    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == openAct) {
        if (!ensurePathExistsOrMark(path)) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    } else if (chosen == revealAct) {
        if (!ensurePathExistsOrMark(path)) return;
        ShellOps::revealInFileManager(path);
    } else if (restoreAct && chosen == restoreAct) {
        restoreSelectedFromTrash();
    } else if (chosen == deleteAct) {
        onDeleteClicked();
    }
}
