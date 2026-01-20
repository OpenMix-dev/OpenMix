#pragma once

#include <QAction>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>

namespace OpenMix {

struct ShortcutInfo {
    QString id;
    QString displayName;
    QKeySequence defaultShortcut;
    QKeySequence currentShortcut;
    QAction* action = nullptr;
};

class ShortcutManager : public QObject {
    Q_OBJECT

  public:
    explicit ShortcutManager(QObject* parent = nullptr);

    // register an action with a shortcut
    void registerAction(const QString& id, QAction* action, const QKeySequence& defaultShortcut);

    // get/set shortcuts
    void setShortcut(const QString& actionId, const QKeySequence& shortcut);
    QKeySequence shortcut(const QString& actionId) const;
    QKeySequence defaultShortcut(const QString& actionId) const;

    // reset to defaults
    void resetToDefaults();
    void resetToDefault(const QString& actionId);

    // get all registered shortcuts
    QList<ShortcutInfo> allShortcuts() const;

    // check for conflicts
    bool hasConflict(const QString& actionId, const QKeySequence& shortcut) const;
    QString conflictingAction(const QKeySequence& shortcut) const;

    // numeric keypad cue jumping
    void setNumpadCueJumpEnabled(bool enabled);
    bool isNumpadCueJumpEnabled() const { return m_numpadEnabled; }

    // handle key press for keypad accumulation
    bool handleKeyPress(QKeyEvent* event);

    // clear accumulated keypad digits
    void clearKeypadBuffer();

    // settings persistence
    void saveToSettings();
    void loadFromSettings();

  signals:
    void shortcutChanged(const QString& actionId, const QKeySequence& shortcut);
    void numpadCueNumberEntered(double cueNumber);
    void keypadBufferChanged(const QString& buffer);

  private slots:
    void onKeypadTimeout();

  private:
    bool isNumpadKey(int key) const;
    int numpadKeyToDigit(int key) const;

    QMap<QString, ShortcutInfo> m_shortcuts;

    // numeric keypad support
    bool m_numpadEnabled = true;
    QString m_keypadBuffer;
    QTimer m_keypadTimer;
    static constexpr int KEYPAD_TIMEOUT_MS = 1500; // 1.5 seconds to complete cue number
};

} // namespace OpenMix
