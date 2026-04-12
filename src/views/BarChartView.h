#pragma once

#include <QtCharts/QChartView>

QT_BEGIN_NAMESPACE
class QGraphicsLineItem;
QT_END_NAMESPACE

QT_FORWARD_DECLARE_CLASS(QBarSeries)
QT_FORWARD_DECLARE_CLASS(QBarSet)
QT_FORWARD_DECLARE_CLASS(QLineSeries)
QT_FORWARD_DECLARE_CLASS(QValueAxis)
QT_FORWARD_DECLARE_CLASS(QBarCategoryAxis)

namespace vsa {

class MockDataProvider;

// Figure 6 — per-frame bit allocation bars (I/P/B colour coded) with a QP
// overlay and an instant-bitrate curve.
class BarChartView : public QChartView {
    Q_OBJECT
public:
    explicit BarChartView(MockDataProvider* data, QWidget* parent = nullptr);

    void setCurrentFrame(int poc);

signals:
    void frameClicked(int poc);

protected:
    void mousePressEvent(QMouseEvent* e) override;

private:
    void rebuild();

    MockDataProvider* m_data = nullptr;

    QBarSeries* m_barSeries = nullptr;
    QBarSet*    m_iSet      = nullptr;
    QBarSet*    m_pSet      = nullptr;
    QBarSet*    m_bSet      = nullptr;
    QLineSeries* m_qpLine   = nullptr;
    QBarCategoryAxis* m_xAxis = nullptr;
    QValueAxis* m_yAxis       = nullptr;
    QValueAxis* m_qpAxis      = nullptr;

    QGraphicsLineItem* m_cursor = nullptr;

    int m_currentFrame = 0;
};

} // namespace vsa
