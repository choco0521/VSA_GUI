#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication::setOrganizationName(QStringLiteral("VSA"));
    QApplication::setOrganizationDomain(QStringLiteral("example.com"));
    QApplication::setApplicationName(QStringLiteral("VSA GUI"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QApplication app(argc, argv);

    vsa::MainWindow w;
    w.resize(1600, 1000);
    w.show();

    return app.exec();
}
