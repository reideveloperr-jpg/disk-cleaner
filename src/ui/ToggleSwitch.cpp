#include "ToggleSwitch.h"

#include "ThemeManager.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(180);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_anim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& v) {
                m_progress = v.toReal();
                update();
            });

    // Drive the slide animation from the toggled() signal.  This is the only
    // signal Qt guarantees to emit on every actual state flip — including
    // user clicks (where checkStateSet() is intentionally suppressed by Qt's
    // internal blockRefresh flag) and programmatic setChecked() calls.
    connect(this, &QAbstractButton::toggled, this,
            [this](bool on) {
                if (m_suppressAnim) {
                    return;
                }
                const qreal target = on ? 1.0 : 0.0;
                m_anim->stop();
                m_anim->setStartValue(m_progress);
                m_anim->setEndValue(target);
                m_anim->start();
            });

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, [this](ThemeManager::Theme) { update(); });
}

void ToggleSwitch::setCheckedSilent(bool on)
{
    m_suppressAnim = true;
    QSignalBlocker b(this);
    setChecked(on);
    m_suppressAnim = false;
    m_anim->stop();
    m_progress = on ? 1.0 : 0.0;
    update();
}

QSize ToggleSwitch::sizeHint() const
{
    const QString lbl = text();
    const QFontMetrics fm(font());
    const int textW = lbl.isEmpty() ? 0 : fm.horizontalAdvance(lbl);
    const int textH = fm.height();
    const int w = kPillWidth + (lbl.isEmpty() ? 0 : kSpacing + textW);
    return { w, qMax(kPillHeight, textH) };
}

void ToggleSwitch::setLabel(const QString& s)
{
    setText(s);
    updateGeometry();
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& tm = ThemeManager::instance();
    const QColor accent  = tm.accent();
    const QColor border  = tm.border();
    const QColor track   = tm.surfaceAlt();
    const QColor textCol = isEnabled() ? tm.text() : tm.mutedText();

    const QRectF pill(0, (height() - kPillHeight) / 2.0, kPillWidth, kPillHeight);

    // Track interpolates from neutral to accent.
    QColor trackOff = track;
    QColor trackOn  = accent;
    QColor mixed(
        int(trackOff.red()   * (1 - m_progress) + trackOn.red()   * m_progress),
        int(trackOff.green() * (1 - m_progress) + trackOn.green() * m_progress),
        int(trackOff.blue()  * (1 - m_progress) + trackOn.blue()  * m_progress));
    p.setPen(QPen(border, 1.0));
    p.setBrush(mixed);
    p.drawRoundedRect(pill, kPillHeight / 2.0, kPillHeight / 2.0);

    // Handle.
    const qreal handleD = kPillHeight - 2 * kHandlePad;
    const qreal x0 = pill.left() + kHandlePad;
    const qreal x1 = pill.right() - kHandlePad - handleD;
    const qreal hx = x0 + (x1 - x0) * m_progress;
    const QRectF handle(hx, pill.top() + kHandlePad, handleD, handleD);
    QColor handleColor = (m_progress > 0.5) ? QColor(0xFF, 0xFF, 0xFF) : tm.text();
    p.setPen(Qt::NoPen);
    p.setBrush(handleColor);
    p.drawEllipse(handle);

    const QString lbl = text();
    if (!lbl.isEmpty()) {
        p.setPen(textCol);
        const QRect textRect(int(pill.right()) + kSpacing, 0,
                             width() - int(pill.right()) - kSpacing, height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, lbl);
    }
}
