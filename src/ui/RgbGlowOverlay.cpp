#include "RgbGlowOverlay.h"

#include <QPainter>
#include <QRadialGradient>
#include <QTimer>
#include <cmath>

RgbGlowOverlay::RgbGlowOverlay(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);

    m_timer = new QTimer(this);
    m_timer->setInterval(50);  // 20 FPS is plenty for slow drift
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_t += 0.004;
        update();
    });
    hide();
}

void RgbGlowOverlay::setActive(bool on)
{
    if (m_active == on) return;
    m_active = on;
    if (on) {
        show();
        raise();
        m_timer->start();
    } else {
        m_timer->stop();
        hide();
    }
}

namespace {
struct Orb {
    qreal phase;     // base phase offset
    qreal radius;    // fraction of min(w,h)
    QColor color;
};
}  // namespace

void RgbGlowOverlay::paintEvent(QPaintEvent*)
{
    if (!m_active) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    // SourceOver (the default) so the orbs don't wash out text underneath.
    // CompositionMode_Plus made the centres almost white when stacking.

    static const Orb orbs[] = {
        { 0.00, 0.42, QColor(  0, 200, 255) },  // cyan
        { 0.33, 0.40, QColor(255,  80, 220) },  // magenta
        { 0.66, 0.42, QColor(120, 220,  90) },  // green
    };

    const qreal w = width();
    const qreal h = height();
    const qreal d = qMin(w, h);

    for (const Orb& orb : orbs) {
        const qreal a = (m_t + orb.phase) * 2.0 * M_PI;
        // Push orb centres well past the visible edges so only the soft
        // outer fringe of each gradient bleeds into the window — keeps text
        // crisp even when several orbs overlap.
        const qreal cx = w * (0.5 + 0.70 * std::cos(a));
        const qreal cy = h * (0.5 + 0.70 * std::sin(a * 0.7));
        const qreal r  = d * orb.radius;
        QRadialGradient g(cx, cy, r);
        QColor c0 = orb.color;
        c0.setAlpha(18);    // very gentle tint, no text wash-out
        QColor c1 = orb.color;
        c1.setAlpha(0);
        g.setColorAt(0.0, c0);
        g.setColorAt(1.0, c1);
        p.fillRect(rect(), g);
    }
}
