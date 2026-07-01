#pragma once

#include <QWidget>

class QLabel;
class QTextEdit;

namespace OpenMix {

class Application;

// Live read-only summary of the current (last-fired) cue: number, name, type,
// fade, and a breakdown of what it changes. Updates as playback advances.
class ActiveCueInfoPanel : public QWidget {
    Q_OBJECT

  public:
    explicit ActiveCueInfoPanel(Application* app, QWidget* parent = nullptr);

  public slots:
    void refresh();

  private:
    void setupUi();
    void showCue(int index);

    Application* m_app;

    QLabel* m_numberLabel;
    QLabel* m_nameLabel;
    QLabel* m_typeLabel;
    QLabel* m_fadeLabel;
    QTextEdit* m_detailsEdit;
};

} // namespace OpenMix
