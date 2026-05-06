#pragma once

#include <QWidget>

class QButtonGroup;
class QPushButton;
class QVBoxLayout;
class QToolButton;
class QLabel;

class Sidebar : public QWidget {
    Q_OBJECT
public:
    enum SectionId {
        Overview = 0,
        Analyzer,
        Files,
        Recent,
        Settings,
    };

    explicit Sidebar(QWidget* parent = nullptr);

    void setActiveSection(SectionId id);
    SectionId activeSection() const { return m_active; }

signals:
    void sectionChanged(SectionId id);
    void themeToggleRequested();

private:
    QPushButton* makeNavButton(const QString& title, const QString& iconKey, int id);

    QButtonGroup* m_group = nullptr;
    QVBoxLayout* m_navLayout = nullptr;
    QToolButton* m_themeBtn = nullptr;
    QLabel* m_navSectionLabel = nullptr;
    QPushButton* m_btnOverview = nullptr;
    QPushButton* m_btnAnalyzer = nullptr;
    QPushButton* m_btnFiles = nullptr;
    QPushButton* m_btnRecent = nullptr;
    QPushButton* m_btnSettings = nullptr;
    SectionId m_active = Overview;
};
