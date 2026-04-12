#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

namespace vsa {

// A single motion-vector candidate attached to a PU.
struct MotionVector {
    QString label = QStringLiteral("mvL0");
    QPointF vec   = {0.0, 0.0};   // in quarter-pel units
    int refIdx    = 0;
};

// One prediction unit inside a coding unit.
struct PredictionUnit {
    int     width       = 16;
    int     height      = 16;
    int     mergeFlag   = 0;
    int     mergeIdx    = 0;
    int     mvpL1Flag   = 0;
    QString interType   = QStringLiteral("Pred_BI");
    QVector<MotionVector> mvCandidates;
};

// A single CU (coding unit) / CTU cell on the frame preview.
// Dimensions are in pixel units. `x`,`y` refer to the top-left corner
// inside the picture.
struct CodingUnit {
    int x      = 0;
    int y      = 0;
    int width  = 64;
    int height = 64;

    // Index / identifier helpers
    int     ctuIdx = 0;
    int     sliceIdx = 0;
    int     tileIdx  = 0;

    QString type              = QStringLiteral("PART_2Nx2N");
    int     depth             = 0;
    int     qp                = 29;

    // Sub-block sizes — feeds the right dock tree.
    QString cuSize            = QStringLiteral("85");
    QString predictionSize    = QStringLiteral("14");
    QString transformSize     = QStringLiteral("70");

    // One CU may have one or more PUs.
    QVector<PredictionUnit> predictionUnits;
};

} // namespace vsa
