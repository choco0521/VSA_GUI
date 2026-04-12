#include "AreaChartView.h"

#include "model/MockDataProvider.h"

#include <QMouseEvent>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <cmath>

namespace vsa {

namespace {

struct AreaChannel {
    const char* label;
    QColor      color;
    double (*accessor)(const FrameData&);
};

} // namespace

AreaChartView::AreaChartView(MockDataProvider* data, QWidget* parent)
    : QChartView(parent), m_data(data) {
    setRenderHint(QPainter::Antialiasing);
    rebuild();
}

void AreaChartView::rebuild() {
    auto* chart = new QChart;
    chart->setTitle(tr("bit allocation;  quant [31 : 40];  metric"));
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->legend()->setAlignment(Qt::AlignRight);

    const AreaChannel channels[] = {
        {"total_size",             QColor(41, 128, 185),
            [](const FrameData& f) { return f.totalSize; }},
        {"split_cu_flag",          QColor(211, 84, 0),
            [](const FrameData& f) { return f.splitCuFlag; }},
        {"split_qt_flag",          QColor(127, 140, 141),
            [](const FrameData& f) { return f.splitQtFlag; }},
        {"mtt_split_cu_vertical_flag", QColor(241, 196, 15),
            [](const FrameData& f) { return f.mttSplitVertFlag; }},
        {"smtt_split_cu_binary_flag",  QColor(93, 173, 226),
            [](const FrameData& f) { return f.smttSplitBinFlag; }},
        {"non_inter_flag",         QColor(46, 204, 113),
            [](const FrameData& f) { return f.nonInterFlag; }},
        {"cu_skip_flag",           QColor(52, 73, 94),
            [](const FrameData& f) { return f.cuSkipFlag; }},
    };

    const auto& frames = m_data->frames();

    // Build a running baseline so each series is stacked above the previous.
    QVector<double> baseline(frames.size(), 0.0);
    double maxY = 1.0;

    for (const auto& ch : channels) {
        auto* upper = new QLineSeries;
        auto* lower = new QLineSeries;
        for (int i = 0; i < frames.size(); ++i) {
            const double v = ch.accessor(frames.at(i));
            lower->append(i, baseline[i]);
            baseline[i] += v;
            upper->append(i, baseline[i]);
            maxY = std::max(maxY, baseline[i]);
        }
        auto* area = new QAreaSeries(upper, lower);
        area->setName(QString::fromLatin1(ch.label));
        area->setColor(ch.color);
        area->setBorderColor(ch.color.darker(120));
        chart->addSeries(area);
    }

    auto* xAxis = new QValueAxis;
    xAxis->setRange(0, std::max(1, frames.size() - 1));
    xAxis->setLabelFormat(QStringLiteral("%i"));
    chart->addAxis(xAxis, Qt::AlignBottom);

    auto* yAxis = new QValueAxis;
    yAxis->setRange(0, maxY * 1.05);
    yAxis->setLabelFormat(QStringLiteral("%.0f"));
    chart->addAxis(yAxis, Qt::AlignLeft);

    const auto seriesList = chart->series();
    for (auto* s : seriesList) {
        s->attachAxis(xAxis);
        s->attachAxis(yAxis);
    }

    setChart(chart);
}

void AreaChartView::setCurrentFrame(int poc) {
    m_currentFrame = poc;
    // For simplicity we don't draw a vertical cursor here; the bar chart
    // already provides the highlight and both tabs can be visible only one
    // at a time.
}

void AreaChartView::mousePressEvent(QMouseEvent* e) {
    if (chart() && e->button() == Qt::LeftButton) {
        const auto scenePos = mapToScene(e->pos());
        const auto valuePos = chart()->mapToValue(scenePos);
        int poc = static_cast<int>(std::round(valuePos.x()));
        if (poc >= 0 && poc < m_data->frameCount()) {
            emit frameClicked(poc);
        }
    }
    QChartView::mousePressEvent(e);
}

} // namespace vsa
