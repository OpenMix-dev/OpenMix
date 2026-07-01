#include "ActiveCueInfoPanel.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"

#include <QFormLayout>
#include <QLabel>
#include <QStringList>
#include <QTextEdit>
#include <QVBoxLayout>

namespace OpenMix {

ActiveCueInfoPanel::ActiveCueInfoPanel(Application* app, QWidget* parent)
    : QWidget(parent), m_app(app) {
    setupUi();

    if (m_app && m_app->playbackEngine())
        connect(m_app->playbackEngine(), &PlaybackEngine::currentCueChanged, this,
                &ActiveCueInfoPanel::showCue);
    refresh();
}

void ActiveCueInfoPanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    QFormLayout* form = new QFormLayout();
    m_numberLabel = new QLabel(this);
    m_nameLabel = new QLabel(this);
    m_nameLabel->setWordWrap(true);
    m_typeLabel = new QLabel(this);
    m_fadeLabel = new QLabel(this);
    form->addRow(tr("Number:"), m_numberLabel);
    form->addRow(tr("Name:"), m_nameLabel);
    form->addRow(tr("Type:"), m_typeLabel);
    form->addRow(tr("Fade:"), m_fadeLabel);
    mainLayout->addLayout(form);

    m_detailsEdit = new QTextEdit(this);
    m_detailsEdit->setReadOnly(true);
    mainLayout->addWidget(m_detailsEdit, 1);
}

void ActiveCueInfoPanel::refresh() {
    showCue(m_app && m_app->playbackEngine() ? m_app->playbackEngine()->currentCueIndex() : -1);
}

void ActiveCueInfoPanel::showCue(int index) {
    const CueList* list = m_app && m_app->show() ? m_app->show()->cueList() : nullptr;
    if (!list || index < 0 || index >= list->count()) {
        m_numberLabel->setText(tr("--"));
        m_nameLabel->clear();
        m_typeLabel->clear();
        m_fadeLabel->clear();
        m_detailsEdit->clear();
        return;
    }

    const Cue& cue = list->at(index);
    m_numberLabel->setText(QString::number(cue.number(), 'f', 2));
    m_nameLabel->setText(cue.name().isEmpty() ? tr("(unnamed)") : cue.name());
    m_typeLabel->setText(cueTypeToString(cue.type()));
    m_fadeLabel->setText(cue.fadeTime() > 0.0 ? tr("%1 s").arg(cue.fadeTime(), 0, 'f', 1)
                                              : tr("instant"));

    QStringList lines;
    if (!cue.targetsAllDCAs())
        lines << tr("Targets %n DCA(s)", "", cue.targetedDCAs().size());
    if (!cue.dcaOverrides().isEmpty())
        lines << tr("%n DCA override(s)", "", cue.dcaOverrides().size());
    if (!cue.channelProfiles().isEmpty())
        lines << tr("%n channel profile(s)", "", cue.channelProfiles().size());
    if (!cue.channelLevels().isEmpty())
        lines << tr("%n channel level(s)", "", cue.channelLevels().size());
    if (!cue.channelPositions().isEmpty())
        lines << tr("%n channel position(s)", "", cue.channelPositions().size());
    if (!cue.fxMutes().isEmpty())
        lines << tr("%n FX mute(s)", "", cue.fxMutes().size());
    if (!cue.snippets().isEmpty())
        lines << tr("%n snippet(s)", "", cue.snippets().size());
    if (cue.skip())
        lines << tr("Skipped in standby advance");
    if (!cue.notes().isEmpty())
        lines << QString() << cue.notes();

    m_detailsEdit->setPlainText(lines.join('\n'));
}

} // namespace OpenMix
