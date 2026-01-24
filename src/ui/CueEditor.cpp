#include "CueEditor.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>

namespace OpenMix {

CueEditor::CueEditor(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    setupUi();
    setEnabled(false);
}

void CueEditor::setupUi() {
    setMinimumSize(280, 500);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    m_mainLayout = new QVBoxLayout(this);

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
    m_typeCombo->addItem(tr("Stop"), static_cast<int>(CueType::Stop));
    m_typeCombo->addItem(tr("Go To"), static_cast<int>(CueType::GoTo));
    m_typeCombo->addItem(tr("Wait"), static_cast<int>(CueType::Wait));
    basicLayout->addRow(tr("Type:"), m_typeCombo);

    m_mainLayout->addWidget(basicGroup);

    // timing group
    QGroupBox* timingGroup = new QGroupBox(tr("Auto-Follow"), this);
    QFormLayout* timingLayout = new QFormLayout(timingGroup);

    m_autoFollowCheck = new QCheckBox(tr("Enable"), this);
    timingLayout->addRow(tr("Auto-follow:"), m_autoFollowCheck);

    m_autoFollowDelaySpin = new QDoubleSpinBox(this);
    m_autoFollowDelaySpin->setRange(0.0, 999.0);
    m_autoFollowDelaySpin->setDecimals(1);
    m_autoFollowDelaySpin->setSingleStep(0.5);
    m_autoFollowDelaySpin->setSuffix(tr(" sec"));
    m_autoFollowDelaySpin->setEnabled(false);
    timingLayout->addRow(tr("Delay:"), m_autoFollowDelaySpin);

    m_mainLayout->addWidget(timingGroup);

    // DCA targeting section
    createDCATargetingSection();
    m_mainLayout->addWidget(m_dcaTargetingGroup);

    // DCA overrides section
    m_dcaOverridesGroup = new QGroupBox(tr("DCA Overrides (Mute/Label)"), this);
    QVBoxLayout* overridesLayout = new QVBoxLayout(m_dcaOverridesGroup);

    m_dcaOverridesScroll = new QScrollArea(this);
    m_dcaOverridesScroll->setWidgetResizable(true);
    m_dcaOverridesScroll->setFrameShape(QFrame::NoFrame);
    m_dcaOverridesScroll->setMaximumHeight(200);

    QWidget* overridesContent = new QWidget();
    QVBoxLayout* overridesContentLayout = new QVBoxLayout(overridesContent);
    overridesContentLayout->setContentsMargins(0, 0, 0, 0);
    overridesContentLayout->setSpacing(4);

    // create override widgets for 8 DCAs (updated based on mixer)
    for (int i = 1; i <= 8; ++i) {
        QGroupBox* dcaBox = new QGroupBox(tr("DCA %1").arg(i), overridesContent);
        QGridLayout* dcaLayout = new QGridLayout(dcaBox);
        dcaLayout->setContentsMargins(4, 4, 4, 4);
        dcaLayout->setSpacing(4);

        DCAOverrideWidgets widgets;

        widgets.enableMute = new QCheckBox(tr("Set Mute:"), dcaBox);
        widgets.muteValue = new QCheckBox(tr("Muted"), dcaBox);
        widgets.muteValue->setEnabled(false);
        dcaLayout->addWidget(widgets.enableMute, 0, 0);
        dcaLayout->addWidget(widgets.muteValue, 0, 1);

        widgets.enableLabel = new QCheckBox(tr("Set Label:"), dcaBox);
        widgets.labelValue = new QLineEdit(dcaBox);
        widgets.labelValue->setPlaceholderText(tr("DCA label"));
        widgets.labelValue->setEnabled(false);
        widgets.labelValue->setMaxLength(12);
        dcaLayout->addWidget(widgets.enableLabel, 1, 0);
        dcaLayout->addWidget(widgets.labelValue, 1, 1);

        // connect enable checkboxes to enable/disable value widgets
        connect(widgets.enableMute, &QCheckBox::toggled, widgets.muteValue, &QCheckBox::setEnabled);
        connect(widgets.enableLabel, &QCheckBox::toggled, widgets.labelValue,
                &QLineEdit::setEnabled);

        // connect changes to save
        int dca = i;
        connect(widgets.enableMute, &QCheckBox::toggled,
                [this, dca]() { onDCAOverrideChanged(dca); });
        connect(widgets.muteValue, &QCheckBox::toggled,
                [this, dca]() { onDCAOverrideChanged(dca); });
        connect(widgets.enableLabel, &QCheckBox::toggled,
                [this, dca]() { onDCAOverrideChanged(dca); });
        connect(widgets.labelValue, &QLineEdit::textChanged,
                [this, dca]() { onDCAOverrideChanged(dca); });

        m_dcaOverrideWidgets.append(widgets);
        overridesContentLayout->addWidget(dcaBox);
    }

    overridesContentLayout->addStretch();
    m_dcaOverridesScroll->setWidget(overridesContent);
    overridesLayout->addWidget(m_dcaOverridesScroll);

    m_mainLayout->addWidget(m_dcaOverridesGroup);

    // notes group
    QGroupBox* notesGroup = new QGroupBox(tr("Notes"), this);
    QVBoxLayout* notesLayout = new QVBoxLayout(notesGroup);

    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setPlaceholderText(tr("Enter cue notes..."));
    m_notesEdit->setMaximumHeight(80);
    notesLayout->addWidget(m_notesEdit);

    m_mainLayout->addWidget(notesGroup);

    m_mainLayout->addStretch();

    // connect signals
    connect(m_numberSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &CueEditor::onNumberChanged);
    connect(m_nameEdit, &QLineEdit::textChanged, this, &CueEditor::onNameChanged);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CueEditor::onTypeChanged);
    connect(m_autoFollowCheck, &QCheckBox::toggled, this, &CueEditor::onAutoFollowChanged);
    connect(m_autoFollowDelaySpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &CueEditor::onAutoFollowDelayChanged);
    connect(m_notesEdit, &QTextEdit::textChanged, this, &CueEditor::onNotesChanged);
}

