#pragma once

#include <QTabWidget>

namespace vsa {

class MockDataProvider;
class BarChartView;
class AreaChartView;
class ThumbnailsView;

// Container for the three center-top charts mimicking the
// "BarChart / Thumbnails / AreaChart" selector in Figure 5.
class ChartTabWidget : public QTabWidget {
    Q_OBJECT
public:
    explicit ChartTabWidget(MockDataProvider* data, QWidget* parent = nullptr);

public slots:
    void setCurrentFrame(int poc);

signals:
    // Emitted when the user clicks a bar / thumbnail / area point.
    void frameSelected(int poc);

private:
    MockDataProvider* m_data = nullptr;

    BarChartView*   m_bar    = nullptr;
    ThumbnailsView* m_thumbs = nullptr;
    AreaChartView*  m_area   = nullptr;
};

} // namespace vsa
