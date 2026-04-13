#include "MainToolbar.h"

#include <QAction>
#include <QFont>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QToolButton>
#include <QWidget>

namespace vsa {

namespace {

// ---------------------------------------------------------------------------
// Icon generation helpers
// ---------------------------------------------------------------------------

// Draw a rounded-rect badge containing the given glyph.
QIcon glyphIcon(const QString& glyph,
                const QColor& bg   = QColor(245, 245, 245),
                const QColor& text = QColor(40, 40, 40),
                int pointSize      = 9) {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(bg);
    p.setPen(QColor(150, 150, 150));
    p.drawRoundedRect(QRect(1, 1, size - 2, size - 2), 3, 3);
    QFont f = p.font();
    f.setPointSize(pointSize);
    f.setBold(true);
    p.setFont(f);
    p.setPen(text);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, glyph);
    return QIcon(pix);
}

// "Panel" icon — outer rectangle with a highlighted sub-rectangle on one side.
QIcon panelIcon(Qt::Edge highlightedEdge) {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect outer(2, 4, size - 4, size - 8);
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(QColor(250, 250, 250));
    p.drawRect(outer);

    QRect hl = outer;
    const int pw = outer.width()  / 3;
    const int ph = outer.height() / 3;
    switch (highlightedEdge) {
        case Qt::LeftEdge:   hl.setWidth(pw); break;
        case Qt::RightEdge:  hl.setLeft(outer.right() - pw); break;
        case Qt::TopEdge:    hl.setHeight(ph); break;
        case Qt::BottomEdge: hl.setTop(outer.bottom() - ph); break;
    }
    p.setBrush(QColor(86, 156, 214));
    p.setPen(Qt::NoPen);
    p.drawRect(hl);
    return QIcon(pix);
}

// Folder icon.
QIcon folderIcon() {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.setBrush(QColor(250, 250, 250));
    QPolygon poly;
    poly << QPoint(2, 7) << QPoint(8, 7) << QPoint(10, 5)
         << QPoint(20, 5) << QPoint(20, 18) << QPoint(2, 18);
    p.drawPolygon(poly);
    return QIcon(pix);
}

// Circular-arrow icon (reopen / refresh).
QIcon refreshIcon() {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(60, 60, 60), 2));
    p.setBrush(Qt::NoBrush);
    p.drawArc(QRect(4, 4, 14, 14), 30 * 16, 270 * 16);
    // small arrow tip
    QPolygon tip;
    tip << QPoint(17, 4) << QPoint(21, 6) << QPoint(17, 9);
    p.setBrush(QColor(60, 60, 60));
    p.setPen(Qt::NoPen);
    p.drawPolygon(tip);
    return QIcon(pix);
}

// File-with-arrow icon (save / export).
QIcon saveIcon() {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(QColor(250, 250, 250));
    p.drawRect(QRect(5, 3, 12, 16));
    p.setPen(QPen(QColor(60, 60, 60), 2));
    p.drawLine(11, 8, 11, 15);
    p.drawLine(11, 15, 8, 12);
    p.drawLine(11, 15, 14, 12);
    return QIcon(pix);
}

// Tower / properties icon — small stacked rectangles.
QIcon towerIcon() {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(QColor(250, 250, 250));
    p.drawRect(QRect(8, 2, 6, 3));
    p.drawRect(QRect(6, 6, 10, 4));
    p.drawRect(QRect(4, 11, 14, 4));
    p.drawRect(QRect(2, 16, 18, 4));
    return QIcon(pix);
}