void CueEditor::addBottomWidget(QWidget* widget) {
    if (m_mainLayout && widget) {
        // insert before stretch
        m_mainLayout->addWidget(widget);
    }
}

void CueEditor::createDCATargetingSection() {
    m_dcaTargetingGroup = new QGroupBox(tr("Target DCAs"), this);
    QVBoxLayout* layout = new QVBoxLayout(m_dcaTargetingGroup);

    m_targetAllDCAsCheck = new QCheckBox(tr("Target All DCAs"), this);
    m_targetAllDCAsCheck->setChecked(true);
    layout->addWidget(m_targetAllDCAsCheck);

    QWidget* dcaChecksWidget = new QWidget(this);
    QGridLayout* dcaGrid = new QGridLayout(dcaChecksWidget);
    dcaGrid->setContentsMargins(0, 0, 0, 0);
    dcaGrid->setSpacing(4);

    // create checkboxes for 8 DCAs (2 rows x 4 cols)
    for (int i = 1; i <= 8; ++i) {
        QCheckBox* check = new QCheckBox(tr("DCA %1").arg(i), dcaChecksWidget);
        int row = (i - 1) / 4;
        int col = (i - 1) % 4;
        dcaGrid->addWidget(check, row, col);
        m_dcaTargetChecks.append(check);

        // when individual DCA is checked, uncheck "target all"
        connect(check, &QCheckBox::toggled, this, [this](bool checked) {
            if (m_updatingUi)
                return;
            if (checked && m_targetAllDCAsCheck->isChecked()) {
                m_targetAllDCAsCheck->blockSignals(true);
                m_targetAllDCAsCheck->setChecked(false);
                m_targetAllDCAsCheck->blockSignals(false);
            }
            onTargetedDCAsChanged();
        });
    }

    layout->addWidget(dcaChecksWidget);

    // when "target all" is checked, uncheck individual DCAs
    connect(m_targetAllDCAsCheck, &QCheckBox::toggled, [this](bool checked) {
        if (m_updatingUi)
            return;
        if (checked) {
            for (QCheckBox* cb : m_dcaTargetChecks) {
                cb->blockSignals(true);
                cb->setChecked(false);
                cb->blockSignals(false);
            }
        }
        onTargetedDCAsChanged();
    });
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

        // map cue type to combo index
        int typeIndex = 0;
        switch (cue->type()) {
        case CueType::Snapshot:
            typeIndex = 0;
            break;
        case CueType::Stop:
            typeIndex = 1;
            break;
        case CueType::GoTo:
            typeIndex = 2;
            break;
        case CueType::Wait:
            typeIndex = 3;
            break;
        case CueType::Macro:
            typeIndex = 0;
            break; // macro not in combo
        }
        m_typeCombo->setCurrentIndex(typeIndex);

        m_autoFollowCheck->setChecked(cue->autoFollow());
        m_autoFollowDelaySpin->setValue(cue->autoFollowDelay());
        m_autoFollowDelaySpin->setEnabled(cue->autoFollow());
        m_notesEdit->setPlainText(cue->notes());

        // DCA targeting
        QSet<int> targetedDCAs = cue->targetedDCAs();
        m_targetAllDCAsCheck->setChecked(targetedDCAs.isEmpty());
        for (int i = 0; i < m_dcaTargetChecks.size(); ++i) {
            int dca = i + 1;
            m_dcaTargetChecks[i]->setChecked(targetedDCAs.contains(dca));
        }

        // DCA overrides
        updateDCAOverridesUI();
    } else {
        m_numberSpin->setValue(0);
        m_nameEdit->clear();
        m_typeCombo->setCurrentIndex(0);
        m_autoFollowCheck->setChecked(false);
        m_autoFollowDelaySpin->setValue(0);
        m_notesEdit->clear();
        m_targetAllDCAsCheck->setChecked(true);
        for (QCheckBox* cb : m_dcaTargetChecks) {
            cb->setChecked(false);
        }
    }

    m_updatingUi = false;
}

