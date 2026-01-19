#pragma once

#include <QVector>
#include <QWidget>

namespace StageBlend {

class Application;
class DCAWidget;

class MixerFeedbackPanel : public QWidget {
    Q_OBJECT

  public:
    explicit MixerFeedbackPanel(Application* app, QWidget* parent = nullptr);

    void setVisibleDCAs(const QVector<int>& dcaNumbers);

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

} // namespace StageBlend
