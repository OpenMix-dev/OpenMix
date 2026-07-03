#include "app/Application.h"
#include "ui/MainWindow.h"
#include "ui/WelcomeDialog.h"
#include "ui/theme/Theme.h"
#include <QApplication>
#include <QIcon>
#include <QSettings>
#include <QFile>
#include <QPalette>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QTimer>
#include <oclero/qlementine/icons/QlementineIcons.hpp>

namespace OpenMix {

class FastTooltipStyle : public QProxyStyle {
  public:
    using QProxyStyle::QProxyStyle;
    int styleHint(StyleHint hint, const QStyleOption* option = nullptr,
                  const QWidget* widget = nullptr,
                  QStyleHintReturn* returnData = nullptr) const override {
        if (hint == SH_ToolTip_WakeUpDelay)
            return 200;
        if (hint == SH_ToolTip_FallAsleepDelay)
            return 0;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};
} // namespace OpenMix

#if defined(Q_OS_MACOS)
// implemented in MoveToApplications.mm; may relaunch the app and not return
extern "C" void openmix_offer_move_to_applications();
#endif

int main(int argc, char* argv[]) {
    QApplication qtApp(argc, argv);

    // metadata
    QApplication::setApplicationName("OpenMix");
    QApplication::setApplicationVersion(OPENMIX_VERSION);
    QApplication::setOrganizationName("OpenMix");
    QApplication::setWindowIcon(QIcon(":/icons/openmix.png"));

#if defined(Q_OS_MACOS)
    // before any real startup work: Sparkle cannot update an app that runs
    // from Downloads, a DMG, or a translocation path
    openmix_offer_move_to_applications();
#endif

    oclero::qlementine::icons::initializeIconTheme();

    // Fusion base, but show tooltips faster than the platform default
    QApplication::setStyle(new OpenMix::FastTooltipStyle(QStyleFactory::create("Fusion")));

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

    // apply global stylesheet (booth-friendly high-contrast variant is opt-in)
    const bool highContrast = QSettings().value("highContrast", false).toBool();
    qtApp.setStyleSheet(OpenMix::Theme::globalStylesheet(highContrast));

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
