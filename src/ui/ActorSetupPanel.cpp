#include "ActorSetupPanel.h"
#include "ActorGroupIo.h"
#include "app/Application.h"
#include "core/Actor.h"
#include "core/ActorProfile.h"
#include "core/ActorProfileLibrary.h"
#include "core/Show.h"
#include "protocol/MixerCapabilities.h"
#include "protocol/MixerProtocol.h"
#include "theme/Icons.h"
#include "theme/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

namespace OpenMix {

// ---------------------------------------------------------------------------
// VoiceEditorWidget: edits one VoiceData (a channel "voice"). Every parameter
// group is gated by a "Set ..." checkbox so the resulting VoiceData only carries
// the fields the operator actually opted into (matching the optional<> model).
// Plain QWidget (no signals) — change
// notification is delivered through the onChanged callback so no moc is needed.
// ---------------------------------------------------------------------------
class VoiceEditorWidget : public QWidget {
  public:
    explicit VoiceEditorWidget(QWidget* parent = nullptr) : QWidget(parent) { build(); }

    std::function<void()> onChanged;

    void setVoice(const VoiceData& v) {
        m_updating = true;

        m_setGain->setChecked(v.gainDb.has_value());
        m_gain->setValue(v.gainDb.value_or(0.0));

        const bool hasHpf = v.hpfOn.has_value() || v.hpfFreq.has_value();
        m_setHpf->setChecked(hasHpf);
        m_hpfOn->setChecked(v.hpfOn.value_or(true));
        m_hpfFreq->setValue(v.hpfFreq.value_or(80.0));

        const bool hasEq = v.eqOn.has_value() || !v.eqBands.isEmpty();
        m_setEq->setChecked(hasEq);
        m_eqOn->setChecked(v.eqOn.value_or(true));
        m_eqTable->setRowCount(0);
        for (const EqBand& b : v.eqBands)
            addEqRow(b);

        const bool hasDyn = v.dynOn.has_value() || v.dynThreshold.has_value() ||
                            v.dynRatio.has_value() || v.dynAttack.has_value() ||
                            v.dynRelease.has_value() || v.dynGain.has_value();
        m_setDyn->setChecked(hasDyn);
        m_dynOn->setChecked(v.dynOn.value_or(true));
        m_thr->setValue(v.dynThreshold.value_or(-20.0));
        m_ratio->setValue(v.dynRatio.value_or(3.0));
        m_att->setValue(v.dynAttack.value_or(10.0));
        m_rel->setValue(v.dynRelease.value_or(100.0));
        m_makeup->setValue(v.dynGain.value_or(0.0));

        updateEnabledStates();
        m_updating = false;
    }

    [[nodiscard]] VoiceData voice() const {
        VoiceData v;
        if (m_setGain->isChecked())
            v.gainDb = m_gain->value();
        if (m_setHpf->isChecked()) {
            v.hpfOn = m_hpfOn->isChecked();
            v.hpfFreq = m_hpfFreq->value();
        }
        if (m_setEq->isChecked()) {
            v.eqOn = m_eqOn->isChecked();
            for (int r = 0; r < m_eqTable->rowCount(); ++r) {
                EqBand b;
                b.band = cellSpin(r, 0)->value();
                b.on = cellCheck(r, 1)->isChecked();
                b.type = cellCombo(r, 2)->currentIndex();
                b.freq = cellDouble(r, 3)->value();
                b.gain = cellDouble(r, 4)->value();
                b.q = cellDouble(r, 5)->value();
                v.eqBands.append(b);
            }
        }
        if (m_setDyn->isChecked()) {
            v.dynOn = m_dynOn->isChecked();
            v.dynThreshold = m_thr->value();
            v.dynRatio = m_ratio->value();
            v.dynAttack = m_att->value();
            v.dynRelease = m_rel->value();
            v.dynGain = m_makeup->value();
        }
        return v;
    }

  private:
    void notify() {
        if (!m_updating && onChanged)
            onChanged();
    }

    QDoubleSpinBox* makeDouble(double lo, double hi, int decimals, double step,
                               const QString& suffix = QString()) {
        auto* s = new QDoubleSpinBox(this);
        s->setRange(lo, hi);
        s->setDecimals(decimals);
        s->setSingleStep(step);
        if (!suffix.isEmpty())
            s->setSuffix(suffix);
        connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { notify(); });
        return s;
    }

