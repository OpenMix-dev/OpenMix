#include "app/Application.h"
#include "ui/MainWindow.h"
#include "ui/WelcomeDialog.h"
#include "ui/theme/Theme.h"
#include <QApplication>
#include <QSettings>
#include <QFile>
#include <QPalette>
#include <QAction>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <oclero/qlementine/icons/QlementineIcons.hpp>

namespace OpenMix {

// Show tooltips horizontally centered under the widget (and center multi-line
// text), rather than the platform's offset-to-the-right placement.
class CenteredToolTipFilter : public QObject {
  public:
    using QObject::QObject;
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::ToolTip) {
            auto* widget = qobject_cast<QWidget*>(obj);
            if (widget) {
                // toolbar buttons carry the tooltip on their action, not the widget
                QString tip = widget->toolTip();
                if (tip.isEmpty())
                    if (auto* button = qobject_cast<QToolButton*>(widget))
                        if (QAction* action = button->defaultAction())
                            tip = action->toolTip();

                if (!tip.isEmpty()) {
                    const QString html = "<div align='center'>" +
                                         tip.toHtmlEscaped().replace("\n", "<br>") + "</div>";
                    const int boxWidth =
                        QFontMetrics(QToolTip::font())
                            .boundingRect(QRect(0, 0, 600, 1000), Qt::TextWordWrap, tip)
                            .width() +
                        20; // padding + border
                    const QPoint cursor = static_cast<QHelpEvent*>(event)->globalPos();
                    QToolTip::showText(QPoint(cursor.x() - boxWidth / 2, cursor.y() + 16), html,
                                       widget);
                    return true;
                }
            }
        }
        return QObject::eventFilter(obj, event);
    }
};

// Show tooltips almost immediately and keep them up longer than the platform
// default, so hovering an icon reveals its label without a long wait.
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

int main(int argc, char* argv[]) {
    QApplication qtApp(argc, argv);

    // metadata
    QApplication::setApplicationName("OpenMix");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("OpenMix");

    oclero::qlementine::icons::initializeIconTheme();

    // Fusion base, but show tooltips faster than the platform default
    QApplication::setStyle(new OpenMix::FastTooltipStyle(QStyleFactory::create("Fusion")));
    qtApp.installEventFilter(new OpenMix::CenteredToolTipFilter(&qtApp));

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
