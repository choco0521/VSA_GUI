#pragma once

#include <QtCharts/QChartView>

QT_FORWARD_DECLARE_CLASS(QLineSeries)

namespace vsa {

class MockDataProvider;

// Bottom curve: source / instant bitrate over time, mirroring the
// "source | level/hbr" block in the lower-centre area of Figure 5.
class BitrateCurveView : public QChartView {
    Q_OBJECT
public:
    explicit BitrateCurveView(MockDataProvider* data, QWidget* parent = nullptr);

public slots:
    void setCurrentFrame(int poc);

private:
    void rebuild();

    MockDataProvider* m_data = nullptr;
    QLineSeries* m_bitrate = nullptr;
    QLineSeries* m_source  = nullptr;
    int m_currentFrame = 0;
};

} // namespace vsa
