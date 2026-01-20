#pragma once

#include <QVector>
#include <QWidget>

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

  private:
    void setupUi();
    bool parseParameterPath(const QString& path, QString& type, int& number, QString& param);

    Application* m_app;
    QVector<DCAWidget*> m_dcaWidgets;
};

} // namespace OpenMix
