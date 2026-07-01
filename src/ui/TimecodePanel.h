#pragma once

#include <QWidget>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace OpenMix {

class Application;
class TimecodeTriggerList;

// Manage timecode-to-cue triggers and show live incoming timecode. Backed by the
// Application-owned TimecodeTriggerList; a trigger fires its cue when playback
// timecode crosses its point.
class TimecodePanel : public QWidget {
    Q_OBJECT

  public:
    explicit TimecodePanel(Application* app, QWidget* parent = nullptr);

  public slots:
    void refresh();

  private slots:
    void addTrigger();
    void removeTrigger();
    void onCellChanged(int row, int column);
    void onIncomingTimecode(int hours, int minutes, int seconds, int frames);
    void onTriggerFired(double cueNumber, const QString& triggerId);

  private:
    void setupUi();
    void populateTable();
    [[nodiscard]] QString rowTriggerId(int row) const;

    Application* m_app;
    TimecodeTriggerList* m_triggers = nullptr;

    QTableWidget* m_table;
    QSpinBox* m_hoursSpin;
    QSpinBox* m_minutesSpin;
    QSpinBox* m_secondsSpin;
    QSpinBox* m_framesSpin;
    QDoubleSpinBox* m_cueSpin;
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QLabel* m_liveLabel;

    bool m_updatingUi = false;
};

} // namespace OpenMix
