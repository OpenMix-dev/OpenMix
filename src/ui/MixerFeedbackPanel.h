#pragma once

#include <QVector>
#include <QWidget>

class QKeyEvent;

namespace OpenMix {

class Application;
class DCAWidget;

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

  protected:
    void keyPressEvent(QKeyEvent* event) override;

  private slots:
    void onDCALabelEdited(int dcaNumber, const QString& newLabel);
    void onTabToNext(int dcaNumber);
    void onTabToPrevious(int dcaNumber);

  private:
    bool isAnyLabelBeingEdited() const;
    DCAWidget* firstEditableDCA() const;
    DCAWidget* lastEditableDCA() const;
    void setupUi();
    void connectDCASignals(DCAWidget* dca);
    bool parseParameterPath(const QString& path, QString& type, int& number, QString& param);
    void loadCueSettings(const QString& cueId);
    void clearCueSettings();

    Application* m_app;
    QVector<DCAWidget*> m_dcaWidgets;
    QString m_activeCueId;
};

} // namespace OpenMix
