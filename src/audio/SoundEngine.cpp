#include "SoundEngine.h"

#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSoundEffect>
#include <QTemporaryDir>
#include <QUrl>
#include <QtEndian>
#include <cmath>

namespace {

constexpr int kSampleRate = 44100;
constexpr int kBitsPerSample = 16;

// ---------- Low-pass smoother ----------
// One-pole IIR filter softens the high-frequency partials so the sounds
// feel rounded and "modern" instead of buzzy.
struct LowPass {
    double cutoffHz;
    double y = 0.0;
    double sample(double x)
    {
        const double rc = 1.0 / (2.0 * M_PI * cutoffHz);
        const double dt = 1.0 / double(kSampleRate);
        const double a = dt / (rc + dt);
        y = y + a * (x - y);
        return y;
    }
};

// ---------- Soft sine-pluck voice ----------
// Pure sine with a barely-audible octave partial, smoothly attacked and
// exponentially decayed. The signal is run through a low-pass to keep the
// upper harmonics gentle. This is the "bell" half of every event.
QByteArray renderChime(double freqHz, double durationS, double amp,
                       double decayS, double partialMix, double cutoffHz)
{
    const int n = int(durationS * kSampleRate);
    QByteArray pcm;
    pcm.resize(n * 2);

    const double dt = 1.0 / double(kSampleRate);
    LowPass lp{ cutoffHz, 0.0 };

    double phaseFund = 0.0;
    double phaseOct  = 0.0;

    // ~5 ms cosine attack — softer than a linear ramp, no click.
    const int attack = std::max(96, kSampleRate / 200);

    char* out = pcm.data();
    for (int i = 0; i < n; ++i) {
        const double t = i * dt;
        const double envExp = std::exp(-t / decayS);
        const double envAtt = (i < attack)
                                  ? 0.5 * (1.0 - std::cos(M_PI * double(i) / attack))
                                  : 1.0;
        const double env = envExp * envAtt;

        const double fund = std::sin(phaseFund);
        const double oct  = std::sin(phaseOct);
        double s = (fund + partialMix * oct) / (1.0 + partialMix);
        s = lp.sample(s) * amp * env;

        const qint16 sample = qint16(qBound(-32767.0, s * 32767.0, 32767.0));
        out[i * 2 + 0] = char(sample & 0xff);
        out[i * 2 + 1] = char((sample >> 8) & 0xff);

        phaseFund += 2.0 * M_PI * freqHz * dt;
        phaseOct  += 2.0 * M_PI * freqHz * 2.0 * dt;  // pure octave, no detune
    }
    return pcm;
}

// ---------- Water-drop voice ----------
// Pitch slides from startHz to endHz across the sound (linear in log freq, so
// the glide sounds musical, not robotic).  This is the "bubble" / "plip"
// half of the palette.  Layer it over a chime for the warm, modern feel the
// user asked for.
QByteArray renderDrop(double startHz, double endHz, double durationS,
                      double amp, double decayS, double cutoffHz)
{
    const int n = int(durationS * kSampleRate);
    QByteArray pcm;
    pcm.resize(n * 2);

    const double dt = 1.0 / double(kSampleRate);
    LowPass lp{ cutoffHz, 0.0 };

    double phase = 0.0;
    const int attack = std::max(96, kSampleRate / 150);
    const double logStart = std::log(startHz);
    const double logEnd   = std::log(endHz);

    char* out = pcm.data();
    for (int i = 0; i < n; ++i) {
        const double t = i * dt;
        const double envExp = std::exp(-t / decayS);
        const double envAtt = (i < attack)
                                  ? 0.5 * (1.0 - std::cos(M_PI * double(i) / attack))
                                  : 1.0;
        const double env = envExp * envAtt;

        const double prog = double(i) / double(n);
        const double freq = std::exp(logStart + (logEnd - logStart) * prog);

        double s = std::sin(phase);
        s = lp.sample(s) * amp * env;
        const qint16 sample = qint16(qBound(-32767.0, s * 32767.0, 32767.0));
        out[i * 2 + 0] = char(sample & 0xff);
        out[i * 2 + 1] = char((sample >> 8) & 0xff);

        phase += 2.0 * M_PI * freq * dt;
    }
    return pcm;
}

QByteArray renderSilence(double durationS)
{
    const int n = int(durationS * kSampleRate);
    return QByteArray(n * 2, '\0');
}

// Mix two equal-length PCM buffers together (16-bit little-endian, mono).
QByteArray mixPcm(const QByteArray& a, const QByteArray& b)
{
    const int n = std::min(a.size(), b.size()) / 2;
    QByteArray out;
    out.resize(n * 2);
    char* o = out.data();
    const char* pa = a.constData();
    const char* pb = b.constData();
    for (int i = 0; i < n; ++i) {
        const qint16 sa = qint16(quint8(pa[i*2]) | (quint8(pa[i*2+1]) << 8));
        const qint16 sb = qint16(quint8(pb[i*2]) | (quint8(pb[i*2+1]) << 8));
        const int m = qBound<int>(-32767, int(sa) + int(sb), 32767);
        o[i*2]     = char(m & 0xff);
        o[i*2 + 1] = char((m >> 8) & 0xff);
    }
    return out;
}

// Build the PCM body for each event.  Each event layers a "drop" (rising
// pitch glide, sounds like a water bubble) under a soft chime, which gives
// a warm, modern, non-robotic timbre.
QByteArray renderSound(SoundEngine::SoundId id)
{
    switch (id) {
        case SoundEngine::ScanStart: {
            // Bubble rising from B5 → A6, body chimes E6 (659.25 Hz).
            const QByteArray drop  = renderDrop(987.77, 1760.0, 0.16,
                                                0.28, 0.06, 4500.0);
            const QByteArray bell  = renderChime(659.25, 0.45, 0.16,
                                                 0.28, 0.18, 4200.0);
            return mixPcm(drop, bell);
        }
        case SoundEngine::ScanComplete: {
            // Two-note pleasant resolution: G5 then C6.  A perfect fourth up,
            // both notes layered with a soft droplet for the "bubbly" feel.
            const QByteArray d1 = renderDrop(440.0, 783.99, 0.12, 0.22, 0.05, 4200.0);
            const QByteArray b1 = renderChime(783.99, 0.55, 0.18, 0.32, 0.20, 4500.0);
            const QByteArray voice1 = mixPcm(d1, b1);

            const QByteArray gap = renderSilence(0.07);

            const QByteArray d2 = renderDrop(587.33, 1046.50, 0.12, 0.22, 0.06, 4500.0);
            const QByteArray b2 = renderChime(1046.50, 0.65, 0.18, 0.42, 0.22, 5000.0);
            const QByteArray voice2 = mixPcm(d2, b2);

            QByteArray pcm;
            pcm.append(voice1);
            pcm.append(gap);
            pcm.append(voice2);
            return pcm;
        }
        case SoundEngine::ScanCancel: {
            // Descending soft drop (A5 → E5) — calmer than a click, no urgency.
            const QByteArray drop = renderDrop(880.0, 523.25, 0.20,
                                               0.16, 0.10, 3200.0);
            const QByteArray bell = renderChime(523.25, 0.35, 0.12,
                                                0.22, 0.18, 3500.0);
            return mixPcm(drop, bell);
        }
        case SoundEngine::Click: {
            // Quick water-drop (E6 → B6), 90 ms total.
            return renderDrop(1318.51, 1975.53, 0.09, 0.18, 0.04, 5000.0);
        }
        case SoundEngine::Error: {
            // Two descending warm chimes (A4 → F4).  Friendly, not buzzy.
            const QByteArray a = renderChime(440.0, 0.30, 0.18, 0.22, 0.18, 2800.0);
            const QByteArray gap = renderSilence(0.05);
            const QByteArray b = renderChime(349.23, 0.40, 0.16, 0.28, 0.18, 2500.0);
            QByteArray pcm;
            pcm.append(a);
            pcm.append(gap);
            pcm.append(b);
            return pcm;
        }
    }
    return {};
}

// Wrap raw mono 16-bit PCM in a minimal RIFF/WAV header.
QByteArray wrapWav(const QByteArray& pcm)
{
    QByteArray out;
    out.reserve(44 + pcm.size());
    auto put32 = [&out](quint32 v) {
        char buf[4];
        qToLittleEndian(v, buf);
        out.append(buf, 4);
    };
    auto put16 = [&out](quint16 v) {
        char buf[2];
        qToLittleEndian(v, buf);
        out.append(buf, 2);
    };
    out.append("RIFF", 4);
    put32(quint32(36 + pcm.size()));
    out.append("WAVE", 4);
    out.append("fmt ", 4);
    put32(16);                    // chunk size
    put16(1);                     // PCM
    put16(1);                     // channels
    put32(kSampleRate);
    put32(kSampleRate * 1 * (kBitsPerSample / 8));  // byte rate
    put16(quint16(1 * (kBitsPerSample / 8)));        // block align
    put16(kBitsPerSample);
    out.append("data", 4);
    put32(quint32(pcm.size()));
    out.append(pcm);
    return out;
}

}  // namespace