    void build() {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(8);

        // gain
        auto* gainBox = new QGroupBox(tr("Preamp Gain"), this);
        auto* gainLayout = new QHBoxLayout(gainBox);
        m_setGain = new QCheckBox(tr("Set"), gainBox);
        m_gain = makeDouble(-90.0, 30.0, 1, 0.5, tr(" dB"));
        gainLayout->addWidget(m_setGain);
        gainLayout->addWidget(m_gain, 1);
        layout->addWidget(gainBox);

        // HPF
        auto* hpfBox = new QGroupBox(tr("High-Pass Filter"), this);
        auto* hpfLayout = new QHBoxLayout(hpfBox);
        m_setHpf = new QCheckBox(tr("Set"), hpfBox);
        m_hpfOn = new QCheckBox(tr("On"), hpfBox);
        m_hpfFreq = makeDouble(20.0, 2000.0, 0, 5.0, tr(" Hz"));
        hpfLayout->addWidget(m_setHpf);
        hpfLayout->addWidget(m_hpfOn);
        hpfLayout->addWidget(m_hpfFreq, 1);
        layout->addWidget(hpfBox);

        // EQ
        auto* eqBox = new QGroupBox(tr("Parametric EQ"), this);
        auto* eqLayout = new QVBoxLayout(eqBox);
        auto* eqHeader = new QHBoxLayout();
        m_setEq = new QCheckBox(tr("Set"), eqBox);
        m_eqOn = new QCheckBox(tr("On"), eqBox);
        m_addBandBtn = new QPushButton(Icons::listAdd(), tr("Add Band"), eqBox);
        m_delBandBtn = new QPushButton(Icons::listRemove(), tr("Remove Band"), eqBox);
        eqHeader->addWidget(m_setEq);
        eqHeader->addWidget(m_eqOn);
        eqHeader->addStretch();
        eqHeader->addWidget(m_addBandBtn);
        eqHeader->addWidget(m_delBandBtn);
        eqLayout->addLayout(eqHeader);

        m_eqTable = new QTableWidget(0, 6, eqBox);
        m_eqTable->setHorizontalHeaderLabels(
            {tr("Band"), tr("On"), tr("Type"), tr("Freq"), tr("Gain"), tr("Q")});
        m_eqTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_eqTable->verticalHeader()->setVisible(false);
        m_eqTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_eqTable->setMinimumHeight(120);
        eqLayout->addWidget(m_eqTable);
        layout->addWidget(eqBox);

        connect(m_addBandBtn, &QPushButton::clicked, this, [this]() {
            EqBand b;
            b.band = m_eqTable->rowCount() + 1;
            addEqRow(b);
            notify();
        });
        connect(m_delBandBtn, &QPushButton::clicked, this, [this]() {
            int row = m_eqTable->currentRow();
            if (row < 0)
                row = m_eqTable->rowCount() - 1;
            if (row >= 0) {
                m_eqTable->removeRow(row);
                notify();
            }
        });

        // dynamics
        auto* dynBox = new QGroupBox(tr("Dynamics"), this);
        auto* dynLayout = new QFormLayout(dynBox);
        auto* dynTop = new QHBoxLayout();
        m_setDyn = new QCheckBox(tr("Set"), dynBox);
        m_dynOn = new QCheckBox(tr("On"), dynBox);
        dynTop->addWidget(m_setDyn);
        dynTop->addWidget(m_dynOn);
        dynTop->addStretch();
        dynLayout->addRow(dynTop);
        m_thr = makeDouble(-60.0, 0.0, 1, 0.5, tr(" dB"));
        m_ratio = makeDouble(1.0, 20.0, 1, 0.1, tr(":1"));
        m_att = makeDouble(0.1, 200.0, 1, 1.0, tr(" ms"));
        m_rel = makeDouble(5.0, 2000.0, 0, 5.0, tr(" ms"));
        m_makeup = makeDouble(0.0, 24.0, 1, 0.5, tr(" dB"));
        dynLayout->addRow(tr("Threshold:"), m_thr);
        dynLayout->addRow(tr("Ratio:"), m_ratio);
        dynLayout->addRow(tr("Attack:"), m_att);
        dynLayout->addRow(tr("Release:"), m_rel);
        dynLayout->addRow(tr("Makeup:"), m_makeup);
        layout->addWidget(dynBox);

        layout->addStretch();

        // gate value widgets behind their "Set" checkbox & notify on every edit
        for (QCheckBox* set : {m_setGain, m_setHpf, m_setEq, m_setDyn}) {
            connect(set, &QCheckBox::toggled, this, [this]() {
                updateEnabledStates();
                notify();
            });
        }
        for (QCheckBox* val : {m_hpfOn, m_eqOn, m_dynOn}) {
            connect(val, &QCheckBox::toggled, this, [this]() { notify(); });
        }
    }

