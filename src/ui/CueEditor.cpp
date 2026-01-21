#include "CueEditor.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/LiveEditSession.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "protocol/MixerProtocol.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace OpenMix {

CueEditor::CueEditor(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    setupUi();
    setEnabled(false);
}

void CueEditor::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // basic properties group
    QGroupBox* basicGroup = new QGroupBox(tr("Cue Properties"), this);
    QFormLayout* basicLayout = new QFormLayout(basicGroup);

    m_numberSpin = new QDoubleSpinBox(this);
    m_numberSpin->setRange(0.0, 9999.9);
    m_numberSpin->setDecimals(1);
    m_numberSpin->setSingleStep(1.0);
    basicLayout->addRow(tr("Number:"), m_numberSpin);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("Enter cue name"));
    basicLayout->addRow(tr("Name:"), m_nameEdit);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("Snapshot"), static_cast<int>(CueType::Snapshot));
    m_typeCombo->addItem(tr("Fade"), static_cast<int>(CueType::Fade));
    m_typeCombo->addItem(tr("Stop"), static_cast<int>(CueType::Stop));
    m_typeCombo->addItem(tr("Go To"), static_cast<int>(CueType::GoTo));
    m_typeCombo->addItem(tr("Wait"), static_cast<int>(CueType::Wait));
    basicLayout->addRow(tr("Type:"), m_typeCombo);

    mainLayout->addWidget(basicGroup);

    // timing group
    QGroupBox* timingGroup = new QGroupBox(tr("Timing"), this);
    QFormLayout* timingLayout = new QFormLayout(timingGroup);

    m_fadeTimeSpin = new QDoubleSpinBox(this);
    m_fadeTimeSpin->setRange(0.0, 999.0);
    m_fadeTimeSpin->setDecimals(1);
    m_fadeTimeSpin->setSingleStep(0.5);
    m_fadeTimeSpin->setSuffix(tr(" sec"));
    timingLayout->addRow(tr("Fade Time:"), m_fadeTimeSpin);

    m_autoFollowCheck = new QCheckBox(tr("Auto-follow"), this);
    timingLayout->addRow(QString(), m_autoFollowCheck);

    m_autoFollowDelaySpin = new QDoubleSpinBox(this);
    m_autoFollowDelaySpin->setRange(0.0, 999.0);
    m_autoFollowDelaySpin->setDecimals(1);
    m_autoFollowDelaySpin->setSingleStep(0.5);
    m_autoFollowDelaySpin->setSuffix(tr(" sec"));
    m_autoFollowDelaySpin->setEnabled(false);
    timingLayout->addRow(tr("Follow Delay:"), m_autoFollowDelaySpin);

    mainLayout->addWidget(timingGroup);

    // capture group
    QGroupBox* captureGroup = new QGroupBox(tr("Mixer Snapshot"), this);
    QVBoxLayout* captureLayout = new QVBoxLayout(captureGroup);

    m_captureButton = new QPushButton(tr("Capture Current Mixer State"), this);
    captureLayout->addWidget(m_captureButton);

    QLabel* captureInfo =
        new QLabel(tr("Captures faders, mutes, & DCA settings from the connected mixer."), this);
    captureInfo->setWordWrap(true);
    captureInfo->setStyleSheet("color: gray; font-size: 10px;");
    captureLayout->addWidget(captureInfo);

    mainLayout->addWidget(captureGroup);

    // live edit group
    QGroupBox* liveEditGroup = new QGroupBox(tr("Live Edit"), this);
    QVBoxLayout* liveEditLayout = new QVBoxLayout(liveEditGroup);

    m_liveEditStatusLabel = new QLabel(tr("Not active"), this);
    m_liveEditStatusLabel->setStyleSheet("color: gray; font-style: italic;");
    liveEditLayout->addWidget(m_liveEditStatusLabel);

    QHBoxLayout* liveEditButtonLayout = new QHBoxLayout();

    m_startLiveEditButton = new QPushButton(tr("Edit Live"), this);
    m_startLiveEditButton->setToolTip(tr("Start editing this cue with live mixer feedback"));
    liveEditButtonLayout->addWidget(m_startLiveEditButton);

    m_commitLiveEditButton = new QPushButton(tr("Commit"), this);
    m_commitLiveEditButton->setToolTip(tr("Save live edits to this cue"));
    m_commitLiveEditButton->setVisible(false);
    liveEditButtonLayout->addWidget(m_commitLiveEditButton);

    m_cancelLiveEditButton = new QPushButton(tr("Revert"), this);
    m_cancelLiveEditButton->setToolTip(tr("Cancel live edits and restore original values"));
    m_cancelLiveEditButton->setVisible(false);
    liveEditButtonLayout->addWidget(m_cancelLiveEditButton);

    liveEditLayout->addLayout(liveEditButtonLayout);

    mainLayout->addWidget(liveEditGroup);

    // notes group
    QGroupBox* notesGroup = new QGroupBox(tr("Notes"), this);
    QVBoxLayout* notesLayout = new QVBoxLayout(notesGroup);

    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setPlaceholderText(tr("Enter cue notes..."));
    m_notesEdit->setMaximumHeight(100);
    notesLayout->addWidget(m_notesEdit);

    mainLayout->addWidget(notesGroup);

    mainLayout->addStretch();

    // connect signals
    connect(m_numberSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &CueEditor::onNumberChanged);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &CueEditor::onNameChanged);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CueEditor::onTypeChanged);
    connect(m_fadeTimeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &CueEditor::onFadeTimeChanged);
    connect(m_autoFollowCheck, &QCheckBox::toggled, this, &CueEditor::onAutoFollowChanged);
    connect(m_autoFollowDelaySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &CueEditor::onAutoFollowDelayChanged);
    connect(m_notesEdit, &QTextEdit::textChanged, this, &CueEditor::onNotesChanged);
    connect(m_captureButton, &QPushButton::clicked, this, &CueEditor::onCaptureSnapshot);
    connect(m_startLiveEditButton, &QPushButton::clicked, this, &CueEditor::onStartLiveEdit);
    connect(m_commitLiveEditButton, &QPushButton::clicked, this, &CueEditor::onCommitLiveEdit);
    connect(m_cancelLiveEditButton, &QPushButton::clicked, this, &CueEditor::onCancelLiveEdit);

    // connect to live edit session signals if available
    if (m_app && m_app->playbackEngine() && m_app->playbackEngine()->liveEditSession()) {
        LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
        connect(session, &LiveEditSession::modeChanged, this, &CueEditor::updateLiveEditState);
        connect(session, &LiveEditSession::sessionStarted, this, &CueEditor::updateLiveEditState);
        connect(session, &LiveEditSession::sessionEnded, this, &CueEditor::updateLiveEditState);
    }
}

