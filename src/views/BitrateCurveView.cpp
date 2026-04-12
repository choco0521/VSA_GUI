#include "BitrateCurveView.h"

#include "model/MockDataProvider.h"

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>

namespace vsa {

BitrateCurveView::BitrateCurveView(MockDataProvider* data, QWidget* parent)
    : QChartView(parent), m_data(data) {
    setRenderHint(QPainter::Antialiasing);
    rebuild();
}

void BitrateCurveView::rebuild() {
    auto* chart = new QChart;
    chart->setTitle(tr("source / instant bitrate"));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->setMargins(QMargins(4, 4, 4, 4));

    m_bitrate = new QLineSeries;
    m_bitrate->setName(tr("bitrate"));
    m_bitrate->setColor(QColor(41, 128, 185));

    m_source = new QLineSeries;
    m_source->setName(tr("source"));
    m_source->setColor(QColor(231, 76, 60));

    const auto& frames = m_data->frames();
    double maxVal = 1.0;
    for (int i = 0; i < frames.size(); ++i) {
        const auto& f = frames.at(i);
        m_bitrate->append(i, f.instantBitrate);
        m_source->append(i,  f.cumulativeBytes);
        maxVal = std::max({maxVal, f.instantBitrate, f.cumulativeBytes});
    }

    chart->addSeries(m_bitrate);
    chart->addSeries(m_source);

    auto* xAxis = new QValueAxis;
    xAxis->setRange(0, std::max<int>(1, static_cast<int>(frames.size()) - 1));
    xAxis->setLabelFormat(QStringLiteral("%i"));
    chart->addAxis(xAxis, Qt::AlignBottom);
    m_bitrate->attachAxis(xAxis);
    m_source->attachAxis(xAxis);

    auto* yAxis = new QValueAxis;
    yAxis->setRange(0, maxVal * 1.05);
    yAxis->setLabelFormat(QStringLiteral("%.0f"));
    chart->addAxis(yAxis, Qt::AlignLeft);
    m_bitrate->attachAxis(yAxis);
    m_source->attachAxis(yAxis);

    setChart(chart);
}

void BitrateCurveView::setCurrentFrame(int poc) {
    m_currentFrame = poc;
    // A vertical cursor could be drawn here; kept lightweight for now.
}

} // namespace vsa
