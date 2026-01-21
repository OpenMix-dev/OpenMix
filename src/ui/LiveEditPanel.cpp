#include "LiveEditPanel.h"
#include "ParameterEditWidget.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/LiveEditSession.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace OpenMix {

LiveEditPanel::LiveEditPanel(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    setupUi();
    connectSignals();
    updateUiState();
}

void LiveEditPanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // status section
    QGroupBox* statusGroup = new QGroupBox(tr("Live Edit Status"), this);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);

    m_statusLabel = new QLabel(tr("Inactive"), this);
    m_statusLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    statusLayout->addWidget(m_statusLabel);

    m_cueLabel = new QLabel(tr("No cue selected"), this);
    m_cueLabel->setStyleSheet("color: gray;");
    statusLayout->addWidget(m_cueLabel);

    mainLayout->addWidget(statusGroup);

    // mode controls
    QGroupBox* modeGroup = new QGroupBox(tr("Edit Mode"), this);
    QVBoxLayout* modeLayout = new QVBoxLayout(modeGroup);

    m_previewCheck = new QCheckBox(tr("Preview Mode (changes not sent to mixer)"), this);
    modeLayout->addWidget(m_previewCheck);

    mainLayout->addWidget(modeGroup);

    // parameter edit area
    QGroupBox* paramGroup = new QGroupBox(tr("Edited Parameters"), this);
    QVBoxLayout* paramGroupLayout = new QVBoxLayout(paramGroup);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setMinimumHeight(200);

    m_scrollWidget = new QWidget(m_scrollArea);
    m_paramLayout = new QVBoxLayout(m_scrollWidget);
    m_paramLayout->setContentsMargins(4, 4, 4, 4);
    m_paramLayout->setSpacing(2);
    m_paramLayout->addStretch();

    m_scrollArea->setWidget(m_scrollWidget);
    paramGroupLayout->addWidget(m_scrollArea);

    mainLayout->addWidget(paramGroup, 1);

    // commit section
    QGroupBox* commitGroup = new QGroupBox(tr("Commit Changes"), this);
    QVBoxLayout* commitLayout = new QVBoxLayout(commitGroup);

    QHBoxLayout* targetLayout = new QHBoxLayout();
    QLabel* targetLabel = new QLabel(tr("Commit to:"), this);
    m_commitTargetCombo = new QComboBox(this);
    m_commitTargetCombo->addItem(tr("Current Cue"), "current");
    m_commitTargetCombo->addItem(tr("Standby Cue"), "standby");
    m_commitTargetCombo->addItem(tr("New Cue"), "new");
    targetLayout->addWidget(targetLabel);
    targetLayout->addWidget(m_commitTargetCombo, 1);
    commitLayout->addLayout(targetLayout);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_commitButton = new QPushButton(tr("Commit"), this);
    m_commitButton->setStyleSheet(
        "QPushButton { background-color: #4a9f4a; color: white; font-weight: bold; }");
    buttonLayout->addWidget(m_commitButton);

    m_revertAllButton = new QPushButton(tr("Revert All"), this);
    buttonLayout->addWidget(m_revertAllButton);

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setStyleSheet("QPushButton { background-color: #cc4444; color: white; }");
    buttonLayout->addWidget(m_cancelButton);

    commitLayout->addLayout(buttonLayout);

    mainLayout->addWidget(commitGroup);
}

void LiveEditPanel::connectSignals() {
    if (!m_app || !m_app->playbackEngine()) {
        return;
    }

    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session) {
        connect(session, &LiveEditSession::modeChanged, this, &LiveEditPanel::onModeChanged);
        connect(session, &LiveEditSession::sessionStarted, this, &LiveEditPanel::onSessionStarted);
        connect(session, &LiveEditSession::sessionEnded, this, &LiveEditPanel::onSessionEnded);
        connect(session, &LiveEditSession::parameterEdited, this,
                &LiveEditPanel::onParameterEdited);
        connect(session, &LiveEditSession::parameterReverted, this,
                &LiveEditPanel::onParameterReverted);
    }

    connect(m_previewCheck, &QCheckBox::toggled, this, &LiveEditPanel::onPreviewToggled);
    connect(m_commitTargetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &LiveEditPanel::onCommitTargetChanged);
    connect(m_commitButton, &QPushButton::clicked, this, &LiveEditPanel::onCommitClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &LiveEditPanel::onCancelClicked);
    connect(m_revertAllButton, &QPushButton::clicked, this, &LiveEditPanel::onRevertAllClicked);
}

