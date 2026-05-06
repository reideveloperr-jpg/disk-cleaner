#pragma once

#include <QWidget>

class QTimer;
class QPropertyAnimation;

// Smooth circular spinner — used everywhere instead of a progress bar.
// Has two modes:
//   * Indeterminate (default): a 270° arc rotates around the ring.
//   * Determinate (setProgress called): an arc grows from 0° to 360° as a
//     fraction (0…1). Setting progress switches the widget into determinate
//     mode automatically. Call resetProgress() (or stop()) to go back to
//     indeterminate.
class SpinnerWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgressValue)
public:
    explicit SpinnerWidget(QWidget* parent = nullptr);

    void start();
    void stop();
    bool isRunning() const;

    void setLineWidth(int px);   // ring thickness, default 4
    void setColor(const QColor& c);

    // Determinate mode: 0.0 = empty, 1.0 = full.  Animates smoothly from the
    // current value to the new value.  Switches the widget into determinate
    // mode.
    void setProgress(qreal value);
    qreal progress() const { return m_progress; }
    void resetProgress();        // back to indeterminate spinning

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;

private:
    void setProgressValue(qreal v);  // for QPropertyAnimation

    QTimer* m_timer = nullptr;
    QPropertyAnimation* m_progressAnim = nullptr;
    int m_angle = 0;
    int m_lineWidth = 2;
    QColor m_color;

    bool m_determinate = false;
    qreal m_progress = 0.0;  // 0..1
};
