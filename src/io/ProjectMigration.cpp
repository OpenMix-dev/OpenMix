#include "ProjectMigration.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>

namespace OpenMix {

MigrationResult ProjectMigration::migrate(QJsonObject& projectJson) {
    MigrationResult result;
    result.success = true;
    result.fromVersion = projectVersion(projectJson);
    result.toVersion = result.fromVersion;

    // check if version is supported
    if (!isVersionSupported(result.fromVersion)) {
        result.success = false;
        result.error = QObject::tr("Project file version %1 is not supported. "
                                   "Minimum supported version is %2.")
                           .arg(result.fromVersion)
                           .arg(PROJECT_VERSION_MIN_SUPPORTED);
        return result;
    }

    // already at current version
    if (result.fromVersion >= PROJECT_VERSION_CURRENT) {
        return result;
    }

    // perform sequential migrations
    int currentVersion = result.fromVersion;

    if (currentVersion == 1) {
        if (!migrateV1toV2(projectJson, result.warnings)) {
            result.success = false;
            result.error = QObject::tr("Failed to migrate from version 1 to 2");
            return result;
        }
        currentVersion = 2;
    }

    if (currentVersion == 2) {
        if (!migrateV2toV3(projectJson, result.warnings)) {
            result.success = false;
            result.error = QObject::tr("Failed to migrate from version 2 to 3");
            return result;
        }
        currentVersion = 3;
    }

    // update version in project
    projectJson["_version"] = PROJECT_VERSION_CURRENT;
    result.toVersion = PROJECT_VERSION_CURRENT;

    return result;
}

bool ProjectMigration::needsMigration(const QJsonObject& projectJson) {
    return projectVersion(projectJson) < PROJECT_VERSION_CURRENT;
}

int ProjectMigration::projectVersion(const QJsonObject& projectJson) {
    // check for explicit version field
    if (projectJson.contains("_version")) {
        return projectJson["_version"].toInt(1);
    }

    // legacy: no version field means version 1
    return 1;
}

bool ProjectMigration::isVersionSupported(int version) {
    return version >= PROJECT_VERSION_MIN_SUPPORTED && version <= PROJECT_VERSION_CURRENT;
}

QString ProjectMigration::versionString(int version) {
    return QObject::tr("Version %1").arg(version);
}

void ProjectMigration::addVersionMetadata(QJsonObject& projectJson) {
    projectJson["_version"] = PROJECT_VERSION_CURRENT;
    projectJson["_app"] = "OpenMix";
    projectJson["_appVersion"] = QCoreApplication::applicationVersion();
    projectJson["_savedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
}

bool ProjectMigration::migrateV1toV2(QJsonObject& json, QStringList& warnings) {
    // v1 to V2 migration:
    // - Add macroExecutionMode to cues (default: Sequential)
    // - Add fadeCurve to cues (default: Linear)

    if (json.contains("cues") && json["cues"].isArray()) {
        QJsonArray cues = json["cues"].toArray();
        migrateCuesV1toV2(cues, warnings);
        json["cues"] = cues;
    }

    warnings.append(
        QObject::tr("Migrated from V1 to V2: Added macro execution mode & fade curves"));
    return true;
}

bool ProjectMigration::migrateV2toV3(QJsonObject& json, QStringList& warnings) {
    // v2 to V3 migration:
    // - Add autoFollowCondition to cues (default: Always)
    // - Add tags array to cues (default: empty)

    if (json.contains("cues") && json["cues"].isArray()) {
        QJsonArray cues = json["cues"].toArray();
        migrateCuesV2toV3(cues, warnings);
        json["cues"] = cues;
    }

    warnings.append(QObject::tr("Migrated from V2 to V3: Added auto-follow conditions & tags"));
    return true;
}

void ProjectMigration::migrateCuesV1toV2(QJsonArray& cues, QStringList& warnings) {
    Q_UNUSED(warnings);

    for (int i = 0; i < cues.size(); ++i) {
        QJsonObject cue = cues[i].toObject();

        // add macroExecutionMode if missing
        if (!cue.contains("macroExecutionMode")) {
            cue["macroExecutionMode"] = "Sequential";
        }

        // add fadeCurve if missing
        if (!cue.contains("fadeCurve")) {
            cue["fadeCurve"] = "Linear";
        }

        cues[i] = cue;
    }
}

void ProjectMigration::migrateCuesV2toV3(QJsonArray& cues, QStringList& warnings) {
    Q_UNUSED(warnings);

    for (int i = 0; i < cues.size(); ++i) {
        QJsonObject cue = cues[i].toObject();

        // add autoFollowCondition if missing
        if (!cue.contains("autoFollowCondition")) {
            cue["autoFollowCondition"] = "Always";
        }

        // add tags array if missing
        if (!cue.contains("tags")) {
            cue["tags"] = QJsonArray();
        }

        cues[i] = cue;
    }
}

} // namespace OpenMix
