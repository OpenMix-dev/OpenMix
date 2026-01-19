#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace StageBlend {

constexpr int PROJECT_VERSION_CURRENT = 3;
constexpr int PROJECT_VERSION_MIN_SUPPORTED = 1;

struct MigrationResult {
    bool success;
    int fromVersion;
    int toVersion;
    QStringList warnings;
    QString error;

    bool hasWarnings() const { return !warnings.isEmpty(); }
};

class ProjectMigration {
  public:
    // main entry point: migrate a project JSON to current version
    static MigrationResult migrate(QJsonObject& projectJson);

    // check if migration is needed
    static bool needsMigration(const QJsonObject& projectJson);

    // get the version of a project JSON
    static int projectVersion(const QJsonObject& projectJson);

    // check if a version is supported
    static bool isVersionSupported(int version);

    // get version info for display
    static QString versionString(int version);

    // add version metadata to a project for saving
    static void addVersionMetadata(QJsonObject& projectJson);

  private:
    // individual migration functions
    static bool migrateV1toV2(QJsonObject& json, QStringList& warnings);
    static bool migrateV2toV3(QJsonObject& json, QStringList& warnings);

    // helper functions
    static void migrateCuesV1toV2(QJsonArray& cues, QStringList& warnings);
    static void migrateCuesV2toV3(QJsonArray& cues, QStringList& warnings);
};

} // namespace StageBlend
