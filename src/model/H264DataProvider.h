#pragma once

#include "FrameData.h"
#include "StreamInfo.h"

#include <QString>
#include <QVector>

namespace vsa {

/*
 * Qt-side adapter around the pure-C vsa_codec library. Given the path
 * to a raw H.264 Annex B elementary stream (e.g. tests/fixtures/
 * tiny_clip.h264, or the output of vsa_extract.sh on any supported
 * container), runs the NAL splitter, parses the first SPS and every
 * slice header, and produces a StreamInfo / QVector<FrameData> pair
 * that MainWindow can drop into its existing MockDataProvider slots.
 *
 * The intent is to share the exact same downstream code path as the
 * mock data: views still talk to a MockDataProvider instance, but
 * the data inside that instance now comes from a real file instead
 * of the embedded JSON fallback. This keeps Phase F focused on the
 * parser → UI bridge and defers any rework of the view APIs.
 *
 * This class is C++ / Qt on the outside and calls vsa_codec via
 * extern "C" on the inside. It owns no Qt widgets; it is a pure
 * data factory.
 */
class H264DataProvider {
public:
    explicit H264DataProvider(const QString& filePath);

    bool isValid() const { return m_valid; }
    const QString& errorMessage() const { return m_error; }

    const StreamInfo&          stream()          const { return m_stream; }
    const QVector<FrameData>&  frames()          const { return m_frames; }
    int                        frameCount()      const { return m_frames.size(); }
    int                        pictureWidth()    const { return m_pictureWidth;  }
    int                        pictureHeight()   const { return m_pictureHeight; }

private:
    void parse(const QString& filePath);

    StreamInfo          m_stream;
    QVector<FrameData>  m_frames;
    int                 m_pictureWidth  = 0;
    int                 m_pictureHeight = 0;

    bool    m_valid = false;
    QString m_error;
};

} // namespace vsa
