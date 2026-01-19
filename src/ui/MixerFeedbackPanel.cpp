#include "MixerFeedbackPanel.h"
#include "DCAWidget.h"
#include "app/Application.h"
#include "protocol/MixerProtocol.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QTimer>

namespace StageBlend {

MixerFeedbackPanel::MixerFeedbackPanel(Application* app, QWidget* parent)
    : QWidget(parent), m_app(app) {
    setupUi();

    // connect to application signals
    if (m_app) {
        connect(m_app, &Application::mixerConnected, this, &MixerFeedbackPanel::onMixerConnected);
        connect(m_app, &Application::mixerDisconnected, this,
                &MixerFeedbackPanel::onMixerDisconnected);

        // if already connected, set up
        if (m_app->mixer() && m_app->mixer()->isConnected()) {
            onMixerConnected();
        }
    }
}

void MixerFeedbackPanel::setupUi() {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // create 8 DCA widgets
    for (int i = 1; i <= 8; ++i) {
        DCAWidget* dca = new DCAWidget(i, this);
        m_dcaWidgets.append(dca);
        layout->addWidget(dca);
    }

    layout->addStretch();
}

void MixerFeedbackPanel::setVisibleDCAs(const QVector<int>& dcaNumbers) {
    for (DCAWidget* dca : m_dcaWidgets) {
        bool visible = dcaNumbers.isEmpty() || dcaNumbers.contains(dca->dcaNumber());
        dca->setVisible(visible);
    }
}

void MixerFeedbackPanel::onParameterChanged(const QString& path, const QVariant& value) {
    QString type;
    int number;
    QString param;

    if (!parseParameterPath(path, type, number, param)) {
        return;
    }

    if (type == "dca" && number >= 1 && number <= 8) {
        DCAWidget* dca = m_dcaWidgets[number - 1];

        if (param == "fader") {
            dca->setLevel(value.toFloat());
            dca->setActive(true);
            // clear active state after a short delay
            QTimer::singleShot(500, [dca]() { dca->setActive(false); });
        } else if (param == "on") {
            // X32 uses inverted logic (1 = unmuted, 0 = muted)
            dca->setMuted(value.toInt() == 0);
        } else if (param == "config/name") {
            dca->setName(value.toString());
        }
    }
}

void MixerFeedbackPanel::refresh() {
    if (!m_app || !m_app->mixer())
        return;

    MixerProtocol* mixer = m_app->mixer();

    // request DCA parameters
    for (int i = 1; i <= 8; ++i) {
        mixer->requestParameter(QString("/dca/%1/fader").arg(i));
        mixer->requestParameter(QString("/dca/%1/on").arg(i));
        mixer->requestParameter(QString("/dca/%1/config/name").arg(i));
    }
}

void MixerFeedbackPanel::onMixerConnected() {
    if (!m_app || !m_app->mixer())
        return;

    // connect to parameter changes
    connect(m_app->mixer(), &MixerProtocol::parameterChanged, this,
            &MixerFeedbackPanel::onParameterChanged, Qt::UniqueConnection);

    // initial refresh
    refresh();
}

void MixerFeedbackPanel::onMixerDisconnected() {
    // reset all DCA displays
    for (DCAWidget* dca : m_dcaWidgets) {
        dca->setLevel(0.0f);
        dca->setMuted(false);
        dca->setName(QString());
        dca->setActive(false);
    }
}

bool MixerFeedbackPanel::parseParameterPath(const QString& path, QString& type, int& number,
                                            QString& param) {
    // parse paths like /dca/1/fader, /dca/1/on, /dca/1/config/name
    static QRegularExpression dcaRe(R"(/dca/(\d+)/(.+))");

    QRegularExpressionMatch match = dcaRe.match(path);
    if (match.hasMatch()) {
        type = "dca";
        number = match.captured(1).toInt();
        param = match.captured(2);
        return true;
    }

    return false;
}

} // namespace StageBlend