void CueEditor::updateDCAOverridesUI() {
    Cue* cue = currentCue();
    if (!cue)
        return;

    QMap<int, DCAOverride> overrides = cue->dcaOverrides();

    for (int i = 0; i < m_dcaOverrideWidgets.size(); ++i) {
        int dca = i + 1;
        DCAOverride override = overrides.value(dca);
        DCAOverrideWidgets& widgets = m_dcaOverrideWidgets[i];

        widgets.enableMute->setChecked(override.mute.has_value());
        widgets.muteValue->setChecked(override.mute.value_or(false));
        widgets.muteValue->setEnabled(override.mute.has_value());

        widgets.enableLabel->setChecked(override.label.has_value());
        widgets.labelValue->setText(override.label.value_or(""));
        widgets.labelValue->setEnabled(override.label.has_value());
    }
}

void CueEditor::setEnabled(bool enabled) {
    m_numberSpin->setEnabled(enabled);
    m_nameEdit->setEnabled(enabled);
    m_typeCombo->setEnabled(enabled);
    m_autoFollowCheck->setEnabled(enabled);
    m_autoFollowDelaySpin->setEnabled(enabled && m_autoFollowCheck->isChecked());
    m_notesEdit->setEnabled(enabled);
    m_dcaTargetingGroup->setEnabled(enabled);
    m_dcaOverridesGroup->setEnabled(enabled);
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
        CueType type = CueType::Snapshot;
        switch (index) {
        case 0:
            type = CueType::Snapshot;
            break;
        case 1:
            type = CueType::Stop;
            break;
        case 2:
            type = CueType::GoTo;
            break;
        case 3:
            type = CueType::Wait;
            break;
        }
        cue->setType(type);
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

void CueEditor::onTargetedDCAsChanged() {
    if (m_updatingUi)
        return;

    Cue* cue = currentCue();
    if (!cue)
        return;

    if (m_targetAllDCAsCheck->isChecked()) {
        cue->clearTargetedDCAs();
    } else {
        QSet<int> targeted;
        for (int i = 0; i < m_dcaTargetChecks.size(); ++i) {
            if (m_dcaTargetChecks[i]->isChecked()) {
                targeted.insert(i + 1);
            }
        }
        cue->setTargetedDCAs(targeted);
    }

    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onDCAOverrideChanged(int dca) {
    if (m_updatingUi)
        return;

    Cue* cue = currentCue();
    if (!cue || dca < 1 || dca > m_dcaOverrideWidgets.size())
        return;

    DCAOverrideWidgets& widgets = m_dcaOverrideWidgets[dca - 1];
    DCAOverride override;

    if (widgets.enableMute->isChecked()) {
        override.mute = widgets.muteValue->isChecked();
    }

    if (widgets.enableLabel->isChecked()) {
        override.label = widgets.labelValue->text();
    }

    cue->setDCAOverride(dca, override);
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

} // namespace OpenMix
