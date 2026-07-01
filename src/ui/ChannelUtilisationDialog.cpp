#include "ChannelUtilisationDialog.h"
#include "app/Application.h"
#include "core/Actor.h"
#include "core/ActorProfileLibrary.h"
#include "core/ChannelUtilisation.h"
#include "core/Show.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

namespace OpenMix {

ChannelUtilisationDialog::ChannelUtilisationDialog(Application* app, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Channel Utilisation"));
    resize(520, 520);

    QVBoxLayout* layout = new QVBoxLayout(this);

    QTableWidget* table = new QTableWidget(0, 4, this);
    table->setHorizontalHeaderLabels({tr("Ch"), tr("Actor"), tr("Cues"), tr("Cue Numbers")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table);

    const ActorProfileLibrary* actors = app && app->show() ? app->show()->actorProfileLibrary()
                                                           : nullptr;
    const QList<ChannelUsage> usage = computeChannelUtilisation(app ? app->show() : nullptr);
    table->setRowCount(usage.size());
    for (int row = 0; row < usage.size(); ++row) {
        const ChannelUsage& u = usage.at(row);

        table->setItem(row, 0, new QTableWidgetItem(QString::number(u.channel)));

        QString actorName;
        if (actors) {
            if (const Actor* a = actors->actorForChannel(u.channel))
                actorName = a->name();
        }
        table->setItem(row, 1, new QTableWidgetItem(actorName));

        table->setItem(row, 2, new QTableWidgetItem(QString::number(u.cueNumbers.size())));

        QStringList numbers;
        for (double n : u.cueNumbers)
            numbers.append(QString::number(n, 'g', 4));
        table->setItem(row, 3, new QTableWidgetItem(numbers.join(", ")));
    }

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

} // namespace OpenMix
