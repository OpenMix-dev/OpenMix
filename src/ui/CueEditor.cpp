#include "CueEditor.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/Show.h"
#include "protocol/MixerProtocol.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace StageBlend {

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

} // namespace StageBlend
