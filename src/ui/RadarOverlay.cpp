#include "RadarOverlay.h"

#include "ThemeManager.h"
#include "core/SizeFormatter.h"
#include "i18n/I18n.h"

#include <QFileInfo>
#include <QFontMetrics>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QTimer>
#include <QtMath>

namespace {
constexpr double kSweepRadPerSec = 1.55;        // ~89 deg/sec — slightly slower, like a real PPI
constexpr qint64 kBlipLifetimeMs = 5500;        // a touch longer so phosphor decay reads
constexpr int kMaxRecent = 6;

// Amber phosphor colour family — matches the rest of the app's accent.
// Tailored to read like a CRT radar: a bright sweep-leading edge, a warmer
// mid-tone for blips and rings, and a deep amber for low-opacity grid.
QColor radarMain()    { return QColor(0xF5, 0x9E, 0x0B); }   // base amber
QColor radarBright()  { return QColor(0xFB, 0xBF, 0x24); }   // hot leading edge
QColor radarDeep()    { return QColor(0xD9, 0x77, 0x06); }   // muted grid lines
}  // namespace

RadarOverlay::RadarOverlay(QWidget* parent) : QWidget(parent)
{
    setObjectName("RadarOverlay");
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_StyledBackground, false);
    hide();

    m_timer = new QTimer(this);
    m_timer->setInterval(16);  // ~60 FPS
    connect(m_timer, &QTimer::timeout, this, &RadarOverlay::onTick);

    // Hide button (current scan only).
    m_hideBtn = new QPushButton(tr_("Hide radar"), this);
    m_hideBtn->setObjectName("RadarHideButton");
    m_hideBtn->setCursor(Qt::PointingHandCursor);
    connect(m_hideBtn, &QPushButton::clicked, this, [this] {
        emit hideRequested();
        hide();
    });

    // Permanently disable the animation.
    m_disableBtn = new QPushButton(tr_("Disable animation"), this);
    m_disableBtn->setObjectName("RadarDisableButton");
    m_disableBtn->setCursor(Qt::PointingHandCursor);
    connect(m_disableBtn, &QPushButton::clicked, this, [this] {
        m_animationEnabled = false;
        emit animationDisabled();
        hide();
    });

    connect(&I18n::instance(), &I18n::languageChanged, this, [this] {
        m_hideBtn->setText(tr_("Hide radar"));
        m_disableBtn->setText(tr_("Disable animation"));
        m_hideBtn->adjustSize();
        m_disableBtn->adjustSize();
        layoutChildren();
        update();
    });
}

void RadarOverlay::setAnimationEnabled(bool enabled)
{
    m_animationEnabled = enabled;
    if (!enabled && m_running) {
        hide();
        m_timer->stop();
        m_running = false;
    }
}

void RadarOverlay::start(const QString& rootPath)
{
    if (!m_animationEnabled) {
        return;
    }
    m_rootPath = rootPath;
    m_filesSeen = 0;
    m_totalSize = 0;
    m_currentDir.clear();
    m_blips.clear();
    m_recentList.clear();
    m_sweepAngle = 0.0;
    m_running = true;
    if (!m_clock.isValid()) {
        m_clock.start();
    } else {
        m_clock.restart();
    }
    layoutChildren();
    show();
    raise();
    m_timer->start();
}

void RadarOverlay::stop()
{
    m_timer->stop();
    m_running = false;
    hide();
}

void RadarOverlay::updateStats(qint64 filesSeen, qint64 totalSize,
                               const QString& currentDir)
{
    m_filesSeen = filesSeen;
    m_totalSize = totalSize;
    m_currentDir = currentDir;
    update();
}

void RadarOverlay::addRecentFiles(const QStringList& paths)
{
    if (!m_animationEnabled || paths.isEmpty()) {
        return;
    }
    auto* rng = QRandomGenerator::global();
    const qint64 nowMs = m_clock.isValid() ? m_clock.elapsed() : 0;
    for (const QString& p : paths) {
        Blip b;
        b.angle = rng->bounded(2.0 * M_PI);
        // Bias new contacts toward the outer half of the dial — the dial
        // centre is reserved for the stats readout, and an outer-ring
        // spread reads more like real radar contacts than a uniform fill.
        b.radius = 0.42 + rng->bounded(48) / 100.0;  // 0.42 .. 0.90
        b.birthMs = nowMs;
        b.name = QFileInfo(p).fileName();
        if (b.name.isEmpty()) {
            b.name = p;
        }
        m_blips.push_back(b);

        m_recentList.enqueue(b.name);
        while (m_recentList.size() > kMaxRecent) {
            m_recentList.dequeue();
        }
    }
    if (m_blips.size() > 60) {
        m_blips.remove(0, m_blips.size() - 60);
    }
}

