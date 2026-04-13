#include "MainWindow.h"

#include "views/BitrateCurveView.h"
#include "views/ChartTabWidget.h"
#include "views/CodingUnitTree.h"
#include "views/MotionVectorView.h"
#include "views/StreamInfoTree.h"
#include "views/VideoPreviewWidget.h"
#include "widgets/MainToolbar.h"
#include "widgets/ViewModeSelector.h"

#include <QDockWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace vsa {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("VSA GUI — /Volumes/4test/test_265.265"));

    setupDataModel();
    setupCentralLayout();
    setupDocks();
    setupToolbarAndMenu();
    setupStatusBar();
    wireSignals();

    // Kick everything off with frame 0.
    setCurrentFrame(0);
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// Setup helpers
// ---------------------------------------------------------------------------

void MainWindow::setupDataModel() {
    // MockDataProvider self-populates in its constructor (from either the
    // embedded resource JSON or a procedurally generated fallback).
    // Nothing to do here yet — kept as a hook for future parser wiring.
}

void MainWindow::setupCentralLayout() {
    // Central column now holds:
    //   Video preview (top, large)
    //   View-mode selector (thin row)
    //   Horizontal split: [source info table | bitrate curve]
    //
    // The chart tabs (BarChart / Thumbnails / AreaChart) are attached to the
    // top dock area in setupDocks() so they span the full window width.
    auto* central = new QWidget(this);
    auto* layout  = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* vSplit = new QSplitter(Qt::Vertical, central);

    m_videoPreview = new VideoPreviewWidget(&m_data, vSplit);

    auto* bottom  = new QWidget(vSplit);
    auto* bottomL = new QVBoxLayout(bottom);
    bottomL->setContentsMargins(0, 0, 0, 0);
    bottomL->setSpacing(0);

    m_viewModeSelector = new ViewModeSelector(bottom);
    bottomL->addWidget(m_viewModeSelector);

    auto* bottomSplit = new QSplitter(Qt::Horizontal, bottom);

    // --- Source info tree (name / value) -----------------------------------
    m_sourceInfoTree = new QTreeView(bottomSplit);
    {
        auto* model = new QStandardItemModel(m_sourceInfoTree);
        model->setHorizontalHeaderLabels({tr("name"), tr("value")});
        auto addRow = [&](const QString& name, const QString& val) {
            auto* n = new QStandardItem(name);
            auto* v = new QStandardItem(val);
            n->setEditable(false);
            v->setEditable(false);
            model->appendRow({n, v});
        };
        addRow(QStringLiteral("source"),          QStringLiteral("level/main"));
        addRow(QStringLiteral("cpb_buffer_size"), QStringLiteral("11 000 000"));
        addRow(QStringLiteral("bitrate"),         QStringLiteral("11 000 000"));
        addRow(QStringLiteral("cbr_flag"),        QStringLiteral("0"));
        m_sourceInfoTree->setModel(model);
        m_sourceInfoTree->setAlternatingRowColors(true);
        m_sourceInfoTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_sourceInfoTree->setRootIsDecorated(false);
        m_sourceInfoTree->header()->setStretchLastSection(true);
        m_sourceInfoTree->setMinimumWidth(180);
    }

    m_bitrateCurve = new BitrateCurveView(&m_data, bottomSplit);

    bottomSplit->addWidget(m_sourceInfoTree);
    bottomSplit->addWidget(m_bitrateCurve);
    bottomSplit->setStretchFactor(0, 1);
    bottomSplit->setStretchFactor(1, 4);
    bottomL->addWidget(bottomSplit, 1);

    vSplit->addWidget(m_videoPreview);
    vSplit->addWidget(bottom);
    vSplit->setStretchFactor(0, 3);
    vSplit->setStretchFactor(1, 1);

    layout->addWidget(vSplit);
    setCentralWidget(central);
}

