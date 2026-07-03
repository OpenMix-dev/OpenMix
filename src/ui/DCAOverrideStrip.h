#pragma once

#include "core/Cue.h" // DCAOverride
#include <QFrame>

class QAbstractItemModel;
class QLabel;
class QLineEdit;
class QPushButton;

namespace OpenMix {

// One cue DCA override as a compact strip, visually matching the mixer-feedback
// DCA widgets (bold identity label, shared mute-button colors): a label
// line-edit (empty = no label change) and a tri-state mute button cycling
// no-change → muted (red) → unmuted (green).
class DCAOverrideStrip : public QFrame {
    Q_OBJECT

  public:
    explicit DCAOverrideStrip(int dcaNumber, QAbstractItemModel* completionModel,
                              QWidget* parent = nullptr);

    [[nodiscard]] int dcaNumber() const noexcept { return m_dcaNumber; }

    // non-emitting; used when loading a cue into the editor
    void setOverride(const DCAOverride& override);
    [[nodiscard]] DCAOverride overrideValue() const;

    [[nodiscard]] QString labelText() const;
    void setAssignInfo(const QString& text); // resolved members line; empty hides

  signals:
    void overrideEdited(int dca);  // mute cycled or label text changed
    void labelCommitted(int dca);  // label editing finished / completion picked

  private:
    void cycleMute();
    void updateMuteButton();

    int m_dcaNumber;
    std::optional<bool> m_mute; // nullopt = no mute change
    bool m_updating = false;

    QLabel* m_titleLabel = nullptr;
    QLineEdit* m_labelEdit = nullptr;
    QPushButton* m_muteButton = nullptr;
    QLabel* m_assignInfo = nullptr;
};

} // namespace OpenMix
