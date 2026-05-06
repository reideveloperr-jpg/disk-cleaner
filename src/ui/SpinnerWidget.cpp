#include "SpinnerWidget.h"

#include "ThemeManager.h"

#include <QConicalGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QTimer>
#include <QtMath>

SpinnerWidget::SpinnerWidget(QWidget* parent) : QWidget(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(16);  // ~60 FPS
    connect(m_timer, &QTimer::timeout, this, [this] {
        // 4°/frame at 60 FPS ≈ 1.5 s/rev — noticeably smoother / less frantic.
        m_angle = (m_angle + 4) % 360;
        update();
    });

    m_progressAnim = new QPropertyAnimation(this, "progress", this);
    m_progressAnim->setDuration(450);
    m_progressAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_color = ThemeManager::instance().accent();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this] {
        m_color = ThemeManager::instance().accent();
        update();
    });
    setAttribute(Qt::WA_TranslucentBackground);
}

void SpinnerWidget::start()
{
    if (m_determinate) {
        // In determinate mode we don't rotate; the arc grows with progress.
        update();
        return;
    }
    if (!m_timer->isActive()) m_timer->start();
    update();
}

void SpinnerWidget::stop()
{
    m_timer->stop();
    update();
}

bool SpinnerWidget::isRunning() const
{
    return m_timer->isActive() || m_determinate;
}

void SpinnerWidget::setLineWidth(int px) { m_lineWidth = px; update(); }
void SpinnerWidget::setColor(const QColor& c) { m_color = c; update(); }

void SpinnerWidget::setProgress(qreal value)
{
    value = qBound<qreal>(0.0, value, 1.0);
    if (!m_determinate) {
        m_determinate = true;
        m_timer->stop();
    }
    if (qFuzzyCompare(value, m_progress)) {
        return;
    }
    m_progressAnim->stop();
    m_progressAnim->setStartValue(m_progress);
    m_progressAnim->setEndValue(value);
    m_progressAnim->start();
}

void SpinnerWidget::resetProgress()
{
    m_progressAnim->stop();
    m_progress = 0.0;
    m_determinate = false;
    if (isVisible() && !m_timer->isActive()) {
        m_timer->start();
    }
    update();
}

void SpinnerWidget::setProgressValue(qreal v)
{
    m_progress = v;
    update();
}

QSize SpinnerWidget::sizeHint() const         { return { 18, 18 }; }
QSize SpinnerWidget::minimumSizeHint() const  { return { 14, 14 }; }

void SpinnerWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (!m_determinate && !m_timer->isActive()) {
        m_timer->start();
    }
}

void SpinnerWidget::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    m_timer->stop();
}

void SpinnerWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int side = qMin(width(), height());
    const QRectF rect(
        (width() - side) / 2.0 + m_lineWidth / 2.0,
        (height() - side) / 2.0 + m_lineWidth / 2.0,
        side - m_lineWidth,
        side - m_lineWidth);

    // Subtle background ring for context.
    QColor bg = m_color;
    bg.setAlphaF(0.18);
    QPen bgPen(bg, m_lineWidth, Qt::SolidLine, Qt::RoundCap);
    p.setPen(bgPen);
    p.drawArc(rect, 0, 360 * 16);

    QPen fgPen(m_color, m_lineWidth, Qt::SolidLine, Qt::RoundCap);
    p.setPen(fgPen);

    if (m_determinate) {
        // Fill from 12 o'clock clockwise as progress grows from 0 to 1.
        const int startAngle = 90 * 16;
        const int spanAngle  = -int(qRound(m_progress * 360.0)) * 16;
        if (spanAngle != 0) {
            p.drawArc(rect, startAngle, spanAngle);
        }
    } else {
        // 270° arc rotating around the ring (indeterminate).
        const int startAngle = (90 - m_angle) * 16;
        const int spanAngle = -270 * 16;
        p.drawArc(rect, startAngle, spanAngle);
    }
}
