#pragma once

#include <QtCharts/QChartView>

namespace vsa {

class MockDataProvider;

// Figure 8 — stacked area chart showing the relative contribution of
// split_cu_flag, split_qt_flag, mtt_split_cu_vertical_flag, etc. together
// with total_size.
class AreaChartView : public QChartView {
    Q_OBJECT
public:
    explicit AreaChartView(MockDataProvider* data, QWidget* parent = nullptr);

    void setCurrentFrame(int poc);

signals:
    void frameClicked(int poc);

protected:
    void mousePressEvent(QMouseEvent* e) override;

private:
    void rebuild();

    MockDataProvider* m_data = nullptr;
    int m_currentFrame = 0;
};

} // namespace vsa
