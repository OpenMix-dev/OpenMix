#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

namespace OpenMix {

class Show;
class PlaybackEngine;

struct CrashState {
    QString projectPath;
    int currentCueIndex;
    int standbyCueIndex;
    QJsonObject mixerState;
    QJsonObject showData;
    qint64 timestamp;
    QString version;
    bool isValid = false;
};

class CrashRecovery : public QObject {
    Q_OBJECT

  public:
    explicit CrashRecovery(QObject* parent = nullptr);
    ~CrashRecovery();

    bool hasCrashState() const;
    CrashState crashState() const;

    void setShow(Show* show);
    void setPlaybackEngine(PlaybackEngine* engine);

    bool recoverFromCrash(Show* show);
    void clearCrashState();

    void saveState();
    void markCleanExit();

    void setAutoSaveInterval(int seconds);
    int autoSaveInterval() const { return m_autoSaveIntervalSec; }

    void setAutoSaveEnabled(bool enabled);
    bool isAutoSaveEnabled() const { return m_autoSaveEnabled; }

    static QString crashStatePath();
    static QString lockFilePath();
    static QString configDir();

  signals:
    void stateSaved();
    void crashDetected(const CrashState& state);
    void recoveryCompleted(bool success);

  private slots:
    void onAutoSaveTimeout();

  private:
    bool createLockFile();
    void removeLockFile();
    bool isLockFileStale() const;
    void loadCrashState();
    qint64 readPidFromLockFile() const;
    bool isProcessRunning(qint64 pid) const;

    Show* m_show = nullptr;
    PlaybackEngine* m_engine = nullptr;

    CrashState m_crashState;
    bool m_crashStateLoaded = false;

    QTimer m_autoSaveTimer;
    int m_autoSaveIntervalSec = 10;
    bool m_autoSaveEnabled = true;

    static constexpr int LOCK_FILE_STALE_SECONDS = 60;
};

} // namespace OpenMix