    void addEqRow(const EqBand& b) {
        const int row = m_eqTable->rowCount();
        m_eqTable->insertRow(row);

        auto* band = new QSpinBox(m_eqTable);
        band->setRange(1, 32);
        band->setValue(b.band);
        connect(band, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { notify(); });
        m_eqTable->setCellWidget(row, 0, band);

        auto* on = new QCheckBox(m_eqTable);
        on->setChecked(b.on);
        connect(on, &QCheckBox::toggled, this, [this]() { notify(); });
        m_eqTable->setCellWidget(row, 1, on);

        auto* type = new QComboBox(m_eqTable);
        type->addItems({tr("PEQ"), tr("Lo Shelf"), tr("Hi Shelf"), tr("HPF"), tr("LPF")});
        type->setCurrentIndex(std::clamp(b.type, 0, type->count() - 1));
        connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this](int) { notify(); });
        m_eqTable->setCellWidget(row, 2, type);

        auto* freq = new QDoubleSpinBox(m_eqTable);
        freq->setRange(20.0, 20000.0);
        freq->setDecimals(0);
        freq->setSuffix(tr(" Hz"));
        freq->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        freq->setValue(b.freq);
        connect(freq, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { notify(); });
        m_eqTable->setCellWidget(row, 3, freq);

        auto* gain = new QDoubleSpinBox(m_eqTable);
        gain->setRange(-18.0, 18.0);
        gain->setDecimals(1);
        gain->setSuffix(tr(" dB"));
        gain->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        gain->setValue(b.gain);
        connect(gain, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { notify(); });
        m_eqTable->setCellWidget(row, 4, gain);

        auto* q = new QDoubleSpinBox(m_eqTable);
        q->setRange(0.1, 10.0);
        q->setDecimals(2);
        q->setValue(b.q);
        connect(q, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { notify(); });
        m_eqTable->setCellWidget(row, 5, q);
    }

    QSpinBox* cellSpin(int row, int col) const {
        return qobject_cast<QSpinBox*>(m_eqTable->cellWidget(row, col));
    }
    QCheckBox* cellCheck(int row, int col) const {
        return qobject_cast<QCheckBox*>(m_eqTable->cellWidget(row, col));
    }
    QComboBox* cellCombo(int row, int col) const {
        return qobject_cast<QComboBox*>(m_eqTable->cellWidget(row, col));
    }
    QDoubleSpinBox* cellDouble(int row, int col) const {
        return qobject_cast<QDoubleSpinBox*>(m_eqTable->cellWidget(row, col));
    }

    void updateEnabledStates() {
        m_gain->setEnabled(m_setGain->isChecked());
        m_hpfOn->setEnabled(m_setHpf->isChecked());
        m_hpfFreq->setEnabled(m_setHpf->isChecked());
        const bool eq = m_setEq->isChecked();
        m_eqOn->setEnabled(eq);
        m_eqTable->setEnabled(eq);
        m_addBandBtn->setEnabled(eq);
        m_delBandBtn->setEnabled(eq);
        const bool dyn = m_setDyn->isChecked();
        m_dynOn->setEnabled(dyn);
        for (QDoubleSpinBox* s : {m_thr, m_ratio, m_att, m_rel, m_makeup})
            s->setEnabled(dyn);
    }

    bool m_updating = false;

    QCheckBox* m_setGain = nullptr;
    QDoubleSpinBox* m_gain = nullptr;
    QCheckBox* m_setHpf = nullptr;
    QCheckBox* m_hpfOn = nullptr;
    QDoubleSpinBox* m_hpfFreq = nullptr;
    QCheckBox* m_setEq = nullptr;
    QCheckBox* m_eqOn = nullptr;
    QTableWidget* m_eqTable = nullptr;
    QPushButton* m_addBandBtn = nullptr;
    QPushButton* m_delBandBtn = nullptr;
    QCheckBox* m_setDyn = nullptr;
    QCheckBox* m_dynOn = nullptr;
    QDoubleSpinBox* m_thr = nullptr;
    QDoubleSpinBox* m_ratio = nullptr;
    QDoubleSpinBox* m_att = nullptr;
    QDoubleSpinBox* m_rel = nullptr;
    QDoubleSpinBox* m_makeup = nullptr;
};

// ---------------------------------------------------------------------------
// ActorSetupPanel
// ---------------------------------------------------------------------------

ActorSetupPanel::ActorSetupPanel(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    if (m_app && m_app->show())
        m_library = m_app->show()->actorProfileLibrary();

    setupUi();
    loadDisplayOptions();

    if (m_library) {
        connect(m_library, &ActorProfileLibrary::changed, this,
                &ActorSetupPanel::onLibraryChanged);
    }

    refresh();
}

void ActorSetupPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // toolbar: copy/paste + import/export + groups
    auto* toolbar = new QHBoxLayout();
    m_copyBtn = new QPushButton(Icons::copy(), tr("Copy Values"), this);
    m_copyBtn->setToolTip(tr("Copy the selected actor's voice values"));
    m_pasteBtn = new QPushButton(Icons::paste(), tr("Paste Values"), this);
    m_pasteBtn->setToolTip(tr("Apply copied voice values to the selected actor"));
    m_importBtn = new QPushButton(Icons::upload(), tr("Import..."), this);
    m_importBtn->setToolTip(tr("Replace the cast with actor values from a file"));
    m_exportBtn = new QPushButton(Icons::download(), tr("Export..."), this);
    m_exportBtn->setToolTip(tr("Export all actor values to a file"));
    m_loadGroupBtn = new QPushButton(Icons::actor(), tr("Load Group..."), this);
    m_loadGroupBtn->setToolTip(tr("Append actors from an actor-group file"));
    m_saveGroupBtn = new QPushButton(Icons::actorSetup(), tr("Save Group..."), this);
    m_saveGroupBtn->setToolTip(tr("Save the selected actor (or whole cast) as an actor group"));
    toolbar->addWidget(m_copyBtn);
    toolbar->addWidget(m_pasteBtn);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_importBtn);
    toolbar->addWidget(m_exportBtn);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_loadGroupBtn);
    toolbar->addWidget(m_saveGroupBtn);
    toolbar->addStretch();
    mainLayout->addLayout(toolbar);

    connect(m_copyBtn, &QPushButton::clicked, this, &ActorSetupPanel::copyActorValues);
    connect(m_pasteBtn, &QPushButton::clicked, this, &ActorSetupPanel::pasteActorValues);
    connect(m_importBtn, &QPushButton::clicked, this, &ActorSetupPanel::importActorValues);
    connect(m_exportBtn, &QPushButton::clicked, this, &ActorSetupPanel::exportActorValues);
    connect(m_loadGroupBtn, &QPushButton::clicked, this, &ActorSetupPanel::loadActorGroup);
    connect(m_saveGroupBtn, &QPushButton::clicked, this, &ActorSetupPanel::saveActorGroup);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ---- left: actor tree + list controls + display options ----
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    m_actorTree = new QTreeWidget(left);
    m_actorTree->setColumnCount(4);
    m_actorTree->setHeaderLabels({tr("Actor"), tr("Role"), tr("Ch"), tr("Active")});
    m_actorTree->setRootIsDecorated(false);
    m_actorTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_actorTree->header()->setStretchLastSection(false);
    m_actorTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_actorTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_actorTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_actorTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connect(m_actorTree, &QTreeWidget::itemSelectionChanged, this,
            &ActorSetupPanel::onActorSelectionChanged);
    leftLayout->addWidget(m_actorTree, 1);

    auto* listButtons = new QHBoxLayout();
    m_addActorBtn = new QPushButton(Icons::listAdd(), tr("Add"), left);
    m_removeActorBtn = new QPushButton(Icons::listRemove(), tr("Remove"), left);
    m_moveUpBtn = new QPushButton(Icons::moveUp(), QString(), left);
    m_moveUpBtn->setToolTip(tr("Move actor up"));
    m_moveDownBtn = new QPushButton(Icons::moveDown(), QString(), left);
    m_moveDownBtn->setToolTip(tr("Move actor down"));
    listButtons->addWidget(m_addActorBtn);
    listButtons->addWidget(m_removeActorBtn);
    listButtons->addStretch();
    listButtons->addWidget(m_moveUpBtn);
    listButtons->addWidget(m_moveDownBtn);
    leftLayout->addLayout(listButtons);

    connect(m_addActorBtn, &QPushButton::clicked, this, &ActorSetupPanel::addActor);
    connect(m_removeActorBtn, &QPushButton::clicked, this, &ActorSetupPanel::removeActor);
    connect(m_moveUpBtn, &QPushButton::clicked, this, &ActorSetupPanel::moveActorUp);
    connect(m_moveDownBtn, &QPushButton::clicked, this, &ActorSetupPanel::moveActorDown);

    auto* optionsBox = new QGroupBox(tr("Check-Cue Display"), left);
    auto* optionsLayout = new QVBoxLayout(optionsBox);
    m_scribbleNamesCheck =
        new QCheckBox(tr("Show actor names on scribble strips in check cues"), optionsBox);
    m_cueZeroLabelsCheck = new QCheckBox(tr("Show actor labels on cue zero"), optionsBox);
    optionsLayout->addWidget(m_scribbleNamesCheck);
    optionsLayout->addWidget(m_cueZeroLabelsCheck);
    leftLayout->addWidget(optionsBox);

    auto saveOptions = [this]() {
        QSettings settings("OpenMix", "OpenMix");
        settings.beginGroup("ActorSetup");
        settings.setValue("scribbleNames", m_scribbleNamesCheck->isChecked());
        settings.setValue("cueZeroLabels", m_cueZeroLabelsCheck->isChecked());
        settings.endGroup();
    };
    connect(m_scribbleNamesCheck, &QCheckBox::toggled, this, saveOptions);
    connect(m_cueZeroLabelsCheck, &QCheckBox::toggled, this, saveOptions);

    splitter->addWidget(left);

    // ---- right: actor editor ----
    auto* scroll = new QScrollArea(splitter);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    m_editor = new QWidget();
    auto* editorLayout = new QVBoxLayout(m_editor);
    editorLayout->setContentsMargins(4, 4, 4, 4);

    auto* identityBox = new QGroupBox(tr("Actor"), m_editor);
    auto* identityForm = new QFormLayout(identityBox);
    m_nameEdit = new QLineEdit(identityBox);
    m_nameEdit->setPlaceholderText(tr("Actor name"));
    m_roleEdit = new QLineEdit(identityBox);
    m_roleEdit->setPlaceholderText(tr("Character / part (e.g. Cosette)"));
    m_roleEdit->setToolTip(
        tr("Typing this role into a cue's DCA slot assigns this actor's channel"));
    m_channelSpin = new QSpinBox(identityBox);
    m_channelSpin->setRange(1, 96);
    m_activeCheck = new QCheckBox(tr("Active"), identityBox);
    m_activeCheck->setToolTip(tr("Inactive actors yield their channel to the next understudy"));
    m_backupCheck = new QCheckBox(tr("Channel on backup / spare mic"), identityBox);
    m_backupCheck->setToolTip(tr("Resolve this channel to the backup voice instead of the main"));
    identityForm->addRow(tr("Name:"), m_nameEdit);
    identityForm->addRow(tr("Role:"), m_roleEdit);
    identityForm->addRow(tr("Channel:"), m_channelSpin);
    identityForm->addRow(QString(), m_activeCheck);
    identityForm->addRow(QString(), m_backupCheck);
    editorLayout->addWidget(identityBox);

    connect(m_nameEdit, &QLineEdit::textChanged, this, &ActorSetupPanel::onNameChanged);
    connect(m_roleEdit, &QLineEdit::textChanged, this, &ActorSetupPanel::onRoleChanged);
    connect(m_channelSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &ActorSetupPanel::onChannelChanged);
    connect(m_activeCheck, &QCheckBox::toggled, this, &ActorSetupPanel::onActiveToggled);
    connect(m_backupCheck, &QCheckBox::toggled, this, &ActorSetupPanel::onBackupToggled);

    auto* slotBox = new QGroupBox(tr("Voice Profile Slots (shared by all actors)"), m_editor);
    auto* slotBoxLayout = new QVBoxLayout(slotBox);
    auto* slotHint = new QLabel(
        tr("Slots are show-wide voice categories (e.g. \"Main\", \"Cold Day\") — not per-actor "
           "mic names. Adding a slot adds it for every actor; the EQ/dynamics below are this "
           "actor's own settings for the selected slot."),
        slotBox);
    slotHint->setWordWrap(true);
    slotHint->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
    slotBoxLayout->addWidget(slotHint);
    auto* slotLayout = new QHBoxLayout();
    m_slotCombo = new QComboBox(slotBox);
    m_addSlotBtn = new QPushButton(Icons::listAdd(), QString(), slotBox);
    m_addSlotBtn->setToolTip(tr("Add a voice profile slot (added for every actor)"));
    m_removeSlotBtn = new QPushButton(Icons::listRemove(), QString(), slotBox);
    m_removeSlotBtn->setToolTip(tr("Remove the current voice profile slot (removed for every actor)"));
    slotLayout->addWidget(m_slotCombo, 1);
    slotLayout->addWidget(m_addSlotBtn);
    slotLayout->addWidget(m_removeSlotBtn);
    slotBoxLayout->addLayout(slotLayout);
    editorLayout->addWidget(slotBox);

    connect(m_slotCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &ActorSetupPanel::onSlotChanged);
    connect(m_addSlotBtn, &QPushButton::clicked, this, &ActorSetupPanel::addSlot);
    connect(m_removeSlotBtn, &QPushButton::clicked, this, &ActorSetupPanel::removeSlot);

    m_voiceTabs = new QTabWidget(m_editor);
    m_mainVoice = new VoiceEditorWidget(m_voiceTabs);
    m_backupVoice = new VoiceEditorWidget(m_voiceTabs);
    m_mainVoice->onChanged = [this]() { commitVoiceEdits(); };
    m_backupVoice->onChanged = [this]() { commitVoiceEdits(); };
    m_voiceTabs->addTab(m_mainVoice, tr("Main Voice"));
    m_voiceTabs->addTab(m_backupVoice, tr("Backup Voice"));
    editorLayout->addWidget(m_voiceTabs, 1);

    scroll->setWidget(m_editor);
    splitter->addWidget(scroll);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 460});
    mainLayout->addWidget(splitter, 1);
}

