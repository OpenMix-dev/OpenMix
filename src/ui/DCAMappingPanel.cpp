#include "DCAMappingPanel.h"
#include "app/Application.h"
#include "core/DCAMapping.h"
#include "core/Show.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/MixerProtocol.h"
#include "theme/Icons.h"
#include "theme/Theme.h"

#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <memory>

namespace OpenMix {

DCAMappingPanel::DCAMappingPanel(Application* app, QWidget* parent)
    : QWidget(parent), m_app(app), m_mapping(nullptr) {
    setupUi();

    if (m_app) {
        connect(m_app, &Application::mixerConnected, this, &DCAMappingPanel::onMixerConnected);
        connect(m_app, &Application::mixerDisconnected, this,
                &DCAMappingPanel::onMixerDisconnected);

        // get mapping from show
        m_mapping = m_app->show()->dcaMapping();
        if (m_mapping) {
            connect(m_mapping, &DCAMapping::mappingCleared, this, &DCAMappingPanel::refresh);
        }
    }

    refresh();
}

void DCAMappingPanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();

    m_syncButton = new QPushButton(Icons::refresh(), tr("Sync from Mixer"), this);
    m_syncButton->setToolTip(tr("Pull current DCA assignments from connected mixer"));
    m_syncButton->setEnabled(false);
    connect(m_syncButton, &QPushButton::clicked, this, &DCAMappingPanel::syncFromMixer);
    toolbarLayout->addWidget(m_syncButton);

    m_savePresetButton = new QPushButton(Icons::download(), tr("Save Preset"), this);
    m_savePresetButton->setToolTip(tr("Save current mapping as a preset file"));
    connect(m_savePresetButton, &QPushButton::clicked, this, &DCAMappingPanel::saveMappingPreset);
    toolbarLayout->addWidget(m_savePresetButton);

    m_loadPresetButton = new QPushButton(Icons::upload(), tr("Load Preset"), this);
    m_loadPresetButton->setToolTip(tr("Load mapping from a preset file"));
    connect(m_loadPresetButton, &QPushButton::clicked, this, &DCAMappingPanel::loadMappingPreset);
    toolbarLayout->addWidget(m_loadPresetButton);

    m_clearAllButton = new QPushButton(Icons::editClear(), tr("Clear All"), this);
    m_clearAllButton->setToolTip(tr("Remove all channel and bus assignments"));
    connect(m_clearAllButton, &QPushButton::clicked, [this]() {
        if (m_mapping) {
            m_mapping->clear();
            m_app->show()->setModified(true);
        }
    });
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

    createChannelSection();
    createBusSection();
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

    // header
    m_channelLayout->addWidget(new QLabel(tr("Channel"), m_channelGroup), 0, 0);
    m_channelLayout->addWidget(new QLabel(tr("DCA"), m_channelGroup), 0, 1);

