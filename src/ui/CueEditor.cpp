#include "CueEditor.h"
#include "app/Application.h"
#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/FadeCurve.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>

namespace OpenMix {

CueEditor::CueEditor(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    if (m_app && m_app->show())
        m_actorLibrary = m_app->show()->actorProfileLibrary();

    setupUi();
    setEnabled(false);

    if (m_actorLibrary) {
        connect(m_actorLibrary, &ActorProfileLibrary::changed, this,
                &CueEditor::onActorLibraryChanged);
    }
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

    // fade transition group
    QGroupBox* fadeGroup = new QGroupBox(tr("Fade Transition"), this);
    QFormLayout* fadeLayout = new QFormLayout(fadeGroup);

    m_fadeTimeSpin = new QDoubleSpinBox(this);
    m_fadeTimeSpin->setRange(0.0, 600.0);
    m_fadeTimeSpin->setDecimals(2);
    m_fadeTimeSpin->setSingleStep(0.5);
    m_fadeTimeSpin->setSuffix(tr(" sec"));
    m_fadeTimeSpin->setToolTip(tr("Fade duration for fader/level moves on fire (0 = instant)"));
    fadeLayout->addRow(tr("Fade time:"), m_fadeTimeSpin);

    m_fadeCurveCombo = new QComboBox(this);
    m_fadeCurveCombo->addItem(tr("Linear"), static_cast<int>(FadeCurve::Linear));
    m_fadeCurveCombo->addItem(tr("Ease In-Out"), static_cast<int>(FadeCurve::EaseInOut));
    m_fadeCurveCombo->addItem(tr("Ease In"), static_cast<int>(FadeCurve::EaseIn));
    m_fadeCurveCombo->addItem(tr("Ease Out"), static_cast<int>(FadeCurve::EaseOut));
    fadeLayout->addRow(tr("Curve:"), m_fadeCurveCombo);

    m_mainLayout->addWidget(fadeGroup);

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

    // per-channel actor profile + level
    createChannelProfilesSection();
    m_mainLayout->addWidget(m_channelProfilesGroup);

    // linked QLab (DAW remote) cue
    QGroupBox* qlabGroup = new QGroupBox(tr("QLab / DAW Remote"), this);
    QFormLayout* qlabLayout = new QFormLayout(qlabGroup);
    m_qLabCueEdit = new QLineEdit(this);
    m_qLabCueEdit->setPlaceholderText(tr("QLab cue number / id"));
    m_qLabCueEdit->setToolTip(tr("Fire this QLab cue when the OpenMix cue executes"));
    qlabLayout->addRow(tr("QLab cue:"), m_qLabCueEdit);
    m_mainLayout->addWidget(qlabGroup);

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

    connect(m_fadeTimeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &CueEditor::onFadeTimeChanged);
    connect(m_fadeCurveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CueEditor::onFadeCurveChanged);
    connect(m_qLabCueEdit, &QLineEdit::textChanged, this, &CueEditor::onQLabCueChanged);
}

void CueEditor::createChannelProfilesSection() {
    m_channelProfilesGroup = new QGroupBox(tr("Channel Profiles & Levels"), this);
    QVBoxLayout* layout = new QVBoxLayout(m_channelProfilesGroup);
    layout->setContentsMargins(4, 4, 4, 4);

    QLabel* hint = new QLabel(
        tr("Per actor-channel: the profile slot applied on fire, and an optional fader level."),
        m_channelProfilesGroup);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
    layout->addWidget(hint);

    m_channelTable = new QTableWidget(0, 5, m_channelProfilesGroup);
    m_channelTable->setHorizontalHeaderLabels(
        {tr("Ch"), tr("Actor"), tr("Profile"), tr("Set"), tr("Level")});
    m_channelTable->verticalHeader()->setVisible(false);
    m_channelTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_channelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_channelTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_channelTable->setMaximumHeight(220);
    layout->addWidget(m_channelTable);
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

        // fade transition
        m_fadeTimeSpin->setValue(cue->fadeTime());
        int curveIdx = m_fadeCurveCombo->findData(static_cast<int>(cue->fadeCurve()));
        m_fadeCurveCombo->setCurrentIndex(curveIdx < 0 ? 0 : curveIdx);

        // linked QLab cue
        m_qLabCueEdit->setText(cue->qLabCue());

        // per-channel profile + level
        rebuildChannelTable();
        populateChannelTable();
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

        m_fadeTimeSpin->setValue(0.0);
        m_fadeCurveCombo->setCurrentIndex(0);
        m_qLabCueEdit->clear();
        if (m_channelTable)
            m_channelTable->setRowCount(0);
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
    m_fadeTimeSpin->setEnabled(enabled);
    m_fadeCurveCombo->setEnabled(enabled);
    m_qLabCueEdit->setEnabled(enabled);
    m_channelProfilesGroup->setEnabled(enabled);
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

void CueEditor::rebuildChannelTable() {
    if (!m_channelTable)
        return;
    m_channelTable->setRowCount(0);
    if (!m_actorLibrary)
        return;

    // one row per actor-assigned channel (deduped, lowest channel first)
    QList<Actor> actors = m_actorLibrary->actors();
    std::stable_sort(actors.begin(), actors.end(),
                     [](const Actor& a, const Actor& b) { return a.channel() < b.channel(); });

    const QStringList slotList = m_actorLibrary->profileSlots();
    QSet<int> seen;

    for (const Actor& a : actors) {
        const int ch = a.channel();
        if (seen.contains(ch))
            continue;
        seen.insert(ch);

        const int row = m_channelTable->rowCount();
        m_channelTable->insertRow(row);

        auto* chItem = new QTableWidgetItem(QString::number(ch));
        chItem->setFlags(Qt::ItemIsEnabled);
        m_channelTable->setItem(row, 0, chItem);

        auto* nameItem = new QTableWidgetItem(a.name().isEmpty() ? tr("(unnamed)") : a.name());
        nameItem->setFlags(Qt::ItemIsEnabled);
        m_channelTable->setItem(row, 1, nameItem);

        auto* profileCombo = new QComboBox(m_channelTable);
        profileCombo->addItem(tr("(none)"), QString());
        for (const QString& s : slotList)
            profileCombo->addItem(s, s);
        profileCombo->setProperty("channel", ch);
        connect(profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, ch]() { onChannelProfileChanged(ch); });
        m_channelTable->setCellWidget(row, 2, profileCombo);

        auto* setCheck = new QCheckBox(m_channelTable);
        setCheck->setToolTip(tr("Override this channel's fader level on fire"));
        setCheck->setProperty("channel", ch);
        connect(setCheck, &QCheckBox::toggled, this,
                [this, ch](bool on) { onChannelLevelToggled(ch, on); });
        m_channelTable->setCellWidget(row, 3, setCheck);

        auto* levelSpin = new QSpinBox(m_channelTable);
        levelSpin->setRange(0, 100);
        levelSpin->setSuffix(tr(" %"));
        levelSpin->setEnabled(false);
        levelSpin->setProperty("channel", ch);
        connect(levelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, ch]() { onChannelLevelChanged(ch); });
        m_channelTable->setCellWidget(row, 4, levelSpin);
    }
}

