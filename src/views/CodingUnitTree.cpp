#include "CodingUnitTree.h"

#include <QHeaderView>
#include <QStandardItem>
#include <QStandardItemModel>

namespace vsa {

CodingUnitTree::CodingUnitTree(QWidget* parent) : QTreeView(parent) {
    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({tr("name"), tr("value")});
    setModel(m_model);
    setAlternatingRowColors(true);
    setUniformRowHeights(true);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    header()->setStretchLastSection(true);
}

QStandardItem* CodingUnitTree::appendRow(QStandardItem* parent,
                                         const QString& name,
                                         const QString& value) {
    auto* n = new QStandardItem(name);
    auto* v = new QStandardItem(value);
    n->setEditable(false);
    v->setEditable(false);
    if (parent) parent->appendRow({n, v});
    else        m_model->appendRow({n, v});
    return n;
}

void CodingUnitTree::setCodingUnit(const CodingUnit& cu) {
    m_model->removeRows(0, m_model->rowCount());

    auto* loc = appendRow(nullptr, tr("location / idx"),
        QStringLiteral("%1 / %2 / %3").arg(cu.x).arg(cu.y).arg(cu.ctuIdx));
    appendRow(loc, tr("slice idx"), QString::number(cu.sliceIdx));
    appendRow(loc, tr("tile idx"),  QString::number(cu.tileIdx));

    auto* size = appendRow(nullptr, tr("size"), QString{});
    appendRow(size, tr("ctu"),        cu.cuSize);
    appendRow(size, tr("prediction"), cu.predictionSize);
    appendRow(size, tr("transform"),  cu.transformSize);

    auto* coding = appendRow(nullptr, tr("coding unit"), QString{});
    appendRow(coding, tr("location"),
              QStringLiteral("%1 / %2 x %3").arg(cu.x).arg(cu.width).arg(cu.height));
    appendRow(coding, tr("type"),  cu.type);
    appendRow(coding, tr("depth"), QString::number(cu.depth));

    auto* dims = appendRow(coding, tr("dimensions"),
        QStringLiteral("%1 x %2").arg(cu.width).arg(cu.height));
    appendRow(dims, tr("qp"), QString::number(cu.qp));

    // Prediction units + MV candidates
    auto* puRoot = appendRow(nullptr, tr("prediction unit"),
                             QString::number(cu.predictionUnits.size()));
    for (int i = 0; i < cu.predictionUnits.size(); ++i) {
        const auto& pu = cu.predictionUnits.at(i);
        auto* puItem = appendRow(puRoot,
            tr("pu[%1]").arg(i),
            QStringLiteral("%1 x %2").arg(pu.width).arg(pu.height));
        appendRow(puItem, tr("merge_flag"), QString::number(pu.mergeFlag));
        appendRow(puItem, tr("merge_idx"),  QString::number(pu.mergeIdx));
        appendRow(puItem, tr("mvp_l1_flag"), QString::number(pu.mvpL1Flag));
        appendRow(puItem, tr("inter type"),  pu.interType);

        auto* cand = appendRow(puItem, tr("candidates"),
                               QString::number(pu.mvCandidates.size()));
        for (int j = 0; j < pu.mvCandidates.size(); ++j) {
            const auto& mv = pu.mvCandidates.at(j);
            auto* mvItem = appendRow(cand,
                tr("[%1] %2").arg(j).arg(mv.label),
                QStringLiteral("(%1, %2)")
                    .arg(mv.vec.x(), 0, 'f', 1)
                    .arg(mv.vec.y(), 0, 'f', 1));
            appendRow(mvItem, tr("refIdx"), QString::number(mv.refIdx));
        }
    }

    expandAll();
    resizeColumnToContents(0);
}

} // namespace vsa
