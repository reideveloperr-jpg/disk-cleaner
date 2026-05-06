#pragma once

#include <QWidget>

class QTimer;

// Slim animated rainbow ribbon shown along the bottom of the main window.
// Visible in every theme; in the RGB theme it doubles as a permanent accent.
class RgbStripe : public QWidget {
    Q_OBJECT
public:
    explicit RgbStripe(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;

private:
    QTimer* m_timer = nullptr;
    qreal m_phase = 0.0;
};