void LiveEditPanel::refresh() {
    updateUiState();
    populateParameterWidgets();
}

void LiveEditPanel::onModeChanged(LiveEditMode mode) {
    m_previewCheck->blockSignals(true);
    m_previewCheck->setChecked(mode == LiveEditMode::Preview);
    m_previewCheck->blockSignals(false);

    updateStatusLabel();
    updateUiState();

    // update state for all parameter widgets
    ParameterEditState widgetState = ParameterEditState::Original;
    if (mode == LiveEditMode::Preview) {
        widgetState = ParameterEditState::Preview;
    } else if (mode == LiveEditMode::Live) {
        widgetState = ParameterEditState::Modified;
    }

    for (auto widget : m_paramWidgets) {
        if (widget->isModified()) {
            widget->setState(widgetState);
        }
    }
}

void LiveEditPanel::onSessionStarted(const QString& cueId) {
    if (!m_app || !m_app->show()->cueList()) {
        return;
    }

    const Cue* cue = m_app->show()->cueList()->findById(cueId);
    if (cue) {
        m_cueLabel->setText(tr("Cue %1: %2")
                                .arg(cue->number(), 0, 'f', 1)
                                .arg(cue->name().isEmpty() ? tr("(unnamed)") : cue->name()));
    } else {
        m_cueLabel->setText(cueId);
    }

    updateStatusLabel();
    updateUiState();
    populateParameterWidgets();
}

void LiveEditPanel::onSessionEnded() {
    m_cueLabel->setText(tr("No cue selected"));
    clearParameterWidgets();
    updateStatusLabel();
    updateUiState();
}

void LiveEditPanel::onParameterEdited(const QString& path, const QVariant& oldValue,
                                      const QVariant& newValue) {
    Q_UNUSED(oldValue);

    ParameterEditWidget* widget = findOrCreateWidget(path);
    widget->setValue(newValue);

    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session) {
        widget->setOriginalValue(session->originalValue(path));
        widget->setState(session->isPreview() ? ParameterEditState::Preview
                                              : ParameterEditState::Modified);
    }
}

void LiveEditPanel::onParameterReverted(const QString& path) {
    if (m_paramWidgets.contains(path)) {
        ParameterEditWidget* widget = m_paramWidgets[path];
        widget->setValue(widget->originalValue());
        widget->setState(ParameterEditState::Original);
    }
}

void LiveEditPanel::onCommitClicked() {
    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (!session || !session->isActive()) {
        return;
    }

    QString target = m_commitTargetCombo->currentData().toString();

    if (target == "current") {
        session->commitToCurrentCue();
    } else if (target == "standby") {
        int standbyIndex = m_app->playbackEngine()->standbyCueIndex();
        if (standbyIndex >= 0) {
            const Cue& standbyCue = m_app->show()->cueList()->at(standbyIndex);
            session->commitToCue(standbyCue.id());
        } else {
            session->commitToCurrentCue();
        }
    } else if (target == "new") {
        session->commitToNewCue();
    }
}

void LiveEditPanel::onCancelClicked() {
    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session) {
        session->cancel();
    }
}

void LiveEditPanel::onRevertAllClicked() {
    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session) {
        session->revertAll();
        // refresh all widgets
        for (auto widget : m_paramWidgets) {
            widget->setValue(widget->originalValue());
            widget->setState(ParameterEditState::Original);
        }
    }
}

void LiveEditPanel::onPreviewToggled(bool checked) {
    Q_UNUSED(checked);
    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session && session->isActive()) {
        session->togglePreviewMode();
    }
}

void LiveEditPanel::onCommitTargetChanged(int index) { Q_UNUSED(index); }