void MainWindow::setupDocks() {
    // Configure corners so:
    //   - Top dock (charts) spans from the left edge over the Full-stream
    //     dock, but stops at the right dock (Motion Vectors / Block Presenter).
    //   - Right dock (Motion Vectors) extends all the way up to the top of
    //     the window, sitting next to the BarChart on the same row — this
    //     matches the reference StreamEye layout.
    setCorner(Qt::TopLeftCorner,     Qt::TopDockWidgetArea);
    setCorner(Qt::TopRightCorner,    Qt::RightDockWidgetArea);
    setCorner(Qt::BottomLeftCorner,  Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

    // --- Top dock: Chart tabs (BarChart / Thumbnails / AreaChart) -----------
    auto* chartDock = new QDockWidget(tr("Charts"), this);
    chartDock->setObjectName(QStringLiteral("ChartsDock"));
    chartDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    chartDock->setTitleBarWidget(new QWidget(chartDock));   // hide title bar
    m_chartTabs = new ChartTabWidget(&m_data, chartDock);
    m_chartTabs->setMinimumHeight(180);
    chartDock->setWidget(m_chartTabs);
    chartDock->setAllowedAreas(Qt::TopDockWidgetArea);
    addDockWidget(Qt::TopDockWidgetArea, chartDock);

    // --- Left dock: Stream info tree ---
    auto* streamDock = new QDockWidget(tr("Full stream"), this);
    streamDock->setObjectName(QStringLiteral("StreamInfoDock"));
    m_streamInfoTree = new StreamInfoTree(streamDock);
    m_streamInfoTree->setStreamInfo(m_data.stream(), m_data.frames());
    m_streamInfoTree->setMinimumWidth(230);
    streamDock->setWidget(m_streamInfoTree);
    streamDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, streamDock);

    // --- Right (top) dock: Motion-vector viewer ---
    auto* mvDock = new QDockWidget(tr("Motion Vectors"), this);
    mvDock->setObjectName(QStringLiteral("MotionVectorDock"));
    m_mvView = new MotionVectorView(mvDock);
    mvDock->setWidget(m_mvView);
    mvDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, mvDock);

    // --- Right (bottom) dock: Coding-unit info tree ---
    auto* cuDock = new QDockWidget(tr("Coding Unit"), this);
    cuDock->setObjectName(QStringLiteral("CodingUnitDock"));
    m_codingUnitTree = new CodingUnitTree(cuDock);
    cuDock->setWidget(m_codingUnitTree);
    cuDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, cuDock);

    // Stack the two right-side docks vertically.
    splitDockWidget(mvDock, cuDock, Qt::Vertical);
}

void MainWindow::setupToolbarAndMenu() {
    m_mainToolbar = new MainToolbar(this);
    addToolBar(Qt::TopToolBarArea, m_mainToolbar);

    // Minimal menu bar (File / View / Help)
    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&Open…"), this, [] {
        // Placeholder — real parser wiring lives in a later milestone.
    });
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), this, &QWidget::close, QKeySequence::Quit);

    auto* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Previous frame"), this, [this] {
        setCurrentFrame(m_currentFrame - 1);
    }, QKeySequence(Qt::Key_Left));
    viewMenu->addAction(tr("Next frame"), this, [this] {
        setCurrentFrame(m_currentFrame + 1);
    }, QKeySequence(Qt::Key_Right));

    auto* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, [this] {
        // Placeholder
    });
}

void MainWindow::setupStatusBar() {
    auto* sb = statusBar();

    m_statusPathLabel   = new QLabel(QStringLiteral("stop"), sb);
    m_statusPsnrLabel   = new QLabel(QStringLiteral("PSNR (yuv.dec/ref) "), sb);
    m_statusBlockLabel  = new QLabel(QStringLiteral("Blk —"), sb);
    m_statusSubLabel    = new QLabel(QStringLiteral("Sub —"), sb);
    m_statusStrmLabel   = new QLabel(QStringLiteral("Strm —"), sb);
    m_statusDispLabel   = new QLabel(QStringLiteral("Disp —"), sb);
    m_statusTypeLabel   = new QLabel(QStringLiteral("Type —"), sb);
    m_statusSizeLabel   = new QLabel(QStringLiteral("Size —"), sb);
    m_statusOffsetLabel = new QLabel(QStringLiteral("Offset —"), sb);

    auto sep = [sb]() -> QFrame* {
        auto* f = new QFrame(sb);
        f->setFrameShape(QFrame::VLine);
        f->setFrameShadow(QFrame::Sunken);
        return f;
    };

    sb->addWidget(m_statusPathLabel);
    sb->addPermanentWidget(m_statusPsnrLabel);
    sb->addPermanentWidget(sep());
    sb->addPermanentWidget(m_statusBlockLabel);
    sb->addPermanentWidget(m_statusSubLabel);
    sb->addPermanentWidget(sep());
    sb->addPermanentWidget(m_statusStrmLabel);
    sb->addPermanentWidget(m_statusDispLabel);
    sb->addPermanentWidget(sep());
    sb->addPermanentWidget(m_statusTypeLabel);
    sb->addPermanentWidget(m_statusSizeLabel);
    sb->addPermanentWidget(m_statusOffsetLabel);
}

