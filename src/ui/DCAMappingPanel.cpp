#include "DCAMappingPanel.h"
#include "DcaGridNav.h"
#include "app/Application.h"
#include "MainWindow.h"
#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/DCAMapping.h"
#include "core/ShortcutManager.h"
#include "core/Show.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/MixerProtocol.h"
#include "theme/Icons.h"
#include "theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QKeyEvent>
#include <QStyleOptionComboBox>
#include <QStylePainter>
#include <QTimer>
#include <QWheelEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <algorithm>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <memory>

namespace OpenMix {

class NoScrollComboBox : public QComboBox {
  public:
    using QComboBox::QComboBox;

    // shorter closed-state text; the popup keeps the full item text
    static constexpr int CompactTextRole = Qt::UserRole + 1;

  protected:
    void paintEvent(QPaintEvent* event) override {
        const QVariant compact = currentData(CompactTextRole);
        if (!compact.isValid()) {
            QComboBox::paintEvent(event);
            return;
        }
        QStylePainter painter(this);
        QStyleOptionComboBox opt;
        initStyleOption(&opt);
        opt.currentText = compact.toString();
        painter.drawComplexControl(QStyle::CC_ComboBox, opt);
        painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
    }

    void wheelEvent(QWheelEvent* event) override { event->ignore(); }

    void showPopup() override {
        m_popupOpen = true;
        QComboBox::showPopup();
    }

    void hidePopup() override {
        QComboBox::hidePopup();
        QTimer::singleShot(100, this, [this]() { m_popupOpen = false; });
    }

    void keyPressEvent(QKeyEvent* event) override {
        int key = event->key();
        if ((key == Qt::Key_Return || key == Qt::Key_Enter) && !m_popupOpen) {
            showPopup();
            event->accept();
        } else {
            QComboBox::keyPressEvent(event);
        }
    }

  private:
    bool m_popupOpen = false;
};

DCAMappingPanel::DCAMappingPanel(Application* app, QWidget* parent)
    : QWidget(parent), m_app(app), m_mapping(nullptr) {
    setupUi();

    if (m_app) {
        connect(m_app, &Application::mixerConnected, this, &DCAMappingPanel::onMixerConnected);
        connect(m_app, &Application::mixerDisconnected, this,
                &DCAMappingPanel::onMixerDisconnected);
        // console-type selection changes the DCA/channel/bus counts shown here
        connect(m_app, &Application::dcaCountChanged, this, &DCAMappingPanel::refresh);

        m_mapping = m_app->show()->dcaMapping();
        if (m_mapping) {
            connect(m_mapping, &DCAMapping::mappingCleared, this, &DCAMappingPanel::refresh);
        }

        // actor renames/role edits change the channel labels shown here
        connect(m_app->show()->actorProfileLibrary(), &ActorProfileLibrary::changed, this,
                &DCAMappingPanel::onActorsChanged);

        // a cleared cue list destroys the cue this panel points at
        connect(m_app->show()->cueList(), &CueList::listCleared, this,
                &DCAMappingPanel::clearCurrentCue);
    }

    refresh();
}

void DCAMappingPanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    m_contextHeader = new QWidget(this);
    QHBoxLayout* contextLayout = new QHBoxLayout(m_contextHeader);
    contextLayout->setContentsMargins(4, 4, 4, 4);
    contextLayout->setSpacing(8);

    // step the cue-list selection from here, so the operator can walk cues
    // without leaving the mapping screen
    m_prevCueButton = new QPushButton(Icons::mediaPrevious(), QString(), m_contextHeader);
    m_prevCueButton->setFixedSize(24, 24);
    m_prevCueButton->setToolTip(tr("Previous cue"));
    m_prevCueButton->setFocusPolicy(Qt::NoFocus);
    connect(m_prevCueButton, &QPushButton::clicked, this,
            [this]() { emit adjacentCueRequested(-1); });
    contextLayout->addWidget(m_prevCueButton);

    m_nextCueButton = new QPushButton(Icons::mediaNext(), QString(), m_contextHeader);
    m_nextCueButton->setFixedSize(24, 24);
    m_nextCueButton->setToolTip(tr("Next cue"));
    m_nextCueButton->setFocusPolicy(Qt::NoFocus);
    connect(m_nextCueButton, &QPushButton::clicked, this,
            [this]() { emit adjacentCueRequested(1); });
    contextLayout->addWidget(m_nextCueButton);

    m_contextLabel = new QLabel(tr("Show Level"), m_contextHeader);
    m_contextLabel->setStyleSheet(
        QString("font-weight: bold; color: %1;").arg(Theme::Colors::TextPrimary));
    contextLayout->addWidget(m_contextLabel);

    // says out loud whether the combos below reflect this cue or the show
    m_mappingStateLabel = new QLabel(m_contextHeader);
    m_mappingStateLabel->setVisible(false);
    contextLayout->addWidget(m_mappingStateLabel);

    m_useCueMappingCheck = new QCheckBox(tr("Use Cue-Specific Mapping"), m_contextHeader);
    m_useCueMappingCheck->setToolTip(tr("Enable to override show-level mapping for this cue only"));
    m_useCueMappingCheck->setVisible(false);
    connect(m_useCueMappingCheck, &QCheckBox::toggled, this,
            &DCAMappingPanel::onUseCueMappingToggled);
    contextLayout->addWidget(m_useCueMappingCheck);

    m_copyFromShowButton = new QPushButton(tr("Copy from Show"), m_contextHeader);
    m_copyFromShowButton->setToolTip(tr("Copy current show-level mapping to this cue"));
    m_copyFromShowButton->setVisible(false);
    connect(m_copyFromShowButton, &QPushButton::clicked, this,
            &DCAMappingPanel::copyShowMappingToCue);
    contextLayout->addWidget(m_copyFromShowButton);

