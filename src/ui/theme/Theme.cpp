#include "Theme.h"
#include <QApplication>
#include <QFile>
#include <QFontDatabase>

namespace OpenMix {
namespace Theme {

QString globalStylesheet() {
    QFile styleFile(":/styles/main.qss");
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString::fromUtf8(styleFile.readAll());
    }

    return QString(R"QSS(
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

    QFontDatabase fontDb;
    for (const QString& fontName : monoFonts) {
        if (fontDb.families().contains(fontName)) {
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

} // namespace Theme
} // namespace OpenMix
