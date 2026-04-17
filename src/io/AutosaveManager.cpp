#include "AutosaveManager.h"
#include "ProjectFile.h"
#include "app/Application.h"
#include "core/Show.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>

namespace OpenMix {

AutosaveManager::AutosaveManager(Application* app, QObject* parent) : QObject(parent), m_app(app) {
    // main autosave timer
    connect(&m_timer, &QTimer::timeout, this, &AutosaveManager::performAutosave);

    // debounce timer for rapid changes
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(30000); // 30 seconds debounce
    connect(&m_debounceTimer, &QTimer::timeout, this, &AutosaveManager::performAutosave);

    // start interval timer
    m_timer.start(m_intervalMinutes * 60 * 1000);
}

void AutosaveManager::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (enabled) {
        m_timer.start(m_intervalMinutes * 60 * 1000);
    } else {
        m_timer.stop();
        m_debounceTimer.stop();
    }
}

void AutosaveManager::setIntervalMinutes(int minutes) {
    m_intervalMinutes = std::max(1, minutes);
    if (m_enabled) {
        m_timer.start(m_intervalMinutes * 60 * 1000);
    }
}

void AutosaveManager::setMaxBackups(int count) {
    m_maxBackups = std::max(1, count);
    cleanupOldBackups();
}

QString AutosaveManager::autosaveDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + "/autosave";
}

QString AutosaveManager::backupDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return dir + "/backups";
}

QString AutosaveManager::showBaseName() const {
    if (!m_app || !m_app->show())
        return QString();

    Show* show = m_app->show();
    if (!show->filePath().isEmpty()) {
        QFileInfo fi(show->filePath());
        return fi.completeBaseName();
    }
    return QStringLiteral("untitled");
}

QString AutosaveManager::autosavePath() const {
    QString filename = showBaseName();
    if (filename.isEmpty())
        return QString();

    return autosaveDir() + "/" + filename + "_autosave.omproj";
}

QString AutosaveManager::generateBackupFilename() const {
    QString baseName = showBaseName();
    if (baseName.isEmpty())
        return QString();

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("%1_%2.omproj.bak").arg(baseName, timestamp);
}

QString AutosaveManager::generateAutosaveFilename() const {
    QString baseName = showBaseName();
    if (baseName.isEmpty())
        return QString();

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("%1_autosave_%2.omproj").arg(baseName, timestamp);
}

void AutosaveManager::onShowModified() {
    if (!m_enabled)
        return;

    // restart debounce timer on each modification
    m_debounceTimer.start();
}

void AutosaveManager::scheduleSave() {
    if (!m_enabled)
        return;
    m_debounceTimer.start();
}

void AutosaveManager::performAutosave() {
    if (!m_enabled || !m_app || !m_app->show())
        return;

    Show* show = m_app->show();
    if (!show->isModified())
        return;

    // ensure autosave directory exists
    QDir dir;
    dir.mkpath(autosaveDir());

    QString path = autosavePath();
    QString error;

    if (ProjectFile::save(show, path, &error)) {
        emit autosaveCompleted(path);
        cleanupOldAutosaves();
    } else {
        emit autosaveFailed(error);
    }
}

void AutosaveManager::createBackup() {
    if (!m_app || !m_app->show())
        return;

    Show* show = m_app->show();
    QString originalPath = show->filePath();

    if (originalPath.isEmpty() || !QFile::exists(originalPath))
        return;

    // ensure backup directory exists
    QDir dir;
    dir.mkpath(backupDir());

    QString backupPath = backupDir() + "/" + generateBackupFilename();

    if (QFile::copy(originalPath, backupPath)) {
        emit backupCreated(backupPath);
        cleanupOldBackups();
    }
}

bool AutosaveManager::restoreFromBackup(const QString& backupPath) {
    if (!m_app || !m_app->show())
        return false;
    if (!QFile::exists(backupPath))
        return false;

    Show* show = m_app->show();
    QString error;

    if (ProjectFile::load(show, backupPath, &error)) {
        return true;
    }
    return false;
}

QStringList AutosaveManager::availableBackups() const {
    if (!m_app || !m_app->show())
        return QStringList();
    return availableBackups(m_app->show()->filePath());
}

QStringList AutosaveManager::availableBackups(const QString& projectPath) const {
    QStringList backups;
    QDir dir(backupDir());

    if (!dir.exists())
        return backups;

    QString baseName;
    if (!projectPath.isEmpty()) {
        QFileInfo fi(projectPath);
        baseName = fi.completeBaseName();
    }

    QStringList filters;
    if (baseName.isEmpty()) {
        filters << "*.omproj.bak";
    } else {
        filters << QString("%1_*.omproj.bak").arg(baseName);
    }

    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);
    for (const QFileInfo& fi : files) {
        backups.append(fi.absoluteFilePath());
    }

    return backups;
}

bool AutosaveManager::hasRecoverableAutosave() const {
    return !recoverableAutosavePath().isEmpty();
}

QString AutosaveManager::recoverableAutosavePath() const {
    if (!m_app || !m_app->show())
        return QString();

    Show* show = m_app->show();
    QString path = autosavePath();

    if (!QFile::exists(path))
        return QString();

    // check if autosave is newer than the saved file
    if (!show->filePath().isEmpty() && QFile::exists(show->filePath())) {
        QFileInfo savedInfo(show->filePath());
        QFileInfo autosaveInfo(path);

        if (autosaveInfo.lastModified() <= savedInfo.lastModified()) {
            return QString(); // saved file is newer
        }
    }

    return path;
}

void AutosaveManager::cleanupOldFiles(const QString& dirPath, const QString& nameFilter,
                                       int maxCount) {
    QDir dir(dirPath);
    if (!dir.exists())
        return;

    // get all matching files sorted by modification time (newest first)
    QFileInfoList files = dir.entryInfoList(QStringList() << nameFilter, QDir::Files, QDir::Time);

    // remove files beyond max count
    for (int i = maxCount; i < files.size(); ++i) {
        QFile::remove(files[i].absoluteFilePath());
    }
}

void AutosaveManager::cleanupOldBackups() {
    cleanupOldFiles(backupDir(), "*.omproj.bak", m_maxBackups);
}

void AutosaveManager::cleanupOldAutosaves() {
    cleanupOldFiles(autosaveDir(), "*_autosave*.omproj", m_maxAutosaves);
}

} // namespace OpenMix
