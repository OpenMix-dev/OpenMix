#include "CueEditor.h"
#include "CollapsibleSection.h"
#include "DCAOverrideStrip.h"
#include "app/Application.h"
#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/Ensemble.h"
#include "core/FadeCurve.h"
#include "core/PlaybackEngine.h"
#include "core/Position.h"
#include "core/Show.h"
#include "theme/Theme.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
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
#include <QStringListModel>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>

#include <algorithm>

namespace OpenMix {

EnsembleLibrary* CueEditor::ensembleLibrary() const {
    return (m_app && m_app->show()) ? m_app->show()->ensembleLibrary() : nullptr;
}

CueEditor::CueEditor(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    if (m_app && m_app->show())
        m_actorLibrary = m_app->show()->actorProfileLibrary();

    m_actorCompletionModel = new QStringListModel(this);
    if (m_actorLibrary)
        m_actorCompletionModel->setStringList(
            m_actorLibrary->completionCandidates(ensembleLibrary()));

    setupUi();
    setEnabled(false);

    if (m_actorLibrary) {
        connect(m_actorLibrary, &ActorProfileLibrary::changed, this,
                &CueEditor::onActorLibraryChanged);
    }
    if (EnsembleLibrary* ensembles = ensembleLibrary()) {
        connect(ensembles, &EnsembleLibrary::changed, this, &CueEditor::onActorLibraryChanged);
    }

    // reflect the engine's check-mode state (it is engine-wide, not per-cue)
    if (m_app && m_app->playbackEngine()) {
        PlaybackEngine* engine = m_app->playbackEngine();
        const QSignalBlocker blocker(m_checkModeCheck);
        m_checkModeCheck->setChecked(engine->checkMode());
        connect(engine, &PlaybackEngine::checkModeChanged, this,
                &CueEditor::onEngineCheckModeChanged);
    }
    if (m_app)
        connect(m_app, &Application::dcaCountChanged, this, &CueEditor::onDcaCountChanged);
    updateGangsUI();
}

void CueEditor::setupUi() {
    setMinimumWidth(280);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    // the editor sections live inside a scroll area so a short pane scrolls
    // rather than crushing the groups on top of each other
    QWidget* content = new QWidget(this);
    m_mainLayout = new QVBoxLayout(content);

    // basic properties group
    auto* basicGroup = new CollapsibleSection(tr("Cue Properties"), this);
    QFormLayout* basicLayout = new QFormLayout();

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
    m_typeCombo->addItem(tr("Macro"), static_cast<int>(CueType::Macro));
    basicLayout->addRow(tr("Type:"), m_typeCombo);

    QWidget* colorRow = new QWidget(this);
    QHBoxLayout* colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    m_colorEdit = new QLineEdit(colorRow);
    m_colorEdit->setPlaceholderText(tr("#rrggbb"));
    m_colorPickButton = new QPushButton(tr("Pick..."), colorRow);
    colorLayout->addWidget(m_colorEdit);
    colorLayout->addWidget(m_colorPickButton);
    basicLayout->addRow(tr("Color:"), colorRow);

    m_skipCheck = new QCheckBox(tr("Skip on standby advance"), this);
    basicLayout->addRow(tr("Skip:"), m_skipCheck);

    basicGroup->setContentLayout(basicLayout);
    basicGroup->setExpanded(true); // primary info stays open
    m_mainLayout->addWidget(basicGroup);

    // timing group
    auto* timingGroup = new CollapsibleSection(tr("Auto-Follow"), this);
    QFormLayout* timingLayout = new QFormLayout();

    m_autoFollowCheck = new QCheckBox(tr("Enable"), this);
    timingLayout->addRow(tr("Auto-follow:"), m_autoFollowCheck);

    m_autoFollowDelaySpin = new QDoubleSpinBox(this);
    m_autoFollowDelaySpin->setRange(0.0, 999.0);
    m_autoFollowDelaySpin->setDecimals(1);
    m_autoFollowDelaySpin->setSingleStep(0.5);
    m_autoFollowDelaySpin->setSuffix(tr(" sec"));
    m_autoFollowDelaySpin->setEnabled(false);
    timingLayout->addRow(tr("Delay:"), m_autoFollowDelaySpin);

    timingGroup->setContentLayout(timingLayout);
    m_mainLayout->addWidget(timingGroup);

    // fade transition group
    auto* fadeGroup = new CollapsibleSection(tr("Fade Transition"), this);
    QFormLayout* fadeLayout = new QFormLayout();

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

    fadeGroup->setContentLayout(fadeLayout);
    m_mainLayout->addWidget(fadeGroup);

    // DCA targeting section
    createDCATargetingSection();
    m_mainLayout->addWidget(m_dcaTargetingGroup);

    // DCA overrides section: one strip per DCA, styled like the mixer-feedback
    // widgets, rebuilt when the effective DCA count changes
    m_dcaOverridesGroup = new CollapsibleSection(tr("DCA Overrides"), this);
    QVBoxLayout* overridesLayout = new QVBoxLayout();

    // no inner scroll here: the whole editor is already scrollable, so let the
    // overrides expand inline rather than nesting a second scrollbar
    QWidget* overridesContent = new QWidget(m_dcaOverridesGroup);
    m_dcaOverridesContentLayout = new QVBoxLayout(overridesContent);
    m_dcaOverridesContentLayout->setContentsMargins(0, 0, 0, 0);
    m_dcaOverridesContentLayout->setSpacing(4);
    m_dcaOverridesContentLayout->addStretch();

    rebuildDcaOverrideStrips(m_app ? m_app->effectiveDcaCount() : 8);

    overridesLayout->addWidget(overridesContent);
    m_dcaOverridesGroup->setContentLayout(overridesLayout);
    m_mainLayout->addWidget(m_dcaOverridesGroup);

    // per-channel actor profile + level
    createChannelProfilesSection();
    m_mainLayout->addWidget(m_channelProfilesGroup);

    // per-FX-unit mutes + console snippets
    createFxMutesSection();
    m_mainLayout->addWidget(m_fxMutesGroup);

    // linked QLab (DAW remote) cue
    auto* qlabGroup = new CollapsibleSection(tr("QLab / DAW Remote"), this);
    QFormLayout* qlabLayout = new QFormLayout();
    m_qLabCueEdit = new QLineEdit(this);
    m_qLabCueEdit->setPlaceholderText(tr("QLab cue number / id"));
    m_qLabCueEdit->setToolTip(tr("Fire this QLab cue when the OpenMix cue executes"));
    qlabLayout->addRow(tr("QLab cue:"), m_qLabCueEdit);
    qlabGroup->setContentLayout(qlabLayout);
    m_mainLayout->addWidget(qlabGroup);

    // L/R gangs (show-level) + soundcheck (check) mode toggle
    auto* rehearsalGroup = new CollapsibleSection(tr("Gangs && Rehearsal"), this);
    QFormLayout* rehearsalLayout = new QFormLayout();
    m_gangEdit = new QLineEdit(rehearsalGroup);
    m_gangEdit->setPlaceholderText(tr("L/R pairs, e.g. 1-2, 3-4"));
    m_gangEdit->setToolTip(tr("Ganged channel pairs; a level on one mirrors to its partner"));
    rehearsalLayout->addRow(tr("L/R gangs:"), m_gangEdit);
    m_checkModeCheck =
        new QCheckBox(tr("Soundcheck mode (GO holds on current cue)"), rehearsalGroup);
    rehearsalLayout->addRow(tr("Rehearsal:"), m_checkModeCheck);
    rehearsalGroup->setContentLayout(rehearsalLayout);
    m_mainLayout->addWidget(rehearsalGroup);

    // notes group
    auto* notesGroup = new CollapsibleSection(tr("Notes"), this);
    QVBoxLayout* notesLayout = new QVBoxLayout();

    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setPlaceholderText(tr("Enter cue notes..."));
    m_notesEdit->setMaximumHeight(120);
    notesLayout->addWidget(m_notesEdit);

    notesGroup->setContentLayout(notesLayout);
    m_mainLayout->addWidget(notesGroup);

    m_mainLayout->addStretch();

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QVBoxLayout* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scroll);

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