bool CueEditor::hasFocus() const { return m_nameEdit->hasFocus() || m_notesEdit->hasFocus(); }

void CueEditor::setCue(int index) {
    m_currentIndex = index;
    updateFromCue();
    setEnabled(index >= 0);
}

Cue* CueEditor::currentCue() {
    if (m_currentIndex < 0)
        return nullptr;
    CueList* list = m_app->show()->cueList();
    if (m_currentIndex >= list->count())
        return nullptr;
    return &(*list)[m_currentIndex];
}

void CueEditor::updateFromCue() {
    m_updatingUi = true;

    Cue* cue = currentCue();
    if (cue) {
        m_numberSpin->setValue(cue->number());
        m_nameEdit->setText(cue->name());
        m_typeCombo->setCurrentIndex(static_cast<int>(cue->type()));
        m_fadeTimeSpin->setValue(cue->fadeTime());
        m_autoFollowCheck->setChecked(cue->autoFollow());
        m_autoFollowDelaySpin->setValue(cue->autoFollowDelay());
        m_autoFollowDelaySpin->setEnabled(cue->autoFollow());
        m_notesEdit->setPlainText(cue->notes());
    } else {
        m_numberSpin->setValue(0);
        m_nameEdit->clear();
        m_typeCombo->setCurrentIndex(0);
        m_fadeTimeSpin->setValue(0);
        m_autoFollowCheck->setChecked(false);
        m_autoFollowDelaySpin->setValue(0);
        m_notesEdit->clear();
    }

    m_updatingUi = false;
}

