#include "AppLogger.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QTextStream>

namespace OpenMix {

QString LogEntry::levelString() const {
    switch (level) {
    case LogLevel::Debug:
        return "Debug";
    case LogLevel::Info:
        return "Info";
    case LogLevel::Warning:
        return "Warning";
    case LogLevel::Error:
        return "Error";
    case LogLevel::Critical:
        return "Critical";
    }
    return "Unknown";
}

QString LogEntry::sourceString() const {
    switch (source) {
    case LogSource::Connection:
        return "Connection";
    case LogSource::Protocol:
        return "Protocol";
    case LogSource::Playback:
        return "Playback";
    case LogSource::UI:
        return "UI";
    case LogSource::System:
        return "System";
    case LogSource::MIDI:
        return "MIDI";
    case LogSource::Discovery:
        return "Discovery";
    }
    return "Unknown";
}

QJsonObject LogEntry::toJson() const {
    QJsonObject obj;
    obj["timestamp"] = timestamp.toString(Qt::ISODateWithMs);
    obj["level"] = levelString();
    obj["source"] = sourceString();
    obj["message"] = message;
    if (!metadata.isEmpty()) {
        obj["metadata"] = metadata;
    }
    return obj;
}

LogEntry LogEntry::fromJson(const QJsonObject& json) {
    LogEntry entry;
    entry.timestamp = QDateTime::fromString(json["timestamp"].toString(), Qt::ISODateWithMs);
    entry.level = levelFromString(json["level"].toString());
    entry.source = sourceFromString(json["source"].toString());
    entry.message = json["message"].toString();
    entry.metadata = json["metadata"].toObject();
    return entry;
}

LogLevel LogEntry::levelFromString(const QString& str) {
    if (str == "Debug")
        return LogLevel::Debug;
    if (str == "Info")
        return LogLevel::Info;
    if (str == "Warning")
        return LogLevel::Warning;
    if (str == "Error")
        return LogLevel::Error;
    if (str == "Critical")
        return LogLevel::Critical;
    return LogLevel::Info;
}

LogSource LogEntry::sourceFromString(const QString& str) {
    if (str == "Connection")
        return LogSource::Connection;
    if (str == "Protocol")
        return LogSource::Protocol;
    if (str == "Playback")
        return LogSource::Playback;
    if (str == "UI")
        return LogSource::UI;
    if (str == "System")
        return LogSource::System;
    if (str == "MIDI")
        return LogSource::MIDI;
    if (str == "Discovery")
        return LogSource::Discovery;
    return LogSource::System;
}

AppLogger::AppLogger(QObject* parent) : QObject(parent) {
    m_batchTimer = new QTimer(this);
    m_batchTimer->setSingleShot(true);
    connect(m_batchTimer, &QTimer::timeout, this, &AppLogger::flushBatch);
}

AppLogger::~AppLogger() {
    flushBatch();
    closeLogFile();
}

void AppLogger::setLogFile(const QString& path) {
    QMutexLocker locker(&m_mutex);

    closeLogFile();
    m_logFilePath = path;

    if (!path.isEmpty()) {
        m_logFile.setFileName(path);
        if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning("AppLogger: Failed to open log file: %s", qPrintable(path));
        }
    }
}