    m_clearCueMappingButton = new QPushButton(tr("Clear Cue Mapping"), m_contextHeader);
    m_clearCueMappingButton->setToolTip(tr("Remove cue-specific mapping, revert to show-level"));
    m_clearCueMappingButton->setVisible(false);
    connect(m_clearCueMappingButton, &QPushButton::clicked, this,
            &DCAMappingPanel::clearCueMapping);
    contextLayout->addWidget(m_clearCueMappingButton);

    contextLayout->addStretch();
    mainLayout->addWidget(m_contextHeader);

    // create actions
    m_syncFromMixerAction = new QAction(Icons::refresh(), tr("Sync from Mixer"), this);
    m_syncFromMixerAction->setToolTip(tr("Pull current DCA assignments from connected mixer"));
    m_syncFromMixerAction->setEnabled(false);
    m_syncFromMixerAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_syncFromMixerAction, &QAction::triggered, this, &DCAMappingPanel::syncFromMixer);

    m_savePresetAction = new QAction(Icons::download(), tr("Save DCA Preset"), this);
    m_savePresetAction->setToolTip(tr("Save current mapping as a preset file"));
    m_savePresetAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_savePresetAction, &QAction::triggered, this, &DCAMappingPanel::saveMappingPreset);

    m_loadPresetAction = new QAction(Icons::upload(), tr("Load DCA Preset"), this);
    m_loadPresetAction->setToolTip(tr("Load mapping from a preset file"));
    m_loadPresetAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_loadPresetAction, &QAction::triggered, this, &DCAMappingPanel::loadMappingPreset);

    m_clearAllAction = new QAction(Icons::editClear(), tr("Clear All DCA Mappings"), this);
    m_clearAllAction->setToolTip(tr("Remove all channel and bus assignments"));
    m_clearAllAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(m_clearAllAction, &QAction::triggered, this, &DCAMappingPanel::clearAllMappings);

    // add actions to widget
    addAction(m_syncFromMixerAction);
    addAction(m_savePresetAction);
    addAction(m_loadPresetAction);
    addAction(m_clearAllAction);

    // register actions w/ shortcut manager
    if (m_app) {
        ShortcutManager* sm = m_app->shortcutManager();
        sm->registerAction("dca.syncFromMixer", m_syncFromMixerAction, QKeySequence());
        sm->registerAction("dca.savePreset", m_savePresetAction, QKeySequence());
        sm->registerAction("dca.loadPreset", m_loadPresetAction, QKeySequence());
        sm->registerAction("dca.clearAll", m_clearAllAction, QKeySequence());
    }

    // toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();

    m_syncButton = new QPushButton(Icons::refresh(), tr("Sync from Mixer"), this);
    m_syncButton->setToolTip(m_syncFromMixerAction->toolTip());
    m_syncButton->setEnabled(false);
    connect(m_syncButton, &QPushButton::clicked, m_syncFromMixerAction, &QAction::trigger);
    toolbarLayout->addWidget(m_syncButton);

    m_savePresetButton = new QPushButton(Icons::download(), tr("Export DCAs"), this);
    m_savePresetButton->setToolTip(m_savePresetAction->toolTip());
    connect(m_savePresetButton, &QPushButton::clicked, m_savePresetAction, &QAction::trigger);
    toolbarLayout->addWidget(m_savePresetButton);

    m_loadPresetButton = new QPushButton(Icons::upload(), tr("Import DCAs"), this);
    m_loadPresetButton->setToolTip(m_loadPresetAction->toolTip());
    connect(m_loadPresetButton, &QPushButton::clicked, m_loadPresetAction, &QAction::trigger);
    toolbarLayout->addWidget(m_loadPresetButton);

    m_clearAllButton = new QPushButton(Icons::editClear(), tr("Clear All"), this);
    m_clearAllButton->setToolTip(m_clearAllAction->toolTip());
    connect(m_clearAllButton, &QPushButton::clicked, m_clearAllAction, &QAction::trigger);
    toolbarLayout->addWidget(m_clearAllButton);

    toolbarLayout->addStretch();
    mainLayout->addLayout(toolbarLayout);

    // scroll area for channel/bus lists
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_scrollContent = new QWidget();
    QVBoxLayout* scrollLayout = new QVBoxLayout(m_scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(12);

    // per-DCA overview of the current cue (label + members)
    m_dcaOverviewGroup = new QGroupBox(tr("DCA Overview (current cue)"), m_scrollContent);
    m_dcaOverviewLayout = new QGridLayout(m_dcaOverviewGroup);
    m_dcaOverviewLayout->setSpacing(4);
    scrollLayout->addWidget(m_dcaOverviewGroup);

    // channel section
    m_channelGroup = new QGroupBox(tr("Channel Assignments"), m_scrollContent);
    m_channelLayout = new QGridLayout(m_channelGroup);
    m_channelLayout->setSpacing(4);
    scrollLayout->addWidget(m_channelGroup);

    // bus section
    m_busGroup = new QGroupBox(tr("Bus Assignments"), m_scrollContent);
    m_busLayout = new QGridLayout(m_busGroup);
    m_busLayout->setSpacing(4);
    scrollLayout->addWidget(m_busGroup);

    scrollLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);
    mainLayout->addWidget(m_scrollArea, 1);

    createDcaOverviewSection();
    createChannelSection();
    createBusSection();
}

