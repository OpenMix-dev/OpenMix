#pragma once

#include <QCryptographicHash>
#include <QObject>
#include <QString>

namespace StageBlend {

enum class AppMode {
    Programmer, // full access to all editing features
    ShowMode    // limited access - playback only
};

class OperationModeManager : public QObject {
    Q_OBJECT

  public:
    explicit OperationModeManager(QObject* parent = nullptr);

    // current mode
    AppMode currentMode() const { return m_mode; }
    void setMode(AppMode mode);

    // string representation
    QString modeString() const;
    static QString modeString(AppMode mode);

    // permission checks
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

    // password protection
    void setShowModePassword(const QString& password);
    void clearPassword();
    bool hasPassword() const { return !m_passwordHash.isEmpty(); }
    bool validatePassword(const QString& password) const;

    // try to switch to programmer mode (may require password)
    bool switchToProgrammerMode(const QString& password = QString());

    // switch to show mode (no password required)
    void switchToShowMode();

    // persistence
    void saveToSettings();
    void loadFromSettings();

  signals:
    void modeChanged(AppMode mode);
    void operationBlocked(const QString& operation);
    void passwordRequired();

  private:
    QString hashPassword(const QString& password) const;

    AppMode m_mode = AppMode::Programmer;
    QString m_passwordHash;
};

} // namespace StageBlend