void CueEditor::populateChannelTable() {
    Cue* cue = currentCue();
    if (!cue || !m_channelTable)
        return;

    const QMap<int, QString> profiles = cue->channelProfiles();
    const QMap<int, double> levels = cue->channelLevels();

    for (int row = 0; row < m_channelTable->rowCount(); ++row) {
        auto* combo = qobject_cast<QComboBox*>(m_channelTable->cellWidget(row, 2));
        auto* check = qobject_cast<QCheckBox*>(m_channelTable->cellWidget(row, 3));
        auto* spin = qobject_cast<QSpinBox*>(m_channelTable->cellWidget(row, 4));
        if (!combo || !check || !spin)
            continue;
        const int ch = combo->property("channel").toInt();

        const int idx = combo->findData(profiles.value(ch));
        combo->setCurrentIndex(idx < 0 ? 0 : idx);

        const bool hasLevel = levels.contains(ch);
        check->setChecked(hasLevel);
        spin->setEnabled(hasLevel);
        spin->setValue(hasLevel ? std::clamp(static_cast<int>(levels.value(ch) * 100.0 + 0.5), 0, 100)
                                : 75);
    }
}

void CueEditor::onFadeTimeChanged(double value) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue)
        return;
    cue->setFadeTime(value);
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onFadeCurveChanged(int index) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue)
        return;
    cue->setFadeCurve(static_cast<FadeCurve>(m_fadeCurveCombo->itemData(index).toInt()));
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onQLabCueChanged(const QString& text) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue)
        return;
    cue->setQLabCue(text);
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onChannelProfileChanged(int channel) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue || !m_channelTable)
        return;

    QComboBox* combo = nullptr;
    for (int row = 0; row < m_channelTable->rowCount(); ++row) {
        auto* c = qobject_cast<QComboBox*>(m_channelTable->cellWidget(row, 2));
        if (c && c->property("channel").toInt() == channel) {
            combo = c;
            break;
        }
    }
    if (!combo)
        return;

    const QString slot = combo->currentData().toString();
    if (slot.isEmpty())
        cue->removeChannelProfile(channel);
    else
        cue->setChannelProfile(channel, slot);

    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onChannelLevelToggled(int channel, bool on) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue || !m_channelTable)
        return;

    QSpinBox* spin = nullptr;
    for (int row = 0; row < m_channelTable->rowCount(); ++row) {
        auto* s = qobject_cast<QSpinBox*>(m_channelTable->cellWidget(row, 4));
        if (s && s->property("channel").toInt() == channel) {
            spin = s;
            break;
        }
    }
    if (spin)
        spin->setEnabled(on);

    if (on)
        cue->setChannelLevel(channel, (spin ? spin->value() : 75) / 100.0);
    else
        cue->removeChannelLevel(channel);

    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onChannelLevelChanged(int channel) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue || !m_channelTable)
        return;

    QSpinBox* spin = nullptr;
    for (int row = 0; row < m_channelTable->rowCount(); ++row) {
        auto* s = qobject_cast<QSpinBox*>(m_channelTable->cellWidget(row, 4));
        if (s && s->property("channel").toInt() == channel) {
            spin = s;
            break;
        }
    }
    if (!spin || !spin->isEnabled())
        return;

    cue->setChannelLevel(channel, spin->value() / 100.0);
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onActorLibraryChanged() {
    // actors/slots changed (e.g. project loaded, or edited in Actor Setup):
    // refresh the per-channel table for the cue currently shown.
    if (m_currentIndex < 0)
        return;
    m_updatingUi = true;
    rebuildChannelTable();
    populateChannelTable();
    m_updatingUi = false;
}

} // namespace OpenMix
