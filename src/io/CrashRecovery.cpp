#include "CrashRecovery.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "protocol/MixerProtocol.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QStandardPaths>

namespace StageBlend {

CrashRecovery::CrashRecovery(QObject* parent) : QObject(parent) {
    connect(&m_autoSaveTimer, &QTimer::timeout, this, &CrashRecovery::onAutoSaveTimeout);

    // load any existing crash state on construction
    loadCrashState();

    // create lock file to indicate active session
    createLockFile();
}

CrashRecovery::~CrashRecovery() {
    // don't remove lock file here - markCleanExit() should be called explicitly
    m_autoSaveTimer.stop();
}

bool CrashRecovery::hasCrashState() const { return m_crashStateLoaded && m_crashState.isValid; }

CrashState CrashRecovery::crashState() const { return m_crashState; }

void CrashRecovery::setShow(Show* show) { m_show = show; }

void CrashRecovery::setPlaybackEngine(PlaybackEngine* engine) { m_engine = engine; }

bool CrashRecovery::recoverFromCrash(Show* show) {
    if (!hasCrashState() || !show) {
        return false;
    }

    // restore show data from crash state
    if (!m_crashState.showData.isEmpty()) {
        show->fromJson(m_crashState.showData);
    }

    // restore playback position
    if (m_engine && m_crashState.standbyCueIndex >= 0) {
        m_engine->goToIndex(m_crashState.standbyCueIndex);
    }

    // clear the crash state after successful recovery
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

    // save playback state
    if (m_engine) {
        state["currentCueIndex"] = m_engine->currentCueIndex();
        state["standbyCueIndex"] = m_engine->standbyCueIndex();
    }

    // save show data
    state["showData"] = m_show->toJson();

    // ensure config directory exists
    QDir dir;
    dir.mkpath(configDir());

    // write state file
    QFile file(crashStatePath());
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(state);
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
        emit stateSaved();
    }

    // update lock file timestamp
    QFile lockFile(lockFilePath());
    if (lockFile.exists()) {
        lockFile.open(QIODevice::WriteOnly);
        lockFile.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
        lockFile.close();
    }
}

void CrashRecovery::markCleanExit() {
    clearCrashState();
    removeLockFile();
    m_autoSaveTimer.stop();
}

void CrashRecovery::setAutoSaveInterval(int seconds) {
    m_autoSaveIntervalSec = qMax(1, seconds);
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
        path = QDir::homePath() + "/.stageblend";
    }
    return path;
}

void CrashRecovery::onAutoSaveTimeout() { saveState(); }

bool CrashRecovery::createLockFile() {
    QDir dir;
    dir.mkpath(configDir());

    QFile file(lockFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QByteArray::number(QDateTime::currentMSecsSinceEpoch()));
        file.close();
        return true;
    }
    return false;
}

void CrashRecovery::removeLockFile() { QFile::remove(lockFilePath()); }

bool CrashRecovery::isLockFileStale() const {
    QFileInfo info(lockFilePath());
    if (!info.exists()) {
        return false;
    }

    // check if lock file is older than threshold
    QFile file(lockFilePath());
    if (file.open(QIODevice::ReadOnly)) {
        qint64 timestamp = file.readAll().toLongLong();
        file.close();

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        qint64 age = (now - timestamp) / 1000; // age in seconds

        return age > LOCK_FILE_STALE_SECONDS;
    }

    // if we can't read the file, use modification time
    qint64 age = info.lastModified().secsTo(QDateTime::currentDateTime());
    return age > LOCK_FILE_STALE_SECONDS;
}

void CrashRecovery::loadCrashState() {
    QString statePath = crashStatePath();
    QString lockPath = lockFilePath();

    // check if crash state file exists
    if (!QFile::exists(statePath)) {
        m_crashStateLoaded = false;
        return;
    }

    // check if lock file exists (indicates another instance or crash)
    bool lockExists = QFile::exists(lockPath);
    bool lockStale = isLockFileStale();

    // if lock exists & is not stale, another instance might be running
    if (lockExists && !lockStale) {
        // could be another instance - don't treat as crash
        // in a production app, you'd want to check if the process is actually running
        m_crashStateLoaded = false;
        return;
    }

    // lock exists but is stale, or no lock - this was likely a crash
    QFile file(statePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_crashStateLoaded = false;
        return;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        m_crashStateLoaded = false;
        return;
    }

    QJsonObject obj = doc.object();

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

} // namespace StageBlend
