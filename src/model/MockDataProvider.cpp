#include "MockDataProvider.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace vsa {

MockDataProvider::MockDataProvider() {
    loadFromResource();
    if (m_frames.isEmpty()) {
        generateFallbackData();
    }
}

void MockDataProvider::loadFromResource(const QString& resourcePath) {
    QFile f(resourcePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    const auto root = doc.object();

    // --- StreamInfo ---
    const auto s = root.value(QStringLiteral("stream")).toObject();
    if (!s.isEmpty()) {
        m_stream.streamType      = s.value("streamType").toString(m_stream.streamType);
        m_stream.profile         = s.value("profile").toString(m_stream.profile);
        m_stream.compatibility   = s.value("compatibility").toString(m_stream.compatibility);
        m_stream.levelTier       = s.value("levelTier").toString(m_stream.levelTier);
        m_stream.chromaFormat    = s.value("chromaFormat").toString(m_stream.chromaFormat);
        m_stream.bitDepth        = s.value("bitDepth").toInt(m_stream.bitDepth);
        m_stream.resolution      = s.value("resolution").toString(m_stream.resolution);
        m_stream.frameRate       = s.value("frameRate").toDouble(m_stream.frameRate);
        m_stream.declaredBitrate = s.value("declaredBitrate").toString(m_stream.declaredBitrate);
        m_stream.duration        = s.value("duration").toString(m_stream.duration);
        m_stream.muxDuration     = s.value("muxDuration").toString(m_stream.muxDuration);
        m_stream.psnrYUV         = s.value("psnrYUV").toString(m_stream.psnrYUV);
        m_stream.frameCount      = s.value("frameCount").toInt(m_stream.frameCount);
    }

    m_pictureWidth  = root.value("pictureWidth").toInt(m_pictureWidth);
    m_pictureHeight = root.value("pictureHeight").toInt(m_pictureHeight);

    // --- Frames ---
    const auto frames = root.value(QStringLiteral("frames")).toArray();
    for (const auto& v : frames) {
        const auto o = v.toObject();
        FrameData fd;
        fd.poc              = o.value("poc").toInt();
        fd.type             = o.value("type").toString(fd.type);
        fd.timeMs           = static_cast<qint64>(o.value("timeMs").toDouble());
        fd.quant            = o.value("quant").toInt(fd.quant);
        fd.sizeBytes        = o.value("sizeBytes").toInt(fd.sizeBytes);
        fd.psnrY            = o.value("psnrY").toDouble(fd.psnrY);
        fd.instantBitrate   = o.value("instantBitrate").toDouble(fd.instantBitrate);
        fd.totalSize        = o.value("totalSize").toDouble(fd.totalSize);
        fd.splitCuFlag      = o.value("splitCuFlag").toDouble();
        fd.splitQtFlag      = o.value("splitQtFlag").toDouble();
        fd.mttSplitVertFlag = o.value("mttSplitVertFlag").toDouble();
        fd.smttSplitBinFlag = o.value("smttSplitBinFlag").toDouble();
        fd.nonInterFlag     = o.value("nonInterFlag").toDouble();
        fd.cuSkipFlag       = o.value("cuSkipFlag").toDouble();

        // CUs (optional in JSON — generator below fills sensible defaults)
        const auto cus = o.value("codingUnits").toArray();
        for (const auto& cv : cus) {
            const auto co = cv.toObject();
            CodingUnit cu;
            cu.x      = co.value("x").toInt();
            cu.y      = co.value("y").toInt();
            cu.width  = co.value("width").toInt(64);
            cu.height = co.value("height").toInt(64);
            cu.type   = co.value("type").toString(cu.type);
            cu.qp     = co.value("qp").toInt(cu.qp);
            fd.codingUnits.push_back(cu);
        }

        m_frames.push_back(fd);
    }
}

void MockDataProvider::setStream(const StreamInfo& stream) {
    m_stream = stream;
}

void MockDataProvider::setFrames(QVector<FrameData> frames) {
    m_frames = std::move(frames);
}

void MockDataProvider::setPictureSize(int width, int height) {
    if (width > 0)  m_pictureWidth  = width;
    if (height > 0) m_pictureHeight = height;
}

void MockDataProvider::generateFallbackData() {
    // Procedurally generate ~200 frames modelled after the screenshots.
    m_stream = StreamInfo{};
    m_stream.frameCount = 200;

    auto* rng = QRandomGenerator::global();
    double cumulative = 0.0;

    m_frames.reserve(200);
    for (int i = 0; i < 200; ++i) {
        FrameData fd;
        fd.poc = i;
        fd.displayIdx = i;
        fd.streamIdx  = i;
        fd.timeMs = static_cast<qint64>(i * 40); // 25 fps
        // Every 25th frame is an I-frame, others alternate P / B roughly 1:3.
        if (i % 25 == 0) {
            fd.type = QStringLiteral("I");
            fd.sizeBytes = 60000 + rng->bounded(20000);
            fd.quant = 22 + rng->bounded(4);
        } else if ((i % 4) == 1) {
            fd.type = QStringLiteral("P");
            fd.sizeBytes = 12000 + rng->bounded(8000);
            fd.quant = 26 + rng->bounded(3);
        } else {
            fd.type = QStringLiteral("B");
            fd.sizeBytes = 3000 + rng->bounded(4000);
            fd.quant = 28 + rng->bounded(4);
        }

        fd.psnrY = 38.0 + std::sin(i * 0.1) * 2.0 + (rng->bounded(100) / 100.0);
        fd.instantBitrate = 1800000.0 + std::sin(i * 0.07) * 400000.0
                                      + rng->bounded(200000);
        cumulative += fd.sizeBytes;
        fd.cumulativeBytes = cumulative;

        fd.totalSize        = fd.sizeBytes;
        fd.splitCuFlag      = fd.sizeBytes * 0.18;
        fd.splitQtFlag      = fd.sizeBytes * 0.09;
        fd.mttSplitVertFlag = fd.sizeBytes * 0.06;
        fd.smttSplitBinFlag = fd.sizeBytes * 0.04;
        fd.nonInterFlag     = fd.sizeBytes * 0.12;
        fd.cuSkipFlag       = fd.sizeBytes * 0.07;

        // Create a CTU grid matching picture size with per-CU randomised
        // depth / type so the preview overlay looks interesting.
        const int ctu = 64;
        for (int y = 0; y < m_pictureHeight; y += ctu) {
            for (int x = 0; x < m_pictureWidth; x += ctu) {
                CodingUnit cu;
                cu.x = x;
                cu.y = y;
                cu.width  = ctu;
                cu.height = ctu;
                cu.depth  = rng->bounded(4);
                cu.qp     = fd.quant;
                static const char* kTypes[] = {
                    "PART_2Nx2N", "PART_2NxN", "PART_Nx2N", "PART_NxN"
                };
                cu.type = QString::fromLatin1(kTypes[rng->bounded(4)]);

                // One trivial PU per CU with a fake motion vector.
                PredictionUnit pu;
                pu.width  = cu.width;
                pu.height = cu.height;
                MotionVector mv;
                mv.label = QStringLiteral("mvL0");
                mv.vec   = QPointF(rng->bounded(40) - 20,
                                   rng->bounded(40) - 20);
                pu.mvCandidates.push_back(mv);
                cu.predictionUnits.push_back(pu);

                fd.codingUnits.push_back(cu);
            }
        }
        m_frames.push_back(fd);
    }
}

} // namespace vsa