void LiveEditPanel::onParameterValueChanged(const QString& path, const QVariant& value) {
    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session && session->isActive()) {
        session->setParameter(path, value);
    }
}

void LiveEditPanel::onParameterRevertRequested(const QString& path) {
    LiveEditSession* session = m_app->playbackEngine()->liveEditSession();
    if (session) {
        session->revertParameter(path);
    }
}

void LiveEditPanel::updateUiState() {
    LiveEditSession* session =
        m_app && m_app->playbackEngine() ? m_app->playbackEngine()->liveEditSession() : nullptr;
    bool isActive = session && session->isActive();
    bool hasEdits = session && session->hasEdits();

    m_previewCheck->setEnabled(isActive);
    m_commitButton->setEnabled(isActive && hasEdits);
    m_cancelButton->setEnabled(isActive);
    m_revertAllButton->setEnabled(isActive && hasEdits);
    m_commitTargetCombo->setEnabled(isActive);
}

void LiveEditPanel::updateStatusLabel() {
    LiveEditSession* session =
        m_app && m_app->playbackEngine() ? m_app->playbackEngine()->liveEditSession() : nullptr;

    if (!session || !session->isActive()) {
        m_statusLabel->setText(tr("Inactive"));
        m_statusLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: gray;");
    } else if (session->isPreview()) {
        m_statusLabel->setText(tr("Preview Mode"));
        m_statusLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: #6699ff;");
    } else {
        m_statusLabel->setText(tr("Live Edit Active"));
        m_statusLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: #44cc44;");
    }
}

void LiveEditPanel::clearParameterWidgets() {
    for (auto widget : m_paramWidgets) {
        m_paramLayout->removeWidget(widget);
        widget->deleteLater();
    }
    m_paramWidgets.clear();
}

void LiveEditPanel::populateParameterWidgets() {
    LiveEditSession* session =
        m_app && m_app->playbackEngine() ? m_app->playbackEngine()->liveEditSession() : nullptr;

    if (!session) {
        return;
    }

    QJsonObject originalValues = session->originalValues();

    // create widgets for all parameters in the original cue
    for (auto it = originalValues.begin(); it != originalValues.end(); ++it) {
        ParameterEditWidget* widget = findOrCreateWidget(it.key());
        widget->setOriginalValue(it.value().toVariant());
        widget->setValue(it.value().toVariant());
    }

    // update widgets for any edited parameters
    QStringList editedPaths = session->editedPaths();
    for (const QString& path : editedPaths) {
        const ParameterEdit* edit = session->edit(path);
        if (edit && m_paramWidgets.contains(path)) {
            ParameterEditWidget* widget = m_paramWidgets[path];
            widget->setValue(edit->currentValue);
            widget->setState(session->isPreview() ? ParameterEditState::Preview
                                                  : ParameterEditState::Modified);
        }
    }
}

ParameterEditWidget* LiveEditPanel::findOrCreateWidget(const QString& path) {
    if (m_paramWidgets.contains(path)) {
        return m_paramWidgets[path];
    }

    ParameterEditWidget* widget = new ParameterEditWidget(path, m_scrollWidget);
    widget->setLabel(formatParameterName(path));

    connect(widget, &ParameterEditWidget::valueChanged, this,
            &LiveEditPanel::onParameterValueChanged);
    connect(widget, &ParameterEditWidget::revertRequested, this,
            &LiveEditPanel::onParameterRevertRequested);

    // insert before stretch
    m_paramLayout->insertWidget(m_paramLayout->count() - 1, widget);
    m_paramWidgets[path] = widget;

    return widget;
}

QString LiveEditPanel::formatParameterName(const QString& path) const {
    // convert path like "/dca/1/fader" to "DCA 1 Fader"
    QString name = path;
    name.remove(0, 1);
    name.replace("/", " ");

    QStringList parts = name.split(" ");
    for (QString& part : parts) {
        if (!part.isEmpty()) {
            part[0] = part[0].toUpper();
        }
    }

    return parts.join(" ");
}

} // namespace OpenMix
