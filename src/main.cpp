#include "app/Application.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication qtApp(argc, argv);

    // metadata
    QApplication::setApplicationName("OpenMix");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("OpenMix");
    QApplication::setOrganizationDomain("OpenMix.org");

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // create app instance
    OpenMix::Application app;
    app.initialize();

    // create & show main window
    OpenMix::MainWindow mainWindow(&app);
    app.setMainWindow(&mainWindow);
    mainWindow.show();

    return qtApp.exec();
}