    connect(m_colorEdit, &QLineEdit::textChanged, this, &CueEditor::onColorChanged);
    connect(m_colorPickButton, &QPushButton::clicked, this, &CueEditor::onColorPick);
    connect(m_skipCheck, &QCheckBox::toggled, this, &CueEditor::onSkipChanged);
    connect(m_snippetsEdit, &QLineEdit::textChanged, this, &CueEditor::onSnippetsChanged);
    connect(m_gangEdit, &QLineEdit::editingFinished, this, &CueEditor::onGangsChanged);
    connect(m_checkModeCheck, &QCheckBox::toggled, this, &CueEditor::onCheckModeToggled);
}

void CueEditor::createFxMutesSection() {
    m_fxMutesGroup = new CollapsibleSection(tr("FX Mutes && Snippets"), this);
    QVBoxLayout* layout = new QVBoxLayout();
    layout->setContentsMargins(4, 4, 4, 4);

    QFormLayout* snippetForm = new QFormLayout();
    m_snippetsEdit = new QLineEdit(m_fxMutesGroup);
    m_snippetsEdit->setPlaceholderText(tr("Snippet indices, e.g. 1, 4, 7"));
    m_snippetsEdit->setToolTip(tr("Console snippets recalled when this cue fires"));
    snippetForm->addRow(tr("Snippets:"), m_snippetsEdit);
    layout->addLayout(snippetForm);

    QLabel* hint =
        new QLabel(tr("Per FX unit: enable to set its mute state on fire."), m_fxMutesGroup);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
    layout->addWidget(hint);

    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    for (int i = 1; i <= 8; ++i) {
        FxMuteWidgets widgets;
        widgets.enable = new QCheckBox(tr("FX %1").arg(i), m_fxMutesGroup);
        widgets.muted = new QCheckBox(tr("Muted"), m_fxMutesGroup);
        widgets.muted->setEnabled(false);

        const int row = (i - 1) / 2;
        const int col = ((i - 1) % 2) * 2;
        grid->addWidget(widgets.enable, row, col);
        grid->addWidget(widgets.muted, row, col + 1);

        connect(widgets.enable, &QCheckBox::toggled, widgets.muted, &QCheckBox::setEnabled);
        connect(widgets.enable, &QCheckBox::toggled, this, &CueEditor::onFxMuteChanged);
        connect(widgets.muted, &QCheckBox::toggled, this, &CueEditor::onFxMuteChanged);

        m_fxMuteWidgets.append(widgets);
    }
    layout->addLayout(grid);
    m_fxMutesGroup->setContentLayout(layout);
}

