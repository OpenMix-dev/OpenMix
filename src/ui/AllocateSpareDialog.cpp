#include "AllocateSpareDialog.h"
#include "app/Application.h"
#include "core/Show.h"
#include "core/SpareBackup.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace OpenMix {

namespace {
// spinbox value 0 means "none"; the model uses -1
int toChannel(int spinValue) {
    return spinValue <= 0 ? -1 : spinValue;
}
} // namespace

AllocateSpareDialog::AllocateSpareDialog(Application* app, QWidget* parent)
    : QDialog(parent), m_app(app), m_spare(app && app->show() ? app->show()->spareBackup()
                                                              : nullptr) {
    setWindowTitle(tr("Allocate Spare Backup"));
    setModal(true);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QFormLayout* form = new QFormLayout();

    QHBoxLayout* spareRow = new QHBoxLayout();
    m_spareChannelSpin = new QSpinBox(this);
    m_spareChannelSpin->setRange(0, 128);
    m_spareChannelSpin->setSpecialValueText(tr("None"));
    m_spareChannelSpin->setToolTip(tr("Console channel reserved as the spare mic"));
    spareRow->addWidget(m_spareChannelSpin);
    QPushButton* setSpareButton = new QPushButton(tr("Set"), this);
    connect(setSpareButton, &QPushButton::clicked, this, &AllocateSpareDialog::setSpareChannel);
    spareRow->addWidget(setSpareButton);
    form->addRow(tr("Spare channel:"), spareRow);

    QHBoxLayout* allocRow = new QHBoxLayout();
    m_allocateSpin = new QSpinBox(this);
    m_allocateSpin->setRange(0, 128);
    m_allocateSpin->setSpecialValueText(tr("None"));
    m_allocateSpin->setToolTip(tr("Channel the spare should cover"));
    allocRow->addWidget(m_allocateSpin);
    m_allocateButton = new QPushButton(tr("Allocate"), this);
    connect(m_allocateButton, &QPushButton::clicked, this, &AllocateSpareDialog::allocate);
    allocRow->addWidget(m_allocateButton);
    form->addRow(tr("Cover channel:"), allocRow);

    mainLayout->addLayout(form);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout* switchRow = new QHBoxLayout();
    m_switchNowButton = new QPushButton(tr("Switch &Now"), this);
    connect(m_switchNowButton, &QPushButton::clicked, this, &AllocateSpareDialog::switchNow);
    switchRow->addWidget(m_switchNowButton);
    m_switchLaterButton = new QPushButton(tr("Switch &Later"), this);
    connect(m_switchLaterButton, &QPushButton::clicked, this, &AllocateSpareDialog::switchLater);
    switchRow->addWidget(m_switchLaterButton);
    m_removeButton = new QPushButton(tr("&Remove Allocation"), this);
    connect(m_removeButton, &QPushButton::clicked, this, &AllocateSpareDialog::removeAllocation);
    switchRow->addWidget(m_removeButton);
    mainLayout->addLayout(switchRow);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    if (m_spare) {
        m_spareChannelSpin->setValue(m_spare->spareChannel() < 0 ? 0 : m_spare->spareChannel());
        if (m_spare->isAllocated())
            m_allocateSpin->setValue(m_spare->allocatedChannel());
    }
    updateStatus();
}

void AllocateSpareDialog::setSpareChannel() {
    if (!m_spare)
        return;
    m_spare->setSpareChannel(toChannel(m_spareChannelSpin->value()));
    updateStatus();
}

void AllocateSpareDialog::allocate() {
    if (!m_spare)
        return;
    m_spare->allocateTo(toChannel(m_allocateSpin->value()));
    updateStatus();
}

void AllocateSpareDialog::switchNow() {
    if (m_spare)
        m_spare->switchNow();
    updateStatus();
}

void AllocateSpareDialog::switchLater() {
    if (m_spare)
        m_spare->switchLater();
    updateStatus();
}

void AllocateSpareDialog::removeAllocation() {
    if (m_spare)
        m_spare->removeAllocation();
    updateStatus();
}

void AllocateSpareDialog::updateStatus() {
    if (!m_spare) {
        m_statusLabel->setText(tr("No show loaded."));
        return;
    }

    const bool hasSpare = m_spare->spareChannel() >= 0;
    const bool allocated = m_spare->isAllocated();
    const bool active = m_spare->isActive();

    QString text;
    if (!hasSpare) {
        text = tr("No spare channel configured.");
    } else if (!allocated) {
        text = tr("Spare channel %1 is idle.").arg(m_spare->spareChannel());
    } else {
        QString stateText;
        switch (m_spare->state()) {
        case SpareBackup::State::Inactive:
            stateText = tr("allocated (primary live)");
            break;
        case SpareBackup::State::Armed:
            stateText = tr("switch armed for next cue");
            break;
        case SpareBackup::State::Active:
            stateText = tr("backup live");
            break;
        }
        text = tr("Spare channel %1 covers channel %2 (%3).")
                   .arg(m_spare->spareChannel())
                   .arg(m_spare->allocatedChannel())
                   .arg(stateText);
    }
    m_statusLabel->setText(text);

    m_allocateButton->setEnabled(hasSpare && !active);
    m_switchNowButton->setEnabled(allocated && !active);
    m_switchLaterButton->setEnabled(allocated && !active);
    m_removeButton->setEnabled(allocated);
}

} // namespace OpenMix
