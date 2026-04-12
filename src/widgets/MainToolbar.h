#pragma once

#include <QToolBar>

namespace vsa {

// Top toolbar — placeholder actions for open/navigate/playback/zoom/filters.
class MainToolbar : public QToolBar {
    Q_OBJECT
public:
    explicit MainToolbar(QWidget* parent = nullptr);

signals:
    void openFileRequested();
    void previousFrameRequested();
    void nextFrameRequested();
    void playRequested();
    void pauseRequested();
    void zoomInRequested();
    void zoomOutRequested();
};

} // namespace vsa
