#include "CrashRecovery.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "protocol/MixerProtocol.h"

#include <algorithm>
#include <optional>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#include <sys/types.h>
#endif

namespace {

bool writeJsonToFile(const QString& path, const QJsonObject& obj) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return true;
}

std::optional<QJsonObject> readJsonFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return std::nullopt;
    return doc.object();
}

} // anonymous namespace

namespace OpenMix {

CrashRecovery::CrashRecovery(QObject* parent) : QObject(parent) {
    connect(&m_autoSaveTimer, &QTimer::timeout, this, &CrashRecovery::onAutoSaveTimeout);

    loadCrashState();
    createLockFile();
}

CrashRecovery::~CrashRecovery() { m_autoSaveTimer.stop(); }

bool CrashRecovery::hasCrashState() const { return m_crashStateLoaded && m_crashState.isValid; }

CrashState CrashRecovery::crashState() const { return m_crashState; }

void CrashRecovery::setShow(Show* show) { m_show = show; }

void CrashRecovery::setPlaybackEngine(PlaybackEngine* engine) { m_engine = engine; }

bool CrashRecovery::recoverFromCrash(Show* show) {
    if (!hasCrashState() || !show) {
        return false;
    }

    if (!m_crashState.showData.isEmpty()) {
        show->fromJson(m_crashState.showData);
    }

    if (m_engine && m_crashState.standbyCueIndex >= 0) {
        m_engine->goToIndex(m_crashState.standbyCueIndex);
    }

    clearCrashState();

    emit recoveryCompleted(true);
    return true;
}

void CrashRecovery::clearCrashState() {
    QFile::remove(crashStatePath());
    m_crashState = CrashState();
    m_crashStateLoaded = false;
}

void CrashRecovery::saveState() {
    if (!m_show) {
        return;
    }

    QJsonObject state;
    state["version"] = QCoreApplication::applicationVersion();
    state["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    state["projectPath"] = m_show->filePath();

    if (m_engine) {
        state["currentCueIndex"] = m_engine->currentCueIndex();
        state["standbyCueIndex"] = m_engine->standbyCueIndex();
    }

    state["showData"] = m_show->toJson();

    QDir dir;
    dir.mkpath(configDir());

    if (writeJsonToFile(crashStatePath(), state)) {
        emit stateSaved();
    }

    if (QFile::exists(lockFilePath())) {
        QJsonObject lockData;
        lockData["timestamp"] = QDateTime::currentMSecsSinceEpoch();
        lockData["pid"] = QCoreApplication::applicationPid();
        writeJsonToFile(lockFilePath(), lockData);
    }
}

void CrashRecovery::markCleanExit() {
    clearCrashState();
    removeLockFile();
    m_autoSaveTimer.stop();
}

void CrashRecovery::setAutoSaveInterval(int seconds) {
    m_autoSaveIntervalSec = std::max(1, seconds);
    if (m_autoSaveTimer.isActive()) {
        m_autoSaveTimer.setInterval(m_autoSaveIntervalSec * 1000);
    }
}

void CrashRecovery::setAutoSaveEnabled(bool enabled) {
    m_autoSaveEnabled = enabled;
    if (enabled && !m_autoSaveTimer.isActive()) {
        m_autoSaveTimer.start(m_autoSaveIntervalSec * 1000);
    } else if (!enabled && m_autoSaveTimer.isActive()) {
        m_autoSaveTimer.stop();
    }
}

QString CrashRecovery::crashStatePath() { return configDir() + "/crash_state.json"; }

QString CrashRecovery::lockFilePath() { return configDir() + "/lock"; }

QString CrashRecovery::configDir() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (path.isEmpty()) {
        path = QDir::homePath() + "/.OpenMix";
    }
    return path;
}

void CrashRecovery::onAutoSaveTimeout() { saveState(); }

bool CrashRecovery::createLockFile() {
    QDir dir;
    dir.mkpath(configDir());

    QJsonObject lockData;
    lockData["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    lockData["pid"] = QCoreApplication::applicationPid();
    return writeJsonToFile(lockFilePath(), lockData);
}

void CrashRecovery::removeLockFile() { QFile::remove(lockFilePath()); }

bool CrashRecovery::isLockFileStale() const {
    QFileInfo info(lockFilePath());
    if (!info.exists()) {
        return false;
    }

    if (auto obj = readJsonFromFile(lockFilePath())) {
        qint64 timestamp = (*obj)["timestamp"].toVariant().toLongLong();
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 age = (now - timestamp) / 1000;
        return age > LOCK_FILE_STALE_SECONDS;
    }

    return true;
}

qint64 CrashRecovery::readPidFromLockFile() const {
    if (auto obj = readJsonFromFile(lockFilePath()))
        return (*obj)["pid"].toVariant().toLongLong();
    return 0;
}

bool CrashRecovery::isProcessRunning(qint64 pid) const {
    if (pid <= 0) {
        return false;
    }

    if (pid == QCoreApplication::applicationPid()) {
        return false;
    }

#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (process != nullptr) {
        DWORD exitCode = 0;
        bool running = GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE;
        CloseHandle(process);
        return running;
    }
    return false;
#else
    return kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

void CrashRecovery::loadCrashState() {
    QString statePath = crashStatePath();
    QString lockPath = lockFilePath();

    if (!QFile::exists(statePath)) {
        m_crashStateLoaded = false;
        return;
    }

    bool lockExists = QFile::exists(lockPath);
    bool lockStale = isLockFileStale();

    if (lockExists && !lockStale) {
        qint64 pid = readPidFromLockFile();
        if (isProcessRunning(pid)) {
            m_crashStateLoaded = false;
            return;
        }
    }

    auto optObj = readJsonFromFile(statePath);
    if (!optObj) {
        m_crashStateLoaded = false;
        return;
    }

    QJsonObject obj = *optObj;

    m_crashState.version = obj["version"].toString();
    m_crashState.timestamp = obj["timestamp"].toVariant().toLongLong();
    m_crashState.projectPath = obj["projectPath"].toString();
    m_crashState.currentCueIndex = obj["currentCueIndex"].toInt(-1);
    m_crashState.standbyCueIndex = obj["standbyCueIndex"].toInt(-1);
    m_crashState.showData = obj["showData"].toObject();
    m_crashState.isValid = true;

    m_crashStateLoaded = true;

    emit crashDetected(m_crashState);
}

} // namespace OpenMix
