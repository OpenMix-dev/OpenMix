#pragma once

#include <QString>
#include <QStringList>

namespace OpenMix {

class Show;

class ProjectFile {
  public:
    static constexpr const char* FILE_EXTENSION = ".omproj";
    static constexpr const char* FILE_FILTER = "OpenMix Projects (*.omproj);;All Files (*)";

    static bool save(const Show* show, const QString& filePath, QString* errorMsg = nullptr);
    static bool load(Show* show, const QString& filePath, QString* errorMsg = nullptr);

    static QStringList recentProjects();
    static void addRecentProject(const QString& filePath);
    static void removeRecentProject(const QString& filePath);
    static void clearRecentProjects();
    static int maxRecentProjects() { return 10; }

  private:
    static QString recentProjectsKey();
};

} // namespace OpenMix
