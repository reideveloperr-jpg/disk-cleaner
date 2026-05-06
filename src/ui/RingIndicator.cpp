#include "RingIndicator.h"

#include "ThemeManager.h"

#include <QPainter>
#include <QPaintEvent>

RingIndicator::RingIndicator(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
}

void RingIndicator::setFraction(double f)
{
    f = qBound(0.0, f, 1.0);
    if (qFuzzyCompare(f, m_fraction)) {
        return;
    }
    m_fraction = f;
    update();
}

void RingIndicator::setLineWidth(int px) { m_lineWidth = px; update(); }
void RingIndicator::setCenterText(const QString& s) { m_centerText = s; update(); }

QSize RingIndicator::sizeHint() const         { return { 96, 96 }; }
QSize RingIndicator::minimumSizeHint() const  { return { 48, 48 }; }

void RingIndicator::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& tm = ThemeManager::instance();

    const int side = qMin(width(), height());
    const QRectF rect(
        (width() - side) / 2.0 + m_lineWidth / 2.0,
        (height() - side) / 2.0 + m_lineWidth / 2.0,
        side - m_lineWidth,
        side - m_lineWidth);

    // Background ring.
    QColor bgColor = tm.accent();
    bgColor.setAlphaF(0.18);
    QPen bgPen(bgColor, m_lineWidth, Qt::SolidLine, Qt::FlatCap);
    p.setPen(bgPen);
    p.drawArc(rect, 0, 360 * 16);

    // Foreground arc proportional to fraction.
    QPen fgPen(tm.accent(), m_lineWidth, Qt::SolidLine, Qt::RoundCap);
    p.setPen(fgPen);
    const int startAngle = 90 * 16;
    const int spanAngle = -static_cast<int>(360.0 * m_fraction * 16.0);
    p.drawArc(rect, startAngle, spanAngle);

    if (!m_centerText.isEmpty()) {
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.05);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(tm.text());
        p.drawText(rect, Qt::AlignCenter, m_centerText);
    }
}
