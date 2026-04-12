#pragma once

#include <QWidget>

class QButtonGroup;

namespace vsa {

// Horizontal exclusive toggle: Decoded / Predicted / Unfiltered / Residual / Reference.
class ViewModeSelector : public QWidget {
    Q_OBJECT
public:
    enum class Mode {
        Decoded = 0,
        Predicted,
        Unfiltered,
        Residual,
        Reference,
    };

    explicit ViewModeSelector(QWidget* parent = nullptr);

signals:
    void modeChanged(int modeIndex);

private:
    QButtonGroup* m_group = nullptr;
};

} // namespace vsa
