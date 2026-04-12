#pragma once

#include "model/FrameData.h"
#include "model/StreamInfo.h"

#include <QTreeView>
#include <QVector>

class QStandardItemModel;
class QStandardItem;

namespace vsa {

// Left-side "Full stream" tree showing codec metadata, frame-type breakdown
// and bit-allocation / QP statistics — mirrors the column in Figure 5.
class StreamInfoTree : public QTreeView {
    Q_OBJECT
public:
    explicit StreamInfoTree(QWidget* parent = nullptr);

    void setStreamInfo(const StreamInfo& s, const QVector<FrameData>& frames);

public slots:
    void onCurrentFrameChanged(int poc);

private:
    QStandardItem* appendRow(QStandardItem* parent,
                             const QString& name,
                             const QString& value);

    QStandardItemModel* m_model = nullptr;
    QStandardItem*      m_currentFrameRoot = nullptr;
    QVector<FrameData>  m_frames;
};

} // namespace vsa
