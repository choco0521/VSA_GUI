#pragma once

#include "model/CodingUnit.h"

#include <QWidget>

namespace vsa {

// Right-top dock — shows the currently selected CU's reference block and
// motion-vector arrows, similar to the small MV widget in Figure 5.
class MotionVectorView : public QWidget {
    Q_OBJECT
public:
    explicit MotionVectorView(QWidget* parent = nullptr);

    void setCodingUnit(const CodingUnit& cu);

protected:
    void paintEvent(QPaintEvent* e) override;

private:
    CodingUnit m_cu;
    bool       m_hasCu = false;
};

} // namespace vsa
