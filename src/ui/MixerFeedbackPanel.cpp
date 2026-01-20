#include "MixerFeedbackPanel.h"
#include "DCAWidget.h"
#include "app/Application.h"
#include "protocol/MixerProtocol.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QRegularExpression>
#include <QTimer>

namespace OpenMix {

MixerFeedbackPanel::MixerFeedbackPanel(Application* app, QWidget* parent)
    : QWidget(parent), m_app(app) {
    setupUi();

    if (m_app) {
        connect(m_app, &Application::mixerConnected, this, &MixerFeedbackPanel::onMixerConnected);
        connect(m_app, &Application::mixerDisconnected, this,
                &MixerFeedbackPanel::onMixerDisconnected);

        if (m_app->mixer() && m_app->mixer()->isConnected()) {
            onMixerConnected();
        }
    }
}

void MixerFeedbackPanel::setupUi() {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    // create default 8 DCA widgets (will be adjusted by setDCACount)
    for (int i = 1; i <= 8; ++i) {
        DCAWidget* dca = new DCAWidget(i, this);
        m_dcaWidgets.append(dca);
        layout->addWidget(dca);
    }

    layout->addStretch();
}

void MixerFeedbackPanel::setDCACount(int count) {
    count = qBound(1, count, 24); // support up to 24 DCAs (WING max)

    QHBoxLayout* layout = qobject_cast<QHBoxLayout*>(this->layout());
    if (!layout)
        return;

    while (m_dcaWidgets.size() > count) {
        DCAWidget* dca = m_dcaWidgets.takeLast();
        layout->removeWidget(dca);
        dca->deleteLater();
    }

    while (m_dcaWidgets.size() < count) {
        int dcaNum = m_dcaWidgets.size() + 1;
        DCAWidget* dca = new DCAWidget(dcaNum, this);
        m_dcaWidgets.append(dca);
        // insert before stretch
        layout->insertWidget(layout->count() - 1, dca);
    }
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

    if (type == "dca" && number >= 1 && number <= m_dcaWidgets.size()) {
        DCAWidget* dca = m_dcaWidgets[number - 1];

        if (param == "fader") {
            dca->setLevel(value.toFloat());
            dca->setActive(true);
            QTimer::singleShot(500, [dca]() { dca->setActive(false); });
        } else if (param == "on" || param == "mute") {
            // X32 uses inverted "on" logic (1 = unmuted, 0 = muted)
            if (param == "on") {
                dca->setMuted(value.toInt() == 0);
            } else {
                dca->setMuted(value.toBool());
            }
        } else if (param == "config/name") {
            dca->setName(value.toString());
        }
    }
}

void MixerFeedbackPanel::refresh() {
    if (!m_app || !m_app->mixer())
        return;

    MixerProtocol* mixer = m_app->mixer();

    for (int i = 1; i <= m_dcaWidgets.size(); ++i) {
        mixer->requestParameter(QString("/dca/%1/fader").arg(i));
        mixer->requestParameter(QString("/dca/%1/on").arg(i));
        mixer->requestParameter(QString("/dca/%1/mute").arg(i));
        mixer->requestParameter(QString("/dca/%1/config/name").arg(i));
    }
}

void MixerFeedbackPanel::onMixerConnected() {
    if (!m_app || !m_app->mixer())
        return;

    connect(m_app->mixer(), &MixerProtocol::parameterChanged, this,
            &MixerFeedbackPanel::onParameterChanged, Qt::UniqueConnection);

    refresh();
}

void MixerFeedbackPanel::onMixerDisconnected() {
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

} // namespace OpenMix