void MainWindow::wireSignals() {
    // MainWindow → child views (broadcast current frame)
    connect(this, &MainWindow::currentFrameChanged,
            m_chartTabs, &ChartTabWidget::setCurrentFrame);
    connect(this, &MainWindow::currentFrameChanged,
            m_videoPreview, &VideoPreviewWidget::setCurrentFrame);
    connect(this, &MainWindow::currentFrameChanged,
            m_bitrateCurve, &BitrateCurveView::setCurrentFrame);
    connect(this, &MainWindow::currentFrameChanged,
            m_streamInfoTree, &StreamInfoTree::onCurrentFrameChanged);

    // Child views → MainWindow (request frame change)
    connect(m_chartTabs, &ChartTabWidget::frameSelected,
            this, &MainWindow::setCurrentFrame);
    connect(m_videoPreview, &VideoPreviewWidget::codingUnitSelected,
            this, &MainWindow::setCurrentCodingUnit);

    // Coding unit selection → right docks
    connect(this, &MainWindow::currentCodingUnitChanged, this, [this](int idx) {
        if (m_currentFrame < 0 || m_currentFrame >= m_data.frameCount()) return;
        const auto& frame = m_data.frames().at(m_currentFrame);
        if (idx < 0 || idx >= frame.codingUnits.size()) return;
        m_codingUnitTree->setCodingUnit(frame.codingUnits.at(idx));
        m_mvView->setCodingUnit(frame.codingUnits.at(idx));
    });

    // Toolbar prev / next buttons
    connect(m_mainToolbar, &MainToolbar::previousFrameRequested, this, [this] {
        setCurrentFrame(m_currentFrame - 1);
    });
    connect(m_mainToolbar, &MainToolbar::nextFrameRequested, this, [this] {
        setCurrentFrame(m_currentFrame + 1);
    });

    // View-mode toggle → video preview overlay
    connect(m_viewModeSelector, &ViewModeSelector::modeChanged, this, [this](int idx) {
        m_videoPreview->setMode(static_cast<VideoPreviewWidget::Mode>(idx));
    });
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void MainWindow::setCurrentFrame(int poc) {
    if (m_data.frameCount() == 0) return;
    poc = std::clamp(poc, 0, m_data.frameCount() - 1);
    if (poc == m_currentFrame) {
        emit currentFrameChanged(poc); // still notify on first call
        refreshStatusBar();
        return;
    }
    m_currentFrame = poc;
    emit currentFrameChanged(m_currentFrame);

    // Auto-select the first CU of the new frame.
    setCurrentCodingUnit(0);
    refreshStatusBar();
}

void MainWindow::setCurrentCodingUnit(int ctuIdx) {
    m_currentCodingUnit = ctuIdx;
    emit currentCodingUnitChanged(m_currentCodingUnit);
}

void MainWindow::refreshStatusBar() {
    if (m_currentFrame < 0 || m_currentFrame >= m_data.frameCount()) return;
    const auto& f = m_data.frames().at(m_currentFrame);
    m_statusPsnrLabel->setText(
        QStringLiteral("PSNR y %1").arg(f.psnrY, 0, 'f', 4));
    m_statusBlockLabel->setText(
        QStringLiteral("Blk %1").arg(f.psnrY, 0, 'f', 4));
    m_statusSubLabel->setText(
        QStringLiteral("Sub %1").arg(f.psnrY, 0, 'f', 4));
    m_statusStrmLabel->setText(QStringLiteral("Strm %1").arg(f.streamIdx));
    m_statusDispLabel->setText(QStringLiteral("Disp %1").arg(f.displayIdx));
    m_statusTypeLabel->setText(QStringLiteral("Type %1").arg(f.type));
    m_statusSizeLabel->setText(QStringLiteral("Size %1").arg(f.sizeBytes));
    m_statusOffsetLabel->setText(
        QStringLiteral("Offset 0x%1").arg(f.offset, 8, 16, QChar('0')));
}

// ---------------------------------------------------------------------------
// Event handling
// ---------------------------------------------------------------------------

void MainWindow::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_Left:
            setCurrentFrame(m_currentFrame - 1);
            break;
        case Qt::Key_Right:
            setCurrentFrame(m_currentFrame + 1);
            break;
        case Qt::Key_Home:
            setCurrentFrame(0);
            break;
        case Qt::Key_End:
            setCurrentFrame(m_data.frameCount() - 1);
            break;
        default:
            QMainWindow::keyPressEvent(e);
    }
}

} // namespace vsa
