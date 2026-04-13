#include "BarChartView.h"

#include "model/MockDataProvider.h"

#include <QGraphicsLineItem>
#include <QMouseEvent>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>

namespace vsa {

BarChartView::BarChartView(MockDataProvider* data, QWidget* parent)
    : QChartView(parent), m_data(data) {
    setRenderHint(QPainter::Antialiasing);
    rebuild();
}

void BarChartView::rebuild() {
    auto* chart = new QChart;
    chart->setTitle(tr("bit allocation;  quant [18 : 28];  psnr y"));
    chart->legend()->setVisible(false);
    chart->setMargins(QMargins(4, 4, 4, 4));

    // Per-frame-type bar sets. Only the value matching the current frame
    // type is populated; the others hold zero. This gives the characteristic
    // I(red) / P(orange) / B(green) look.
    m_iSet = new QBarSet(tr("I"));
    m_pSet = new QBarSet(tr("P"));
    m_bSet = new QBarSet(tr("B"));
    m_iSet->setColor(QColor(231, 76, 60));
    m_pSet->setColor(QColor(243, 156, 18));
    m_bSet->setColor(QColor(46, 204, 113));

    QStringList categories;
    const auto& frames = m_data->frames();
    categories.reserve(frames.size());

    double maxSize = 1.0;
    for (const auto& f : frames) {
        categories << QString::number(f.poc);
        *m_iSet << (f.type == "I" ? f.sizeBytes : 0);
        *m_pSet << (f.type == "P" ? f.sizeBytes : 0);
        *m_bSet << (f.type == "B" ? f.sizeBytes : 0);
        maxSize = std::max<double>(maxSize, f.sizeBytes);
    }

    m_barSeries = new QBarSeries;
    m_barSeries->append(m_iSet);
    m_barSeries->append(m_pSet);
    m_barSeries->append(m_bSet);
    m_barSeries->setLabelsVisible(false);
    chart->addSeries(m_barSeries);

    // QP overlay line
    m_qpLine = new QLineSeries;
    m_qpLine->setColor(QColor(52, 152, 219));
    for (int i = 0; i < frames.size(); ++i) {
        m_qpLine->append(i, frames.at(i).quant);
    }
    chart->addSeries(m_qpLine);

    // Axes
    m_xAxis = new QBarCategoryAxis;
    m_xAxis->append(categories);
    m_xAxis->setLabelsVisible(false);
    chart->addAxis(m_xAxis, Qt::AlignBottom);
    m_barSeries->attachAxis(m_xAxis);

    m_yAxis = new QValueAxis;
    // Show only the bottom quarter of the data range (0 .. maxSize/4) so the
    // tall I-frame spikes are clipped and the small B/P bars become readable.
    m_yAxis->setRange(0, maxSize * 0.25);
    m_yAxis->setLabelFormat(QStringLiteral("%i"));
    chart->addAxis(m_yAxis, Qt::AlignLeft);
    m_barSeries->attachAxis(m_yAxis);

    m_qpAxis = new QValueAxis;
    m_qpAxis->setRange(0, 52);
    m_qpAxis->setLabelsVisible(false);
    chart->addAxis(m_qpAxis, Qt::AlignRight);
    m_qpLine->attachAxis(m_xAxis);
    m_qpLine->attachAxis(m_qpAxis);

    setChart(chart);
}

void BarChartView::setCurrentFrame(int poc) {
    m_currentFrame = poc;
    if (!chart() || m_data->frameCount() == 0) return;
    // Draw the vertical cursor as a scene-coordinate line over the plot.
    if (!m_cursor) {
        m_cursor = new QGraphicsLineItem;
        QPen pen(QColor(0, 0, 0, 180));
        pen.setWidth(2);
        m_cursor->setPen(pen);
        m_cursor->setZValue(100);
        chart()->scene()->addItem(m_cursor);
    }
    if (poc < 0 || poc >= m_data->frameCount()) return;
    // Compute the x position manually from the plot-area width — this avoids
    // mapToValue/mapToPosition quirks with QBarCategoryAxis.
    const QRectF plot = chart()->plotArea();
    const double x = plot.left() + plot.width() *
        ((poc + 0.5) / std::max(1, m_data->frameCount()));
    m_cursor->setLine(x, plot.top(), x, plot.bottom());
}

void BarChartView::mousePressEvent(QMouseEvent* e) {
    if (chart() && m_barSeries && e->button() == Qt::LeftButton) {
        const auto scenePos = mapToScene(e->pos());
        const auto valuePos = chart()->mapToValue(scenePos, m_barSeries);
        int poc = static_cast<int>(std::round(valuePos.x()));
        if (poc >= 0 && poc < m_data->frameCount()) {
            emit frameClicked(poc);
        }
    }
    QChartView::mousePressEvent(e);
}

} // namespace vsa