void CueEditor::createChannelProfilesSection() {
    m_channelProfilesGroup = new CollapsibleSection(tr("Channel Profiles && Levels"), this);
    QVBoxLayout* layout = new QVBoxLayout();
    layout->setContentsMargins(4, 4, 4, 4);

    QLabel* hint = new QLabel(
        tr("Per actor-channel: the profile slot applied on fire, and an optional fader level."),
        m_channelProfilesGroup);
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
    layout->addWidget(hint);

    m_channelTable = new QTableWidget(0, 6, m_channelProfilesGroup);
    m_channelTable->setHorizontalHeaderLabels(
        {tr("Ch"), tr("Actor"), tr("Profile"), tr("Set"), tr("Level"), tr("Position")});
    m_channelTable->verticalHeader()->setVisible(false);
    m_channelTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_channelTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_channelTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_channelTable->setMaximumHeight(340);
    layout->addWidget(m_channelTable);
    m_channelProfilesGroup->setContentLayout(layout);
}

void CueEditor::addBottomWidget(QWidget* widget) {
    // add below the scroll area so it stays fixed while the sections scroll,
    // horizontally centered
    if (widget) {
        if (auto* box = qobject_cast<QBoxLayout*>(layout()))
            box->addWidget(widget, 0, Qt::AlignHCenter);
        else if (layout())
            layout()->addWidget(widget);
    }
}

void CueEditor::createDCATargetingSection() {
    m_dcaTargetingGroup = new CollapsibleSection(tr("Target DCAs"), this);
    QVBoxLayout* layout = new QVBoxLayout();

    m_targetAllDCAsCheck = new QCheckBox(tr("Target All DCAs"), this);
    m_targetAllDCAsCheck->setChecked(true);
    layout->addWidget(m_targetAllDCAsCheck);

    QWidget* dcaChecksWidget = new QWidget(this);
    m_dcaTargetGrid = new QGridLayout(dcaChecksWidget);
    m_dcaTargetGrid->setContentsMargins(0, 0, 0, 0);
    m_dcaTargetGrid->setSpacing(4);

    rebuildDcaTargetChecks(m_app ? m_app->effectiveDcaCount() : 8);

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

    m_dcaTargetingGroup->setContentLayout(layout);
}

