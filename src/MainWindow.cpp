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
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTabWidget>
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
    //     dock, but stops at the right dock (Block Presenter / Block Info).
    //   - Right dock (Block Presenter) extends up to the top of the window,
    //     sitting next to the BarChart on the same row.
    //   - Bottom dock (Stream Viewer / Hex-DPB-Message-Buffer-Comment)
    //     spans the full window width below every other dock.
    setCorner(Qt::TopLeftCorner,     Qt::TopDockWidgetArea);
    setCorner(Qt::TopRightCorner,    Qt::RightDockWidgetArea);
    setCorner(Qt::BottomLeftCorner,  Qt::BottomDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

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

    // --- Right (top) dock: Block Presenter (motion-vector viewer) ---
    auto* mvDock = new QDockWidget(tr("Block Presenter"), this);
    mvDock->setObjectName(QStringLiteral("BlockPresenterDock"));
    m_mvView = new MotionVectorView(mvDock);
    mvDock->setWidget(m_mvView);
    mvDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, mvDock);

    // --- Right (bottom) dock: Block Info (coding-unit info tree) ---
    auto* cuDock = new QDockWidget(tr("Block Info"), this);
    cuDock->setObjectName(QStringLiteral("BlockInfoDock"));
    m_codingUnitTree = new CodingUnitTree(cuDock);
    cuDock->setWidget(m_codingUnitTree);
    cuDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, cuDock);

    // Stack the two right-side docks vertically.
    splitDockWidget(mvDock, cuDock, Qt::Vertical);

    setupBottomDocks();
}

// Helper used by setupDocks() — creates a monospace QPlainTextEdit preloaded
// with some placeholder text. Used for Hex Viewer / Message / Comment tabs.
static QPlainTextEdit* makeMonoEdit(const QString& initial, QWidget* parent) {
    auto* edit = new QPlainTextEdit(parent);
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono(QStringLiteral("Menlo"));
    mono.setStyleHint(QFont::Monospace);
    mono.setPointSize(9);
    edit->setFont(mono);
    edit->setPlainText(initial);
    return edit;
}