// Playback control icons.
QIcon playbackIcon(QChar kind) {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(QColor(60, 60, 60));
    const int cy = size / 2;
    if (kind == QChar('|<')) kind = QChar('f'); // sentinel (unused)

    auto triangle = [&](int x1, int x2, bool pointRight) {
        QPolygon tri;
        if (pointRight) {
            tri << QPoint(x1, cy - 5) << QPoint(x2, cy) << QPoint(x1, cy + 5);
        } else {
            tri << QPoint(x2, cy - 5) << QPoint(x1, cy) << QPoint(x2, cy + 5);
        }
        p.drawPolygon(tri);
    };

    switch (kind.unicode()) {
        case 'F': // first  |<
            p.drawRect(QRect(4, cy - 5, 2, 10));
            triangle(7, 16, false);
            break;
        case 'P': // prev   <|
            p.drawRect(QRect(17, cy - 5, 2, 10));
            triangle(6, 15, false);
            break;
        case 'R': // play   >
            triangle(6, 17, true);
            break;
        case 'N': // next   |>
            p.drawRect(QRect(3, cy - 5, 2, 10));
            triangle(7, 16, true);
            break;
        case 'L': // last   >|
            p.drawRect(QRect(16, cy - 5, 2, 10));
            triangle(6, 15, true);
            break;
        case 'X': // fast   >>
            triangle(3, 11, true);
            triangle(11, 19, true);
            break;
    }
    return QIcon(pix);
}

// Grid overlay icon with configurable division.
QIcon gridIcon(int divisions, const QColor& accent = QColor(86, 156, 214)) {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, false);
    const QRect outer(3, 3, size - 6, size - 6);
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(QColor(250, 250, 250));
    p.drawRect(outer);
    p.setPen(QPen(accent, 1));
    for (int i = 1; i < divisions; ++i) {
        const int x = outer.left() + outer.width()  * i / divisions;
        const int y = outer.top()  + outer.height() * i / divisions;
        p.drawLine(x, outer.top(),  x, outer.bottom());
        p.drawLine(outer.left(), y, outer.right(), y);
    }
    return QIcon(pix);
}

// Horizontal bar-chart icon.
QIcon barsIcon() {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(86, 156, 214));
    p.drawRect(QRect(3, 12, 3, 8));
    p.drawRect(QRect(7, 8,  3, 12));
    p.drawRect(QRect(11, 4, 3, 16));
    p.drawRect(QRect(15, 10, 3, 10));
    return QIcon(pix);
}

// Sphere / 3-axis icon (color space).
QIcon sphereIcon() {
    const int size = 22;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.setBrush(QColor(250, 250, 250));
    p.drawEllipse(QRect(3, 3, 16, 16));
    p.drawArc  (QRect(3, 3, 16, 16),   0 * 16, 180 * 16);
    p.drawLine(3, 11, 19, 11);
    p.drawLine(11, 3, 11, 19);
    return QIcon(pix);
}

// Create a QToolButton with a dropdown menu attached. Returns the button so
// the caller can insert it with QToolBar::addWidget().
QToolButton* menuButton(QToolBar* bar, const QIcon& icon, const QString& tip,
                        std::initializer_list<const char*> entries) {
    auto* btn = new QToolButton(bar);
    btn->setIcon(icon);
    btn->setToolTip(tip);
    btn->setAutoRaise(true);
    btn->setPopupMode(QToolButton::MenuButtonPopup);
    auto* m = new QMenu(btn);
    for (const char* e : entries) {
        m->addAction(QString::fromUtf8(e));
    }
    btn->setMenu(m);
    return btn;
}

} // namespace

// ---------------------------------------------------------------------------
// MainToolbar
// ---------------------------------------------------------------------------