void CueEditor::rebuildDcaTargetChecks(int count) {
    for (QCheckBox* check : m_dcaTargetChecks)
        check->deleteLater();
    m_dcaTargetChecks.clear();

    for (int i = 1; i <= count; ++i) {
        auto* check = new QCheckBox(tr("DCA %1").arg(i), m_dcaTargetGrid->parentWidget());
        m_dcaTargetGrid->addWidget(check, (i - 1) / 4, (i - 1) % 4);
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
}

void CueEditor::rebuildDcaOverrideStrips(int count) {
    for (DCAOverrideStrip* strip : m_dcaOverrideStrips)
        strip->deleteLater();
    m_dcaOverrideStrips.clear();

    QWidget* content = m_dcaOverridesContentLayout->parentWidget();
    for (int i = 1; i <= count; ++i) {
        auto* strip = new DCAOverrideStrip(i, m_actorCompletionModel, content);
        connect(strip, &DCAOverrideStrip::overrideEdited, this, &CueEditor::onDCAOverrideChanged);
        connect(strip, &DCAOverrideStrip::labelCommitted, this, &CueEditor::onDCALabelCommitted);
        // keep the trailing stretch last
        m_dcaOverridesContentLayout->insertWidget(m_dcaOverridesContentLayout->count() - 1, strip);
        m_dcaOverrideStrips.append(strip);
    }
}

void CueEditor::onDcaCountChanged(int count) {
    rebuildDcaTargetChecks(count);
    rebuildDcaOverrideStrips(count);
    updateFromCue(); // repopulate the fresh widgets from the current cue
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

        // map cue type to combo index by its stored data (robust to ordering)
        const int typeIndex = m_typeCombo->findData(static_cast<int>(cue->type()));
        m_typeCombo->setCurrentIndex(typeIndex < 0 ? 0 : typeIndex);

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

        // color + skip
        m_colorEdit->setText(cue->color());
        m_skipCheck->setChecked(cue->skip());

        // console snippets, with cached names surfaced in the tooltip
        const ConsoleNameCache* names =
            m_app && m_app->show() ? m_app->show()->consoleNameCache() : nullptr;
        QStringList snippetStrs;
        QStringList snippetNamed;
        for (int snippet : cue->snippets()) {
            snippetStrs << QString::number(snippet);
            const QString name = names ? names->snippetName(snippet) : QString();
            snippetNamed << (name.isEmpty() ? QString::number(snippet)
                                            : QString("%1 (%2)").arg(snippet).arg(name));
        }
        m_snippetsEdit->setText(snippetStrs.join(", "));
        m_snippetsEdit->setToolTip(
            snippetNamed.isEmpty() ? tr("Console snippets recalled when this cue fires")
                                   : tr("Snippets: %1").arg(snippetNamed.join(", ")));

        // per-FX-unit mutes
        updateFxMutesUI();

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
        m_colorEdit->clear();
        m_skipCheck->setChecked(false);
        m_snippetsEdit->clear();
        updateFxMutesUI();
        if (m_channelTable)
            m_channelTable->setRowCount(0);
    }

    // L/R gangs are show-level, so refresh them regardless of the selected cue
    updateGangsUI();

    m_updatingUi = false;
}

void CueEditor::updateDCAOverridesUI() {
    Cue* cue = currentCue();
    if (!cue)
        return;

    QMap<int, DCAOverride> overrides = cue->dcaOverrides();

    for (DCAOverrideStrip* strip : m_dcaOverrideStrips)
        strip->setOverride(overrides.value(strip->dcaNumber()));

    updateDCAAssignInfo();
}

void CueEditor::updateFxMutesUI() {
    Cue* cue = currentCue();
    const QMap<int, bool> mutes = cue ? cue->fxMutes() : QMap<int, bool>();

    for (int i = 0; i < m_fxMuteWidgets.size(); ++i) {
        const int fxUnit = i + 1;
        const bool set = mutes.contains(fxUnit);
        FxMuteWidgets& widgets = m_fxMuteWidgets[i];

        widgets.enable->setChecked(set);
        widgets.muted->setChecked(set && mutes.value(fxUnit));
        widgets.muted->setEnabled(set);
    }
}