void MainWindow::setupBottomDocks() {
    // --- Bottom-left dock: Stream Viewer -----------------------------------
    // A NAL-unit / syntax-element listing. Implemented as a QTreeView backed
    // by a QStandardItemModel with mock bitstream entries.
    auto* streamDock = new QDockWidget(tr("Stream Viewer"), this);
    streamDock->setObjectName(QStringLiteral("StreamViewerDock"));
    auto* streamTree = new QTreeView(streamDock);
    {
        auto* model = new QStandardItemModel(streamTree);
        model->setHorizontalHeaderLabels(
            {tr("#"), tr("offset"), tr("nal unit type"), tr("size")});

        struct NalRow { int idx; qint64 offset; const char* type; int size; };
        static const NalRow kNals[] = {
            { 0, 0x00000000, "VPS_NUT",                12 },
            { 1, 0x0000000c, "SPS_NUT",                24 },
            { 2, 0x00000024, "PPS_NUT",                 8 },
            { 3, 0x0000002c, "PREFIX_SEI_NUT",        236 },
            { 4, 0x00000118, "IDR_W_RADL (I-slice)", 53482 },
            { 5, 0x0000d1a2, "TRAIL_R (P-slice)",    14017 },
            { 6, 0x00010843, "TRAIL_N (B-slice)",     4258 },
            { 7, 0x000118e5, "TRAIL_N (B-slice)",     3935 },
            { 8, 0x00012840, "TRAIL_R (P-slice)",    14121 },
            { 9, 0x000160d9, "TRAIL_N (B-slice)",     4471 },
            {10, 0x00017230, "TRAIL_N (B-slice)",     3912 },
            {11, 0x00018178, "TRAIL_R (P-slice)",    13887 },
        };
        for (const auto& n : kNals) {
            auto* idx  = new QStandardItem(QString::number(n.idx));
            auto* off  = new QStandardItem(
                QStringLiteral("0x%1").arg(n.offset, 8, 16, QChar('0')));
            auto* type = new QStandardItem(QLatin1String(n.type));
            auto* size = new QStandardItem(QString::number(n.size));
            for (auto* it : {idx, off, type, size}) it->setEditable(false);
            model->appendRow({idx, off, type, size});
        }
        streamTree->setModel(model);
        streamTree->setAlternatingRowColors(true);
        streamTree->setRootIsDecorated(false);
        streamTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        streamTree->header()->setStretchLastSection(true);
        streamTree->setUniformRowHeights(true);
    }
    streamDock->setWidget(streamTree);
    streamDock->setMinimumHeight(140);
    addDockWidget(Qt::BottomDockWidgetArea, streamDock);

    // --- Bottom-right dock: Hex Viewer / DPB / Message / Buffer / Comment ---
    auto* bitstreamDock = new QDockWidget(tr("Bitstream"), this);
    bitstreamDock->setObjectName(QStringLiteral("BitstreamDock"));
    auto* tabs = new QTabWidget(bitstreamDock);
    tabs->setDocumentMode(true);
    tabs->setTabPosition(QTabWidget::South);

    // Hex Viewer tab -------------------------------------------------------
    const QString hexDump = QStringLiteral(
        "00000000  00 00 00 01 40 01 0c 01  ff ff 01 60 00 00 03 00  ....@......`....\n"
        "00000010  80 00 00 03 00 00 03 00  99 ac 0c 00 00 00 01 42  ...............B\n"
        "00000020  01 01 01 60 00 00 03 00  80 00 00 03 00 00 03 00  ...`............\n"
        "00000030  99 a0 02 80 80 2d 16 59  59 a4 93 2b 9a 80 80 80  .....-.YY..+....\n"
        "00000040  81 00 00 00 01 44 01 c1  72 b4 62 40 00 00 00 01  .....D..r.b@....\n"
        "00000050  4e 01 05 0a 72 cb f3 96  71 80 12 34 56 78 9a bc  N...r...q..4Vx..\n"
        "00000060  de f0 80 00 00 00 01 26  01 af 04 9c 26 e1 3f 74  .......&....&.?t\n"
        "00000070  7e 12 c8 24 19 0d a7 18  c0 00 00 00 00 00 00 00  ~..$............\n"
        "00000080  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  ................\n"
        "00000090  ...");
    tabs->addTab(makeMonoEdit(hexDump, tabs), tr("Hex Viewer"));

    // DPB tab --------------------------------------------------------------
    auto* dpbTree = new QTreeView(tabs);
    {
        auto* model = new QStandardItemModel(dpbTree);
        model->setHorizontalHeaderLabels(
            {tr("slot"), tr("poc"), tr("type"), tr("is_ref"), tr("used_for")});
        struct DpbRow { int slot; int poc; const char* type; const char* ref;
                        const char* used; };
        static const DpbRow kDpb[] = {
            {0, 48, "P",      "yes", "short-term"},
            {1, 44, "P",      "yes", "short-term"},
            {2, 40, "I",      "yes", "long-term" },
            {3, 49, "B",      "no",  "output"    },
            {4, 50, "B",      "no",  "output"    },
            {5, 51, "B",      "no",  "(empty)"   },
            {6, 52, "(free)", "-",   "-"         },
            {7, 53, "(free)", "-",   "-"         },
        };
        for (const auto& r : kDpb) {
            auto* s = new QStandardItem(QString::number(r.slot));
            auto* p = new QStandardItem(QString::number(r.poc));
            auto* t = new QStandardItem(QLatin1String(r.type));
            auto* i = new QStandardItem(QLatin1String(r.ref));
            auto* u = new QStandardItem(QLatin1String(r.used));
            for (auto* it : {s, p, t, i, u}) it->setEditable(false);
            model->appendRow({s, p, t, i, u});
        }
        dpbTree->setModel(model);
        dpbTree->setAlternatingRowColors(true);
        dpbTree->setRootIsDecorated(false);
        dpbTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        dpbTree->header()->setStretchLastSection(true);
        dpbTree->setUniformRowHeights(true);
    }
    tabs->addTab(dpbTree, tr("DPB"));

    // Message tab ----------------------------------------------------------
    const QString msg = QStringLiteral(
        "[info]  Stream opened: /Volumes/4test/test_265.265\n"
        "[info]  Container: raw HEVC annex-B\n"
        "[info]  VPS id=0 max_sub_layers=1 profile=Main level=3.1\n"
        "[info]  SPS id=0 chroma_format=4:2:0 bit_depth=8 1280x720\n"
        "[info]  PPS id=0 entropy_coding=CABAC\n"
        "[warn] picture 157: CRA with no preceding IDR (handled)\n"
        "[info]  decode finished: 250 frames, 10.000 s\n"
        "[info]  PSNR(y) avg 43.331 dB");
    tabs->addTab(makeMonoEdit(msg, tabs), tr("Message"));

    // Buffer tab -----------------------------------------------------------
    const QString buf = QStringLiteral(
        "cpb_buffer_size    : 11 000 000 bits\n"
        "initial_cpb_removal: 90000 (1.00 s)\n"
        "cpb_fullness       : 7 842 110 bits  (71.3 %)\n"
        "dpb_max_num_reorder: 2\n"
        "dpb_max_latency    : 3\n"
        "hrd_conformance    : ok\n");
    tabs->addTab(makeMonoEdit(buf, tabs), tr("Buffer"));

    // Comment tab ----------------------------------------------------------
    auto* commentEdit = makeMonoEdit(QString{}, tabs);
    commentEdit->setReadOnly(false);
    commentEdit->setPlaceholderText(tr("Type a comment for this bitstream…"));
    tabs->addTab(commentEdit, tr("Comment"));

    bitstreamDock->setWidget(tabs);
    bitstreamDock->setMinimumHeight(140);
    addDockWidget(Qt::BottomDockWidgetArea, bitstreamDock);

    // Place the two bottom docks side-by-side.
    splitDockWidget(streamDock, bitstreamDock, Qt::Horizontal);
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
