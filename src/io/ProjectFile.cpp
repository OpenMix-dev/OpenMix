#include "ProjectFile.h"
#include "core/Show.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>

namespace StageBlend {

bool ProjectFile::save(const Show* show, const QString& filePath, QString* errorMsg) {
    if (!show) {
        if (errorMsg)
            *errorMsg = "No show to save";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMsg)
            *errorMsg = QString("Cannot open file for writing: %1").arg(file.errorString());
        return false;
    }

    QJsonDocument doc(show->toJson());
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    if (file.write(data) == -1) {
        if (errorMsg)
            *errorMsg = QString("Failed to write file: %1").arg(file.errorString());
        return false;
    }

    file.close();
    addRecentProject(filePath);
    return true;
}

bool ProjectFile::load(Show* show, const QString& filePath, QString* errorMsg) {
    if (!show) {
        if (errorMsg)
            *errorMsg = "No show object provided";
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMsg)
            *errorMsg = QString("Cannot open file for reading: %1").arg(file.errorString());
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        if (errorMsg)
            *errorMsg = QString("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }

    if (!doc.isObject()) {
        if (errorMsg)
            *errorMsg = "Invalid project file format";
        return false;
    }

    show->fromJson(doc.object());
    show->setFilePath(filePath);
    show->setModified(false);

    addRecentProject(filePath);
    return true;
}

QString ProjectFile::recentProjectsKey() { return "recentProjects"; }

QStringList ProjectFile::recentProjects() {
    QSettings settings("StageBlend", "StageBlend");
    return settings.value(recentProjectsKey()).toStringList();
}

void ProjectFile::addRecentProject(const QString& filePath) {
    QSettings settings("StageBlend", "StageBlend");
    QStringList recent = settings.value(recentProjectsKey()).toStringList();

    // remove if already exists
    recent.removeAll(filePath);

    // add to front
    recent.prepend(filePath);

    // trim to max size
    while (recent.size() > maxRecentProjects()) {
        recent.removeLast();
    }

    // remove non-existent files
    QStringList validRecent;
    for (const QString& path : recent) {
        if (QFileInfo::exists(path)) {
            validRecent.append(path);
        }
    }

    settings.setValue(recentProjectsKey(), validRecent);
}

void ProjectFile::removeRecentProject(const QString& filePath) {
    QSettings settings("StageBlend", "StageBlend");
    QStringList recent = settings.value(recentProjectsKey()).toStringList();
    recent.removeAll(filePath);
    settings.setValue(recentProjectsKey(), recent);
}

void ProjectFile::clearRecentProjects() {
    QSettings settings("StageBlend", "StageBlend");
    settings.remove(recentProjectsKey());
}

} // namespace StageBlend
