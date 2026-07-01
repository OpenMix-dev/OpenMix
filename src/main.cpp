#include "app/Application.h"
#include "ui/MainWindow.h"
#include "ui/WelcomeDialog.h"
#include "ui/theme/Theme.h"
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QTimer>
#include <oclero/qlementine/icons/QlementineIcons.hpp>

int main(int argc, char* argv[]) {
    QApplication qtApp(argc, argv);

    // metadata
    QApplication::setApplicationName("OpenMix");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("OpenMix");

    oclero::qlementine::icons::initializeIconTheme();

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // force dark theme on all platforms
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(0x1a, 0x1a, 0x1a));
    darkPalette.setColor(QPalette::WindowText, QColor(0xe8, 0xe8, 0xe8));
    darkPalette.setColor(QPalette::Base, QColor(0x2e, 0x2e, 0x2e));
    darkPalette.setColor(QPalette::AlternateBase, QColor(0x24, 0x24, 0x24));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(0x2e, 0x2e, 0x2e));
    darkPalette.setColor(QPalette::ToolTipText, QColor(0xe8, 0xe8, 0xe8));
    darkPalette.setColor(QPalette::Text, QColor(0xe8, 0xe8, 0xe8));
    darkPalette.setColor(QPalette::Button, QColor(0x2e, 0x2e, 0x2e));
    darkPalette.setColor(QPalette::ButtonText, QColor(0xe8, 0xe8, 0xe8));
    darkPalette.setColor(QPalette::BrightText, Qt::white);
    darkPalette.setColor(QPalette::Link, QColor(0x5c, 0x9c, 0xe6));
    darkPalette.setColor(QPalette::Highlight, QColor(0x5c, 0x9c, 0xe6));
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(0x66, 0x66, 0x66));
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x66, 0x66, 0x66));
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x66, 0x66, 0x66));
    QApplication::setPalette(darkPalette);

    // apply global stylesheet
    qtApp.setStyleSheet(OpenMix::Theme::globalStylesheet());

    // create app instance
    OpenMix::Application app;
    app.initialize();

    // create & show main window
    OpenMix::MainWindow mainWindow(&app);
    app.setMainWindow(&mainWindow);
    mainWindow.show();
    if (OpenMix::WelcomeDialog::showAtStartup())
        QTimer::singleShot(0, &mainWindow, &OpenMix::MainWindow::showWelcomeDialog);
    else
        QTimer::singleShot(0, &mainWindow, &OpenMix::MainWindow::openConnectionPanel);
    QTimer::singleShot(500, &app, &OpenMix::Application::startupScan);

    return qtApp.exec();
}
