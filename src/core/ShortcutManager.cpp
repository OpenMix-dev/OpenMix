#include "ShortcutManager.h"
#include <QKeyEvent>
#include <QSettings>

namespace OpenMix {

ShortcutManager::ShortcutManager(QObject* parent) : QObject(parent) {
    m_keypadTimer.setSingleShot(true);
    m_keypadTimer.setInterval(KEYPAD_TIMEOUT_MS);
    connect(&m_keypadTimer, &QTimer::timeout, this, &ShortcutManager::onKeypadTimeout);
}

void ShortcutManager::registerAction(const QString& id, QAction* action,
                                     const QKeySequence& defaultShortcut) {
    ShortcutInfo info;
    info.id = id;
    info.displayName = action->text().remove('&'); // remove mnemonic markers
    info.defaultShortcut = defaultShortcut;
    info.currentShortcut = defaultShortcut;
    info.action = action;

    m_shortcuts[id] = info;

    // apply the default shortcut to the action
    action->setShortcut(defaultShortcut);
}

void ShortcutManager::setShortcut(const QString& actionId, const QKeySequence& shortcut) {
    if (!m_shortcuts.contains(actionId)) {
        return;
    }

    ShortcutInfo& info = m_shortcuts[actionId];
    info.currentShortcut = shortcut;

    if (info.action) {
        info.action->setShortcut(shortcut);
    }

    emit shortcutChanged(actionId, shortcut);
}

QKeySequence ShortcutManager::shortcut(const QString& actionId) const {
    if (m_shortcuts.contains(actionId)) {
        return m_shortcuts[actionId].currentShortcut;
    }
    return QKeySequence();
}

QKeySequence ShortcutManager::defaultShortcut(const QString& actionId) const {
    if (m_shortcuts.contains(actionId)) {
        return m_shortcuts[actionId].defaultShortcut;
    }
    return QKeySequence();
}

void ShortcutManager::resetToDefaults() {
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        resetToDefault(it.key());
    }
}

void ShortcutManager::resetToDefault(const QString& actionId) {
    if (!m_shortcuts.contains(actionId)) {
        return;
    }

    setShortcut(actionId, m_shortcuts[actionId].defaultShortcut);
}

QList<ShortcutInfo> ShortcutManager::allShortcuts() const { return m_shortcuts.values(); }

bool ShortcutManager::hasConflict(const QString& actionId, const QKeySequence& shortcut) const {
    if (shortcut.isEmpty()) {
        return false;
    }

    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        if (it.key() != actionId && it.value().currentShortcut == shortcut) {
            return true;
        }
    }
    return false;
}

QString ShortcutManager::conflictingAction(const QKeySequence& shortcut) const {
    if (shortcut.isEmpty()) {
        return QString();
    }

    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        if (it.value().currentShortcut == shortcut) {
            return it.key();
        }
    }
    return QString();
}

void ShortcutManager::setNumpadCueJumpEnabled(bool enabled) {
    m_numpadEnabled = enabled;
    if (!enabled) {
        clearKeypadBuffer();
    }
}

bool ShortcutManager::handleKeyPress(QKeyEvent* event) {
    if (!m_numpadEnabled) {
        return false;
    }

    int key = event->key();

    // handle numeric keypad keys
    if (isNumpadKey(key)) {
        int digit = numpadKeyToDigit(key);
        if (digit >= 0) {
            m_keypadBuffer += QString::number(digit);
            emit keypadBufferChanged(m_keypadBuffer);
            m_keypadTimer.start();
            return true;
        }
    }


    // handle decimal point
    if (key == Qt::Key_Period || key == Qt::Key_Comma) {
        if (!m_keypadBuffer.contains('.')) {
            if (m_keypadBuffer.isEmpty()) {
                m_keypadBuffer = "0";
            }
            m_keypadBuffer += '.';
            emit keypadBufferChanged(m_keypadBuffer);
            m_keypadTimer.start();
            return true;
        }
    }


    // handle Enter key to confirm cue number
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (!m_keypadBuffer.isEmpty()) {
            bool ok;
            double cueNumber = m_keypadBuffer.toDouble(&ok);
            if (ok) {
                emit numpadCueNumberEntered(cueNumber);
            }
            clearKeypadBuffer();
            return true;
        }
    }


    // handle Escape to clear buffer
    if (key == Qt::Key_Escape) {
        if (!m_keypadBuffer.isEmpty()) {
            clearKeypadBuffer();
            return true;
        }
    }


    // handle Backspace to remove last digit
    if (key == Qt::Key_Backspace) {
        if (!m_keypadBuffer.isEmpty()) {
            m_keypadBuffer.chop(1);
            emit keypadBufferChanged(m_keypadBuffer);
            if (m_keypadBuffer.isEmpty()) {
                m_keypadTimer.stop();
            } else {
                m_keypadTimer.start();
            }
            return true;
        }
    }

    return false;
}

void ShortcutManager::clearKeypadBuffer() {
    m_keypadBuffer.clear();
    m_keypadTimer.stop();
    emit keypadBufferChanged(m_keypadBuffer);
}

void ShortcutManager::saveToSettings() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("Shortcuts");

    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        settings.setValue(it.key(), it.value().currentShortcut.toString());
    }

    settings.endGroup();
    settings.setValue("NumpadCueJumpEnabled", m_numpadEnabled);
}

void ShortcutManager::loadFromSettings() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("Shortcuts");

    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        QString saved = settings.value(it.key()).toString();
        if (!saved.isEmpty()) {
            setShortcut(it.key(), QKeySequence(saved));
        }
    }

    settings.endGroup();
    m_numpadEnabled = settings.value("NumpadCueJumpEnabled", true).toBool();
}

void ShortcutManager::onKeypadTimeout() {
    if (!m_keypadBuffer.isEmpty()) {
        bool ok;
        double cueNumber = m_keypadBuffer.toDouble(&ok);
        if (ok) {
            emit numpadCueNumberEntered(cueNumber);
        }
        clearKeypadBuffer();
    }
}

bool ShortcutManager::isNumpadKey(int key) const { return (key >= Qt::Key_0 && key <= Qt::Key_9); }

int ShortcutManager::numpadKeyToDigit(int key) const {
    if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        return key - Qt::Key_0;
    }
    return -1;
}

} // namespace OpenMix
