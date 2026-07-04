#pragma once

#include <QDialog>

class QListWidget;

namespace OpenMix {

// Startup screen: create a new show, open one, or pick a recent show. The choice
// is read back by the caller after exec(). A "show at startup" preference is
// persisted so the dialog can be skipped on later launches.
class WelcomeDialog : public QDialog {
    Q_OBJECT

  public:
    enum class Choice { None, NewShow, OpenShow, OpenRecent, Import };

    explicit WelcomeDialog(QWidget* parent = nullptr);

    [[nodiscard]] Choice choice() const { return m_choice; }
    [[nodiscard]] QString recentPath() const { return m_recentPath; }

    // startup preference (default true)
    [[nodiscard]] static bool showAtStartup();

  private slots:
    void chooseNew();
    void chooseOpen();
    void chooseImport();
    void chooseRecent();

  private:
    void setupUi();

    Choice m_choice = Choice::None;
    QString m_recentPath;
    QListWidget* m_recentList;
};

} // namespace OpenMix
