#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QSoundEffect;
class QTemporaryDir;

// Tiny sound bank for the UI. Generates a handful of soft sine-tone WAV
// blobs once on first use, then plays them via QSoundEffect. The whole
// system can be muted from settings.
class SoundEngine : public QObject {
    Q_OBJECT
public:
    enum SoundId {
        ScanStart,
        ScanComplete,
        ScanCancel,
        Click,
        Error,
    };

    static SoundEngine& instance();

    // Plays the given sound, but only when sounds are enabled in settings.
    void play(SoundId id);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);

    void loadFromSettings();

private:
    SoundEngine();
    ~SoundEngine() override;

    void ensureLoaded();
    QString writeWav(SoundId id);

    bool m_enabled = true;
    bool m_loaded  = false;
    QTemporaryDir* m_dir = nullptr;
    QHash<int, QSoundEffect*> m_fx;
};