void DCAMappingPanel::createDcaOverviewSection() {
    while (QLayoutItem* item = m_dcaOverviewLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_dcaOverviewTitles.clear();
    m_dcaOverviewMembers.clear();

    for (int d = 1; d <= m_dcaCount; ++d) {
        int row = (d - 1) % 4;
        int col = ((d - 1) / 4) * 2;

        QLabel* title = new QLabel(tr("DCA %1").arg(d), m_dcaOverviewGroup);
        title->setStyleSheet(QString("font-weight: bold; color: %1;").arg(Theme::Colors::TextPrimary));
        title->setMinimumWidth(120);
        m_dcaOverviewLayout->addWidget(title, row, col);
        m_dcaOverviewTitles.append(title);

        QLabel* members = new QLabel(m_dcaOverviewGroup);
        members->setWordWrap(true);
        members->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
        m_dcaOverviewLayout->addWidget(members, row, col + 1);
        m_dcaOverviewLayout->setColumnStretch(col + 1, 1);
        m_dcaOverviewMembers.append(members);
    }
}

void DCAMappingPanel::createChannelSection() {
    // clear existing
    while (QLayoutItem* item = m_channelLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_channelCombos.clear();
    m_channelLabels.clear();
    m_channelNameEdits.clear();

    // header
    m_channelLayout->addWidget(new QLabel(tr("Channel"), m_channelGroup), 0, 0);
    m_channelLayout->addWidget(new QLabel(tr("DCA"), m_channelGroup), 0, 1);

    // create rows for each channel (2 columns per row, up to 4 channel pairs)
    int colOffset = 0;
    for (int ch = 1; ch <= m_channelCount; ++ch) {
        int row = ((ch - 1) % 16) + 1;
        colOffset = ((ch - 1) / 16) * 3;

        // stacked label + line edit, like the bus rows: double-click renames
        // the actor on the channel (or casts a new one)
        QWidget* nameContainer = new QWidget(m_channelGroup);
        QVBoxLayout* nameLayout = new QVBoxLayout(nameContainer);
        nameLayout->setContentsMargins(0, 0, 0, 0);
        nameLayout->setSpacing(0);

        QLabel* label = new QLabel(channelDisplayName(ch), nameContainer);
        label->setMinimumWidth(60);
        label->setProperty("channelIndex", ch);
        label->installEventFilter(this);
        label->setToolTip(tr("Double-click to rename the actor on this channel"));
        nameLayout->addWidget(label);
        m_channelLabels.append(label);

        QLineEdit* edit = new QLineEdit(nameContainer);
        edit->setVisible(false);
        edit->setPlaceholderText(tr("Actor name"));
        edit->setProperty("channelIndex", ch);
        edit->installEventFilter(this);
        nameLayout->addWidget(edit);
        m_channelNameEdits.append(edit);

        connect(edit, &QLineEdit::returnPressed, [this, ch]() { finishChannelNameEdit(ch); });

        m_channelLayout->addWidget(nameContainer, row, colOffset);

        NoScrollComboBox* combo = new NoScrollComboBox(m_channelGroup);
        // item texts vary with cue labels; keep the closed combo width stable
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(14);
        combo->addItem(tr("None"), -1);
        for (int d = 1; d <= m_dcaCount; ++d) {
            combo->addItem(tr("DCA %1").arg(d), d);
        }
        combo->setProperty("channel", ch);
        combo->installEventFilter(this); // arrow/digit keyboard navigation
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, ch](int index) {
            if (!m_updatingUi) {
                int dca = m_channelCombos[ch - 1]->currentData().toInt();
                onChannelDCAChanged(ch, dca);
                updateComboItemStates();
            }
        });
        m_channelLayout->addWidget(combo, row, colOffset + 1);
        m_channelCombos.append(combo);
    }
}

void DCAMappingPanel::createBusSection() {
    // clear existing
    while (QLayoutItem* item = m_busLayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_busCombos.clear();
    m_busLabels.clear();
    m_busNameEdits.clear();

    // header
    m_busLayout->addWidget(new QLabel(tr("Bus"), m_busGroup), 0, 0);
    m_busLayout->addWidget(new QLabel(tr("DCA"), m_busGroup), 0, 1);

    // create rows for each bus
    int colOffset = 0;
    for (int bus = 1; bus <= m_busCount; ++bus) {
        int row = ((bus - 1) % 8) + 1;
        colOffset = ((bus - 1) / 8) * 3;

        // stacked label + line edit in a container
        QWidget* nameContainer = new QWidget(m_busGroup);
        QVBoxLayout* nameLayout = new QVBoxLayout(nameContainer);
        nameLayout->setContentsMargins(0, 0, 0, 0);
        nameLayout->setSpacing(0);

        QLabel* label = new QLabel(busDisplayName(bus), nameContainer);
        label->setMinimumWidth(80);
        label->setProperty("busIndex", bus);
        label->installEventFilter(this);
        label->setToolTip(tr("Double-click to rename"));
        nameLayout->addWidget(label);
        m_busLabels.append(label);

        QLineEdit* edit = new QLineEdit(nameContainer);
        edit->setVisible(false);
        edit->setProperty("busIndex", bus);
        edit->installEventFilter(this);
        nameLayout->addWidget(edit);
        m_busNameEdits.append(edit);

        connect(edit, &QLineEdit::returnPressed, [this, bus]() { finishBusNameEdit(bus); });

        m_busLayout->addWidget(nameContainer, row, colOffset);

        NoScrollComboBox* combo = new NoScrollComboBox(m_busGroup);
        // item texts vary with cue labels; keep the closed combo width stable
        combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        combo->setMinimumContentsLength(14);
        combo->addItem(tr("None"), -1);
        for (int d = 1; d <= m_dcaCount; ++d) {
            combo->addItem(tr("DCA %1").arg(d), d);
        }
        combo->setProperty("bus", bus);
        combo->installEventFilter(this); // arrow/digit keyboard navigation
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, bus](int index) {
            if (!m_updatingUi) {
                int dca = m_busCombos[bus - 1]->currentData().toInt();
                onBusDCAChanged(bus, dca);
                updateComboItemStates();
            }
        });
        m_busLayout->addWidget(combo, row, colOffset + 1);
        m_busCombos.append(combo);
    }
}

