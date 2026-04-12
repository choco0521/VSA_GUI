#pragma once

#include <QScrollArea>
#include <QVector>

class QHBoxLayout;
class QFrame;

namespace vsa {

class MockDataProvider;

// Figure 7 — horizontally scrolling frame-thumbnail strip.
class ThumbnailsView : public QScrollArea {
    Q_OBJECT
public:
    explicit ThumbnailsView(MockDataProvider* data, QWidget* parent = nullptr);

    void setCurrentFrame(int poc);

    // Internal hook used by ThumbCell to re-emit click events. Exposed
    // publicly so the cell, which lives in an anonymous namespace inside
    // the .cpp, can reach it.
    void emitCellClicked(int poc) { emit frameClicked(poc); }

signals:
    void frameClicked(int poc);

private:
    void populate();

    MockDataProvider* m_data = nullptr;
    QHBoxLayout*      m_strip = nullptr;
    QVector<QFrame*>  m_cells;
    int m_currentFrame = 0;
};

} // namespace vsa