void CueEditor::updateGangsUI() {
    if (!m_gangEdit || !m_app || !m_app->show())
        return;

    QStringList parts;
    for (const auto& gang : m_app->show()->channelGangs())
        parts << QString("%1-%2").arg(gang.first).arg(gang.second);

    const QSignalBlocker blocker(m_gangEdit);
    m_gangEdit->setText(parts.join(", "));
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
    m_colorEdit->setEnabled(enabled);
    m_colorPickButton->setEnabled(enabled);
    m_skipCheck->setEnabled(enabled);
    m_snippetsEdit->setEnabled(enabled);
    m_fxMutesGroup->setEnabled(enabled);
    // m_gangEdit and m_checkModeCheck stay enabled: show-/engine-level, not per-cue
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
        const CueType type = static_cast<CueType>(m_typeCombo->itemData(index).toInt());
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
    if (!cue || dca < 1 || dca > m_dcaOverrideStrips.size())
        return;

    cue->setDCAOverride(dca, m_dcaOverrideStrips[dca - 1]->overrideValue());
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onDCALabelCommitted(int dca) {
    if (m_updatingUi)
        return;

    Cue* cue = currentCue();
    if (!cue || dca < 1 || dca > m_dcaOverrideStrips.size() || !m_actorLibrary)
        return;

    // a matching role/actor/ensemble name assigns all its channels to this DCA
    // for this cue (replacing the DCA's previous members); any other text stays
    // a plain scribble label
    const QList<int> channels = m_actorLibrary->resolveChannels(
        m_dcaOverrideStrips[dca - 1]->labelText(), ensembleLibrary());
    if (channels.isEmpty())
        return;

    cue->assignChannelsToDCAMapping(channels, dca, m_app->show()->dcaMapping());
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    updateDCAAssignInfo();
    emit cueModified();
}

void CueEditor::updateDCAAssignInfo() {
    Cue* cue = currentCue();
    const DCAMapping* showMapping =
        (m_app && m_app->show()) ? m_app->show()->dcaMapping() : nullptr;

    for (DCAOverrideStrip* strip : m_dcaOverrideStrips) {
        if (!cue) {
            strip->setAssignInfo(QString());
            continue;
        }

        // same precedence as playback: cue custom mapping, else show mapping
        const int dca = strip->dcaNumber();
        QList<int> channels;
        if (cue->hasCustomDCAMapping())
            channels = cue->dcaChannelMapping().value(dca);
        else if (showMapping)
            channels = showMapping->channelsForDCA(dca);

        if (channels.isEmpty()) {
            strip->setAssignInfo(tr("No channels assigned"));
            continue;
        }

        // when the DCA label names one of the actor's roles, show that role
        const QString dcaLabel = cue->dcaOverride(dca).label.value_or(QString());
        QStringList parts;
        for (int ch : channels) {
            const Actor* actor = m_actorLibrary ? m_actorLibrary->actorForChannel(ch) : nullptr;
            if (actor) {
                const QString secondary = actor->secondaryName(actor->matchedRole(dcaLabel));
                if (!secondary.isEmpty())
                    parts << tr("Ch %1 %2 (%3)").arg(ch).arg(actor->displayName(), secondary);
                else
                    parts << tr("Ch %1 %2").arg(ch).arg(actor->displayName());
            } else {
                parts << tr("Ch %1").arg(ch);
            }
        }
        strip->setAssignInfo(tr("Members: %1").arg(parts.join(", ")));
    }
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

        QString display = a.displayName().isEmpty() ? tr("(unnamed)") : a.displayName();
        const QString secondary = a.useRoleName() ? a.name() : a.rolesDisplay();
        if (!secondary.isEmpty() && secondary.compare(display, Qt::CaseInsensitive) != 0)
            display = tr("%1 (%2)").arg(display, secondary);
        auto* nameItem = new QTableWidgetItem(display);
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

        auto* positionCombo = new QComboBox(m_channelTable);
        positionCombo->addItem(tr("(none)"), QString());
        if (m_app && m_app->show()) {
            for (const Position& p : m_app->show()->positionLibrary()->positions())
                positionCombo->addItem(p.name().isEmpty() ? tr("(unnamed)") : p.name(), p.id());
        }
        positionCombo->setProperty("channel", ch);
        connect(positionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, ch]() { onChannelPositionChanged(ch); });
        m_channelTable->setCellWidget(row, 5, positionCombo);
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

        if (auto* posCombo = qobject_cast<QComboBox*>(m_channelTable->cellWidget(row, 5))) {
            const int posIdx = posCombo->findData(cue->channelPosition(ch));
            posCombo->setCurrentIndex(posIdx < 0 ? 0 : posIdx);
        }
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

void CueEditor::onChannelPositionChanged(int channel) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue || !m_channelTable)
        return;

    QComboBox* combo = nullptr;
    for (int row = 0; row < m_channelTable->rowCount(); ++row) {
        auto* c = qobject_cast<QComboBox*>(m_channelTable->cellWidget(row, 5));
        if (c && c->property("channel").toInt() == channel) {
            combo = c;
            break;
        }
    }
    if (!combo)
        return;

    const QString positionId = combo->currentData().toString();
    if (positionId.isEmpty())
        cue->clearChannelPosition(channel);
    else
        cue->setChannelPosition(channel, positionId);

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
    // refresh the autocomplete names and the per-channel table for the cue
    // currently shown.
    if (m_actorLibrary)
        m_actorCompletionModel->setStringList(
            m_actorLibrary->completionCandidates(ensembleLibrary()));
    if (m_currentIndex < 0)
        return;
    m_updatingUi = true;
    rebuildChannelTable();
    populateChannelTable();
    updateDCAAssignInfo();
    m_updatingUi = false;
}

