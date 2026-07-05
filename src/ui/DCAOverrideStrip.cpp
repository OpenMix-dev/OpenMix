#include "DCAOverrideStrip.h"
#include "theme/Theme.h"
#include <QCompleter>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace OpenMix {

DCAOverrideStrip::DCAOverrideStrip(int dcaNumber, QAbstractItemModel* completionModel,
                                   QWidget* parent)
    : QFrame(parent), m_dcaNumber(dcaNumber) {
    setStyleSheet(QString("DCAOverrideStrip { border: 1px solid %1; border-radius: 4px; }")
                      .arg(Theme::Colors::BorderSubtle));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(Theme::Spacing::XS, Theme::Spacing::XS, Theme::Spacing::XS,
                               Theme::Spacing::XS);
    layout->setSpacing(2);

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(4);

    m_titleLabel = new QLabel(tr("DCA %1").arg(m_dcaNumber), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(9);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    m_labelEdit = new QLineEdit(this);
    m_labelEdit->setPlaceholderText(tr("(no label change)"));
    m_labelEdit->setToolTip(
        tr("Typing a role, actor, or ensemble name assigns their channels to this DCA "
           "for this cue; empty leaves the console label untouched"));
    m_labelEdit->setClearButtonEnabled(true);
    m_labelEdit->setMaxLength(32);
    if (completionModel) {
        auto* completer = new QCompleter(completionModel, this);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        m_labelEdit->setCompleter(completer);
        connect(completer, QOverload<const QString&>::of(&QCompleter::activated), this,
                [this](const QString&) { emit labelCommitted(m_dcaNumber); });
    }

    m_muteButton = new QPushButton(tr("M"), this);
    m_muteButton->setFixedSize(24, 24);
    m_muteButton->setFocusPolicy(Qt::TabFocus);

    topRow->addWidget(m_titleLabel);
    topRow->addWidget(m_labelEdit, 1);
    topRow->addWidget(m_muteButton);
    layout->addLayout(topRow);

    m_assignInfo = new QLabel(this);
    m_assignInfo->setWordWrap(true);
    m_assignInfo->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
    m_assignInfo->setVisible(false);
    layout->addWidget(m_assignInfo);

    connect(m_muteButton, &QPushButton::clicked, this, &DCAOverrideStrip::cycleMute);
    connect(m_labelEdit, &QLineEdit::textChanged, this, [this]() {
        if (!m_updating)
            emit overrideEdited(m_dcaNumber);
    });
    // name resolution runs at commit time, not per keystroke, so a prefix of
    // one name never assigns while typing another ("Anna" vs "Annabelle")
    connect(m_labelEdit, &QLineEdit::editingFinished, this,
            [this]() { emit labelCommitted(m_dcaNumber); });

    updateMuteButton();
}

void DCAOverrideStrip::setOverride(const DCAOverride& override) {
    m_updating = true;
    m_mute = override.mute;
    m_labelEdit->setText(override.label.value_or(QString()));
    updateMuteButton();
    m_updating = false;
}

DCAOverride DCAOverrideStrip::overrideValue() const {
    DCAOverride override;
    override.mute = m_mute;
    const QString text = m_labelEdit->text();
    if (!text.isEmpty())
        override.label = text;
    return override;
}

QString DCAOverrideStrip::labelText() const { return m_labelEdit->text(); }

void DCAOverrideStrip::setAssignInfo(const QString& text) {
    m_assignInfo->setText(text);
    m_assignInfo->setVisible(!text.isEmpty());
}

void DCAOverrideStrip::cycleMute() {
    // no-change → muted → unmuted → no-change
    if (!m_mute.has_value())
        m_mute = true;
    else if (*m_mute)
        m_mute = false;
    else
        m_mute.reset();
    updateMuteButton();
    if (!m_updating)
        emit overrideEdited(m_dcaNumber);
}

void DCAOverrideStrip::updateMuteButton() {
    m_muteButton->setStyleSheet(Theme::muteButtonStyle(m_mute));
    if (!m_mute.has_value())
        m_muteButton->setToolTip(tr("No mute change on fire (click to cycle)"));
    else if (*m_mute)
        m_muteButton->setToolTip(tr("Mutes DCA %1 on fire (click to cycle)").arg(m_dcaNumber));
    else
        m_muteButton->setToolTip(tr("Unmutes DCA %1 on fire (click to cycle)").arg(m_dcaNumber));
}

} // namespace OpenMix
