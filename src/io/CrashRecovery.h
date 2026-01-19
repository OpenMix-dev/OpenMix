#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

namespace StageBlend {

class Show;
class PlaybackEngine;

struct CrashState {
    QString projectPath;
    int currentCueIndex;
    int standbyCueIndex;
    QJsonObject mixerState;
    QJsonObject showData; // serialized show data
    qint64 timestamp;
    QString version;
    bool isValid = false;
};

class CrashRecovery : public QObject {
    Q_OBJECT

  public:
    explicit CrashRecovery(QObject* parent = nullptr);
    ~CrashRecovery();

    // check if there's a crash state from a previous session
    bool hasCrashState() const;
    CrashState crashState() const;

    // set the show & playback engine to monitor
    void setShow(Show* show);
    void setPlaybackEngine(PlaybackEngine* engine);

    // recovery operations
    bool recoverFromCrash(Show* show);
    void clearCrashState();

    // state persistence
    void saveState();     // called periodically
    void markCleanExit(); // clears crash state on normal exit

    // auto-save interval
    void setAutoSaveInterval(int seconds);
    int autoSaveInterval() const { return m_autoSaveIntervalSec; }

    // enable/disable auto-save
    void setAutoSaveEnabled(bool enabled);
    bool isAutoSaveEnabled() const { return m_autoSaveEnabled; }

    // file paths
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

    Show* m_show = nullptr;
    PlaybackEngine* m_engine = nullptr;

    CrashState m_crashState;
    bool m_crashStateLoaded = false;

    QTimer m_autoSaveTimer;
    int m_autoSaveIntervalSec = 10; // default: save every 10 seconds
    bool m_autoSaveEnabled = true;

    static constexpr int LOCK_FILE_STALE_SECONDS =
        60; // lock file older than this is considered stale
};

} // namespace StageBlend
