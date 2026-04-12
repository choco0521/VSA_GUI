#include "ChartTabWidget.h"

#include "AreaChartView.h"
#include "BarChartView.h"
#include "ThumbnailsView.h"
#include "model/MockDataProvider.h"

namespace vsa {

ChartTabWidget::ChartTabWidget(MockDataProvider* data, QWidget* parent)
    : QTabWidget(parent), m_data(data) {
    setTabPosition(QTabWidget::North);
    setDocumentMode(true);

    m_bar    = new BarChartView(data, this);
    m_thumbs = new ThumbnailsView(data, this);
    m_area   = new AreaChartView(data, this);

    addTab(m_bar,    tr("BarChart"));
    addTab(m_thumbs, tr("Thumbnails"));
    addTab(m_area,   tr("AreaChart"));

    // Bubble up click events as frameSelected.
    connect(m_bar,    &BarChartView::frameClicked,
            this,     &ChartTabWidget::frameSelected);
    connect(m_thumbs, &ThumbnailsView::frameClicked,
            this,     &ChartTabWidget::frameSelected);
    connect(m_area,   &AreaChartView::frameClicked,
            this,     &ChartTabWidget::frameSelected);
}

void ChartTabWidget::setCurrentFrame(int poc) {
    m_bar->setCurrentFrame(poc);
    m_thumbs->setCurrentFrame(poc);
    m_area->setCurrentFrame(poc);
}

} // namespace vsa
