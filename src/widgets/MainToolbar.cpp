#include "MainToolbar.h"

#include <QAction>
#include <QFont>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QWidget>

namespace vsa {

namespace {

// Build a small square icon displaying a single glyph/letter on a coloured
// background. Used to populate the toolbar without shipping dozens of SVGs.
QIcon glyphIcon(const QString& glyph,
                const QColor& bg   = QColor(60, 60, 60),
                const QColor& text = QColor(235, 235, 235)) {
    const int size = 20;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(bg);
    p.setPen(QColor(30, 30, 30));
    p.drawRoundedRect(QRect(1, 1, size - 2, size - 2), 3, 3);
    QFont f = p.font();
    f.setPointSize(9);
    f.setBold(true);
    p.setFont(f);
    p.setPen(text);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, glyph);
    return QIcon(pix);
}

QIcon loadOrGlyph(const QString& resourcePath, const QString& glyph,
                  const QColor& bg = QColor(60, 60, 60)) {
    QIcon icon(resourcePath);
    if (!icon.isNull() && !icon.availableSizes().isEmpty()) return icon;
    return glyphIcon(glyph, bg);
}

} // namespace

MainToolbar::MainToolbar(QWidget* parent) : QToolBar(tr("Main"), parent) {
    setObjectName(QStringLiteral("MainToolbar"));
    setMovable(false);
    setIconSize(QSize(20, 20));
    setToolButtonStyle(Qt::ToolButtonIconOnly);

    // --- File group ---------------------------------------------------------
    auto* openAction = addAction(loadOrGlyph(":/icons/open.svg", QStringLiteral("O"),
                                             QColor(52, 120, 180)), tr("Open"));
    connect(openAction, &QAction::triggered, this, &MainToolbar::openFileRequested);

    addAction(glyphIcon(QStringLiteral("S"), QColor(60, 140, 80)), tr("Save"));
    addAction(glyphIcon(QStringLiteral("R"), QColor(60, 140, 80)), tr("Reload"));

    addSeparator();

    // --- Playback / navigation group ---------------------------------------
    addAction(glyphIcon(QStringLiteral("⏮"), QColor(70, 70, 70)), tr("First"));
    addAction(glyphIcon(QStringLiteral("⤺"), QColor(70, 70, 70)), tr("Prev I"));

    auto* prevAction = addAction(loadOrGlyph(":/icons/prev.svg", QStringLiteral("◀"),
                                             QColor(70, 70, 70)), tr("Prev"));
    connect(prevAction, &QAction::triggered, this, &MainToolbar::previousFrameRequested);

    auto* playAction = addAction(loadOrGlyph(":/icons/play.svg", QStringLiteral("▶"),
                                             QColor(46, 140, 70)), tr("Play"));
    connect(playAction, &QAction::triggered, this, &MainToolbar::playRequested);

    auto* pauseAction = addAction(loadOrGlyph(":/icons/pause.svg", QStringLiteral("❚❚"),
                                              QColor(170, 110, 30)), tr("Pause"));
    connect(pauseAction, &QAction::triggered, this, &MainToolbar::pauseRequested);

    addAction(glyphIcon(QStringLiteral("■"), QColor(180, 60, 60)), tr("Stop"));

    auto* nextAction = addAction(loadOrGlyph(":/icons/next.svg", QStringLiteral("▶"),
                                             QColor(70, 70, 70)), tr("Next"));
    connect(nextAction, &QAction::triggered, this, &MainToolbar::nextFrameRequested);

    addAction(glyphIcon(QStringLiteral("⤻"), QColor(70, 70, 70)), tr("Next I"));
    addAction(glyphIcon(QStringLiteral("⏭"), QColor(70, 70, 70)), tr("Last"));

    addSeparator();

    // --- Path display (expandable) -----------------------------------------
    m_pathLabel = new QLabel(QStringLiteral("/Volumes/4test/test_265.265"), this);
    m_pathLabel->setContentsMargins(8, 0, 8, 0);
    m_pathLabel->setStyleSheet(
        QStringLiteral("QLabel { "
                       "  background: #2b2b2b; color: #e8e8e8; "
                       "  border: 1px solid #1d1d1d; padding: 2px 6px; "
                       "  font-family: Menlo, monospace; "
                       "} "));
    addWidget(m_pathLabel);

    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);

    addSeparator();

    // --- Zoom group ---------------------------------------------------------
    addAction(glyphIcon(QStringLiteral("1:1"), QColor(70, 70, 70)), tr("1:1"));
    addAction(glyphIcon(QStringLiteral("Fit"), QColor(70, 70, 70)), tr("Fit"));

    auto* zoomInAction = addAction(loadOrGlyph(":/icons/zoom_in.svg",
                                               QStringLiteral("+"),
                                               QColor(70, 70, 70)), tr("Zoom In"));
    connect(zoomInAction, &QAction::triggered, this, &MainToolbar::zoomInRequested);

    auto* zoomOutAction = addAction(loadOrGlyph(":/icons/zoom_out.svg",
                                                QStringLiteral("−"),
                                                QColor(70, 70, 70)), tr("Zoom Out"));
    connect(zoomOutAction, &QAction::triggered, this, &MainToolbar::zoomOutRequested);

    addAction(glyphIcon(QStringLiteral("×2"), QColor(70, 70, 70)), tr("2x"));

    addSeparator();

    // --- Filter / overlay group --------------------------------------------
    addAction(glyphIcon(QStringLiteral("Q"),  QColor(52, 152, 219)), tr("Show QP"));
    addAction(glyphIcon(QStringLiteral("P"),  QColor(231, 76, 60)),  tr("Show PSNR"));
    addAction(glyphIcon(QStringLiteral("M"),  QColor(155, 89, 182)), tr("Show MV"));
    addAction(glyphIcon(QStringLiteral("C"),  QColor(241, 196, 15)), tr("Show CU"));
    addAction(glyphIcon(QStringLiteral("ƒₓ"), QColor(22, 160, 133)), tr("Filters"));
    addAction(glyphIcon(QStringLiteral("Σ"),  QColor(127, 140, 141)), tr("Stats"));
}

void MainToolbar::setFilePath(const QString& path) {
    if (m_pathLabel) m_pathLabel->setText(path);
}

} // namespace vsa