void CueEditor::onColorChanged(const QString& text) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue)
        return;
    cue->setColor(text.trimmed());
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onColorPick() {
    const QColor initial(m_colorEdit->text().trimmed());
    const QColor chosen = QColorDialog::getColor(initial.isValid() ? initial : QColor(Qt::white),
                                                 this, tr("Cue Color"));
    if (chosen.isValid())
        m_colorEdit->setText(chosen.name()); // fires onColorChanged
}

void CueEditor::onSkipChanged(bool checked) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue)
        return;
    cue->setSkip(checked);
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onSnippetsChanged(const QString& text) {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue)
        return;

    QList<int> snippets;
    const QStringList parts = text.split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        bool ok = false;
        const int n = part.trimmed().toInt(&ok);
        if (ok && !snippets.contains(n))
            snippets.append(n);
    }

    cue->setSnippets(snippets);
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onFxMuteChanged() {
    if (m_updatingUi)
        return;
    Cue* cue = currentCue();
    if (!cue)
        return;

    QMap<int, bool> mutes;
    for (int i = 0; i < m_fxMuteWidgets.size(); ++i) {
        if (m_fxMuteWidgets[i].enable->isChecked())
            mutes[i + 1] = m_fxMuteWidgets[i].muted->isChecked();
    }

    cue->setFxMutes(mutes);
    m_app->show()->cueList()->updateCue(m_currentIndex, *cue);
    emit cueModified();
}

void CueEditor::onGangsChanged() {
    if (m_updatingUi || !m_app || !m_app->show())
        return;

    QList<QPair<int, int>> gangs;
    const QStringList parts = m_gangEdit->text().split(',', Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QStringList nums = part.split('-', Qt::SkipEmptyParts);
        if (nums.size() != 2)
            continue;
        bool ok1 = false;
        bool ok2 = false;
        const int a = nums[0].trimmed().toInt(&ok1);
        const int b = nums[1].trimmed().toInt(&ok2);
        if (ok1 && ok2 && a > 0 && b > 0)
            gangs.append(qMakePair(a, b));
    }

    m_app->show()->setChannelGangs(gangs);
    if (m_app->playbackEngine())
        m_app->playbackEngine()->setChannelGangs(gangs);

    updateGangsUI(); // reflect the normalized form back into the field
    emit cueModified();
}

void CueEditor::onCheckModeToggled(bool checked) {
    if (m_updatingUi)
        return;
    if (m_app && m_app->playbackEngine())
        m_app->playbackEngine()->setCheckMode(checked);
}

void CueEditor::onEngineCheckModeChanged(bool enabled) {
    if (!m_checkModeCheck || m_checkModeCheck->isChecked() == enabled)
        return;
    const QSignalBlocker blocker(m_checkModeCheck);
    m_checkModeCheck->setChecked(enabled);
}

} // namespace OpenMix
