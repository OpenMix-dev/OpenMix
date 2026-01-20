#include "OperationMode.h"
#include <QSettings>

namespace OpenMix {

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

bool OperationModeManager::canSaveShow() const { return true; }

bool OperationModeManager::canGo() const { return true; }

bool OperationModeManager::canStop() const { return true; }

bool OperationModeManager::canNavigateCues() const { return true; }

bool OperationModeManager::canUsePanic() const { return true; }

bool OperationModeManager::canViewTimeline() const { return true; }

bool OperationModeManager::canViewMixerFeedback() const { return true; }

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
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("OperationMode");

    // save password hash
    settings.setValue("PasswordHash", m_passwordHash);

    settings.setValue("SaveModeEnabled", m_saveMode);
    if (m_saveMode) {
        settings.setValue("Mode", m_mode == AppMode::Programmer ? "programmer" : "showmode");
    }

    settings.endGroup();
}

void OperationModeManager::loadFromSettings() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("OperationMode");

    m_passwordHash = settings.value("PasswordHash").toString();

    m_saveMode = settings.value("SaveModeEnabled", false).toBool();
    if (m_saveMode) {
        QString savedMode = settings.value("Mode", "programmer").toString();
        if (savedMode == "showmode") {
            m_mode = AppMode::ShowMode;
        } else {
            m_mode = AppMode::Programmer;
        }
    }

    settings.endGroup();
}

QString OperationModeManager::hashPassword(const QString& password) const {
    QByteArray data = password.toUtf8();
    QByteArray hash = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

} // namespace OpenMix