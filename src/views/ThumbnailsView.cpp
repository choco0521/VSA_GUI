#include "ThumbnailsView.h"

#include "model/MockDataProvider.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

namespace vsa {

namespace {

// A small clickable cell in the thumbnail strip.
class ThumbCell : public QFrame {
public:
    ThumbCell(int poc, const QString& label,
              const QPixmap& pix, QWidget* parent = nullptr)
        : QFrame(parent), m_poc(poc) {
        setFrameShape(QFrame::Box);
        setLineWidth(1);
        setFixedSize(84, 96);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(2, 2, 2, 2);
        layout->setSpacing(0);

        auto* img = new QLabel(this);
        img->setPixmap(pix);
        img->setFixedSize(80, 48);
        img->setScaledContents(true);

        auto* txt = new QLabel(label, this);
        txt->setAlignment(Qt::AlignHCenter);
        QFont f = txt->font();
        f.setPointSize(9);
        txt->setFont(f);

        layout->addWidget(img);
        layout->addWidget(txt, 1);
    }

    int poc() const { return m_poc; }

    void setCurrent(bool on) {
        m_current = on;
        setStyleSheet(on
            ? QStringLiteral("QFrame { border: 2px solid #2980b9; }")
            : QStringLiteral("QFrame { border: 1px solid #bdc3c7; }"));
    }

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            emitClicked();
        }
        QFrame::mousePressEvent(e);
    }

private:
    void emitClicked() {
        // Walk up to the ThumbnailsView and forward through its public hook.
        QWidget* w = parentWidget();
        while (w) {
            if (auto* tv = qobject_cast<ThumbnailsView*>(w)) {
                tv->emitCellClicked(m_poc);
                return;
            }
            w = w->parentWidget();
        }
    }

    int  m_poc     = 0;
    bool m_current = false;
};

static QPixmap makePlaceholderPixmap(int index, const QString& type) {
    QPixmap pm(80, 48);
    QLinearGradient g(0, 0, 80, 48);
    QColor base;
    if (type == "I") base = QColor(231, 76, 60);
    else if (type == "P") base = QColor(243, 156, 18);
    else                  base = QColor(46, 204, 113);
    g.setColorAt(0.0, base.lighter(130));
    g.setColorAt(1.0, base.darker(130));
    QPainter p(&pm);
    p.fillRect(pm.rect(), g);
    p.setPen(Qt::white);
    p.drawText(pm.rect(), Qt::AlignCenter, QString::number(index));
    return pm;
}

} // namespace

ThumbnailsView::ThumbnailsView(MockDataProvider* data, QWidget* parent)
    : QScrollArea(parent), m_data(data) {
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* host = new QWidget(this);
    m_strip = new QHBoxLayout(host);
    m_strip->setContentsMargins(4, 4, 4, 4);
    m_strip->setSpacing(2);
    host->setLayout(m_strip);
    setWidget(host);

    populate();
}

void ThumbnailsView::populate() {
    const auto& frames = m_data->frames();
    m_cells.reserve(frames.size());
    for (const auto& f : frames) {
        const auto pix = makePlaceholderPixmap(f.poc, f.type);
        auto* cell = new ThumbCell(f.poc,
            QStringLiteral("%1\npoc=%2").arg(f.type).arg(f.poc), pix);
        m_strip->addWidget(cell);
        m_cells.push_back(cell);
    }
    m_strip->addStretch(1);
}

void ThumbnailsView::setCurrentFrame(int poc) {
    if (poc < 0 || poc >= m_cells.size()) return;
    if (m_currentFrame >= 0 && m_currentFrame < m_cells.size()) {
        static_cast<ThumbCell*>(m_cells.at(m_currentFrame))->setCurrent(false);
    }
    static_cast<ThumbCell*>(m_cells.at(poc))->setCurrent(true);
    ensureWidgetVisible(m_cells.at(poc));
    m_currentFrame = poc;
}

} // namespace vsa
