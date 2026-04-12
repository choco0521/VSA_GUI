#pragma once

#include "model/CodingUnit.h"

#include <QTreeView>

class QStandardItemModel;
class QStandardItem;

namespace vsa {

// Right-hand dock showing the hierarchy of the currently selected CU:
// location / size / coding unit / prediction unit / motion-vector candidates.
class CodingUnitTree : public QTreeView {
    Q_OBJECT
public:
    explicit CodingUnitTree(QWidget* parent = nullptr);

    void setCodingUnit(const CodingUnit& cu);

private:
    QStandardItem* appendRow(QStandardItem* parent,
                             const QString& name,
                             const QString& value);

    QStandardItemModel* m_model = nullptr;
};

} // namespace vsa
