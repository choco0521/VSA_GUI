#pragma once

#include "model/MockDataProvider.h"

#include <QLabel>
#include <QMainWindow>

class QTreeView;

namespace vsa {

class StreamInfoTree;
class CodingUnitTree;
class ChartTabWidget;
class VideoPreviewWidget;
class BitrateCurveView;
class ViewModeSelector;
class MotionVectorView;
class MainToolbar;

// Top-level window that assembles all docks, toolbars and the central splitter.
// All per-view widgets are owned via Qt parent/child relationships.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

public slots:
    // Broadcast the new current frame to every connected view.
    void setCurrentFrame(int poc);
    // Update all CU-dependent views when the user clicks a block in the preview.
    void setCurrentCodingUnit(int ctuIdx);

signals:
    void currentFrameChanged(int poc);
    void currentCodingUnitChanged(int ctuIdx);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    void setupDataModel();
    void setupCentralLayout();
    void setupDocks();
    void setupBottomDocks();
    void setupToolbarAndMenu();
    void setupStatusBar();
    void wireSignals();
    void refreshStatusBar();

    MockDataProvider  m_data;

    // Central widgets
    ChartTabWidget*     m_chartTabs       = nullptr;
    VideoPreviewWidget* m_videoPreview    = nullptr;
    BitrateCurveView*   m_bitrateCurve    = nullptr;
    ViewModeSelector*   m_viewModeSelector = nullptr;
    QTreeView*          m_sourceInfoTree  = nullptr;

    // Dock widgets
    StreamInfoTree*   m_streamInfoTree = nullptr;
    CodingUnitTree*   m_codingUnitTree = nullptr;
    MotionVectorView* m_mvView         = nullptr;

    // Toolbar
    MainToolbar* m_mainToolbar = nullptr;

    // Status bar labels
    QLabel* m_statusPathLabel   = nullptr;
    QLabel* m_statusPsnrLabel   = nullptr;
    QLabel* m_statusBlockLabel  = nullptr;
    QLabel* m_statusSubLabel    = nullptr;
    QLabel* m_statusStrmLabel   = nullptr;
    QLabel* m_statusDispLabel   = nullptr;
    QLabel* m_statusTypeLabel   = nullptr;
    QLabel* m_statusSizeLabel   = nullptr;
    QLabel* m_statusOffsetLabel = nullptr;

    int m_currentFrame      = -1;
    int m_currentCodingUnit = -1;
};

} // namespace vsa
