#pragma once

#include "CodingUnit.h"

#include <QString>
#include <QVector>

namespace vsa {

// Per-frame statistics rendered by the charts and the video preview.
struct FrameData {
    int     poc      = 0;             // Picture order count / display index
    int     streamIdx = 0;            // Stream number (e.g. 49)
    int     displayIdx = 0;           // "display" index shown in charts
    int     temporalId = 0;
    int     layerId    = 0;

    QString type     = QStringLiteral("B"); // "I", "P", "B"

    // Timing (ms)
    qint64  timeMs   = 0;

    int     quant    = 27;  // Average QP
    int     sizeBytes = 3935;
    qint64  offset   = 0x00005b3c;
    int     bitAllocationBytes = 1706032;

    bool    isKey    = false;
    bool    show     = true;

    double  psnrY    = 41.4186;

    // Cumulative / instantaneous bitrate (bps) used by bottom curve
    double  instantBitrate = 2209000.0;
    double  cumulativeBytes = 0.0;

    // Per-frame AreaChart stacked contributions (Figure 8).
    // Values are in bytes or percent — treated as relative weights.
    double  totalSize          = 11613.0;
    double  splitCuFlag        = 0.0;
    double  splitQtFlag        = 0.0;
    double  mttSplitVertFlag   = 0.0;
    double  smttSplitBinFlag   = 0.0;
    double  nonInterFlag       = 0.0;
    double  cuSkipFlag         = 0.0;

    // CTU / CU layout for the frame preview overlay.
    QVector<CodingUnit> codingUnits;
};

} // namespace vsa
