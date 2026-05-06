#include "UsageBar.h"

#include <QPainter>
#include <QPainterPath>

UsageBar::UsageBar(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(8);
    setMinimumWidth(80);
}

void UsageBar::setValue(qint64 value, qint64 max)
{
    if (m_value == value && m_max == max) return;
    m_value = value;
    m_max = max;
    update();
}

void UsageBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0, 0, -1, -1);
    const qreal radius = r.height() / 2.0;

    QPainterPath path;
    path.addRoundedRect(r, radius, radius);
    p.fillPath(path, QColor(255, 255, 255, 22));   // track

    if (m_max <= 0 || m_value <= 0) return;

    const double pct = double(m_value) / double(m_max);
    const double w = std::min(1.0, std::max(0.0, pct)) * r.width();

    QColor c;
    if (pct < 0.70) {
        c = QColor(68, 224, 132);   // green
    } else if (pct < 0.90) {
        c = QColor(255, 187, 71);   // amber
    } else {
        c = QColor(255, 95, 95);    // red
    }

    QPainterPath fill;
    fill.addRoundedRect(QRectF(r.left(), r.top(), w, r.height()), radius, radius);
    p.fillPath(fill, c);
}
