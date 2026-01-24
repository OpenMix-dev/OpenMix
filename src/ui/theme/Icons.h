#pragma once

#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace OpenMix::Icons {

inline constexpr const char* IconColor = "#d0d0d0";

// load/recolor SVG icons
inline QIcon fromPath(const char* path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QIcon();

    QString svgContent = QString::fromUtf8(file.readAll());
    svgContent.replace("fill=\"#000\"", QString("fill=\"%1\"").arg(IconColor));
    svgContent.replace("fill=\"#000000\"", QString("fill=\"%1\"").arg(IconColor));
    svgContent.replace("fill=\"black\"", QString("fill=\"%1\"").arg(IconColor));

    QSvgRenderer renderer(svgContent.toUtf8());

    QIcon icon;
    for (int size : {16, 24, 32}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        icon.addPixmap(pixmap);
    }
    return icon;
}

inline QIcon fileNew() { return fromPath(":/qlementine/icons/16/document/new.svg"); }
inline QIcon fileOpen() { return fromPath(":/qlementine/icons/16/document/open.svg"); }
inline QIcon fileSave() { return fromPath(":/qlementine/icons/16/action/save.svg"); }
inline QIcon fileSaveAs() { return fromPath(":/qlementine/icons/16/action/save-all.svg"); }
inline QIcon appExit() { return fromPath(":/qlementine/icons/16/action/close.svg"); }

inline QIcon editUndo() { return fromPath(":/qlementine/icons/16/action/undo.svg"); }
inline QIcon editRedo() { return fromPath(":/qlementine/icons/16/action/redo.svg"); }
inline QIcon listAdd() { return fromPath(":/qlementine/icons/16/action/plus.svg"); }
inline QIcon listRemove() { return fromPath(":/qlementine/icons/16/action/trash.svg"); }
inline QIcon editClear() { return fromPath(":/qlementine/icons/16/action/clear.svg"); }

inline QIcon mediaPlay() { return fromPath(":/qlementine/icons/16/media/play.svg"); }
inline QIcon mediaStop() { return fromPath(":/qlementine/icons/16/media/stop.svg"); }
inline QIcon mediaPrevious() { return fromPath(":/qlementine/icons/16/navigation/chevron-up.svg"); }
inline QIcon mediaNext() { return fromPath(":/qlementine/icons/16/navigation/chevron-down.svg"); }
inline QIcon warning() { return fromPath(":/qlementine/icons/16/misc/warning.svg"); }

inline QIcon sliders() { return fromPath(":/qlementine/icons/16/navigation/sliders-vertical.svg"); }
inline QIcon audioVolume() { return fromPath(":/qlementine/icons/16/audio/speaker-2.svg"); }
inline QIcon network() { return fromPath(":/qlementine/icons/16/hardware/network.svg"); }
inline QIcon refresh() { return fromPath(":/qlementine/icons/16/action/refresh.svg"); }
inline QIcon disconnect() { return fromPath(":/qlementine/icons/16/action/link-break.svg"); }
inline QIcon download() { return fromPath(":/qlementine/icons/16/action/download.svg"); }
inline QIcon upload() { return fromPath(":/qlementine/icons/16/document/open.svg"); }
inline QIcon helpAbout() { return fromPath(":/qlementine/icons/16/misc/help.svg"); }

} // namespace OpenMix::Icons
