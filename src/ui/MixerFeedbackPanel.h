#pragma once

#include <QVector>
#include <QWidget>

class QKeyEvent;
class QHBoxLayout;
class QLabel;
class QToolButton;

namespace OpenMix {

class Application;
class DCAWidget;
class CueConfidenceIndicator;

class MixerFeedbackPanel : public QWidget {
    Q_OBJECT

  public:
    explicit MixerFeedbackPanel(Application* app, QWidget* parent = nullptr);

    void setVisibleDCAs(const QVector<int>& dcaNumbers);

    // set number of DCA widgets to display (for different console types)
    void setDCACount(int count);
    int dcaCount() const { return m_dcaWidgets.size(); }

  public slots:
    void onParameterChanged(const QString& path, const QVariant& value);
    void refresh();
    void onMixerConnected();
    void onMixerDisconnected();

    void onActiveCueChanged(int cueIndex);

    // post-fire confidence from PlaybackEngine cue verification
    void onCueLanded(int cueIndex);
    void onCueDrifted(int cueIndex, const QStringList& paths);

  protected:
    void keyPressEvent(QKeyEvent* event) override;

  private slots:
    void onDCALabelEdited(int dcaNumber, const QString& newLabel);
    void onTabToNext(int dcaNumber);
    void onTabToPrevious(int dcaNumber);

  private:
    void applyActiveDcaVisibility();
    bool isAnyLabelBeingEdited() const;
    DCAWidget* firstEditableDCA() const;
    DCAWidget* lastEditableDCA() const;
    void setupUi();
    void connectDCASignals(DCAWidget* dca);
    bool parseParameterPath(const QString& path, QString& type, int& number, QString& param);
    void loadCueSettings(const QString& cueId);
    void clearCueSettings();

    QString cueDescription(int cueIndex) const;

    Application* m_app;
    QHBoxLayout* m_dcaLayout = nullptr;
    QVector<DCAWidget*> m_dcaWidgets;
    QString m_activeCueId;

    // post-fire ("did the cue land?") status row
    CueConfidenceIndicator* m_fireIndicator = nullptr;
    QLabel* m_fireStatusLabel = nullptr;

    // cue transport (standby back/next, same semantics as the main toolbar)
    QToolButton* m_prevCueButton = nullptr;
    QToolButton* m_nextCueButton = nullptr;
};

} // namespace OpenMix
