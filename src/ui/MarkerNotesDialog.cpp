#include "MarkerNotesDialog.h"
#include "WindowSizing.h"
#include "app/Application.h"
#include "app/ReaperClient.h"
#include "core/Show.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace OpenMix {

MarkerNotesDialog::MarkerNotesDialog(Application* app, QWidget* parent)
    : QDialog(parent), m_app(app) {
    setWindowTitle(tr("Marker Notes"));
    resize(560, 480);
    WindowSizing::widenOnShow(this);
    setupUi();
    reload();

    if (m_app && m_app->reaperClient())
        connect(m_app->reaperClient(), &ReaperClient::markersChanged, this,
                &MarkerNotesDialog::reload);
}

void MarkerNotesDialog::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* intro = new QLabel(
        tr("Markers dropped during REAPER sound check. Add notes, then export for paperwork."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({tr("Cue"), tr("Name"), tr("Note")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->verticalHeader()->setVisible(false);
    connect(m_table, &QTableWidget::cellChanged, this, &MarkerNotesDialog::onCellChanged);
    layout->addWidget(m_table);

    QHBoxLayout* buttonRow = new QHBoxLayout();
    QPushButton* exportButton = new QPushButton(tr("Export to HTML..."), this);
    connect(exportButton, &QPushButton::clicked, this, &MarkerNotesDialog::exportHtml);
    QPushButton* clearButton = new QPushButton(tr("Clear Session"), this);
    connect(clearButton, &QPushButton::clicked, this, &MarkerNotesDialog::clearSession);
    buttonRow->addWidget(exportButton);
    buttonRow->addWidget(clearButton);
    buttonRow->addStretch();
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttonRow->addWidget(buttons);
    layout->addLayout(buttonRow);
}

void MarkerNotesDialog::reload() {
    if (!m_app || !m_app->reaperClient())
        return;
    const QList<ReaperClient::MarkerEntry> markers = m_app->reaperClient()->markers();

    m_updating = true;
    m_table->setRowCount(markers.size());
    for (int row = 0; row < markers.size(); ++row) {
        const ReaperClient::MarkerEntry& marker = markers.at(row);
        auto* cueItem = new QTableWidgetItem(QString::number(marker.cueNumber, 'f', 2));
        cueItem->setFlags(cueItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, cueItem);
        auto* nameItem = new QTableWidgetItem(marker.cueName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 1, nameItem);
        m_table->setItem(row, 2, new QTableWidgetItem(marker.note));
    }
    m_updating = false;
}

void MarkerNotesDialog::onCellChanged(int row, int column) {
    if (m_updating || column != 2 || !m_app || !m_app->reaperClient())
        return;
    QTableWidgetItem* item = m_table->item(row, column);
    m_app->reaperClient()->setMarkerNoteAt(row, item ? item->text() : QString());
}

void MarkerNotesDialog::exportHtml() {
    if (!m_app || !m_app->reaperClient())
        return;
    const QList<ReaperClient::MarkerEntry> markers = m_app->reaperClient()->markers();
    if (markers.isEmpty()) {
        QMessageBox::information(this, tr("Export Marker Notes"),
                                 tr("There are no markers to export."));
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, tr("Export Marker Notes"), QString(),
                                                    tr("HTML Files (*.html)"));
    if (filePath.isEmpty())
        return;
    if (!filePath.endsWith(".html", Qt::CaseInsensitive))
        filePath += ".html";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not write %1").arg(filePath));
        return;
    }

    const QString showName = m_app->show() ? m_app->show()->name() : tr("Show");
    QTextStream out(&file);
    out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n"
        << "<style>body{font-family:sans-serif;margin:2em}"
        << "table{border-collapse:collapse;width:100%}"
        << "th,td{border:1px solid #999;padding:6px 10px;text-align:left;vertical-align:top}"
        << "th{background:#eee}</style></head><body>\n";
    out << "<h2>" << showName.toHtmlEscaped() << " &mdash; Marker Notes</h2>\n";
    out << "<table><tr><th>Cue</th><th>Name</th><th>Note</th></tr>\n";
    for (const ReaperClient::MarkerEntry& marker : markers) {
        out << "<tr><td>" << QString::number(marker.cueNumber, 'f', 2) << "</td><td>"
            << marker.cueName.toHtmlEscaped() << "</td><td>" << marker.note.toHtmlEscaped()
            << "</td></tr>\n";
    }
    out << "</table>\n</body></html>\n";
    file.close();

    QMessageBox::information(this, tr("Export Marker Notes"),
                             tr("Marker notes exported to %1").arg(filePath));
}

void MarkerNotesDialog::clearSession() {
    if (!m_app || !m_app->reaperClient())
        return;
    if (QMessageBox::question(this, tr("Clear Session"),
                              tr("Discard all markers and notes from this session?")) !=
        QMessageBox::Yes)
        return;
    m_app->reaperClient()->resetMarkers();
}

} // namespace OpenMix
