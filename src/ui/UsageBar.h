#pragma once

#include <QWidget>

// Slim horizontal usage bar.  The fill colour switches from green → orange →
// red as `value` approaches `max`, matching the way most disk-usage widgets
// communicate "we're getting full".
class UsageBar : public QWidget {
    Q_OBJECT
public:
    explicit UsageBar(QWidget* parent = nullptr);

    void setValue(qint64 value, qint64 max);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    qint64 m_value = 0;
    qint64 m_max = 0;
};
