#pragma once

#include "core/DriveInfo.h"

#include <QWidget>

class QLabel;
class RingIndicator;
class QPushButton;

class DriveCard : public QWidget {
    Q_OBJECT
public:
    explicit DriveCard(const DriveInfo& info, QWidget* parent = nullptr);

signals:
    void analyzeRequested(const QString& rootPath);

private:
    DriveInfo m_info;
    RingIndicator* m_ring = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;
    QLabel* m_usedLabel = nullptr;
    QLabel* m_freeLabel = nullptr;
    QLabel* m_fsLabel = nullptr;
    QPushButton* m_analyzeBtn = nullptr;
};