    // create rows for each channel (2 columns per row, up to 4 channel pairs)
    int colOffset = 0;
    for (int ch = 1; ch <= m_channelCount; ++ch) {
        int row = ((ch - 1) % 16) + 1;
        colOffset = ((ch - 1) / 16) * 3;

        QLabel* label = new QLabel(tr("Ch %1").arg(ch), m_channelGroup);
        label->setMinimumWidth(60);
        m_channelLayout->addWidget(label, row, colOffset);
        m_channelLabels.append(label);

        QComboBox* combo = new QComboBox(m_channelGroup);
        combo->addItem(tr("None"), -1);
        for (int d = 1; d <= m_dcaCount; ++d) {
            combo->addItem(tr("DCA %1").arg(d), d);
        }
        combo->setProperty("channel", ch);
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

    // header
    m_busLayout->addWidget(new QLabel(tr("Bus"), m_busGroup), 0, 0);
    m_busLayout->addWidget(new QLabel(tr("DCA"), m_busGroup), 0, 1);

    // create rows for each bus
    int colOffset = 0;
    for (int bus = 1; bus <= m_busCount; ++bus) {
        int row = ((bus - 1) % 8) + 1;
        colOffset = ((bus - 1) / 8) * 3;

        QLabel* label = new QLabel(tr("Bus %1").arg(bus), m_busGroup);
        label->setMinimumWidth(60);
        m_busLayout->addWidget(label, row, colOffset);
        m_busLabels.append(label);

        QComboBox* combo = new QComboBox(m_busGroup);
        combo->addItem(tr("None"), -1);
        for (int d = 1; d <= m_dcaCount; ++d) {
            combo->addItem(tr("DCA %1").arg(d), d);
        }
        combo->setProperty("bus", bus);
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
    // update number of DCA options based on mixer capabilities
    if (m_app && m_app->mixer() && m_app->mixer()->isConnected()) {
        const MixerCapabilities& caps = m_app->mixer()->capabilities();
        m_dcaCount = caps.dcaCount;
        m_channelCount = caps.inputChannels;
        m_busCount = caps.mixBuses;
    } else {
        // default values
        m_dcaCount = 8;
        m_channelCount = 32;
        m_busCount = 16;
    }

    // rebuild sections with new counts
    createChannelSection();
    createBusSection();
    populateFromMapping();
}

void DCAMappingPanel::populateFromMapping() {
    if (!m_mapping)
        return;

    m_updatingUi = true;

    // set channel combos
    for (int ch = 1; ch <= m_channelCombos.size(); ++ch) {
        int dca = m_mapping->dcaForChannel(ch);
        int index = 0;
        if (dca > 0 && dca <= m_dcaCount) {
            index = dca; // DCA 1 is at index 1, etc.
        }
        m_channelCombos[ch - 1]->setCurrentIndex(index);
    }

    // set bus combos
    for (int bus = 1; bus <= m_busCombos.size(); ++bus) {
        int dca = m_mapping->dcaForBus(bus);
        int index = 0;
        if (dca > 0 && dca <= m_dcaCount) {
            index = dca;
        }
        m_busCombos[bus - 1]->setCurrentIndex(index);
    }

    m_updatingUi = false;

    updateComboItemStates();
}

void DCAMappingPanel::updateComboItemStates() {
    if (!m_mapping)
        return;

    // count assignments per DCA
    QMap<int, int> channelCounts;
    QMap<int, int> busCounts;
    for (int d = 1; d <= m_dcaCount; ++d) {
        channelCounts[d] = m_mapping->channelsForDCA(d).size();
        busCounts[d] = m_mapping->busesForDCA(d).size();
    }

    // update channel labels w/ assignment status
    const QString assignedStyle =
        QString("color: %1; font-weight: bold;").arg(Theme::Colors::AccentBlue);
    for (int ch = 1; ch <= m_channelLabels.size(); ++ch) {
        int dca = m_mapping->dcaForChannel(ch);
        QLabel* label = m_channelLabels[ch - 1];

        if (dca > 0) {
            label->setText(tr("Ch %1 [%2]").arg(ch).arg(dca));
            label->setStyleSheet(assignedStyle);
            label->setToolTip(tr("Assigned to DCA %1 - locked from other DCAs").arg(dca));
        } else {
            label->setText(tr("Ch %1").arg(ch));
            label->setStyleSheet("");
            label->setToolTip(tr("Not assigned to any DCA"));
        }
    }

    // update bus labels w/ assignment status
    for (int bus = 1; bus <= m_busLabels.size(); ++bus) {
        int dca = m_mapping->dcaForBus(bus);
        QLabel* label = m_busLabels[bus - 1];

        if (dca > 0) {
            label->setText(tr("Bus %1 [%2]").arg(bus).arg(dca));
            label->setStyleSheet(assignedStyle);
            label->setToolTip(tr("Assigned to DCA %1 - locked from other DCAs").arg(dca));
        } else {
            label->setText(tr("Bus %1").arg(bus));
            label->setStyleSheet("");
            label->setToolTip(tr("Not assigned to any DCA"));
        }
    }

    // update combo box items w/ assignment counts
    for (QComboBox* combo : m_channelCombos) {
        for (int d = 1; d <= m_dcaCount; ++d) {
            int total = channelCounts[d] + busCounts[d];
            QString text = total > 0 ? tr("DCA %1 (%2)").arg(d).arg(total) : tr("DCA %1").arg(d);
            combo->setItemText(d, text);
        }
    }

    for (QComboBox* combo : m_busCombos) {
        for (int d = 1; d <= m_dcaCount; ++d) {
            int total = channelCounts[d] + busCounts[d];
            QString text = total > 0 ? tr("DCA %1 (%2)").arg(d).arg(total) : tr("DCA %1").arg(d);
            combo->setItemText(d, text);
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
        QMessageBox::warning(this, tr("Sync from Mixer"), tr("No mixer connected."));
        return;
    }

    MixerProtocol* mixer = m_app->mixer();
    if (!mixer->isConnected()) {
        QMessageBox::warning(this, tr("Sync from Mixer"), tr("Mixer is not connected."));
        return;
    }

    const MixerCapabilities& caps = mixer->capabilities();

    // only OSC (X32/M32/WING) & loopback support DCA queries
    if (caps.protocol != ProtocolType::OscUdp && caps.protocol != ProtocolType::Internal) {
        QMessageBox::information(
            this, tr("Sync from Mixer"),
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

    int channelCount = qMin(caps.inputChannels, 32);
    int busCount = qMin(caps.mixBuses, 16);
    state->pendingCount = channelCount + busCount;

    // capture references for callbacks
    DCAMapping* mapping = m_mapping;
    QPointer<DCAMappingPanel> self = this;
    int dcaCount = caps.dcaCount;

    auto processCompletedSync = [self, mapping, state, dcaCount]() {
        if (--state->pendingCount > 0)
            return;

        // requests completed, populate mapping from bitmasks
        for (auto it = state->channelDCAMasks.constBegin(); it != state->channelDCAMasks.constEnd();
             ++it) {
            int channel = it.key();
            int mask = it.value();
            for (int d = 1; d <= dcaCount; ++d) {
                if (mask & (1 << (d - 1))) {
                    mapping->assignChannelToDCA(channel, d);
                }
            }
        }

        for (auto it = state->busDCAMasks.constBegin(); it != state->busDCAMasks.constEnd(); ++it) {
            int bus = it.key();
            int mask = it.value();
            for (int d = 1; d <= dcaCount; ++d) {
                if (mask & (1 << (d - 1))) {
                    mapping->assignBusToDCA(bus, d);
                }
            }
        }

        if (self) {
            self->refresh();
            QMessageBox::information(self, tr("Sync from Mixer"),
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

    QString filePath = QFileDialog::getSaveFileName(this, tr("Save DCA Mapping Preset"), QString(),
                                                    tr("DCA Mapping (*.dcamap)"));
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(".dcamap")) {
        filePath += ".dcamap";
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Failed to save preset: %1").arg(file.errorString()));
        return;
    }

    QJsonDocument doc(m_mapping->toJson());
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void DCAMappingPanel::loadMappingPreset() {
    if (!m_mapping)
        return;

    QString filePath = QFileDialog::getOpenFileName(this, tr("Load DCA Mapping Preset"), QString(),
                                                    tr("DCA Mapping (*.dcamap)"));
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Failed to load preset: %1").arg(file.errorString()));
        return;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::warning(this, tr("Error"),
                             tr("Invalid preset file: %1").arg(parseError.errorString()));
        return;
    }

    m_mapping->loadFromJson(doc.object());
    m_app->show()->setModified(true);
    populateFromMapping();
}

void DCAMappingPanel::onChannelDCAChanged(int channel, int dca) {
    if (!m_mapping)
        return;

    if (dca < 0) {
        m_mapping->clearChannelFromAllDCAs(channel);
    } else {
        m_mapping->assignChannelToDCA(channel, dca);
    }
    m_app->show()->setModified(true);
}

void DCAMappingPanel::onBusDCAChanged(int bus, int dca) {
    if (!m_mapping)
        return;

    if (dca < 0) {
        m_mapping->clearBusFromAllDCAs(bus);
    } else {
        m_mapping->assignBusToDCA(bus, dca);
    }
    m_app->show()->setModified(true);
}

void DCAMappingPanel::onMixerConnected() {
    m_syncButton->setEnabled(true);
    updateDCAOptions();
}

void DCAMappingPanel::onMixerDisconnected() { m_syncButton->setEnabled(false); }

} // namespace OpenMix
