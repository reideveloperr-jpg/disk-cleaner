#pragma once

#include <QWidget>

// A static ring/donut showing a fraction (0..1) — used on drive cards.
class RingIndicator : public QWidget {
    Q_OBJECT
public:
    explicit RingIndicator(QWidget* parent = nullptr);

    void setFraction(double f);          // 0..1
    void setLineWidth(int px);
    void setCenterText(const QString& s); // optional small label

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;

private:
    double m_fraction = 0.0;
    int m_lineWidth = 8;
    QString m_centerText;
};
