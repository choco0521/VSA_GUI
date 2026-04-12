#include "MainToolbar.h"

#include <QAction>
#include <QIcon>

namespace vsa {

namespace {

QIcon loadIcon(const QString& resourcePath) {
    // Resource paths live under `:/icons/...`. Fall back to an empty icon if
    // the SVG cannot be loaded so the toolbar still renders labels.
    QIcon icon(resourcePath);
    if (icon.isNull()) {
        icon = QIcon::fromTheme(QStringLiteral("document-open"));
    }
    return icon;
}

} // namespace

MainToolbar::MainToolbar(QWidget* parent) : QToolBar(tr("Main"), parent) {
    setObjectName(QStringLiteral("MainToolbar"));
    setMovable(false);
    setIconSize(QSize(20, 20));

    auto* openAction = addAction(loadIcon(":/icons/open.svg"), tr("Open"));
    connect(openAction, &QAction::triggered, this, &MainToolbar::openFileRequested);
    addSeparator();

    auto* prevAction = addAction(loadIcon(":/icons/prev.svg"), tr("Prev"));
    connect(prevAction, &QAction::triggered, this, &MainToolbar::previousFrameRequested);

    auto* playAction = addAction(loadIcon(":/icons/play.svg"), tr("Play"));
    connect(playAction, &QAction::triggered, this, &MainToolbar::playRequested);

    auto* pauseAction = addAction(loadIcon(":/icons/pause.svg"), tr("Pause"));
    connect(pauseAction, &QAction::triggered, this, &MainToolbar::pauseRequested);

    auto* nextAction = addAction(loadIcon(":/icons/next.svg"), tr("Next"));
    connect(nextAction, &QAction::triggered, this, &MainToolbar::nextFrameRequested);

    addSeparator();

    auto* zoomInAction = addAction(loadIcon(":/icons/zoom_in.svg"), tr("Zoom In"));
    connect(zoomInAction, &QAction::triggered, this, &MainToolbar::zoomInRequested);

    auto* zoomOutAction = addAction(loadIcon(":/icons/zoom_out.svg"), tr("Zoom Out"));
    connect(zoomOutAction, &QAction::triggered, this, &MainToolbar::zoomOutRequested);
}

} // namespace vsa
