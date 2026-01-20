#include "PlaybackLogger.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QTextStream>

namespace OpenMix {

QString PlaybackLogEntry::typeString() const {
    switch (type) {
    case CueExecuted:
        return "CueExecuted";
    case FadeStarted:
        return "FadeStarted";
    case FadeCompleted:
        return "FadeCompleted";
    case MacroExpanded:
        return "MacroExpanded";
    case AutoFollowTriggered:
        return "AutoFollowTriggered";
    case ValidationWarning:
        return "ValidationWarning";
    case ValidationError:
        return "ValidationError";
    case EmergencyStop:
        return "EmergencyStop";
    case GoLockout:
        return "GoLockout";
    case ParameterSent:
        return "ParameterSent";
    case StateCapture:
        return "StateCapture";
    case Custom:
        return "Custom";
    }
    return "Unknown";
}

QJsonObject PlaybackLogEntry::toJson() const {
    QJsonObject obj;
    obj["timestamp"] = timestamp.toString(Qt::ISODateWithMs);
    obj["type"] = typeString();
    obj["cueId"] = cueId;
    obj["details"] = details;
    if (!parameters.isEmpty()) {
        obj["parameters"] = parameters;
    }
    return obj;
}

PlaybackLogEntry PlaybackLogEntry::fromJson(const QJsonObject& json) {
    PlaybackLogEntry entry;
    entry.timestamp = QDateTime::fromString(json["timestamp"].toString(), Qt::ISODateWithMs);

    QString typeStr = json["type"].toString();
    if (typeStr == "CueExecuted")
        entry.type = CueExecuted;
    else if (typeStr == "FadeStarted")
        entry.type = FadeStarted;
    else if (typeStr == "FadeCompleted")
        entry.type = FadeCompleted;
    else if (typeStr == "MacroExpanded")
        entry.type = MacroExpanded;
    else if (typeStr == "AutoFollowTriggered")
        entry.type = AutoFollowTriggered;
    else if (typeStr == "ValidationWarning")
        entry.type = ValidationWarning;
    else if (typeStr == "ValidationError")
        entry.type = ValidationError;
    else if (typeStr == "EmergencyStop")
        entry.type = EmergencyStop;
    else if (typeStr == "GoLockout")
        entry.type = GoLockout;
    else if (typeStr == "ParameterSent")
        entry.type = ParameterSent;
    else if (typeStr == "StateCapture")
        entry.type = StateCapture;
    else
        entry.type = Custom;

    entry.cueId = json["cueId"].toString();
    entry.details = json["details"].toString();
    entry.parameters = json["parameters"].toObject();

    return entry;
}

PlaybackLogger::PlaybackLogger(QObject* parent) : QObject(parent) {}

PlaybackLogger::~PlaybackLogger() { closeLogFile(); }

void PlaybackLogger::setLogFile(const QString& path) {
    QMutexLocker locker(&m_mutex);

    closeLogFile();
    m_logFilePath = path;

    if (!path.isEmpty()) {
        m_logFile.setFileName(path);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning("PlaybackLogger: Failed to open log file: %s", qPrintable(path));
        }
    }
}

void PlaybackLogger::closeLogFile() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void PlaybackLogger::log(PlaybackLogEntry::Type type, const QString& cueId, const QString& details,
                         const QJsonObject& params) {
    PlaybackLogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.type = type;
    entry.cueId = cueId;
    entry.details = details;
    entry.parameters = params;

    addEntry(entry);
}

void PlaybackLogger::logCueExecuted(const QString& cueId, const QString& cueName,
                                    double cueNumber) {
    QString details = QString("Executed cue %1: %2").arg(cueNumber, 0, 'f', 1).arg(cueName);
    log(PlaybackLogEntry::CueExecuted, cueId, details);
}

void PlaybackLogger::logFadeStarted(const QString& cueId, double duration) {
    QString details = QString("Fade started (duration: %1s)").arg(duration, 0, 'f', 2);
    log(PlaybackLogEntry::FadeStarted, cueId, details);
}

void PlaybackLogger::logFadeCompleted(const QString& cueId) {
    log(PlaybackLogEntry::FadeCompleted, cueId, "Fade completed");
}

void PlaybackLogger::logMacroExpanded(const QString& parentId, const QString& childId) {
    QString details = QString("Macro expanded: child %1").arg(childId);
    log(PlaybackLogEntry::MacroExpanded, parentId, details);
}

