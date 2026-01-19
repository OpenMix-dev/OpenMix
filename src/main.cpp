#include "app/Application.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication qtApp(argc, argv);

    // metadata
    QApplication::setApplicationName("StageBlend");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("StageBlend");
    QApplication::setOrganizationDomain("stageblend.org");

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // create app instance
    StageBlend::Application app;
    app.initialize();

    // create & show main window
    StageBlend::MainWindow mainWindow(&app);
    app.setMainWindow(&mainWindow);
    mainWindow.show();

    return qtApp.exec();
}
