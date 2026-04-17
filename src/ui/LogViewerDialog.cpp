#include "LogViewerDialog.h"
#include "LogItemDelegate.h"
#include "LogListModel.h"
#include "core/AppLogger.h"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QScrollBar>
#include <QVBoxLayout>

namespace OpenMix {

LogViewerDialog::LogViewerDialog(AppLogger* logger, QWidget* parent)
    : QDialog(parent), m_logger(logger) {
    setWindowTitle(tr("Application Log"));
    setMinimumSize(800, 500);
    resize(1000, 600);

    m_model = new LogListModel(logger, this);
    m_delegate = new LogItemDelegate(this);

    setupUi();

    connect(m_logger, &AppLogger::entryAdded, this, &LogViewerDialog::onEntryAdded);
}

LogViewerDialog::~LogViewerDialog() = default;

void LogViewerDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // filter bar
    QHBoxLayout* filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(12);

    // level filter
    QLabel* levelLabel = new QLabel(tr("Level:"), this);
    m_levelCombo = new QComboBox(this);
    m_levelCombo->addItem(tr("Debug"), static_cast<int>(LogLevel::Debug));
    m_levelCombo->addItem(tr("Info"), static_cast<int>(LogLevel::Info));
    m_levelCombo->addItem(tr("Warning"), static_cast<int>(LogLevel::Warning));
    m_levelCombo->addItem(tr("Error"), static_cast<int>(LogLevel::Error));
    m_levelCombo->addItem(tr("Critical"), static_cast<int>(LogLevel::Critical));
    m_levelCombo->setCurrentIndex(0);
    m_levelCombo->setMinimumWidth(100);
    connect(m_levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &LogViewerDialog::onLevelFilterChanged);

    // source filter
    QLabel* sourceLabel = new QLabel(tr("Source:"), this);
    m_sourceCombo = new QComboBox(this);
    m_sourceCombo->addItem(tr("All Sources"), -1);
    m_sourceCombo->addItem(tr("Connection"), static_cast<int>(LogSource::Connection));
    m_sourceCombo->addItem(tr("Protocol"), static_cast<int>(LogSource::Protocol));
    m_sourceCombo->addItem(tr("Playback"), static_cast<int>(LogSource::Playback));
    m_sourceCombo->addItem(tr("UI"), static_cast<int>(LogSource::UI));
    m_sourceCombo->addItem(tr("System"), static_cast<int>(LogSource::System));
    m_sourceCombo->addItem(tr("MIDI"), static_cast<int>(LogSource::MIDI));
    m_sourceCombo->addItem(tr("Discovery"), static_cast<int>(LogSource::Discovery));
    m_sourceCombo->setCurrentIndex(0);
    m_sourceCombo->setMinimumWidth(120);
    connect(m_sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &LogViewerDialog::onSourceFilterChanged);

    // search
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(200);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &LogViewerDialog::onSearchTextChanged);

    // auto scroll
    m_autoScrollCheck = new QCheckBox(tr("Auto-scroll"), this);
    m_autoScrollCheck->setChecked(true);
    connect(m_autoScrollCheck, &QCheckBox::stateChanged, this,
            &LogViewerDialog::onAutoScrollChanged);

    filterLayout->addWidget(levelLabel);
    filterLayout->addWidget(m_levelCombo);
    filterLayout->addWidget(sourceLabel);
    filterLayout->addWidget(m_sourceCombo);
    filterLayout->addWidget(m_searchEdit, 1);
    filterLayout->addWidget(m_autoScrollCheck);

    mainLayout->addLayout(filterLayout);

    // log view
    m_logView = new QListView(this);
    m_logView->setModel(m_model);
    m_logView->setItemDelegate(m_delegate);
    m_logView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_logView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_logView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_logView->setUniformItemSizes(true);
    m_logView->setObjectName("LogListView");

    mainLayout->addWidget(m_logView, 1);

    // button bar
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setProperty("role", "secondary");
    connect(m_clearButton, &QPushButton::clicked, this, &LogViewerDialog::onClearClicked);

    m_exportButton = new QPushButton(tr("Export..."), this);
    m_exportButton->setProperty("role", "secondary");
    connect(m_exportButton, &QPushButton::clicked, this, &LogViewerDialog::onExportClicked);

    m_closeButton = new QPushButton(tr("Close"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    buttonLayout->addWidget(m_clearButton);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    // scroll to bottom on initial load
    scrollToBottom();
}

void LogViewerDialog::onLevelFilterChanged(int index) {
    int level = m_levelCombo->itemData(index).toInt();
    m_model->setLevelFilter(static_cast<LogLevel>(level));
}

void LogViewerDialog::onSourceFilterChanged(int index) {
    int source = m_sourceCombo->itemData(index).toInt();
    if (source < 0) {
        m_model->setSourceFilter(LogSource::System, false);
    } else {
        m_model->setSourceFilter(static_cast<LogSource>(source), true);
    }
}

void LogViewerDialog::onSearchTextChanged(const QString& text) { m_model->setSearchText(text); }

void LogViewerDialog::onAutoScrollChanged([[maybe_unused]] int state) {}

void LogViewerDialog::onClearClicked() {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Clear Log"), tr("Are you sure you want to clear all log entries?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_logger->clear();
    }
}

void LogViewerDialog::onExportClicked() {
    QString filter = tr("JSON Files (*.json);;CSV Files (*.csv);;All Files (*)");
    QString filePath = QFileDialog::getSaveFileName(this, tr("Export Log"), QString(), filter);

    if (filePath.isEmpty()) {
        return;
    }

    bool success = false;
    if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
        success = m_logger->exportToCSV(filePath);
    } else {
        if (!filePath.endsWith(".json", Qt::CaseInsensitive)) {
            filePath += ".json";
        }
        success = m_logger->exportToFile(filePath);
    }

    if (success) {
        QMessageBox::information(this, tr("Export Complete"),
                                 tr("Log exported successfully to:\n%1").arg(filePath));
    } else {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Failed to export log to:\n%1").arg(filePath));
    }
}

void LogViewerDialog::onEntryAdded() {
    if (m_autoScrollCheck->isChecked()) {
        scrollToBottom();
    }
}

void LogViewerDialog::scrollToBottom() {
    QScrollBar* vbar = m_logView->verticalScrollBar();
    if (vbar) {
        vbar->setValue(vbar->maximum());
    }
}

} // namespace OpenMix