void DCAMappingPanel::updateDCAOptions() {
    // counts follow the connected mixer, else the configured console type
    if (m_app) {
        const MixerCapabilities caps = m_app->effectiveCapabilities();
        m_dcaCount = caps.dcaCount;
        m_channelCount = caps.inputChannels;
        m_busCount = caps.mixBuses;
    } else {
        m_dcaCount = 8;
        m_channelCount = 32;
        m_busCount = 16;
    }

    // rebuild sections with new counts
    createDcaOverviewSection();
    createChannelSection();
    createBusSection();
    populateFromMapping();
}

void DCAMappingPanel::populateFromMapping() {
    if (!m_mapping)
        return;

    m_updatingUi = true;

    QMap<int, QList<int>> channelMap;
    QMap<int, QList<int>> busMap;

    if (m_showingCueMapping && m_currentCue && m_currentCue->hasCustomDCAMapping()) {
        channelMap = m_currentCue->dcaChannelMapping();
        busMap = m_currentCue->dcaBusMapping();
    } else {
        channelMap = m_mapping->channelAssignments();
        busMap = m_mapping->busAssignments();
    }

    // build reverse lookup from channel -> DCA
    QMap<int, int> channelToDCA;
    for (const auto& [dca, channels] : channelMap.asKeyValueRange()) {
        for (int ch : channels) {
            channelToDCA[ch] = dca;
        }
    }

    // build reverse lookup from bus -> DCA
    QMap<int, int> busToDCA;
    for (const auto& [dca, buses] : busMap.asKeyValueRange()) {
        for (int bus : buses) {
            busToDCA[bus] = dca;
        }
    }

    // set channel combos
    for (int ch = 1; ch <= m_channelCombos.size(); ++ch) {
        int dca = channelToDCA.value(ch, -1);
        int index = 0;
        if (dca > 0 && dca <= m_dcaCount) {
            index = dca; // DCA 1 is at index 1, etc.
        }
        m_channelCombos[ch - 1]->setCurrentIndex(index);
    }

    // set bus combos
    for (int bus = 1; bus <= m_busCombos.size(); ++bus) {
        int dca = busToDCA.value(bus, -1);
        int index = 0;
        if (dca > 0 && dca <= m_dcaCount) {
            index = dca;
        }
        m_busCombos[bus - 1]->setCurrentIndex(index);
    }

    m_updatingUi = false;

    updateComboItemStates();
    updateDcaOverview();
}

void DCAMappingPanel::updateComboItemStates() {
    if (!m_mapping)
        return;

    // determine which mapping to use
    QMap<int, QList<int>> channelMap;
    QMap<int, QList<int>> busMap;

    if (m_showingCueMapping && m_currentCue && m_currentCue->hasCustomDCAMapping()) {
        channelMap = m_currentCue->dcaChannelMapping();
        busMap = m_currentCue->dcaBusMapping();
    } else {
        channelMap = m_mapping->channelAssignments();
        busMap = m_mapping->busAssignments();
    }

    // build reverse lookup from channel -> DCA
    QMap<int, int> channelToDCA;
    for (const auto& [dca, channels] : channelMap.asKeyValueRange()) {
        for (int ch : channels) {
            channelToDCA[ch] = dca;
        }
    }

    // build reverse lookup from bus -> DCA
    QMap<int, int> busToDCA;
    for (const auto& [dca, buses] : busMap.asKeyValueRange()) {
        for (int bus : buses) {
            busToDCA[bus] = dca;
        }
    }

    // count assignments per DCA
    QMap<int, int> channelCounts;
    QMap<int, int> busCounts;
    for (int d = 1; d <= m_dcaCount; ++d) {
        channelCounts[d] = channelMap.value(d).size();
        busCounts[d] = busMap.value(d).size();
    }

    // update channel labels w/ assignment status
    const QString assignedStyle =
        QString("color: %1; font-weight: bold;").arg(Theme::Colors::AccentBlue);
    for (int ch = 1; ch <= m_channelLabels.size(); ++ch) {
        int dca = channelToDCA.value(ch, -1);
        QLabel* label = m_channelLabels[ch - 1];
        const QString display = channelDisplayName(ch);

        if (dca > 0) {
            label->setText(display);
            label->setStyleSheet(assignedStyle);
            label->setToolTip(tr("%1: assigned to DCA %2, locked from other DCAs; "
                                 "double-click to rename")
                                  .arg(display)
                                  .arg(dca));
        } else {
            label->setText(display);
            label->setStyleSheet("");
            label->setToolTip(
                tr("%1: not assigned to any DCA; double-click to rename").arg(display));
        }
    }

    // update bus labels w/ assignment status
    for (int bus = 1; bus <= m_busLabels.size(); ++bus) {
        int dca = busToDCA.value(bus, -1);
        QLabel* label = m_busLabels[bus - 1];

        if (dca > 0) {
            label->setText(busDisplayName(bus));
            label->setStyleSheet(assignedStyle);
            label->setToolTip(tr("Assigned to DCA %1; double-click to rename").arg(dca));
        } else {
            label->setText(busDisplayName(bus));
            label->setStyleSheet("");
            label->setToolTip(tr("Double-click to rename"));
        }
    }

    QStringList itemTexts;
    QStringList compactTexts;
    for (int d = 1; d <= m_dcaCount; ++d) {
        const int total = channelCounts[d] + busCounts[d];
        const QString name = dcaDisplayName(d);
        const QString plain = tr("DCA %1").arg(d);
        itemTexts << (total > 0 ? tr("%1 (%2)").arg(name).arg(total) : name);
        compactTexts << (total > 0 ? tr("%1 (%2)").arg(plain).arg(total) : plain);
    }

    for (QComboBox* combo : m_channelCombos) {
        for (int d = 1; d <= m_dcaCount; ++d) {
            combo->setItemText(d, itemTexts[d - 1]);
            combo->setItemData(d, compactTexts[d - 1], NoScrollComboBox::CompactTextRole);
        }
    }

    for (QComboBox* combo : m_busCombos) {
        for (int d = 1; d <= m_dcaCount; ++d) {
            combo->setItemText(d, itemTexts[d - 1]);
            combo->setItemData(d, compactTexts[d - 1], NoScrollComboBox::CompactTextRole);
        }
    }
}

