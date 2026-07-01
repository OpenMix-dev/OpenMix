#include "TimecodePanel.h"
#include "app/Application.h"
#include "core/TimecodeTrigger.h"
#include "midi/MidiInputManager.h"
#include "theme/Icons.h"

#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace OpenMix {

namespace {
constexpr int TriggerIdRole = Qt::UserRole + 1;

QString formatTimecode(int h, int m, int s, int f) {
    return QString("%1:%2:%3:%4")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'))
        .arg(f, 2, 10, QChar('0'));
}

QString formatCue(double number) {
    QString text = QString::number(number, 'f', 2);
    while (text.endsWith('0'))
        text.chop(1);
    if (text.endsWith('.'))
        text.chop(1);
    return text;
}
} // namespace

TimecodePanel::TimecodePanel(Application* app, QWidget* parent) : QWidget(parent), m_app(app) {
    if (m_app)
        m_triggers = m_app->timecodeTriggers();

    setupUi();

    if (m_triggers)
        connect(m_triggers, &TimecodeTriggerList::triggerFired, this,
                &TimecodePanel::onTriggerFired);
    if (m_app && m_app->midiInputManager())
        connect(m_app->midiInputManager(), &MidiInputManager::timecodeChanged, this,
                &TimecodePanel::onIncomingTimecode);

    refresh();
}

void TimecodePanel::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // live incoming timecode readout
    m_liveLabel = new QLabel(tr("Incoming: --:--:--:--"), this);
    m_liveLabel->setToolTip(tr("Last timecode received from the MIDI input"));
    QFont mono = m_liveLabel->font();
    mono.setStyleHint(QFont::Monospace);
    mono.setBold(true);
    m_liveLabel->setFont(mono);
    mainLayout->addWidget(m_liveLabel);

    // trigger table
    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({tr("Timecode"), tr("Cue"), tr("Enabled")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_table, &QTableWidget::cellChanged, this, &TimecodePanel::onCellChanged);
    mainLayout->addWidget(m_table, 1);

    // add-trigger controls
    QGroupBox* addGroup = new QGroupBox(tr("Add Trigger"), this);
    QHBoxLayout* addLayout = new QHBoxLayout(addGroup);

    auto makeSpin = [this](int max, const QString& suffix) {
        QSpinBox* spin = new QSpinBox(this);
        spin->setRange(0, max);
        spin->setSuffix(suffix);
        spin->setAlignment(Qt::AlignRight);
        return spin;
    };
    m_hoursSpin = makeSpin(23, tr("h"));
    m_minutesSpin = makeSpin(59, tr("m"));
    m_secondsSpin = makeSpin(59, tr("s"));
    m_framesSpin = makeSpin(29, tr("f"));
    addLayout->addWidget(m_hoursSpin);
    addLayout->addWidget(m_minutesSpin);
    addLayout->addWidget(m_secondsSpin);
    addLayout->addWidget(m_framesSpin);

    addLayout->addWidget(new QLabel(tr("Cue:"), this));
    m_cueSpin = new QDoubleSpinBox(this);
    m_cueSpin->setRange(0.0, 99999.0);
    m_cueSpin->setDecimals(2);
    m_cueSpin->setSingleStep(1.0);
    addLayout->addWidget(m_cueSpin);

    m_addButton = new QPushButton(Icons::listAdd(), tr("Add"), this);
    connect(m_addButton, &QPushButton::clicked, this, &TimecodePanel::addTrigger);
    addLayout->addWidget(m_addButton);

    mainLayout->addWidget(addGroup);

    // remove
    QHBoxLayout* removeLayout = new QHBoxLayout();
    removeLayout->addStretch();
    m_removeButton = new QPushButton(Icons::listRemove(), tr("Remove Selected"), this);
    connect(m_removeButton, &QPushButton::clicked, this, &TimecodePanel::removeTrigger);
    removeLayout->addWidget(m_removeButton);
    mainLayout->addLayout(removeLayout);
}

void TimecodePanel::refresh() {
    if (m_app)
        m_triggers = m_app->timecodeTriggers();
    populateTable();
}

void TimecodePanel::populateTable() {
    m_updatingUi = true;
    m_table->setRowCount(0);
    if (m_triggers) {
        const QList<TimecodeTrigger> list = m_triggers->triggers();
        m_table->setRowCount(list.size());
        for (int row = 0; row < list.size(); ++row) {
            const TimecodeTrigger& t = list.at(row);

            QTableWidgetItem* tcItem = new QTableWidgetItem(
                formatTimecode(t.hours, t.minutes, t.seconds, t.frames));
            tcItem->setData(TriggerIdRole, t.id);
            tcItem->setFlags(tcItem->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(row, 0, tcItem);

            QTableWidgetItem* cueItem = new QTableWidgetItem(formatCue(t.cueNumber));
            cueItem->setFlags(cueItem->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(row, 1, cueItem);

            QTableWidgetItem* enItem = new QTableWidgetItem();
            enItem->setFlags((enItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
            enItem->setCheckState(t.enabled ? Qt::Checked : Qt::Unchecked);
            m_table->setItem(row, 2, enItem);
        }
    }
    m_updatingUi = false;
}

QString TimecodePanel::rowTriggerId(int row) const {
    QTableWidgetItem* item = m_table->item(row, 0);
    return item ? item->data(TriggerIdRole).toString() : QString();
}

void TimecodePanel::addTrigger() {
    if (!m_triggers)
        return;
    m_triggers->addTrigger(m_hoursSpin->value(), m_minutesSpin->value(), m_secondsSpin->value(),
                           m_framesSpin->value(), m_cueSpin->value());
    populateTable();
}

void TimecodePanel::removeTrigger() {
    if (!m_triggers)
        return;
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const QString id = rowTriggerId(row);
    if (id.isEmpty())
        return;
    m_triggers->removeTrigger(id);
    populateTable();
}

void TimecodePanel::onCellChanged(int row, int column) {
    if (m_updatingUi || !m_triggers || column != 2)
        return;
    QTableWidgetItem* item = m_table->item(row, column);
    if (!item)
        return;
    m_triggers->setTriggerEnabled(rowTriggerId(row), item->checkState() == Qt::Checked);
}

void TimecodePanel::onIncomingTimecode(int hours, int minutes, int seconds, int frames) {
    m_liveLabel->setText(tr("Incoming: %1").arg(formatTimecode(hours, minutes, seconds, frames)));
}

void TimecodePanel::onTriggerFired(double cueNumber, const QString& triggerId) {
    Q_UNUSED(cueNumber);
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (rowTriggerId(row) == triggerId) {
            m_table->selectRow(row);
            break;
        }
    }
}

} // namespace OpenMix