void ActorSetupPanel::loadDisplayOptions() {
    QSettings settings("OpenMix", "OpenMix");
    settings.beginGroup("ActorSetup");
    QSignalBlocker b1(m_scribbleNamesCheck);
    QSignalBlocker b2(m_cueZeroLabelsCheck);
    m_scribbleNamesCheck->setChecked(settings.value("scribbleNames", true).toBool());
    m_cueZeroLabelsCheck->setChecked(settings.value("cueZeroLabels", false).toBool());
    settings.endGroup();
}

int ActorSetupPanel::channelCount() const {
    if (m_app && m_app->mixer() && m_app->mixer()->isConnected())
        return std::max(1, m_app->mixer()->capabilities().inputChannels);
    return 96;
}

QString ActorSetupPanel::selectedActorId() const {
    auto* item = m_actorTree->currentItem();
    return item ? item->data(0, Qt::UserRole).toString() : QString();
}

void ActorSetupPanel::refresh() {
    if (m_app && m_app->show())
        m_library = m_app->show()->actorProfileLibrary();
    m_channelSpin->setMaximum(channelCount());
    rebuildActorTree(selectedActorId());
}

void ActorSetupPanel::rebuildActorTree(const QString& selectId) {
    if (!m_library)
        return;

    m_updatingUi = true;
    m_actorTree->clear();

    // present actors ordered by their order field (lowest first = top of list)
    QList<Actor> actors = m_library->actors();
    std::stable_sort(actors.begin(), actors.end(),
                     [](const Actor& a, const Actor& b) { return a.order() < b.order(); });

    QTreeWidgetItem* toSelect = nullptr;
    for (const Actor& a : actors) {
        auto* item = new QTreeWidgetItem(m_actorTree);
        item->setText(0, a.name().isEmpty() ? tr("(unnamed)") : a.name());
        item->setText(1, a.role());
        item->setText(2, QString::number(a.channel()));
        item->setText(3, a.active() ? tr("Yes") : tr("No"));
        item->setData(0, Qt::UserRole, a.id());
        if (a.id() == selectId)
            toSelect = item;
    }

    if (!toSelect && m_actorTree->topLevelItemCount() > 0)
        toSelect = m_actorTree->topLevelItem(0);

    m_updatingUi = false;

    if (toSelect)
        m_actorTree->setCurrentItem(toSelect);
    else
        loadActorIntoEditor();

    updateButtonStates();
}

