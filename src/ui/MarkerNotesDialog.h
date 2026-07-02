#pragma once

#include <QDialog>

class QTableWidget;

namespace OpenMix {

class Application;

// Review and annotate the REAPER virtual-sound-check markers dropped this
// session, and export them (with notes) to an HTML file.
class MarkerNotesDialog : public QDialog {
    Q_OBJECT

  public:
    explicit MarkerNotesDialog(Application* app, QWidget* parent = nullptr);

  private slots:
    void reload();
    void onCellChanged(int row, int column);
    void exportHtml();
    void clearSession();

  private:
    void setupUi();

    Application* m_app;
    QTableWidget* m_table;
    bool m_updating = false;
};

} // namespace OpenMix