void CueEditor::setEnabled(bool enabled) {
    m_numberSpin->setEnabled(enabled);
    m_nameEdit->setEnabled(enabled);
    m_typeCombo->setEnabled(enabled);
    m_fadeTimeSpin->setEnabled(enabled);
    m_autoFollowCheck->setEnabled(enabled);
    m_autoFollowDelaySpin->setEnabled(enabled && m_autoFollowCheck->isChecked());
    m_notesEdit->setEnabled(enabled);
    m_captureButton->setEnabled(enabled && m_app->mixer() && m_app->mixer()->isConnected());
}

void CueEditor::onNumberChanged(double value) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (cue) {
        cue->setNumber(value);
        m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
        emit cueModified();
    }
}

void CueEditor::onNameChanged(const QString& text) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (cue) {
        cue->setName(text);
        m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
        emit cueModified();
    }
}

void CueEditor::onTypeChanged(int index) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (cue) {
        cue->setType(static_cast<CueType>(index));
        m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
        emit cueModified();
    }
}

void CueEditor::onFadeTimeChanged(double value) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (cue) {
        cue->setFadeTime(value);
        m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
        emit cueModified();
    }
}

void CueEditor::onAutoFollowChanged(bool checked) {
    if (m_updatingUi)
        return;
    m_autoFollowDelaySpin->setEnabled(checked);
    Cue* cue = currentCue();
    if (cue) {
        cue->setAutoFollow(checked);
        m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
        emit cueModified();
    }
}

void CueEditor::onAutoFollowDelayChanged(double value) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (cue) {
        cue->setAutoFollowDelay(value);
        m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
        emit cueModified();
    }
}

void CueEditor::onNotesChanged() {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (cue) {
        cue->setNotes(m_notesEdit->toPlainText());
        m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
        emit cueModified();
    }
}

void CueEditor::onCaptureSnapshot() {
    Cue* cue = currentCue();
    MixerProtocol* mixer = m_app->mixer();
    if (cue && mixer && mixer->isConnected()) {
        mixer->captureSnapshot(*cue);
        m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
        emit cueModified();
    }
}

void CueEditor::onStartLiveEdit() {
    if (!m_app || !m_app->playbackEngine()) {
        return;
    }

    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (!session) {
        return;
    }

    Cue* cue = currentCue();
    if (cue) {
        session->startLiveEdit(cue->id());
    }
}

void CueEditor::onCommitLiveEdit() {
    if (!m_app || !m_app->playbackEngine()) {
        return;
    }

    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session && session->isActive()) {
        session->commitToCurrentCue();
        emit cueModified();
    }
}

void CueEditor::onCancelLiveEdit() {
    if (!m_app || !m_app->playbackEngine()) {
        return;
    }

    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session && session->isActive()) {
        session->cancel();
    }
}

void CueEditor::updateLiveEditState() {
    if (!m_app || !m_app->playbackEngine()) {
        return;
    }

    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    bool isActive = session && session->isActive();
    bool isThisCue = isActive && currentCue() && session->activeCueId() == currentCue()->id();

    // update visibility
    m_startLiveEditButton->setVisible(!isActive);
    m_commitLiveEditButton->setVisible(isActive && isThisCue);
    m_cancelLiveEditButton->setVisible(isActive && isThisCue);

    // update status label
    if (!isActive) {
        m_liveEditStatusLabel->setText(tr("Not active"));
        m_liveEditStatusLabel->setStyleSheet("color: gray; font-style: italic;");
    } else if (isThisCue) {
        if (session->isPreview()) {
            m_liveEditStatusLabel->setText(tr("Preview mode - %1 edits").arg(session->editCount()));
            m_liveEditStatusLabel->setStyleSheet("color: #6699ff; font-weight: bold;");
        } else {
            m_liveEditStatusLabel->setText(tr("Live editing - %1 edits").arg(session->editCount()));
            m_liveEditStatusLabel->setStyleSheet("color: #44cc44; font-weight: bold;");
        }
    } else {
        m_liveEditStatusLabel->setText(tr("Editing another cue"));
        m_liveEditStatusLabel->setStyleSheet("color: orange; font-style: italic;");
    }

    // enable/disable start button
    bool canStart =
        m_currentIndex >= 0 && !isActive && m_app->mixer() && m_app->mixer()->isConnected();
    m_startLiveEditButton->setEnabled(canStart);

    // enable/disable commit/cancel
    m_commitLiveEditButton->setEnabled(isThisCue && session->hasEdits());
    m_cancelLiveEditButton->setEnabled(isThisCue);
}

} // namespace OpenMix
