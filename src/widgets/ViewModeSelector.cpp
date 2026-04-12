#include "ViewModeSelector.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QToolButton>

namespace vsa {

ViewModeSelector::ViewModeSelector(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    const struct { const char* label; Mode mode; } entries[] = {
        {"Decoded",    Mode::Decoded},
        {"Predicted",  Mode::Predicted},
        {"Unfiltered", Mode::Unfiltered},
        {"Residual",   Mode::Residual},
        {"Reference",  Mode::Reference},
    };

    int i = 0;
    for (const auto& e : entries) {
        auto* btn = new QToolButton(this);
        btn->setText(tr(e.label));
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setToolButtonStyle(Qt::ToolButtonTextOnly);
        btn->setMinimumWidth(74);
        if (e.mode == Mode::Decoded) btn->setChecked(true);
        m_group->addButton(btn, i++);
        layout->addWidget(btn);
    }
    layout->addStretch(1);

    connect(m_group, &QButtonGroup::idToggled,
            this, [this](int id, bool checked) {
        if (checked) emit modeChanged(id);
    });
}

} // namespace vsa
