#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>

namespace OpenMix {

class AppLogger;
class LogListModel;
class LogItemDelegate;

class LogViewerDialog : public QDialog {
    Q_OBJECT

  public:
    explicit LogViewerDialog(AppLogger* logger, QWidget* parent = nullptr);
    ~LogViewerDialog() override;

  private slots:
    void onLevelFilterChanged(int index);
    void onSourceFilterChanged(int index);
    void onSearchTextChanged(const QString& text);
    void onAutoScrollChanged(Qt::CheckState state);
    void onClearClicked();
    void onExportClicked();
    void onEntryAdded();

  private:
    void setupUi();
    void scrollToBottom();

    AppLogger* m_logger;
    LogListModel* m_model;
    LogItemDelegate* m_delegate;

    // filter controls
    QComboBox* m_levelCombo;
    QComboBox* m_sourceCombo;
    QLineEdit* m_searchEdit;
    QCheckBox* m_autoScrollCheck;

    // log view
    QListView* m_logView;

    // buttons
    QPushButton* m_clearButton;
    QPushButton* m_exportButton;
    QPushButton* m_closeButton;
};

} // namespace OpenMix