void DCAMappingPanel::refresh() {
    if (m_app && m_app->show()) {
        m_mapping = m_app->show()->dcaMapping();
    }
    updateDCAOptions();
}

void DCAMappingPanel::syncFromMixer() {
    if (!m_app || !m_app->mixer() || !m_mapping) {
        QMessageBox::warning(m_app ? m_app->mainWindow() : nullptr, tr("Sync from Mixer"),
                             tr("No mixer connected."));
        return;
    }

    MixerProtocol* mixer = m_app->mixer();
    if (!mixer->isConnected()) {
        QMessageBox::warning(m_app->mainWindow(), tr("Sync from Mixer"),
                             tr("Mixer is not connected."));
        return;
    }

    const MixerCapabilities& caps = mixer->capabilities();

    // only OSC (X32/M32/WING) & loopback support DCA queries
    if (caps.protocol != ProtocolType::OscUdp && caps.protocol != ProtocolType::Internal) {
        QMessageBox::information(
            m_app->mainWindow(), tr("Sync from Mixer"),
            tr("DCA assignment sync is not supported for %1.").arg(caps.displayName));
        return;
    }

    // clear existing assignments before sync
    m_mapping->clear();

    // track pending requests
    struct SyncState {
        int pendingCount = 0;
        QMap<int, int> channelDCAMasks;
        QMap<int, int> busDCAMasks;
    };
    auto state = std::make_shared<SyncState>();

    // X32/M32/loopback use zero-padded paths, WING uses non-padded
    bool useZeroPadding = (caps.type != ConsoleType::Wing);

    int channelCount = std::min(caps.inputChannels, 32);
    int busCount = std::min(caps.mixBuses, 16);
    state->pendingCount = channelCount + busCount;

    // capture references for callbacks
    DCAMapping* mapping = m_mapping;
    QPointer<DCAMappingPanel> self = this;
    QWidget* mainWindow = m_app->mainWindow();
    int dcaCount = caps.dcaCount;

    auto processCompletedSync = [self, mapping, state, dcaCount, mainWindow]() {
        if (--state->pendingCount > 0)
            return;

        // requests completed, populate mapping from bitmasks
        for (const auto& [channel, mask] : state->channelDCAMasks.asKeyValueRange()) {
            for (int d = 1; d <= dcaCount; ++d) {
                if (mask & (1 << (d - 1))) {
                    mapping->assignChannelToDCA(channel, d);
                }
            }
        }

        for (const auto& [bus, mask] : state->busDCAMasks.asKeyValueRange()) {
            for (int d = 1; d <= dcaCount; ++d) {
                if (mask & (1 << (d - 1))) {
                    mapping->assignBusToDCA(bus, d);
                }
            }
        }

        if (self) {
            self->refresh();
            QMessageBox::information(mainWindow, tr("Sync from Mixer"),
                                     tr("DCA assignments synced from mixer."));
        }
    };

    // request channel DCA assignments
    for (int ch = 1; ch <= channelCount; ++ch) {
        QString path = useZeroPadding ? QString("/ch/%1/grp/dca").arg(ch, 2, 10, QChar('0'))
                                      : QString("/ch/%1/grp/dca").arg(ch);

        mixer->requestParameterAsync(
            path,
            [state, ch, processCompletedSync](const QString&, const QVariant& value, bool success) {
                if (success && value.isValid()) {
                    state->channelDCAMasks[ch] = value.toInt();
                }
                processCompletedSync();
            });
    }

    // request bus DCA assignments
    for (int bus = 1; bus <= busCount; ++bus) {
        QString path = useZeroPadding ? QString("/bus/%1/grp/dca").arg(bus, 2, 10, QChar('0'))
                                      : QString("/bus/%1/grp/dca").arg(bus);

        mixer->requestParameterAsync(path, [state, bus, processCompletedSync](const QString&,
                                                                              const QVariant& value,
                                                                              bool success) {
            if (success && value.isValid()) {
                state->busDCAMasks[bus] = value.toInt();
            }
            processCompletedSync();
        });
    }
}

