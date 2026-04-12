#pragma once

#include <QString>

namespace vsa {

// Top-level metadata displayed in the left "Full stream" tree.
// Mirrors the fields visible in Figure 5 (left-hand column).
struct StreamInfo {
    QString streamType       = QStringLiteral("HEVC/H.265");
    QString profile          = QStringLiteral("Main");
    QString compatibility    = QStringLiteral("Main, Main 10");
    QString levelTier        = QStringLiteral("3.1 / Main");
    QString chromaFormat     = QStringLiteral("4:2:0");
    int     bitDepth         = 8;
    QString resolution       = QStringLiteral("1280 x 720");
    double  frameRate        = 25.0;
    QString declaredBitrate  = QStringLiteral("Undefined");
    QString duration         = QStringLiteral("00:00:10.000");
    QString muxDuration      = QStringLiteral("00:00:10.000");
    QString psnrYUV          = QStringLiteral("43, 331 / 46, 00...");
    int     frameCount       = 250;

    // Frame-type breakdown (I / P / B counts)
    int iFrames = 7;
    int pFrames = 52;
    int bFrames = 191;

    // Aggregate bit-allocation stats (bytes)
    int bitsPerFrameAvg = 53482;
    int bitsPerFrameMax = 2387704;
    int bitsPerFrameMin = 339880;

    // Instant-bitrate stats (bps)
    int instantBitrateAvg = 2209000;
    int instantBitrateMax = 310000;
    int instantBitrateMin = 28;

    // QP stats
    int qpAvg = 25;
    int qpMax = 33;
    int qpMin = 18;

    QString colourPrimaries = QStringLiteral("Image charact...");
};

} // namespace vsa