void ActorSetupPanel::rebuildSlotCombo() {
    if (!m_library)
        return;
    m_updatingUi = true;
    m_slotCombo->clear();
    const QStringList slotNames = m_library->profileSlots();
    m_slotCombo->addItems(slotNames);
    if (!slotNames.contains(m_currentSlot))
        m_currentSlot = slotNames.isEmpty() ? QString() : slotNames.first();
    m_slotCombo->setCurrentText(m_currentSlot);
    m_updatingUi = false;
}

void ActorSetupPanel::onActorSelectionChanged() {
    if (m_updatingUi)
        return;
    loadActorIntoEditor();
    updateButtonStates();
}

void ActorSetupPanel::loadActorIntoEditor() {
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;

    if (!a) {
        setEditorEnabled(false);
        m_updatingUi = true;
        m_nameEdit->clear();
        m_roleEdit->clear();
        m_channelSpin->setValue(1);
        m_activeCheck->setChecked(false);
        m_backupCheck->setChecked(false);
        m_mainVoice->setVoice(VoiceData());
        m_backupVoice->setVoice(VoiceData());
        m_updatingUi = false;
        return;
    }

    setEditorEnabled(true);
    rebuildSlotCombo();

    m_updatingUi = true;
    m_nameEdit->setText(a->name());
    m_roleEdit->setText(a->role());
    m_channelSpin->setValue(a->channel());
    m_activeCheck->setChecked(a->active());
    m_backupCheck->setChecked(m_library->isBackup(a->channel()));

    const ActorProfile profile = a->profile(m_currentSlot);
    m_mainVoice->setVoice(profile.main());
    m_backupVoice->setVoice(profile.backup());
    m_updatingUi = false;
}

void ActorSetupPanel::setEditorEnabled(bool on) {
    m_editor->setEnabled(on);
}

void ActorSetupPanel::updateButtonStates() {
    const bool hasSel = !selectedActorId().isEmpty();
    m_removeActorBtn->setEnabled(hasSel);
    m_copyBtn->setEnabled(hasSel);
    m_pasteBtn->setEnabled(hasSel && !m_copiedProfiles.isEmpty());
    m_saveGroupBtn->setEnabled(m_library && m_library->actorCount() > 0);

    auto* item = m_actorTree->currentItem();
    const int idx = item ? m_actorTree->indexOfTopLevelItem(item) : -1;
    m_moveUpBtn->setEnabled(idx > 0);
    m_moveDownBtn->setEnabled(idx >= 0 && idx < m_actorTree->topLevelItemCount() - 1);
}