void PlaybackLogger::logAutoFollowTriggered(const QString& cueId, double delay) {
    QString details = QString("Auto-follow triggered after %1s delay").arg(delay, 0, 'f', 2);
    log(PlaybackLogEntry::AutoFollowTriggered, cueId, details);
}

void PlaybackLogger::logValidationWarning(const QString& cueId, const QString& message) {
    log(PlaybackLogEntry::ValidationWarning, cueId, message);
}

void PlaybackLogger::logValidationError(const QString& cueId, const QString& message) {
    log(PlaybackLogEntry::ValidationError, cueId, message);
}

void PlaybackLogger::logEmergencyStop(const QString& reason) {
    log(PlaybackLogEntry::EmergencyStop, QString(), reason);
}

void PlaybackLogger::logGoLockout(const QString& reason) {
    log(PlaybackLogEntry::GoLockout, QString(), reason);
}

void PlaybackLogger::logParameterSent(const QString& path, const QVariant& value) {
    QJsonObject params;
    params["path"] = path;
    params["value"] = QJsonValue::fromVariant(value);
    log(PlaybackLogEntry::ParameterSent, QString(),
        QString("Sent %1 = %2").arg(path).arg(value.toString()), params);
}

void PlaybackLogger::logStateCapture(const QString& description, const QJsonObject& state) {
    log(PlaybackLogEntry::StateCapture, QString(), description, state);
}

QVector<PlaybackLogEntry> PlaybackLogger::allEntries() const {
    QMutexLocker locker(&m_mutex);
    return m_entries;
}

QVector<PlaybackLogEntry> PlaybackLogger::recentEntries(int count) const {
    QMutexLocker locker(&m_mutex);

    if (count <= 0 || count >= m_entries.size()) {
        return m_entries;
    }

    return m_entries.mid(m_entries.size() - count);
}

QVector<PlaybackLogEntry> PlaybackLogger::entriesSince(const QDateTime& since) const {
    QMutexLocker locker(&m_mutex);

    QVector<PlaybackLogEntry> result;
    for (const PlaybackLogEntry& entry : m_entries) {
        if (entry.timestamp >= since) {
            result.append(entry);
        }
    }
    return result;
}

QVector<PlaybackLogEntry> PlaybackLogger::entriesForCue(const QString& cueId) const {
    QMutexLocker locker(&m_mutex);

    QVector<PlaybackLogEntry> result;
    for (const PlaybackLogEntry& entry : m_entries) {
        if (entry.cueId == cueId) {
            result.append(entry);
        }
    }
    return result;
}

bool PlaybackLogger::exportToFile(const QString& path) const {
    QMutexLocker locker(&m_mutex);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QJsonArray array;
    for (const PlaybackLogEntry& entry : m_entries) {
        array.append(entry.toJson());
    }

    QJsonDocument doc(array);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool PlaybackLogger::exportToCSV(const QString& path) const {
    QMutexLocker locker(&m_mutex);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << "Timestamp,Type,CueID,Details\n";

    for (const PlaybackLogEntry& entry : m_entries) {
        QString details = entry.details;
        // escape CSV special characters
        details.replace("\"", "\"\"");
        if (details.contains(',') || details.contains('\n')) {
            details = "\"" + details + "\"";
        }

        stream << entry.timestamp.toString(Qt::ISODateWithMs) << "," << entry.typeString() << ","
               << entry.cueId << "," << details << "\n";
    }

    return true;
}

void PlaybackLogger::clear() {
    QMutexLocker locker(&m_mutex);
    m_entries.clear();
    emit logCleared();
}

void PlaybackLogger::addEntry(const PlaybackLogEntry& entry) {
    {
        QMutexLocker locker(&m_mutex);
        m_entries.append(entry);
        pruneOldEntries();
        writeToFile(entry);
    }

    emit entryAdded(entry);
}

void PlaybackLogger::writeToFile(const PlaybackLogEntry& entry) {
    if (!m_logFile.isOpen())
        return;

    QTextStream stream(&m_logFile);
    stream << entry.timestamp.toString(Qt::ISODateWithMs) << " " << "[" << entry.typeString()
           << "] " << entry.details;
    if (!entry.cueId.isEmpty()) {
        stream << " (cue: " << entry.cueId << ")";
    }
    stream << "\n";
    stream.flush();
}

void PlaybackLogger::pruneOldEntries() {
    if (m_maxMemoryEntries <= 0)
        return;

    while (m_entries.size() > m_maxMemoryEntries) {
        m_entries.removeFirst();
    }
}

} // namespace OpenMix
