#pragma once

#include <QWidget>

class QTimer;

// Soft animated colour orbs that drift around the main column. Active only
// in the RGB theme. Pointer-transparent so it never steals input.
class RgbGlowOverlay : public QWidget {
    Q_OBJECT
public:
    explicit RgbGlowOverlay(QWidget* parent = nullptr);

    void setActive(bool on);
    bool isActive() const { return m_active; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QTimer* m_timer = nullptr;
    qreal m_t = 0.0;
    bool m_active = false;
};
