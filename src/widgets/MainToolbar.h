#pragma once

#include <QToolBar>

class QLabel;

namespace vsa {

// Top toolbar — placeholder actions for open/navigate/playback/zoom/filters.
class MainToolbar : public QToolBar {
    Q_OBJECT
public:
    explicit MainToolbar(QWidget* parent = nullptr);

    void setFilePath(const QString& path);

signals:
    void openFileRequested();
    void previousFrameRequested();
    void nextFrameRequested();
    void playRequested();
    void pauseRequested();
    void zoomInRequested();
    void zoomOutRequested();

private:
    QLabel* m_pathLabel = nullptr;
};

} // namespace vsa
