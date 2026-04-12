#include "StreamInfoTree.h"

#include <QHeaderView>
#include <QStandardItem>
#include <QStandardItemModel>

#include <algorithm>

namespace vsa {

StreamInfoTree::StreamInfoTree(QWidget* parent) : QTreeView(parent) {
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({tr("name"), tr("value")});
    setModel(m_model);

    setAlternatingRowColors(true);
    setUniformRowHeights(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setRootIsDecorated(true);
    header()->setSectionResizeMode(QHeaderView::Interactive);
    header()->setStretchLastSection(true);
}

QStandardItem* StreamInfoTree::appendRow(QStandardItem* parent,
                                         const QString& name,
                                         const QString& value) {
    auto* nameItem  = new QStandardItem(name);
    auto* valueItem = new QStandardItem(value);
    nameItem->setEditable(false);
    valueItem->setEditable(false);
    if (parent) {
        parent->appendRow({nameItem, valueItem});
    } else {
        m_model->appendRow({nameItem, valueItem});
    }
    return nameItem;
}

void StreamInfoTree::setStreamInfo(const StreamInfo& s,
                                   const QVector<FrameData>& frames) {
    m_frames = frames;
    m_model->removeRows(0, m_model->rowCount());

    // Top-level codec fields
    appendRow(nullptr, tr("stream type"),       s.streamType);

    auto* profile = appendRow(nullptr, tr("profile"), s.profile);
    appendRow(profile, tr("compatibility"),     s.compatibility);

    appendRow(nullptr, tr("level / tier"),      s.levelTier);
    appendRow(nullptr, tr("chroma format"),     s.chromaFormat);
    appendRow(nullptr, tr("bitdepth"),          QString::number(s.bitDepth));
    appendRow(nullptr, tr("resolution"),        s.resolution);
    appendRow(nullptr, tr("frame rate"),        QString::number(s.frameRate, 'f', 2));
    appendRow(nullptr, tr("declared bitrate"),  s.declaredBitrate);
    appendRow(nullptr, tr("duration"),          s.duration);
    appendRow(nullptr, tr("mux duration"),      s.muxDuration);
    appendRow(nullptr, tr("PSNR(y; u; v)"),     s.psnrYUV);

    // Frame-type breakdown
    auto* frameGroup = appendRow(nullptr, tr("frames"),
                                 QString::number(s.frameCount));
    appendRow(frameGroup, tr("I"), QStringLiteral("%1 (%2%)")
        .arg(s.iFrames).arg(s.iFrames * 100.0 / std::max(1, s.frameCount), 0, 'f', 2));
    appendRow(frameGroup, tr("P"), QStringLiteral("%1 (%2%)")
        .arg(s.pFrames).arg(s.pFrames * 100.0 / std::max(1, s.frameCount), 0, 'f', 2));
    appendRow(frameGroup, tr("B"), QStringLiteral("%1 (%2%)")
        .arg(s.bFrames).arg(s.bFrames * 100.0 / std::max(1, s.frameCount), 0, 'f', 2));

    // Bit allocation (bytes / encoded frame)
    auto* bitAlloc = appendRow(nullptr,
        tr("size (byte) / encode frame"), QString::number(s.bitsPerFrameAvg));
    appendRow(bitAlloc, tr("max"), QString::number(s.bitsPerFrameMax));
    appendRow(bitAlloc, tr("min"), QString::number(s.bitsPerFrameMin));

    // Instant bitrate
    auto* ibr = appendRow(nullptr,
        tr("instant bitrate"), QString::number(s.instantBitrateAvg));
    appendRow(ibr, tr("max"), QString::number(s.instantBitrateMax));
    appendRow(ibr, tr("min"), QString::number(s.instantBitrateMin));

    // QP stats
    auto* qp = appendRow(nullptr, tr("qp"), QString::number(s.qpAvg));
    appendRow(qp, tr("max"), QString::number(s.qpMax));
    appendRow(qp, tr("min"), QString::number(s.qpMin));

    appendRow(nullptr, tr("colour description"), QString{});
    auto* cp = appendRow(nullptr, tr("colour primaries"), s.colourPrimaries);
    Q_UNUSED(cp);

    // Dynamic "current frame" sub-tree (filled by onCurrentFrameChanged).
    m_currentFrameRoot = appendRow(nullptr, tr("current frame"), QString{});

    expandToDepth(1);
    resizeColumnToContents(0);
}

void StreamInfoTree::onCurrentFrameChanged(int poc) {
    if (!m_currentFrameRoot || poc < 0 || poc >= m_frames.size()) return;
    // Clear existing children
    m_currentFrameRoot->removeRows(0, m_currentFrameRoot->rowCount());
    const auto& f = m_frames.at(poc);

    appendRow(m_currentFrameRoot, tr("poc"),    QString::number(f.poc));
    appendRow(m_currentFrameRoot, tr("type"),   f.type);
    appendRow(m_currentFrameRoot, tr("size"),   QString::number(f.sizeBytes));
    appendRow(m_currentFrameRoot, tr("qp"),     QString::number(f.quant));
    appendRow(m_currentFrameRoot, tr("PSNR Y"),
              QString::number(f.psnrY, 'f', 3));

    // Keep the section header value in sync with the current POC.
    if (auto* valueCell = m_model->item(m_currentFrameRoot->row(), 1)) {
        valueCell->setText(QString::number(f.poc));
    }
}

} // namespace vsa