void RadarOverlay::onTick()
{
    if (!m_running) {
        return;
    }
    const double dt = 0.016;
    m_sweepAngle += kSweepRadPerSec * dt;
    if (m_sweepAngle > 2.0 * M_PI) {
        m_sweepAngle -= 2.0 * M_PI;
    }

    // Drop expired blips.
    if (m_clock.isValid()) {
        const qint64 now = m_clock.elapsed();
        m_blips.erase(std::remove_if(m_blips.begin(), m_blips.end(),
                                     [now](const Blip& b) {
                                         return (now - b.birthMs) > kBlipLifetimeMs;
                                     }),
                      m_blips.end());
    }
    update();
}

void RadarOverlay::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    layoutChildren();
}

QRectF RadarOverlay::dialRect() const
{
    const double maxByH = height() - 200.0;
    const double maxByW = width() * 0.62;
    const double side = qMin(qMin(maxByH, maxByW), 460.0);
    const double finalSide = qMax(side, 240.0);
    return QRectF((width() - finalSide) / 2.0,
                  (height() - finalSide) / 2.0 - 60,
                  finalSide, finalSide);
}

void RadarOverlay::layoutChildren()
{
    const QRectF dial = dialRect();
    const int streamHeight = 6 * 18;  // matches kMaxRecent
    const int btnY = int(dial.bottom()) + 4 + streamHeight + 18;
    const int gap = 12;
    m_hideBtn->adjustSize();
    m_disableBtn->adjustSize();
    const int totalW = m_hideBtn->width() + gap + m_disableBtn->width();
    int x = (width() - totalW) / 2;
    m_hideBtn->move(x, btnY);
    x += m_hideBtn->width() + gap;
    m_disableBtn->move(x, btnY);
}

QString RadarOverlay::shorten(const QString& s, int max) const
{
    if (s.size() <= max) {
        return s;
    }
    return QStringLiteral("…") + s.right(max - 1);
}

namespace {
// Smallest absolute angle between two radians, in [0, π].
double angleDiff(double a, double b)
{
    double d = std::fmod(a - b, 2.0 * M_PI);
    if (d < 0) d += 2.0 * M_PI;
    if (d > M_PI) d = 2.0 * M_PI - d;
    return d;
}
}  // namespace

void RadarOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto& tm = ThemeManager::instance();
    const QColor accent = radarMain();
    const QColor accentBright = radarBright();
    const QColor accentDeep = radarDeep();
    const QColor textCol = tm.text();
    const QColor mutedCol = tm.mutedText();

    // Frosted backdrop over the whole overlay.
    QColor backdrop = (tm.current() == ThemeManager::Light) ? QColor(20, 20, 24, 200)
                                                            : QColor(0, 0, 0, 200);
    p.fillRect(rect(), backdrop);

    // ---------- Title / subtitle ----------
    {
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.6);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        p.setPen(textCol);
        const QString title = tr_("Scanning…");
        const QRect titleRect(0, int(dialRect().top() - 86),
                              width(), 36);
        p.drawText(titleRect, Qt::AlignHCenter | Qt::AlignBottom, title);

        QFont sf = font();
        sf.setPointSizeF(sf.pointSizeF() * 1.0);
        p.setFont(sf);
        p.setPen(mutedCol);
        const QString sub = m_rootPath.isEmpty()
            ? QString()
            : tr_("Root: %1").arg(m_rootPath);
        p.drawText(QRect(0, int(dialRect().top() - 50), width(), 24),
                   Qt::AlignHCenter | Qt::AlignBottom, sub);
    }

    const QRectF dial = dialRect();
    const QPointF center = dial.center();
    const double radius = dial.width() / 2.0;

    // ---------- Bezel: outer dark ring with subtle highlight ----------
    {
        const double bezelOuter = radius + 10.0;
        QRadialGradient bezelGrad(center, bezelOuter);
        bezelGrad.setColorAt(0.95, QColor(0, 0, 0, 0));
        bezelGrad.setColorAt(0.97, QColor(20, 20, 22, 230));
        bezelGrad.setColorAt(1.00, QColor(8, 8, 10, 240));
        p.setPen(Qt::NoPen);
        p.setBrush(bezelGrad);
        p.drawEllipse(center, bezelOuter, bezelOuter);

        // Thin inner highlight stroke
        QPen hi(accentDeep, 1.0);
        hi.setColor(QColor(accent.red(), accent.green(), accent.blue(), 90));
        p.setPen(hi);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, radius + 2.0, radius + 2.0);
    }

    // ---------- Dish background — radial gradient (depth) ----------
    {
        QRadialGradient dishGrad(center, radius);
        // The dish is darker in the centre and very subtly lighter near the
        // rim, so the eye reads it as a curved screen rather than a flat
        // disc.  Both stops are extremely low alpha so the dim backdrop
        // shows through.
        QColor inner = (tm.current() == ThemeManager::Light)
                           ? QColor(20, 14, 6, 30)
                           : QColor(0, 0, 0, 60);
        QColor outer = accent;
        outer.setAlphaF(0.045);
        dishGrad.setColorAt(0.00, inner);
        dishGrad.setColorAt(0.85, inner);
        dishGrad.setColorAt(1.00, outer);
        p.setPen(Qt::NoPen);
        p.setBrush(dishGrad);
        p.drawEllipse(center, radius, radius);
    }

    // ---------- Range rings ----------
    {
        // Major rings: 4 amber dashed lines.
        QColor majorCol = accent;
        majorCol.setAlphaF(0.22);
        QPen majorPen(majorCol, 1.4);
        p.setPen(majorPen);
        p.setBrush(Qt::NoBrush);
        for (int i = 1; i <= 4; ++i) {
            const double r = radius * (i / 4.0);
            p.drawEllipse(center, r, r);
        }
        // Minor rings between majors: thinner dotted lines.
        QColor minorCol = accentDeep;
        minorCol.setAlphaF(0.10);
        QPen minorPen(minorCol, 0.8, Qt::DotLine);
        p.setPen(minorPen);
        for (int i = 1; i <= 4; ++i) {
            const double r = radius * ((i - 0.5) / 4.0);
            p.drawEllipse(center, r, r);
        }
    }

    // ---------- Cross-hairs with tick marks ----------
    {
        QColor crossCol = accent;
        crossCol.setAlphaF(0.22);
        QPen crossPen(crossCol, 1.0);
        p.setPen(crossPen);
        p.drawLine(QPointF(center.x() - radius, center.y()),
                   QPointF(center.x() + radius, center.y()));
        p.drawLine(QPointF(center.x(), center.y() - radius),
                   QPointF(center.x(), center.y() + radius));
        // Tick marks every 10% of radius along the four cross-hair arms.
        QColor tickCol = accent;
        tickCol.setAlphaF(0.30);
        QPen tickPen(tickCol, 1.2);
        p.setPen(tickPen);
        for (int i = 1; i <= 9; ++i) {
            const double t = radius * (i / 10.0);
            const double tickLen = (i % 5 == 0) ? 6.0 : 3.0;
            p.drawLine(QPointF(center.x() + t, center.y() - tickLen),
                       QPointF(center.x() + t, center.y() + tickLen));
            p.drawLine(QPointF(center.x() - t, center.y() - tickLen),
                       QPointF(center.x() - t, center.y() + tickLen));
            p.drawLine(QPointF(center.x() - tickLen, center.y() + t),
                       QPointF(center.x() + tickLen, center.y() + t));
            p.drawLine(QPointF(center.x() - tickLen, center.y() - t),
                       QPointF(center.x() + tickLen, center.y() - t));
        }
    }

    // ---------- Compass labels (N/E/S/W) ----------
    {
        QFont cf = font();
        cf.setPointSizeF(cf.pointSizeF() * 0.9);
        cf.setWeight(QFont::DemiBold);
        p.setFont(cf);
        QColor lblCol = accent;
        lblCol.setAlphaF(0.55);
        p.setPen(lblCol);
        const double off = radius + 18.0;
        const QFontMetrics fm(cf);
        auto drawLabel = [&](const QString& s, QPointF at) {
            const QRectF box(at.x() - 12, at.y() - 10, 24, 20);
            p.drawText(box, Qt::AlignCenter, s);
        };
        drawLabel(QStringLiteral("N"), QPointF(center.x(),         center.y() - off));
        drawLabel(QStringLiteral("S"), QPointF(center.x(),         center.y() + off));
        drawLabel(QStringLiteral("E"), QPointF(center.x() + off,   center.y()));
        drawLabel(QStringLiteral("W"), QPointF(center.x() - off,   center.y()));
    }

    // ---------- Sweep with phosphor afterglow ----------
    {
        // QConicalGradient measures angles in degrees, counter-clockwise,
        // 0 = +X axis (matches our m_sweepAngle convention since we
        // already invert Y when placing the tip).
        const double sweepDeg = qRadiansToDegrees(m_sweepAngle);
        QConicalGradient grad(center, sweepDeg);
        QColor head = accentBright;
        head.setAlphaF(0.95);
        QColor tail = accent;
        tail.setAlphaF(0.05);
        QColor fade = accent;
        fade.setAlphaF(0.0);
        // Conic gradient runs from 0..1, with 0 at the start angle.  A
        // value of 0.0 sits AT the leading edge, then unwinds backwards
        // (i.e. the trailing phosphor) up to ~0.75 of the dial before
        // disappearing entirely.
        grad.setColorAt(0.00, head);
        grad.setColorAt(0.04, accent);
        grad.setColorAt(0.30, tail);
        grad.setColorAt(0.75, fade);
        grad.setColorAt(1.00, fade);
        QPainterPath disc;
        disc.addEllipse(center, radius, radius);
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawPath(disc);
    }

    // ---------- Sweep arm leading edge ----------
    {
        const double cx = center.x();
        const double cy = center.y();
        const double tipX = cx + radius * std::cos(m_sweepAngle);
        const double tipY = cy - radius * std::sin(m_sweepAngle);
        QPen armPen(accentBright, 2.6, Qt::SolidLine, Qt::RoundCap);
        p.setPen(armPen);
        p.drawLine(QPointF(cx, cy), QPointF(tipX, tipY));

        // Soft halo at the tip.
        QColor head = accentBright;
        head.setAlphaF(0.55);
        p.setBrush(head);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(tipX, tipY), 7.0, 7.0);
        // A tighter brighter core inside the halo.
        QColor core = accentBright;
        core.setAlphaF(0.95);
        p.setBrush(core);
        p.drawEllipse(QPointF(tipX, tipY), 3.0, 3.0);
    }

    // ---------- Outer rim subtle pulse ----------
    {
        const double pulse = 0.5 + 0.5 * std::sin(m_sweepAngle * 2.0);
        QColor edge = accent;
        edge.setAlphaF(0.10 + 0.10 * pulse);
        QPen edgePen(edge, 2.0);
        p.setPen(edgePen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, radius - 1.0, radius - 1.0);
    }

    // ---------- Blips (target contacts) ----------
    if (m_clock.isValid()) {
        const qint64 now = m_clock.elapsed();
        for (const Blip& b : m_blips) {
            const qint64 age = now - b.birthMs;
            if (age < 0 || age > kBlipLifetimeMs) {
                continue;
            }
            const double t = double(age) / double(kBlipLifetimeMs);
            // Base alpha decays over the blip's lifetime.
            const double baseAlpha = std::pow(1.0 - t, 1.3);

            // Target-acquisition flash: brighter when the sweep is near
            // the contact's angle.  Sigma = 14 deg → narrow ridge that
            // tracks the sweep arm cleanly.
            const double diff = angleDiff(m_sweepAngle, b.angle);
            const double sigma = qDegreesToRadians(14.0);
            const double flash = std::exp(-(diff * diff) / (sigma * sigma));

            const double r = radius * b.radius;
            const double x = center.x() + r * std::cos(b.angle);
            const double y = center.y() - r * std::sin(b.angle);

            // Inner bright dot — flashes white-hot when sweep crosses,
            // then settles back to amber.
            QColor dot = accentBright;
            dot.setAlphaF(qBound(0.0, baseAlpha * (0.55 + 0.45 * flash), 1.0));
            const double dotR = 3.0 + 3.5 * baseAlpha + 2.5 * flash;
            p.setBrush(dot);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(x, y), dotR, dotR);

            // Outer halo: large, very transparent — bigger when the
            // sweep just lit the target.
            QColor halo = accent;
            halo.setAlphaF(qBound(0.0, baseAlpha * (0.18 + 0.45 * flash), 0.85));
            p.setBrush(halo);
            const double haloR = dotR * (2.0 + 1.4 * flash);
            p.drawEllipse(QPointF(x, y), haloR, haloR);

            // Crosshair tick marks just outside the dot — the standard
            // PPI scope target reticle.
            QColor tick = accent;
            tick.setAlphaF(qBound(0.0, baseAlpha * (0.35 + 0.55 * flash), 1.0));
            QPen tickPen(tick, 1.4, Qt::SolidLine, Qt::FlatCap);
            p.setPen(tickPen);
            p.setBrush(Qt::NoBrush);
            const double tickInner = dotR + 3.0;
            const double tickOuter = tickInner + 4.0 + 3.0 * flash;
            p.drawLine(QPointF(x + tickInner, y), QPointF(x + tickOuter, y));
            p.drawLine(QPointF(x - tickInner, y), QPointF(x - tickOuter, y));
            p.drawLine(QPointF(x, y + tickInner), QPointF(x, y + tickOuter));
            p.drawLine(QPointF(x, y - tickInner), QPointF(x, y - tickOuter));
        }
    }

    // ---------- Subtle scanline overlay (CRT feel) ----------
    {
        QColor scan(0, 0, 0, 18);
        p.setPen(Qt::NoPen);
        p.setBrush(scan);
        // Clip to the dial so scanlines only show inside the screen.
        p.save();
        QPainterPath clip;
        clip.addEllipse(center, radius - 1.0, radius - 1.0);
        p.setClipPath(clip);
        for (int yy = int(dial.top()); yy < int(dial.bottom()); yy += 3) {
            p.drawRect(QRectF(dial.left(), yy, dial.width(), 1));
        }
        p.restore();
    }

    // ---------- Center stats ----------
    {
        QFont big = font();
        big.setPointSizeF(big.pointSizeF() * 2.0);
        big.setWeight(QFont::DemiBold);
        p.setFont(big);
        p.setPen(textCol);
        const QString filesText = QLocale().toString(qlonglong(m_filesSeen));
        const QFontMetrics fm(big);
        const QRectF tr(center.x() - 120, center.y() - 30, 240, 36);
        p.drawText(tr, Qt::AlignHCenter | Qt::AlignVCenter, filesText);

        QFont small = font();
        small.setPointSizeF(small.pointSizeF() * 1.0);
        p.setFont(small);
        p.setPen(mutedCol);
        const QString sizeText = SizeFormatter::humanReadable(m_totalSize);
        const QRectF sr(center.x() - 120, center.y() + 4, 240, 24);
        p.drawText(sr, Qt::AlignHCenter | Qt::AlignVCenter, sizeText);

        const QString lbl = tr_("Files count");
        QFont label = font();
        label.setPointSizeF(label.pointSizeF() * 0.85);
        p.setFont(label);
        p.setPen(mutedCol);
        const QRectF lr(center.x() - 120, center.y() - 56, 240, 22);
        p.drawText(lr, Qt::AlignHCenter | Qt::AlignVCenter, lbl);
    }

    // ---------- Recent file names below the dial ----------
    if (!m_recentList.isEmpty()) {
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 0.95);
        p.setFont(f);
        const int yStart = int(dial.bottom() + 4);
        int y = yStart;
        const int n = m_recentList.size();
        for (int i = n - 1; i >= 0; --i) {
            const double age = double((n - 1) - i) / double(qMax(n, 1));
            QColor c = textCol;
            c.setAlphaF(qBound(0.25, 1.0 - age * 0.85, 1.0));
            p.setPen(c);
            const QString text = shorten(m_recentList.at(i), 60);
            p.drawText(QRect(0, y, width(), 18),
                       Qt::AlignHCenter | Qt::AlignVCenter, text);
            y += 18;
        }
    }
}
