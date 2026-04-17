#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace OpenMix {

class Application;
class Show;

class AutosaveManager : public QObject {
    Q_OBJECT

  public:
    explicit AutosaveManager(Application* app, QObject* parent = nullptr);

    // configuration
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    void setIntervalMinutes(int minutes);
    int intervalMinutes() const { return m_intervalMinutes; }

    void setMaxBackups(int count);
    int maxBackups() const { return m_maxBackups; }

    // manual operations
    void createBackup();
    bool restoreFromBackup(const QString& backupPath);
    QStringList availableBackups() const;
    QStringList availableBackups(const QString& projectPath) const;

    // check for recovery on startup
    bool hasRecoverableAutosave() const;
    QString recoverableAutosavePath() const;

    // paths
    static QString autosaveDir();
    static QString backupDir();
    QString autosavePath() const;

  signals:
    void autosaveCompleted(const QString& path);
    void autosaveFailed(const QString& error);

    void backupCreated(const QString& path);

  public slots:
    void onShowModified();
    void scheduleSave();

  private slots:
    void performAutosave();

  private:
    void cleanupOldBackups();
    void cleanupOldAutosaves();
    QString generateBackupFilename() const;
    QString generateAutosaveFilename() const;
    QString showBaseName() const;
    void cleanupOldFiles(const QString& dirPath, const QString& nameFilter, int maxCount);

    Application* m_app;
    QTimer m_timer;
    QTimer m_debounceTimer;
    bool m_enabled = true;
    int m_intervalMinutes = 5;
    int m_maxBackups = 10;
    int m_maxAutosaves = 3;
};

} // namespace OpenMix
