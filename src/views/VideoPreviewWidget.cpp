#include "VideoPreviewWidget.h"

#include "model/MockDataProvider.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace vsa {

VideoPreviewWidget::VideoPreviewWidget(MockDataProvider* data, QWidget* parent)
    : QWidget(parent), m_data(data) {
    setMinimumHeight(260);
    setMouseTracking(true);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(30, 30, 30));
    setPalette(pal);
}

void VideoPreviewWidget::setCurrentFrame(int poc) {
    m_currentFrame = poc;
    m_hoveredCu = -1;
    update();
}

void VideoPreviewWidget::setMode(Mode m) {
    m_mode = m;
    update();
}

QRect VideoPreviewWidget::pictureRect() const {
    // Fit the picture rect into the widget while preserving aspect ratio.
    const double pw = m_data->pictureWidth();
    const double ph = m_data->pictureHeight();
    const double aspect = pw / std::max(1.0, ph);

    const int margin = 8;
    int maxW = width()  - margin * 2;
    int maxH = height() - margin * 2;

    int w = maxW;
    int h = static_cast<int>(w / aspect);
    if (h > maxH) {
        h = maxH;
        w = static_cast<int>(h * aspect);
    }
    const int x = (width()  - w) / 2;
    const int y = (height() - h) / 2;
    return QRect(x, y, w, h);
}

QRect VideoPreviewWidget::cuRect(const CodingUnit& cu) const {
    const QRect pr = pictureRect();
    const double sx = pr.width()  / static_cast<double>(m_data->pictureWidth());
    const double sy = pr.height() / static_cast<double>(m_data->pictureHeight());
    return QRect(pr.left() + static_cast<int>(cu.x * sx),
                 pr.top()  + static_cast<int>(cu.y * sy),
                 static_cast<int>(cu.width  * sx),
                 static_cast<int>(cu.height * sy));
}

int VideoPreviewWidget::hitTestCu(const QPoint& pos) const {
    if (m_currentFrame < 0 || m_currentFrame >= m_data->frameCount()) return -1;
    const auto& f = m_data->frames().at(m_currentFrame);
    for (int i = 0; i < f.codingUnits.size(); ++i) {
        if (cuRect(f.codingUnits.at(i)).contains(pos)) {
            return i;
        }
    }
    return -1;
}

void VideoPreviewWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor(25, 25, 25));

    const QRect pr = pictureRect();

    // ---- Background gradient standing in for the decoded frame -----------
    QLinearGradient g(pr.topLeft(), pr.bottomRight());
    g.setColorAt(0.0, QColor(80, 110, 60));
    g.setColorAt(0.5, QColor(120, 90, 60));
    g.setColorAt(1.0, QColor(60, 80, 120));
    p.fillRect(pr, g);

    // ---- Mode tint overlay -----------------------------------------------
    switch (m_mode) {
        case Mode::Predicted:  p.fillRect(pr, QColor(0, 0, 255, 30));   break;
        case Mode::Unfiltered: p.fillRect(pr, QColor(255, 255, 0, 25)); break;
        case Mode::Residual:   p.fillRect(pr, QColor(255, 0, 255, 30)); break;
        case Mode::Reference:  p.fillRect(pr, QColor(0, 255, 255, 25)); break;
        case Mode::Decoded:    break;
    }

    // ---- CU / PU grid ----------------------------------------------------
    if (m_currentFrame >= 0 && m_currentFrame < m_data->frameCount()) {
        const auto& f = m_data->frames().at(m_currentFrame);

        QPen cuPen(QColor(255, 255, 255, 110));
        cuPen.setWidth(1);
        p.setPen(cuPen);
        for (const auto& cu : f.codingUnits) {
            p.drawRect(cuRect(cu));
        }

        // Highlight hovered CU.
        if (m_hoveredCu >= 0 && m_hoveredCu < f.codingUnits.size()) {
            QPen hot(QColor(255, 200, 0));
            hot.setWidth(2);
            p.setPen(hot);
            p.drawRect(cuRect(f.codingUnits.at(m_hoveredCu)));
        }

        // ---- Hover tooltip panel -----------------------------------------
        if (m_hoveredCu >= 0) {
            const QString info =
                QStringLiteral(
                    "stream       %1\n"
                    "display      %2\n"
                    "temporal_id  %3\n"
                    "layer_id     %4\n"
                    "type         %5\n"
                    "time         %6\n"
                    "quant        %7\n"
                    "size         %8\n"
                    "offset       0x%9\n"
                    "bit allocation  %10\n"
                    "is key       %11\n"
                    "PSNR         %12")
                .arg(f.streamIdx)
                .arg(f.displayIdx)
                .arg(f.temporalId)
                .arg(f.layerId)
                .arg(f.type)
                .arg(f.timeMs)
                .arg(f.quant)
                .arg(f.sizeBytes)
                .arg(f.offset, 8, 16, QChar('0'))
                .arg(f.bitAllocationBytes)
                .arg(f.isKey ? "yes" : "no")
                .arg(f.psnrY, 0, 'f', 4);

            QFont mono(QStringLiteral("Menlo"));
            mono.setStyleHint(QFont::Monospace);
            mono.setPointSize(9);
            p.setFont(mono);
            QFontMetrics fm(mono);
            const QRect txtRect = fm.boundingRect(
                QRect(0, 0, 260, 300),
                Qt::AlignLeft | Qt::TextWordWrap, info);

            const QRect panel(pr.left() + 6, pr.top() + 6,
                              txtRect.width() + 16, txtRect.height() + 12);
            p.fillRect(panel, QColor(255, 255, 255, 220));
            p.setPen(QColor(30, 30, 30));
            p.drawRect(panel);
            p.drawText(panel.adjusted(8, 6, -8, -6),
                       Qt::AlignLeft | Qt::AlignTop, info);
        }
    }

    // ---- Frame border ----------------------------------------------------
    p.setPen(QColor(200, 200, 200));
    p.drawRect(pr);
}

void VideoPreviewWidget::mouseMoveEvent(QMouseEvent* e) {
    const int idx = hitTestCu(e->pos());
    if (idx != m_hoveredCu) {
        m_hoveredCu = idx;
        update();
    }
}

void VideoPreviewWidget::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const int idx = hitTestCu(e->pos());
    if (idx >= 0) {
        emit codingUnitSelected(idx);
        update();
    }
}

void VideoPreviewWidget::leaveEvent(QEvent*) {
    if (m_hoveredCu != -1) {
        m_hoveredCu = -1;
        update();
    }
}

} // namespace vsa
