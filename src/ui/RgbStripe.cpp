#include "RgbStripe.h"

#include <QLinearGradient>
#include <QPainter>
#include <QTimer>

RgbStripe::RgbStripe(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setFixedHeight(4);
    m_timer = new QTimer(this);
    m_timer->setInterval(33);  // ~30 FPS, easy on the CPU
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_phase += 0.008;
        if (m_phase > 1.0) m_phase -= 1.0;
        update();
    });
}

QSize RgbStripe::sizeHint() const         { return { 200, 4 }; }
QSize RgbStripe::minimumSizeHint() const  { return {  60, 4 }; }

void RgbStripe::showEvent(QShowEvent*) { m_timer->start(); }
void RgbStripe::hideEvent(QHideEvent*) { m_timer->stop(); }

void RgbStripe::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient g(0, 0, width(), 0);
    // Rainbow stops are offset by m_phase to scroll right→left.
    constexpr int kStops = 8;
    for (int i = 0; i <= kStops; ++i) {
        const qreal pos = qreal(i) / kStops;
        qreal h = pos + m_phase;
        h = h - std::floor(h);
        g.setColorAt(pos, QColor::fromHsvF(h, 0.85, 1.0));
    }
    p.fillRect(rect(), g);
}
