#pragma once

#include "core/FileType.h"

#include <QHash>
#include <QWidget>

class QHBoxLayout;
class QButtonGroup;
class QPushButton;

class FilterChips : public QWidget {
    Q_OBJECT
public:
    explicit FilterChips(QWidget* parent = nullptr);

    void setActive(FileCategory cat);
    FileCategory active() const { return m_active; }

    void setCounts(const QHash<FileCategory, int>& counts);

signals:
    void categoryChanged(FileCategory cat);

private:
    QHBoxLayout* m_layout = nullptr;
    QButtonGroup* m_group = nullptr;
    FileCategory m_active = FileCategory::All;
    QHash<int, QPushButton*> m_buttons;  // by FileCategory int
    QHash<FileCategory, int> m_counts;

    void rebuild();
    QString labelFor(FileCategory cat) const;
};
