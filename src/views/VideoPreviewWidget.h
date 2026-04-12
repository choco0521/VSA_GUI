#pragma once

#include "model/FrameData.h"

#include <QWidget>

namespace vsa {

class MockDataProvider;

// Center canvas: shows the decoded frame with a CTU / PU grid overlay and
// a hover tooltip panel describing the frame (stream/display/time/size/qp).
class VideoPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoPreviewWidget(MockDataProvider* data, QWidget* parent = nullptr);

    // Overlay mode = bottom toggle (decoded / predicted / unfiltered / residual / reference).
    enum class Mode {
        Decoded,
        Predicted,
        Unfiltered,
        Residual,
        Reference,
    };
    void setMode(Mode m);

public slots:
    void setCurrentFrame(int poc);

signals:
    void codingUnitSelected(int ctuIdx);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    QRect pictureRect() const;
    QRect cuRect(const CodingUnit& cu) const;
    int hitTestCu(const QPoint& pos) const;

    MockDataProvider* m_data = nullptr;
    int  m_currentFrame = 0;
    int  m_hoveredCu    = -1;
    Mode m_mode         = Mode::Decoded;
};

} // namespace vsa
