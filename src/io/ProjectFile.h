#pragma once

#include <QString>
#include <QStringList>

namespace StageBlend {

class Show;

class ProjectFile {
  public:
    // file extension
    static constexpr const char* FILE_EXTENSION = ".sbproj";
    static constexpr const char* FILE_FILTER = "StageBlend Projects (*.sbproj);;All Files (*)";

    // save/Load operations
    static bool save(const Show* show, const QString& filePath, QString* errorMsg = nullptr);
    static bool load(Show* show, const QString& filePath, QString* errorMsg = nullptr);

    // recent projects management
    static QStringList recentProjects();
    static void addRecentProject(const QString& filePath);
    static void removeRecentProject(const QString& filePath);
    static void clearRecentProjects();
    static int maxRecentProjects() { return 10; }

  private:
    static QString recentProjectsKey();
};

} // namespace StageBlend
