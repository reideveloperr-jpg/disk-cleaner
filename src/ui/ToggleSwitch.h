#pragma once

#include <QAbstractButton>

class QVariantAnimation;

// Minimalist sliding "toggle switch" — a more tactile replacement for a
// QCheckBox. Emits the same toggled() signal so we can drop it in.
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;

    // Optional descriptive text shown to the right of the pill.
    // (Forwards to QAbstractButton::setText so accessibility works too.)
    void setLabel(const QString& s);

    // Set the checked state without animating and without emitting toggled().
    // Useful when restoring state from settings.
    void setCheckedSilent(bool on);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    qreal m_progress = 0.0;  // 0 = off, 1 = on
    QVariantAnimation* m_anim = nullptr;
    bool m_suppressAnim = false;

    static constexpr int kPillWidth  = 42;
    static constexpr int kPillHeight = 22;
    static constexpr int kHandlePad  = 3;
    static constexpr int kSpacing    = 12;
};
