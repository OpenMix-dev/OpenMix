#include "OperationMode.h"
#include <QSettings>

namespace StageBlend {

OperationModeManager::OperationModeManager(QObject* parent) : QObject(parent) {}

void OperationModeManager::setMode(AppMode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(mode);
    }
}

QString OperationModeManager::modeString() const { return modeString(m_mode); }

QString OperationModeManager::modeString(AppMode mode) {
    switch (mode) {
    case AppMode::Programmer:
        return tr("Programmer");
    case AppMode::ShowMode:
        return tr("Show Mode");
    }
    return tr("Unknown");
}

bool OperationModeManager::canEditCues() const {
    if (m_mode != AppMode::Programmer) {
        emit const_cast<OperationModeManager*>(this)->operationBlocked(tr("Edit cues"));
        return false;
    }
    return true;
}

bool OperationModeManager::canDeleteCues() const {
    if (m_mode != AppMode::Programmer) {
        emit const_cast<OperationModeManager*>(this)->operationBlocked(tr("Delete cues"));
        return false;
    }
    return true;
}

bool OperationModeManager::canModifyShow() const {
    if (m_mode != AppMode::Programmer) {
        emit const_cast<OperationModeManager*>(this)->operationBlocked(tr("Modify show"));
        return false;
    }
    return true;
}

bool OperationModeManager::canAddCues() const {
    if (m_mode != AppMode::Programmer) {
        emit const_cast<OperationModeManager*>(this)->operationBlocked(tr("Add cues"));
        return false;
    }
    return true;
}

bool OperationModeManager::canRenumberCues() const {
    if (m_mode != AppMode::Programmer) {
        emit const_cast<OperationModeManager*>(this)->operationBlocked(tr("Renumber cues"));
        return false;
    }
    return true;
}

bool OperationModeManager::canOpenShow() const {
    if (m_mode != AppMode::Programmer) {
        emit const_cast<OperationModeManager*>(this)->operationBlocked(tr("Open show"));
        return false;
    }
    return true;
}

bool OperationModeManager::canNewShow() const {
    if (m_mode != AppMode::Programmer) {
        emit const_cast<OperationModeManager*>(this)->operationBlocked(tr("New show"));
        return false;
    }
    return true;
}

bool OperationModeManager::canSaveShow() const {
    // always allow saving
    return true;
}

bool OperationModeManager::canGo() const {
    // always allow GO
    return true;
}

bool OperationModeManager::canStop() const {
    // always allow stop
    return true;
}

bool OperationModeManager::canNavigateCues() const {
    // always allow navigation
    return true;
}

bool OperationModeManager::canUsePanic() const {
    // always allow panic
    return true;
}

bool OperationModeManager::canViewTimeline() const {
    // always allow timeline viewing
    return true;
}

bool OperationModeManager::canViewMixerFeedback() const {
    // always allow mixer feedback viewing
    return true;
}

void OperationModeManager::setShowModePassword(const QString& password) {
    if (password.isEmpty()) {
        m_passwordHash.clear();
    } else {
        m_passwordHash = hashPassword(password);
    }
}

void OperationModeManager::clearPassword() { m_passwordHash.clear(); }

bool OperationModeManager::validatePassword(const QString& password) const {
    if (m_passwordHash.isEmpty()) {
        return true; // no password set
    }
    return hashPassword(password) == m_passwordHash;
}

bool OperationModeManager::switchToProgrammerMode(const QString& password) {
    if (m_mode == AppMode::Programmer) {
        return true; // already in programmer mode
    }

    if (hasPassword()) {
        if (password.isEmpty()) {
            emit passwordRequired();
            return false;
        }

        if (!validatePassword(password)) {
            emit operationBlocked(tr("Invalid password"));
            return false;
        }
    }

    setMode(AppMode::Programmer);
    return true;
}

void OperationModeManager::switchToShowMode() { setMode(AppMode::ShowMode); }

void OperationModeManager::saveToSettings() {
    QSettings settings("StageBlend", "StageBlend");
    settings.beginGroup("OperationMode");

    // note: we don't save the current mode, so always start in programmer mode
    // we do save the password hash for persistence
    settings.setValue("PasswordHash", m_passwordHash);

    settings.endGroup();
}

void OperationModeManager::loadFromSettings() {
    QSettings settings("StageBlend", "StageBlend");
    settings.beginGroup("OperationMode");

    m_passwordHash = settings.value("PasswordHash").toString();

    settings.endGroup();
}

QString OperationModeManager::hashPassword(const QString& password) const {
    QByteArray data = password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

} // namespace StageBlend