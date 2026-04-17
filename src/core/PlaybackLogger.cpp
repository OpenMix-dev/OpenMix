#include "PlaybackLogger.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QTextStream>

namespace OpenMix {

QString PlaybackLogEntry::typeString() const {
    switch (type) {
    case Type::CueExecuted:
        return "CueExecuted";
    case Type::MacroExpanded:
        return "MacroExpanded";
    case Type::AutoFollowTriggered:
        return "AutoFollowTriggered";
    case Type::ValidationWarning:
        return "ValidationWarning";
    case Type::ValidationError:
        return "ValidationError";
    case Type::EmergencyStop:
        return "EmergencyStop";
    case Type::GoLockout:
        return "GoLockout";
    case Type::ParameterSent:
        return "ParameterSent";
    case Type::StateCapture:
        return "StateCapture";
    case Type::Custom:
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
        entry.type = Type::CueExecuted;
    else if (typeStr == "MacroExpanded")
        entry.type = Type::MacroExpanded;
    else if (typeStr == "AutoFollowTriggered")
        entry.type = Type::AutoFollowTriggered;
    else if (typeStr == "ValidationWarning")
        entry.type = Type::ValidationWarning;
    else if (typeStr == "ValidationError")
        entry.type = Type::ValidationError;
    else if (typeStr == "EmergencyStop")
        entry.type = Type::EmergencyStop;
    else if (typeStr == "GoLockout")
        entry.type = Type::GoLockout;
    else if (typeStr == "ParameterSent")
        entry.type = Type::ParameterSent;
    else if (typeStr == "StateCapture")
        entry.type = Type::StateCapture;
    else
        entry.type = Type::Custom;

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
    log(PlaybackLogEntry::Type::CueExecuted, cueId, details);
}

void PlaybackLogger::logMacroExpanded(const QString& parentId, const QString& childId) {
    QString details = QString("Macro expanded: child %1").arg(childId);
    log(PlaybackLogEntry::Type::MacroExpanded, parentId, details);
}

void PlaybackLogger::logAutoFollowTriggered(const QString& cueId, double delay) {
    QString details = QString("Auto-follow triggered after %1s delay").arg(delay, 0, 'f', 2);
    log(PlaybackLogEntry::Type::AutoFollowTriggered, cueId, details);
}

void PlaybackLogger::logValidationWarning(const QString& cueId, const QString& message) {
    log(PlaybackLogEntry::Type::ValidationWarning, cueId, message);
}

void PlaybackLogger::logValidationError(const QString& cueId, const QString& message) {
    log(PlaybackLogEntry::Type::ValidationError, cueId, message);
}

void PlaybackLogger::logEmergencyStop(const QString& reason) {
    log(PlaybackLogEntry::Type::EmergencyStop, QString(), reason);
}

void PlaybackLogger::logGoLockout(const QString& reason) {
    log(PlaybackLogEntry::Type::GoLockout, QString(), reason);
}

void PlaybackLogger::logParameterSent(const QString& path, const QVariant& value) {
    QJsonObject params;
    params["path"] = path;
    params["value"] = QJsonValue::fromVariant(value);
    log(PlaybackLogEntry::Type::ParameterSent, QString(),
        QString("Sent %1 = %2").arg(path).arg(value.toString()), params);
}

void PlaybackLogger::logStateCapture(const QString& description, const QJsonObject& state) {
    log(PlaybackLogEntry::Type::StateCapture, QString(), description, state);
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
    return entriesMatching([&](const PlaybackLogEntry& e) { return e.timestamp >= since; });
}

QVector<PlaybackLogEntry> PlaybackLogger::entriesForCue(const QString& cueId) const {
    return entriesMatching([&](const PlaybackLogEntry& e) { return e.cueId == cueId; });
}

QVector<PlaybackLogEntry>
PlaybackLogger::entriesMatching(std::function<bool(const PlaybackLogEntry&)> predicate) const {
    QMutexLocker locker(&m_mutex);

    QVector<PlaybackLogEntry> result;
    for (const PlaybackLogEntry& entry : m_entries) {
        if (predicate(entry)) {
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
