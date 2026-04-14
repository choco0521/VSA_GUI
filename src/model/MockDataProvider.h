#pragma once

#include "FrameData.h"
#include "StreamInfo.h"

#include <QString>
#include <QVector>

namespace vsa {

// Loads the canned mock data embedded via the Qt resource system
// (`:/sample/mock_stream.json`). If that file is missing or malformed the
// provider falls back to a procedurally generated dataset so the GUI still
// starts up with something meaningful on screen.
class MockDataProvider {
public:
    MockDataProvider();

    void loadFromResource(const QString& resourcePath = QStringLiteral(":/sample/mock_stream.json"));

    // Phase F: replace the internal data wholesale, typically with
    // values produced by H264DataProvider. Views keep their existing
    // pointer to this MockDataProvider instance and re-read through
    // the getters on the next redraw, so MainWindow only needs to
    // retrigger setCurrentFrame(0) after calling these.
    void setStream(const StreamInfo& stream);
    void setFrames(QVector<FrameData> frames);
    void setPictureSize(int width, int height);

    const StreamInfo&            stream() const { return m_stream; }
    const QVector<FrameData>&    frames() const { return m_frames; }
    int                          frameCount() const { return m_frames.size(); }

    // Frame dimensions (used by VideoPreviewWidget)
    int pictureWidth()  const { return m_pictureWidth;  }
    int pictureHeight() const { return m_pictureHeight; }

private:
    void generateFallbackData();

    StreamInfo        m_stream;
    QVector<FrameData> m_frames;

    int m_pictureWidth  = 1280;
    int m_pictureHeight = 720;
};

} // namespace vsa