void AppLogger::closeLogFile() {
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void AppLogger::setBatchInterval(int ms) { m_batchInterval = qMax(0, ms); }

void AppLogger::log(LogLevel level, LogSource source, const QString& message,
                    const QJsonObject& metadata) {
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.source = source;
    entry.message = message;
    entry.metadata = metadata;

    addEntry(entry);
}

void AppLogger::debug(LogSource source, const QString& message, const QJsonObject& metadata) {
    log(LogLevel::Debug, source, message, metadata);
}

void AppLogger::info(LogSource source, const QString& message, const QJsonObject& metadata) {
    log(LogLevel::Info, source, message, metadata);
}

void AppLogger::warning(LogSource source, const QString& message, const QJsonObject& metadata) {
    log(LogLevel::Warning, source, message, metadata);
}

void AppLogger::error(LogSource source, const QString& message, const QJsonObject& metadata) {
    log(LogLevel::Error, source, message, metadata);
}

void AppLogger::critical(LogSource source, const QString& message, const QJsonObject& metadata) {
    log(LogLevel::Critical, source, message, metadata);
}

void AppLogger::logConnectionAttempt(const QString& protocol, const QString& host, int port) {
    QJsonObject meta;
    meta["protocol"] = protocol;
    meta["host"] = host;
    meta["port"] = port;
    info(LogSource::Connection,
         QString("Connecting to %1:%2 (%3)").arg(host).arg(port).arg(protocol), meta);
}

void AppLogger::logConnectionSuccess(const QString& protocol, const QString& host, int port) {
    QJsonObject meta;
    meta["protocol"] = protocol;
    meta["host"] = host;
    meta["port"] = port;
    info(LogSource::Connection,
         QString("Connected to %1:%2 (%3)").arg(host).arg(port).arg(protocol), meta);
}

void AppLogger::logConnectionFailed(const QString& protocol, const QString& host, int port,
                                    const QString& error) {
    QJsonObject meta;
    meta["protocol"] = protocol;
    meta["host"] = host;
    meta["port"] = port;
    meta["error"] = error;
    this->error(
        LogSource::Connection,
        QString("Connection failed to %1:%2 (%3): %4").arg(host).arg(port).arg(protocol).arg(error),
        meta);
}

void AppLogger::logConnectionLost(const QString& protocol, const QString& host, int port) {
    QJsonObject meta;
    meta["protocol"] = protocol;
    meta["host"] = host;
    meta["port"] = port;
    warning(LogSource::Connection,
            QString("Connection lost to %1:%2 (%3)").arg(host).arg(port).arg(protocol), meta);
}

void AppLogger::logReconnectAttempt(const QString& protocol, const QString& host, int port,
                                    int attempt, int maxAttempts) {
    QJsonObject meta;
    meta["protocol"] = protocol;
    meta["host"] = host;
    meta["port"] = port;
    meta["attempt"] = attempt;
    meta["maxAttempts"] = maxAttempts;
    info(LogSource::Connection,
         QString("Reconnecting to %1:%2 (%3) - attempt %4/%5")
             .arg(host)
             .arg(port)
             .arg(protocol)
             .arg(attempt)
             .arg(maxAttempts),
         meta);
}

void AppLogger::logDisconnected(const QString& protocol, const QString& host, int port) {
    QJsonObject meta;
    meta["protocol"] = protocol;
    meta["host"] = host;
    meta["port"] = port;
    info(LogSource::Connection,
         QString("Disconnected from %1:%2 (%3)").arg(host).arg(port).arg(protocol), meta);
}

QVector<LogEntry> AppLogger::allEntries() const {
    QMutexLocker locker(&m_mutex);
    return m_entries;
}

QVector<LogEntry> AppLogger::recentEntries(int count) const {
    QMutexLocker locker(&m_mutex);

    if (count <= 0 || count >= m_entries.size()) {
        return m_entries;
    }

    return m_entries.mid(m_entries.size() - count);
}

QVector<LogEntry> AppLogger::entriesSince(const QDateTime& since) const {
    QMutexLocker locker(&m_mutex);

    QVector<LogEntry> result;
    for (const LogEntry& entry : m_entries) {
        if (entry.timestamp >= since) {
            result.append(entry);
        }
    }
    return result;
}

QVector<LogEntry> AppLogger::entriesFiltered(LogLevel minLevel, LogSource source,
                                             bool filterBySource) const {
    QMutexLocker locker(&m_mutex);

    QVector<LogEntry> result;
    for (const LogEntry& entry : m_entries) {
        if (static_cast<int>(entry.level) < static_cast<int>(minLevel)) {
            continue;
        }
        if (filterBySource && entry.source != source) {
            continue;
        }
        result.append(entry);
    }
    return result;
}

bool AppLogger::exportToFile(const QString& path) const {
    QMutexLocker locker(&m_mutex);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QJsonArray array;
    for (const LogEntry& entry : m_entries) {
        array.append(entry.toJson());
    }

    QJsonDocument doc(array);
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool AppLogger::exportToCSV(const QString& path) const {
    QMutexLocker locker(&m_mutex);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream << "Timestamp,Level,Source,Message\n";

    for (const LogEntry& entry : m_entries) {
        QString message = entry.message;
        message.replace("\"", "\"\"");
        if (message.contains(',') || message.contains('\n') || message.contains('"')) {
            message = "\"" + message + "\"";
        }

        stream << entry.timestamp.toString(Qt::ISODateWithMs) << "," << entry.levelString() << ","
               << entry.sourceString() << "," << message << "\n";
    }

    return true;
}

void AppLogger::clear() {
    {
        QMutexLocker locker(&m_mutex);
        m_entries.clear();
        m_pendingBatch.clear();
    }
    emit logCleared();
}

void AppLogger::flushBatch() {
    QVector<LogEntry> batch;
    {
        QMutexLocker locker(&m_mutex);
        if (m_pendingBatch.isEmpty()) {
            return;
        }
        batch = m_pendingBatch;
        m_pendingBatch.clear();
    }

    emit entriesBatchAdded(batch);
}

void AppLogger::addEntry(const LogEntry& entry) {
    {
        QMutexLocker locker(&m_mutex);
        m_entries.append(entry);
        pruneOldEntries();
        writeToFile(entry);
        m_pendingBatch.append(entry);
    }

    emit entryAdded(entry);

    // schedule batch emission if not already pending
    if (!m_batchTimer->isActive() && m_batchInterval > 0) {
        m_batchTimer->start(m_batchInterval);
    }
}

void AppLogger::writeToFile(const LogEntry& entry) {
    if (!m_logFile.isOpen())
        return;

    // write as JSON Lines format
    QJsonDocument doc(entry.toJson());
    QTextStream stream(&m_logFile);
    stream << doc.toJson(QJsonDocument::Compact) << "\n";
    stream.flush();
}

void AppLogger::pruneOldEntries() {
    if (m_maxMemoryEntries <= 0)
        return;

    while (m_entries.size() > m_maxMemoryEntries) {
        m_entries.removeFirst();
    }
}

} // namespace OpenMix
