#include "Theme.h"
#include <QApplication>
#include <QFile>
#include <QFontDatabase>

namespace OpenMix {
namespace Theme {

// Booth-friendly overrides: pure-white text, brighter borders and gridlines so
// the UI stays legible from a distance in a darkened room.
static QString highContrastOverrides() {
    return QString(R"QSS(
QWidget { color: #ffffff; }
QLabel { color: #ffffff; }
QGroupBox { border: 1px solid #8a93a3; }
QGroupBox::title { color: #ffffff; }
QTableView { gridline-color: #8a93a3; color: #ffffff; }
QHeaderView::section { color: #ffffff; }
QPushButton, QToolButton { border: 1px solid #8a93a3; }
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox { border: 1px solid #8a93a3; color: #ffffff; }
QMenu, QMenuBar { color: #ffffff; }
)QSS");
}

QString globalStylesheet(bool highContrast) {
    QString base;
    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        base = QString::fromUtf8(styleFile.readAll());
    } else {
        base = QString(R"QSS(
* {
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", system-ui, sans-serif;
    font-size: 12px;
}
QMainWindow, QDialog {
    background-color: #18191c;
}
QWidget {
    color: #f0f1f3;
}
)QSS");
    }

    if (highContrast)
        base += highContrastOverrides();
    return base;
}

QColor color(const char* themeColor) { return QColor(themeColor); }

QColor withAlpha(const char* themeColor, int alpha) {
    QColor c(themeColor);
    c.setAlpha(alpha);
    return c;
}

QColor withAlpha(const QColor& color, int alpha) {
    QColor c = color;
    c.setAlpha(alpha);
    return c;
}

QFont monoFont(int size) {
    QStringList monoFonts = {"JetBrains Mono", "Cascadia Code", "SF Mono",         "Consolas",
                             "Monaco",         "Menlo",         "DejaVu Sans Mono"};

    for (const QString& fontName : monoFonts) {
        if (QFontDatabase::families().contains(fontName)) {
            QFont font(fontName);
            font.setPointSize(size);
            font.setStyleHint(QFont::Monospace);
            return font;
        }
    }

    QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    font.setPointSize(size);
    return font;
}

QFont headerFont(int size) {
    QFont font = QApplication::font();
    font.setPointSize(size);
    font.setWeight(QFont::DemiBold);
    return font;
}

QFont uiFont(int size, int weight) {
    QFont font = QApplication::font();
    font.setPointSize(size);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

QString muteButtonStyle(const std::optional<bool>& muted) {
    if (muted.has_value() && *muted) {
        return QString("QPushButton { background-color: %1; color: white; font-weight: bold; }")
            .arg(Colors::AccentRed);
    }
    if (muted.has_value()) {
        return QString("QPushButton { background-color: %1; color: #131416; font-weight: bold; }")
            .arg(Colors::AccentGreen);
    }
    return QString("QPushButton { background-color: %1; color: %2; }")
        .arg(Colors::BgActive)
        .arg(Colors::TextSecondary);
}

} // namespace Theme
} // namespace OpenMix
