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

    [[nodiscard]] AppMode currentMode() const noexcept { return m_mode; }
    void setMode(AppMode mode);

    [[nodiscard]] QString modeString() const;
    [[nodiscard]] static QString modeString(AppMode mode);

    [[nodiscard]] bool canEditCues();                // programmer only
    [[nodiscard]] bool canDeleteCues();              // programmer only
    [[nodiscard]] bool canModifyShow();              // programmer only
    [[nodiscard]] bool canAddCues();                 // programmer only
    [[nodiscard]] bool canRenumberCues();            // programmer only
    [[nodiscard]] bool canOpenShow();                // programmer only
    [[nodiscard]] bool canNewShow();                 // programmer only
    [[nodiscard]] bool canSaveShow() const;          // always (emergency save)
    [[nodiscard]] bool canGo() const;                // always
    [[nodiscard]] bool canStop() const;              // always
    [[nodiscard]] bool canNavigateCues() const;      // always
    [[nodiscard]] bool canUsePanic() const;          // always
    [[nodiscard]] bool canViewTimeline() const;      // always
    [[nodiscard]] bool canViewMixerFeedback() const; // always

    void setShowModePassword(const QString& password);
    void clearPassword();
    [[nodiscard]] bool hasPassword() const noexcept { return !m_passwordHash.isEmpty(); }
    [[nodiscard]] bool validatePassword(const QString& password) const;

    bool switchToProgrammerMode(const QString& password = QString());
    void switchToShowMode();

    void saveToSettings();
    void loadFromSettings();

    void setSaveMode(bool enabled) { m_saveMode = enabled; }
    [[nodiscard]] bool saveMode() const noexcept { return m_saveMode; }

  signals:
    void modeChanged(AppMode mode);
    void operationBlocked(const QString& operation);
    void passwordRequired();

  private:
    bool requiresProgrammerMode(const QString& operation);
    QString hashPassword(const QString& password) const;

    AppMode m_mode = AppMode::Programmer;
    QString m_passwordHash;
    bool m_saveMode = false;
};

} // namespace OpenMix