MainToolbar::MainToolbar(QWidget* parent) : QToolBar(tr("Main"), parent) {
    setObjectName(QStringLiteral("MainToolbar"));
    setMovable(false);
    setIconSize(QSize(22, 22));
    setToolButtonStyle(Qt::ToolButtonIconOnly);

    // --- Group 1: panel visibility toggles --------------------------------
    auto* leftPanel   = new QAction(panelIcon(Qt::LeftEdge),   tr("Show left panel"),   this);
    auto* bottomPanel = new QAction(panelIcon(Qt::BottomEdge), tr("Show bottom panel"), this);
    auto* rightPanel  = new QAction(panelIcon(Qt::RightEdge),  tr("Show right panel"),  this);
    for (auto* a : {leftPanel, bottomPanel, rightPanel}) {
        a->setCheckable(true);
        a->setChecked(true);
        addAction(a);
    }

    addSeparator();

    // --- Group 2: file operations -----------------------------------------
    addWidget(menuButton(this, folderIcon(),  tr("Open"),
                         {"Open…", "Open Recent…"}));
    connect(actions().last(), &QAction::triggered,
            this, &MainToolbar::openFileRequested);

    addWidget(menuButton(this, refreshIcon(), tr("Reopen / Recent"),
                         {"Reopen", "Clear recent"}));

    addWidget(menuButton(this, saveIcon(),    tr("Save / Export"),
                         {"Save report…", "Export frames…", "Export YUV…"}));

    addWidget(menuButton(this, towerIcon(),   tr("Properties"),
                         {"Stream properties…", "Decoder settings…"}));

    addSeparator();

    // --- Group 3: playback controls ---------------------------------------
    addAction(playbackIcon(QChar('F')), tr("First frame"));
    auto* prevAct = addAction(playbackIcon(QChar('P')), tr("Previous frame"));
    connect(prevAct, &QAction::triggered, this, &MainToolbar::previousFrameRequested);

    auto* playAct = addAction(playbackIcon(QChar('R')), tr("Play"));
    connect(playAct, &QAction::triggered, this, &MainToolbar::playRequested);

    auto* nextAct = addAction(playbackIcon(QChar('N')), tr("Next frame"));
    connect(nextAct, &QAction::triggered, this, &MainToolbar::nextFrameRequested);

    addAction(playbackIcon(QChar('L')), tr("Last frame"));
    addAction(playbackIcon(QChar('X')), tr("Fast forward"));

    addSeparator();

    // --- Group 4: overlay / view modes ------------------------------------
    addWidget(menuButton(this, gridIcon(2, QColor(86, 156, 214)),
                         tr("CTU grid"),
                         {"None", "CTU grid", "Slice grid"}));
    addWidget(menuButton(this, gridIcon(3, QColor(230, 120, 50)),
                         tr("CU grid"),
                         {"None", "CU grid", "Depth colour"}));
    addWidget(menuButton(this, gridIcon(4, QColor(46, 204, 113)),
                         tr("PU grid"),
                         {"None", "PU grid", "Part type"}));
    addWidget(menuButton(this, gridIcon(2, QColor(231, 76, 60)),
                         tr("TU grid"),
                         {"None", "TU grid", "Skip flag"}));

    addSeparator();

    addWidget(menuButton(this, gridIcon(4, QColor(155, 89, 182)),
                         tr("Motion vectors"),
                         {"None", "L0 vectors", "L1 vectors", "Both"}));
    addWidget(menuButton(this, glyphIcon(QStringLiteral("QP"),
                                         QColor(245, 245, 245),
                                         QColor(40, 40, 40)),
                         tr("QP map"),
                         {"None", "QP map", "Delta QP"}));
    addWidget(menuButton(this, glyphIcon(QStringLiteral("PSN"),
                                         QColor(245, 245, 245),
                                         QColor(180, 60, 60), 7),
                         tr("PSNR map"),
                         {"None", "PSNR Y", "PSNR YUV"}));

    addSeparator();

    // --- Group 5: analysis / zoom -----------------------------------------
    addWidget(menuButton(this, barsIcon(),
                         tr("Bit allocation histogram"),
                         {"Bar chart", "Thumbnails", "Area chart"}));
    addWidget(menuButton(this, glyphIcon(QStringLiteral("x2"),
                                         QColor(245, 245, 245),
                                         QColor(40, 40, 40), 8),
                         tr("Zoom factor"),
                         {"1:1", "×2", "×4", "Fit"}));
    connect(actions().last(), &QAction::triggered,
            this, &MainToolbar::zoomInRequested);

    addWidget(menuButton(this, sphereIcon(),
                         tr("Colour space"),
                         {"YCbCr", "YUV", "RGB"}));
}

void MainToolbar::setFilePath(const QString& path) {
    Q_UNUSED(path);
    // The file path is now surfaced via the window title, not the toolbar.
}

} // namespace vsa