SoundEngine::SoundEngine()
{
    loadFromSettings();
}

SoundEngine::~SoundEngine()
{
    qDeleteAll(m_fx);
    m_fx.clear();
    delete m_dir;
}

SoundEngine& SoundEngine::instance()
{
    static SoundEngine inst;
    return inst;
}

void SoundEngine::loadFromSettings()
{
    QSettings s;
    m_enabled = s.value(QStringLiteral("soundsEnabled"), true).toBool();
}

void SoundEngine::setEnabled(bool on)
{
    if (m_enabled == on) return;
    m_enabled = on;
    QSettings s;
    s.setValue(QStringLiteral("soundsEnabled"), on);
}

QString SoundEngine::writeWav(SoundId id)
{
    const QByteArray pcm = renderSound(id);
    if (pcm.isEmpty()) return {};

    const QByteArray wav = wrapWav(pcm);
    const QString path = m_dir->filePath(QStringLiteral("snd_%1.wav").arg(int(id)));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return {};
    f.write(wav);
    f.close();
    return path;
}

void SoundEngine::ensureLoaded()
{
    if (m_loaded) return;
    m_loaded = true;
    m_dir = new QTemporaryDir;
    if (!m_dir->isValid()) return;

    const SoundId all[] = { ScanStart, ScanComplete, ScanCancel, Click, Error };
    for (SoundId id : all) {
        const QString p = writeWav(id);
        if (p.isEmpty()) continue;
        auto* fx = new QSoundEffect(this);
        fx->setSource(QUrl::fromLocalFile(p));
        fx->setVolume(0.45);
        m_fx.insert(int(id), fx);
    }
}

void SoundEngine::play(SoundId id)
{
    if (!m_enabled) return;
    ensureLoaded();
    auto it = m_fx.constFind(int(id));
    if (it == m_fx.constEnd()) return;
    QSoundEffect* fx = it.value();
    if (!fx) return;
    fx->stop();
    fx->play();
}