void ActorSetupPanel::addActor() {
    if (!m_library)
        return;

    // choose the lowest free channel and a unique-ish order at the end
    QSet<int> used;
    int maxOrder = 0;
    for (const Actor& a : m_library->actors()) {
        used.insert(a.channel());
        maxOrder = std::max(maxOrder, a.order());
    }
    int channel = 1;
    while (used.contains(channel) && channel < channelCount())
        ++channel;

    Actor actor(tr("New Actor"), channel);
    actor.setOrder(maxOrder + 1);
    // seed an empty profile for the current slots so the slot combo is populated
    const QStringList slotNames = m_library->profileSlots();
    if (!slotNames.isEmpty())
        actor.setProfile(slotNames.first(), ActorProfile());

    const QString id = actor.id();
    m_library->addActor(actor);
    rebuildActorTree(id);
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void ActorSetupPanel::removeActor() {
    const QString id = selectedActorId();
    if (id.isEmpty() || !m_library)
        return;

    const Actor* a = m_library->actorById(id);
    const QString name = a ? a->name() : QString();
    if (QMessageBox::question(this, tr("Remove Actor"),
                              tr("Remove actor \"%1\"?").arg(name)) != QMessageBox::Yes)
        return;

    m_library->removeActor(id);
    rebuildActorTree();
}

void ActorSetupPanel::moveActorUp() {
    auto* item = m_actorTree->currentItem();
    if (!item || !m_library)
        return;
    const int idx = m_actorTree->indexOfTopLevelItem(item);
    if (idx <= 0)
        return;

    const QString idA = item->data(0, Qt::UserRole).toString();
    const QString idB = m_actorTree->topLevelItem(idx - 1)->data(0, Qt::UserRole).toString();
    const Actor* a = m_library->actorById(idA);
    const Actor* b = m_library->actorById(idB);
    if (!a || !b)
        return;

    Actor ca = *a, cb = *b;
    const int oa = ca.order();
    ca.setOrder(cb.order());
    cb.setOrder(oa);
    m_library->updateActor(idA, ca);
    m_library->updateActor(idB, cb);
    rebuildActorTree(idA);
}

void ActorSetupPanel::moveActorDown() {
    auto* item = m_actorTree->currentItem();
    if (!item || !m_library)
        return;
    const int idx = m_actorTree->indexOfTopLevelItem(item);
    if (idx < 0 || idx >= m_actorTree->topLevelItemCount() - 1)
        return;

    const QString idA = item->data(0, Qt::UserRole).toString();
    const QString idB = m_actorTree->topLevelItem(idx + 1)->data(0, Qt::UserRole).toString();
    const Actor* a = m_library->actorById(idA);
    const Actor* b = m_library->actorById(idB);
    if (!a || !b)
        return;

    Actor ca = *a, cb = *b;
    const int oa = ca.order();
    ca.setOrder(cb.order());
    cb.setOrder(oa);
    m_library->updateActor(idA, ca);
    m_library->updateActor(idB, cb);
    rebuildActorTree(idA);
}

void ActorSetupPanel::onNameChanged(const QString& text) {
    if (m_updatingUi)
        return;
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a)
        return;
    Actor copy = *a;
    copy.setName(text);
    m_updatingUi = true;
    m_library->updateActor(id, copy);
    m_updatingUi = false;
    if (auto* item = m_actorTree->currentItem())
        item->setText(0, text.isEmpty() ? tr("(unnamed)") : text);
}

void ActorSetupPanel::onRoleChanged(const QString& text) {
    if (m_updatingUi)
        return;
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a)
        return;
    Actor copy = *a;
    copy.setRole(text);
    m_updatingUi = true;
    m_library->updateActor(id, copy);
    m_updatingUi = false;
    if (auto* item = m_actorTree->currentItem())
        item->setText(1, text);
}

void ActorSetupPanel::onChannelChanged(int channel) {
    if (m_updatingUi)
        return;
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a)
        return;
    Actor copy = *a;
    copy.setChannel(channel);
    m_updatingUi = true;
    m_library->updateActor(id, copy);
    m_backupCheck->setChecked(m_library->isBackup(channel)); // backup is per-channel
    m_updatingUi = false;
    if (auto* item = m_actorTree->currentItem())
        item->setText(2, QString::number(channel));
}

void ActorSetupPanel::onActiveToggled(bool on) {
    if (m_updatingUi)
        return;
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a)
        return;
    Actor copy = *a;
    copy.setActive(on);
    m_updatingUi = true;
    m_library->updateActor(id, copy);
    m_updatingUi = false;
    if (auto* item = m_actorTree->currentItem())
        item->setText(3, on ? tr("Yes") : tr("No"));
}

void ActorSetupPanel::onBackupToggled(bool on) {
    if (m_updatingUi)
        return;
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a)
        return;
    m_updatingUi = true;
    m_library->setBackup(a->channel(), on);
    m_updatingUi = false;
}

void ActorSetupPanel::onSlotChanged(int) {
    if (m_updatingUi)
        return;
    m_currentSlot = m_slotCombo->currentText();
    // reload the selected actor's voices for the newly chosen slot
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a)
        return;
    const ActorProfile profile = a->profile(m_currentSlot);
    m_updatingUi = true;
    m_mainVoice->setVoice(profile.main());
    m_backupVoice->setVoice(profile.backup());
    m_updatingUi = false;
}

void ActorSetupPanel::addSlot() {
    if (!m_library)
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Add Voice Profile Slot"),
                                               tr("Slot name (added for every actor):"),
                                               QLineEdit::Normal, QString(), &ok)
                             .trimmed();
    if (!ok || name.isEmpty())
        return;
    if (m_library->profileSlots().contains(name)) {
        QMessageBox::information(this, tr("Add Voice Profile Slot"),
                                 tr("A slot named \"%1\" already exists.").arg(name));
        return;
    }
    m_library->addSlot(name);
    m_currentSlot = name;
    rebuildSlotCombo();
    loadActorIntoEditor();
}

void ActorSetupPanel::removeSlot() {
    if (!m_library || m_currentSlot.isEmpty())
        return;
    if (m_library->profileSlots().size() <= 1) {
        QMessageBox::information(this, tr("Remove Profile Slot"),
                                 tr("At least one profile slot is required."));
        return;
    }
    if (QMessageBox::question(this, tr("Remove Profile Slot"),
                              tr("Remove profile slot \"%1\"? Stored voices for this slot are "
                                 "kept but no longer applied.")
                                  .arg(m_currentSlot)) != QMessageBox::Yes)
        return;
    m_library->removeSlot(m_currentSlot);
    m_currentSlot.clear();
    rebuildSlotCombo();
    loadActorIntoEditor();
}

