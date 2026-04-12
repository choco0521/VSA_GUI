#include "MotionVectorView.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace vsa {

MotionVectorView::MotionVectorView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(220, 200);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(40, 40, 40));
    setAutoFillBackground(true);
    setPalette(pal);
}

void MotionVectorView::setCodingUnit(const CodingUnit& cu) {
    m_cu = cu;
    m_hasCu = true;
    update();
}

void MotionVectorView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int margin = 12;
    const QRect area = rect().adjusted(margin, margin, -margin, -margin);
    if (area.width() <= 0 || area.height() <= 0) return;

    // Reference-block placeholder (simple gradient thumbnail).
    QLinearGradient g(area.topLeft(), area.bottomRight());
    g.setColorAt(0.0, QColor(90, 130, 70));
    g.setColorAt(1.0, QColor(60, 80, 110));
    p.fillRect(area, g);

    // Reference block square at the centre.
    const int blockSize = std::min(area.width(), area.height()) / 2;
    const QRect block(area.center().x() - blockSize / 2,
                      area.center().y() - blockSize / 2,
                      blockSize, blockSize);
    p.setPen(QPen(QColor(255, 255, 255, 220), 2));
    p.drawRect(block);

    if (!m_hasCu) {
        p.setPen(Qt::white);
        p.drawText(area, Qt::AlignCenter, tr("(no CU selected)"));
        return;
    }

    // Draw MV arrows for every candidate of the first PU.
    if (m_cu.predictionUnits.isEmpty()) return;
    const auto& pu = m_cu.predictionUnits.first();

    const QPointF origin = block.center();
    for (const auto& mv : pu.mvCandidates) {
        const QPointF tip = origin + QPointF(mv.vec.x() * 2, mv.vec.y() * 2);
        QPen pen(QColor(46, 204, 113));
        pen.setWidth(2);
        p.setPen(pen);
        p.drawLine(origin, tip);

        // Arrow head
        QPainterPath head;
        const QLineF line(tip, origin);
        const double angle = std::atan2(line.dy(), line.dx());
        const double ah = 6.0;
        head.moveTo(tip);
        head.lineTo(tip + QPointF(std::cos(angle + 0.4) * ah,
                                  std::sin(angle + 0.4) * ah));
        head.lineTo(tip + QPointF(std::cos(angle - 0.4) * ah,
                                  std::sin(angle - 0.4) * ah));
        head.closeSubpath();
        p.fillPath(head, QColor(46, 204, 113));
    }

    // Label the CU location
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPointSize(9);
    p.setFont(f);
    p.drawText(area.adjusted(4, 2, -4, -2),
               Qt::AlignLeft | Qt::AlignTop,
               tr("CU (%1, %2)  %3x%4")
                   .arg(m_cu.x).arg(m_cu.y)
                   .arg(m_cu.width).arg(m_cu.height));
}

} // namespace vsa