void DCAMappingPanel::saveMappingPreset() {
    if (!m_mapping)
        return;

    QString filePath =
        QFileDialog::getSaveFileName(m_app->mainWindow(), tr("Save DCA Mapping Preset"), QString(),
                                     tr("DCA Mapping (*.dcamap)"));
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(".dcamap")) {
        filePath += ".dcamap";
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(m_app->mainWindow(), tr("Error"),
                             tr("Failed to export: %1").arg(file.errorString()));
        return;
    }

    QJsonDocument doc(m_mapping->toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void DCAMappingPanel::loadMappingPreset() {
    if (!m_mapping)
        return;

    QString filePath =
        QFileDialog::getOpenFileName(m_app->mainWindow(), tr("Load DCA Mapping Preset"), QString(),
                                     tr("DCA Mapping (*.dcamap)"));
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(m_app->mainWindow(), tr("Error"),
                             tr("Failed to load: %1").arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(m_app->mainWindow(), tr("Error"),
                             tr("Invalid preset file: %1").arg(parseError.errorString()));
        return;
    }

    m_mapping->loadFromJson(doc.object());
    m_app->show()->setModified(true);
    populateFromMapping();
}

void DCAMappingPanel::clearAllMappings() {
    if (!m_mapping)
        return;
    if (QMessageBox::question(this, tr("Clear All DCA Mappings"),
                              tr("Remove all channel and bus DCA assignments?")) !=
        QMessageBox::Yes)
        return;
    m_mapping->clear();
    m_app->show()->setModified(true);
}

void DCAMappingPanel::onChannelDCAChanged(int channel, int dca) {
    if (m_showingCueMapping && m_currentCue) {
        // editing cue-specific mapping
        QMap<int, QList<int>> channelMap = m_currentCue->dcaChannelMapping();

        // remove from any existing DCA
        for (auto& channels : channelMap) {
            channels.removeAll(channel);
        }

        // add to new DCA
        if (dca > 0) {
            channelMap[dca].append(channel);
        }

        m_currentCue->setDCAChannelMapping(channelMap);
        m_app->show()->setModified(true);
    } else if (m_mapping) {
        // editing show-level mapping
        if (dca < 0) {
            m_mapping->clearChannelFromAllDCAs(channel);
        } else {
            m_mapping->assignChannelToDCA(channel, dca);
        }
        m_app->show()->setModified(true);
    }
}

void DCAMappingPanel::onBusDCAChanged(int bus, int dca) {
    if (m_showingCueMapping && m_currentCue) {
        // editing cue-specific mapping
        QMap<int, QList<int>> busMap = m_currentCue->dcaBusMapping();

        // remove from any existing DCA
        for (auto& buses : busMap) {
            buses.removeAll(bus);
        }

        // add to new DCA
        if (dca > 0) {
            busMap[dca].append(bus);
        }

        m_currentCue->setDCABusMapping(busMap);
        m_app->show()->setModified(true);
    } else if (m_mapping) {
        // editing show-level mapping
        if (dca < 0) {
            m_mapping->clearBusFromAllDCAs(bus);
        } else {
            m_mapping->assignBusToDCA(bus, dca);
        }
        m_app->show()->setModified(true);
    }
}

void DCAMappingPanel::onMixerConnected() {
    m_syncButton->setEnabled(true);
    m_syncFromMixerAction->setEnabled(true);
    // count changes arrive via Application::dcaCountChanged -> refresh(), but
    // same-count consoles still need channel/bus counts re-read
    updateDCAOptions();
}

void DCAMappingPanel::onMixerDisconnected() {
    m_syncButton->setEnabled(false);
    m_syncFromMixerAction->setEnabled(false);
}

void DCAMappingPanel::setCurrentCue(Cue* cue) {
    m_currentCue = cue;

    if (cue) {
        // check if cue has custom mapping
        m_showingCueMapping = cue->hasCustomDCAMapping();
        m_useCueMappingCheck->blockSignals(true);
        m_useCueMappingCheck->setChecked(m_showingCueMapping);
        m_useCueMappingCheck->blockSignals(false);
    } else {
        m_showingCueMapping = false;
    }

    updateContextHeader();
    populateFromMapping();
}

void DCAMappingPanel::clearCurrentCue() {
    m_currentCue = nullptr;
    m_showingCueMapping = false;
    updateContextHeader();
    populateFromMapping();
}

void DCAMappingPanel::updateContextHeader() {
    if (m_currentCue) {
        // show cue context
        QString cueText = QString("Cue %1").arg(m_currentCue->number(), 0, 'f', 1);
        if (!m_currentCue->name().isEmpty()) {
            cueText += QString(": %1").arg(m_currentCue->name());
        }
        m_contextLabel->setText(cueText);

        if (m_currentCue->hasCustomDCAMapping()) {
            m_mappingStateLabel->setText(tr("cue-specific mapping"));
            m_mappingStateLabel->setStyleSheet(
                QString("color: %1;").arg(Theme::Colors::AccentBlue));
        } else {
            m_mappingStateLabel->setText(tr("using show-level mapping"));
            m_mappingStateLabel->setStyleSheet(
                QString("color: %1;").arg(Theme::Colors::TextSecondary));
        }
        m_mappingStateLabel->setVisible(true);

        m_useCueMappingCheck->setVisible(true);
        m_copyFromShowButton->setVisible(m_showingCueMapping);
        m_clearCueMappingButton->setVisible(m_showingCueMapping &&
                                            m_currentCue->hasCustomDCAMapping());
    } else {
        // show-level context
        m_contextLabel->setText(tr("Show Level"));
        m_mappingStateLabel->setVisible(false);
        m_useCueMappingCheck->setVisible(false);
        m_copyFromShowButton->setVisible(false);
        m_clearCueMappingButton->setVisible(false);
    }
}

void DCAMappingPanel::onUseCueMappingToggled(bool enabled) {
    if (!m_currentCue)
        return;

    m_showingCueMapping = enabled;

    if (enabled && !m_currentCue->hasCustomDCAMapping()) {
        // copy show mapping as starting point
        m_currentCue->copyDCAMappingFrom(m_mapping);
        m_app->show()->setModified(true);
    }

    updateContextHeader();
    populateFromMapping();
}

void DCAMappingPanel::copyShowMappingToCue() {
    if (!m_currentCue || !m_mapping)
        return;

    m_currentCue->copyDCAMappingFrom(m_mapping);
    m_app->show()->setModified(true);
    populateFromMapping();
}

void DCAMappingPanel::clearCueMapping() {
    if (!m_currentCue)
        return;

    m_currentCue->clearCustomDCAMapping();
    m_showingCueMapping = false;

    m_useCueMappingCheck->blockSignals(true);
    m_useCueMappingCheck->setChecked(false);
    m_useCueMappingCheck->blockSignals(false);

    m_app->show()->setModified(true);
    updateContextHeader();
    populateFromMapping();
}

QString DCAMappingPanel::busDisplayName(int bus) const {
    if (m_mapping) {
        QString name = m_mapping->busName(bus);
        if (!name.isEmpty())
            return name;
    }
    return tr("Bus %1").arg(bus);
}

QString DCAMappingPanel::channelDisplayName(int channel) const {
    const ActorProfileLibrary* library =
        (m_app && m_app->show()) ? m_app->show()->actorProfileLibrary() : nullptr;
    const Actor* actor = library ? library->actorForChannel(channel) : nullptr;
    if (actor && !actor->displayName().isEmpty()) {
        const QString secondary = actor->secondaryName();
        if (!secondary.isEmpty())
            return tr("Ch %1: %2 (%3)").arg(channel).arg(actor->displayName(), secondary);
        return tr("Ch %1: %2").arg(channel).arg(actor->displayName());
    }
    return tr("Ch %1").arg(channel);
}

QString DCAMappingPanel::dcaDisplayName(int dca) const {
    if (m_currentCue) {
        const QString label = m_currentCue->dcaOverride(dca).label.value_or(QString());
        if (!label.isEmpty())
            return tr("DCA %1: %2").arg(dca).arg(label);
    }
    return tr("DCA %1").arg(dca);
}

QString DCAMappingPanel::overviewMemberName(int channel, const QString& dcaLabel) const {
    // drop the part of the channel's label that just repeats the DCA name
    const ActorProfileLibrary* library =
        (m_app && m_app->show()) ? m_app->show()->actorProfileLibrary() : nullptr;
    const Actor* actor = library ? library->actorForChannel(channel) : nullptr;
    if (!actor || actor->displayName().isEmpty() || dcaLabel.isEmpty())
        return channelDisplayName(channel);

    const QString primary = actor->displayName();
    const QString secondary = actor->secondaryName(actor->matchedRole(dcaLabel));
    if (primary.compare(dcaLabel, Qt::CaseInsensitive) == 0) {
        return secondary.isEmpty() ? tr("Ch %1").arg(channel)
                                   : tr("Ch %1 (%2)").arg(channel).arg(secondary);
    }
    if (!secondary.isEmpty() && secondary.compare(dcaLabel, Qt::CaseInsensitive) == 0)
        return tr("Ch %1: %2").arg(channel).arg(primary);
    return channelDisplayName(channel);
}

void DCAMappingPanel::updateDcaOverview() {
    if (!m_mapping)
        return;

    // effective mapping = what playback would use: cue custom when present,
    // else show-level (deliberately not gated on the editing toggle)
    const bool cueMapped = m_currentCue && m_currentCue->hasCustomDCAMapping();
    const QMap<int, QList<int>> channelMap =
        cueMapped ? m_currentCue->dcaChannelMapping() : m_mapping->channelAssignments();
    const QMap<int, QList<int>> busMap =
        cueMapped ? m_currentCue->dcaBusMapping() : m_mapping->busAssignments();

    for (int d = 1; d <= m_dcaOverviewTitles.size(); ++d) {
        QLabel* title = m_dcaOverviewTitles[d - 1];
        QLabel* members = m_dcaOverviewMembers[d - 1];

        title->setText(dcaDisplayName(d));

        const QString dcaLabel =
            m_currentCue ? m_currentCue->dcaOverride(d).label.value_or(QString()) : QString();
        QStringList parts;
        for (int ch : channelMap.value(d))
            parts << overviewMemberName(ch, dcaLabel);
        for (int bus : busMap.value(d)) {
            const QString busName = busDisplayName(bus);
            parts << (busName.compare(dcaLabel, Qt::CaseInsensitive) == 0 ? tr("Bus %1").arg(bus)
                                                                          : busName);
        }
        members->setText(parts.isEmpty() ? tr("(empty)") : parts.join(", "));
    }
}

void DCAMappingPanel::onActorsChanged() {
    updateComboItemStates();
    updateDcaOverview();
}

bool DCAMappingPanel::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (auto* label = qobject_cast<QLabel*>(obj); label && label->property("busIndex").isValid()) {
            int bus = label->property("busIndex").toInt();
            startBusNameEdit(bus);
            return true;
        }
        if (auto* label = qobject_cast<QLabel*>(obj);
            label && label->property("channelIndex").isValid()) {
            startChannelNameEdit(label->property("channelIndex").toInt());
            return true;
        }
    }
    if (event->type() == QEvent::KeyPress) {
        if (auto* edit = qobject_cast<QLineEdit*>(obj); edit && edit->property("busIndex").isValid()) {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                int bus = edit->property("busIndex").toInt();
                cancelBusNameEdit(bus);
                return true;
            }
        }
        if (auto* edit = qobject_cast<QLineEdit*>(obj);
            edit && edit->property("channelIndex").isValid()) {
            QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                cancelChannelNameEdit(edit->property("channelIndex").toInt());
                return true;
            }
        }
        // grid keyboard navigation on the assignment combos: arrows move
        // focus, digits assign, 0/Delete/Backspace clear. Enter stays with
        // NoScrollComboBox (opens the popup); popup-open arrows go to the
        // popup view, never through this filter.
        if (auto* combo = qobject_cast<QComboBox*>(obj)) {
            const bool isChannel = combo->property("channel").isValid();
            const bool isBus = combo->property("bus").isValid();
            if (isChannel || isBus) {
                QKeyEvent* ke = static_cast<QKeyEvent*>(event);
                if (ke->modifiers() & (Qt::ControlModifier | Qt::AltModifier))
                    return QWidget::eventFilter(obj, event);

                QVector<QComboBox*>& combos = isChannel ? m_channelCombos : m_busCombos;
                const int rowsPerBlock = isChannel ? 16 : 8;
                const int index =
                    combo->property(isChannel ? "channel" : "bus").toInt() - 1;

                int dRow = 0, dCol = 0;
                switch (ke->key()) {
                case Qt::Key_Up:
                    dRow = -1;
                    break;
                case Qt::Key_Down:
                    dRow = 1;
                    break;
                case Qt::Key_Left:
                    dCol = -1;
                    break;
                case Qt::Key_Right:
                    dCol = 1;
                    break;
                default: {
                    const int comboRow = DcaGridNav::comboRowForKey(ke->key(), m_dcaCount);
                    if (comboRow >= 0 && comboRow < combo->count()) {
                        combo->setCurrentIndex(comboRow); // fires the commit path
                        return true;
                    }
                    return QWidget::eventFilter(obj, event);
                }
                }

                const int target =
                    DcaGridNav::move(index, combos.size(), rowsPerBlock, dRow, dCol);
                if (target >= 0) {
                    combos[target]->setFocus(Qt::OtherFocusReason);
                    m_scrollArea->ensureWidgetVisible(combos[target]);
                }
                return true; // swallow arrows either way: no silent value change
            }
        }
    }
    if (event->type() == QEvent::FocusOut) {
        if (auto* edit = qobject_cast<QLineEdit*>(obj); edit && edit->property("busIndex").isValid()) {
            int bus = edit->property("busIndex").toInt();
            finishBusNameEdit(bus);
            return false;
        }
        if (auto* edit = qobject_cast<QLineEdit*>(obj);
            edit && edit->property("channelIndex").isValid()) {
            finishChannelNameEdit(edit->property("channelIndex").toInt());
            return false;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DCAMappingPanel::startBusNameEdit(int bus) {
    if (bus < 1 || bus > m_busLabels.size())
        return;

    QLabel* label = m_busLabels[bus - 1];
    QLineEdit* edit = m_busNameEdits[bus - 1];

    label->setVisible(false);
    edit->setVisible(true);
    edit->setText(m_mapping ? m_mapping->busName(bus) : QString());
    edit->setPlaceholderText(tr("Bus %1").arg(bus));
    edit->setFocus();
    edit->selectAll();
}

void DCAMappingPanel::finishBusNameEdit(int bus) {
    if (bus < 1 || bus > m_busNameEdits.size())
        return;

    QLineEdit* edit = m_busNameEdits[bus - 1];
    if (!edit->isVisible())
        return;
    QString name = edit->text().trimmed();

    if (m_mapping) {
        m_mapping->setBusName(bus, name);
        m_app->show()->setModified(true);
    }

    edit->setVisible(false);
    m_busLabels[bus - 1]->setVisible(true);
    updateComboItemStates();
}

void DCAMappingPanel::cancelBusNameEdit(int bus) {
    if (bus < 1 || bus > m_busNameEdits.size())
        return;

    m_busNameEdits[bus - 1]->setVisible(false);
    m_busLabels[bus - 1]->setVisible(true);
}

void DCAMappingPanel::startChannelNameEdit(int channel) {
    if (channel < 1 || channel > m_channelNameEdits.size())
        return;

    const ActorProfileLibrary* library =
        (m_app && m_app->show()) ? m_app->show()->actorProfileLibrary() : nullptr;
    const Actor* actor = library ? library->actorForChannel(channel) : nullptr;

    QLabel* label = m_channelLabels[channel - 1];
    QLineEdit* edit = m_channelNameEdits[channel - 1];

    label->setVisible(false);
    edit->setVisible(true);
    edit->setText(actor ? actor->name() : QString());
    edit->setFocus();
    edit->selectAll();
}

void DCAMappingPanel::finishChannelNameEdit(int channel) {
    if (channel < 1 || channel > m_channelNameEdits.size())
        return;

    QLineEdit* edit = m_channelNameEdits[channel - 1];
    if (!edit->isVisible())
        return;
    const QString name = edit->text().trimmed();

    edit->setVisible(false);
    m_channelLabels[channel - 1]->setVisible(true);

    ActorProfileLibrary* library =
        (m_app && m_app->show()) ? m_app->show()->actorProfileLibrary() : nullptr;
    if (!library)
        return;

    // rename the actor on this channel; with no actor, cast a new one. Empty
    // text never deletes cast from this panel. The library's changed() signal
    // relabels this panel, Actor Setup, the cue list, and the console strips.
    if (const Actor* existing = library->actorForChannel(channel)) {
        if (!name.isEmpty() && name != existing->name()) {
            Actor copy = *existing;
            copy.setName(name);
            library->updateActor(copy.id(), copy);
        }
    } else if (!name.isEmpty()) {
        library->addActor(Actor(name, channel));
    }
}

void DCAMappingPanel::cancelChannelNameEdit(int channel) {
    if (channel < 1 || channel > m_channelNameEdits.size())
        return;

    m_channelNameEdits[channel - 1]->setVisible(false);
    m_channelLabels[channel - 1]->setVisible(true);
}

} // namespace OpenMix