void ActorSetupPanel::commitVoiceEdits() {
    if (m_updatingUi)
        return;
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a || m_currentSlot.isEmpty())
        return;
    Actor copy = *a;
    ActorProfile profile = copy.profile(m_currentSlot);
    profile.setMain(m_mainVoice->voice());
    profile.setBackup(m_backupVoice->voice());
    copy.setProfile(m_currentSlot, profile);
    m_updatingUi = true;
    m_library->updateActor(id, copy);
    m_updatingUi = false;
}

void ActorSetupPanel::copyActorValues() {
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a)
        return;
    // capture just the per-slot profiles (the "voice values"), not identity
    m_copiedProfiles = a->toJson().value("profiles").toObject();
    updateButtonStates();
}

void ActorSetupPanel::pasteActorValues() {
    const QString id = selectedActorId();
    const Actor* a = m_library ? m_library->actorById(id) : nullptr;
    if (!a || m_copiedProfiles.isEmpty())
        return;

    Actor copy = *a;
    for (auto it = m_copiedProfiles.constBegin(); it != m_copiedProfiles.constEnd(); ++it) {
        copy.setProfile(it.key(), ActorProfile::fromJson(it.value().toObject()));
        if (!m_library->profileSlots().contains(it.key()))
            m_library->addSlot(it.key());
    }
    m_library->updateActor(id, copy);
    loadActorIntoEditor();
}

void ActorSetupPanel::exportActorValues() {
    if (!m_library)
        return;
    QString path = QFileDialog::getSaveFileName(this, tr("Export Actor Values"), QString(),
                                                tr("OpenMix Actors (*.omactors)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".omactors"))
        path += ".omactors";

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Actor Values"),
                             tr("Failed to write file: %1").arg(file.errorString()));
        return;
    }
    file.write(QJsonDocument(m_library->toJson()).toJson(QJsonDocument::Indented));
}

void ActorSetupPanel::importActorValues() {
    if (!m_library)
        return;
    QString path = QFileDialog::getOpenFileName(this, tr("Import Actor Values"), QString(),
                                                tr("OpenMix Actors (*.omactors);;All Files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Import Actor Values"),
                             tr("Failed to read file: %1").arg(file.errorString()));
        return;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("Import Actor Values"),
                             tr("Invalid actor file: %1").arg(err.errorString()));
        return;
    }
    if (m_library->actorCount() > 0 &&
        QMessageBox::question(this, tr("Import Actor Values"),
                              tr("Replace the current cast with the imported actors?")) !=
            QMessageBox::Yes)
        return;

    m_library->loadFromJson(doc.object());
    rebuildActorTree();
}

void ActorSetupPanel::saveActorGroup() {
    if (!m_library || m_library->actorCount() == 0)
        return;

    // save the selected actor if one is chosen, otherwise the whole cast
    QList<Actor> toSave;
    const QString id = selectedActorId();
    if (const Actor* sel = m_library->actorById(id))
        toSave.append(*sel);
    else
        toSave = m_library->actors();

    QString path = QFileDialog::getSaveFileName(this, tr("Save Actor Group"), QString(),
                                                tr("OpenMix Actor Group (*.omgroup)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".omgroup"))
        path += ".omgroup";

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save Actor Group"),
                             tr("Failed to write file: %1").arg(file.errorString()));
        return;
    }
    const QJsonObject doc = ActorGroupIo::toJson(toSave, m_library->profileSlots());
    file.write(QJsonDocument(doc).toJson(QJsonDocument::Indented));
}

void ActorSetupPanel::loadActorGroup() {
    if (!m_library)
        return;
    QString path =
        QFileDialog::getOpenFileName(this, tr("Load Actor Group"), QString(),
                                     tr("OpenMix Actor Group (*.omgroup);;All Files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Load Actor Group"),
                             tr("Failed to read file: %1").arg(file.errorString()));
        return;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject() ||
        !ActorGroupIo::isActorGroup(doc.object())) {
        QMessageBox::warning(this, tr("Load Actor Group"), tr("Not a valid actor-group file."));
        return;
    }

    // merge the group's slots, then append its actors (fresh ids avoid collisions)
    for (const QString& slot : ActorGroupIo::slotsFromJson(doc.object()))
        m_library->addSlot(slot);

    const QList<Actor> actors = ActorGroupIo::actorsFromJson(doc.object(), /*regenerateIds=*/true);
    int order = 0;
    for (const Actor& a : m_library->actors())
        order = std::max(order, a.order());

    QString firstId;
    for (Actor a : actors) {
        a.setOrder(++order);
        if (firstId.isEmpty())
            firstId = a.id();
        m_library->addActor(a);
    }
    rebuildActorTree(firstId);
}

void ActorSetupPanel::onLibraryChanged() {
    // external change (e.g. a project was loaded); ignore our own edits which are
    // guarded by m_updatingUi to avoid clobbering in-progress editing.
    if (m_updatingUi)
        return;
    refresh();
}

} // namespace OpenMix
