#pragma once

#include <QCryptographicHash>
#include <QObject>
#include <QString>

namespace OpenMix {

enum class AppMode {
    Programmer, // full access to editing features
    ShowMode    // playback only
};

class OperationModeManager : public QObject {
    Q_OBJECT

  public:
    explicit OperationModeManager(QObject* parent = nullptr);

    AppMode currentMode() const { return m_mode; }
    void setMode(AppMode mode);

    QString modeString() const;
    static QString modeString(AppMode mode);

    bool canEditCues() const;          // programmer only
    bool canDeleteCues() const;        // programmer only
    bool canModifyShow() const;        // programmer only
    bool canAddCues() const;           // programmer only
    bool canRenumberCues() const;      // programmer only
    bool canOpenShow() const;          // programmer only
    bool canNewShow() const;           // programmer only
    bool canSaveShow() const;          // always (emergency save)
    bool canGo() const;                // always
    bool canStop() const;              // always
    bool canNavigateCues() const;      // always
    bool canUsePanic() const;          // always
    bool canViewTimeline() const;      // always
    bool canViewMixerFeedback() const; // always

    void setShowModePassword(const QString& password);
    void clearPassword();
    bool hasPassword() const { return !m_passwordHash.isEmpty(); }
    bool validatePassword(const QString& password) const;

    bool switchToProgrammerMode(const QString& password = QString());
    void switchToShowMode();

    void saveToSettings();
    void loadFromSettings();

    void setSaveMode(bool enabled) { m_saveMode = enabled; }
    bool saveMode() const { return m_saveMode; }

  signals:
    void modeChanged(AppMode mode);
    void operationBlocked(const QString& operation);
    void passwordRequired();

  private:
    QString hashPassword(const QString& password) const;

    AppMode m_mode = AppMode::Programmer;
    QString m_passwordHash;
    bool m_saveMode = false;
};

} // namespace OpenMix
