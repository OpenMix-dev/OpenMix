#include "MixerFeedbackPanel.h"
#include "DCAWidget.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/LiveEditSession.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
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
        connectDCASignals(dca);
        m_dcaWidgets.append(dca);
        layout->addWidget(dca);
    }

    layout->addStretch();
}

void MixerFeedbackPanel::connectDCASignals(DCAWidget* dca) {
    connect(dca, &DCAWidget::labelEdited, this, &MixerFeedbackPanel::onDCALabelEdited);
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
        connectDCASignals(dca);
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
            dca->setMixerName(value.toString());
        } else if (param == "label") {
            dca->setCueLabel(value.toString());
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
        dca->setMixerName(QString());
        dca->setCueLabel(QString());
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

void MixerFeedbackPanel::onLiveEditModeChanged(int mode) {
    // 0 = inactive, 1 = live, 2 = preview
    bool editMode = mode != 0;
    bool previewMode = mode == 2;

    for (DCAWidget* dca : m_dcaWidgets) {
        dca->setEditMode(editMode);
        dca->setPreviewMode(previewMode);
    }
}

void MixerFeedbackPanel::onLiveEditSessionStarted(const QString& cueId) {
    // capture current levels as original values
    for (DCAWidget* dca : m_dcaWidgets) {
        dca->setOriginalLevel(dca->level());
        dca->setEditMode(true);
    }

    // load cue-specific labels (also sets m_activeCueId)
    if (!cueId.isEmpty()) {
        m_activeCueId = cueId;
        loadCueSettings(cueId);
    }
}

void MixerFeedbackPanel::onLiveEditSessionEnded() {
    for (DCAWidget* dca : m_dcaWidgets) {
        dca->setEditMode(false);
        dca->setPreviewMode(false);
    }
}

void MixerFeedbackPanel::onActiveCueChanged(int cueIndex) {
    if (!m_app || !m_app->show() || !m_app->show()->cueList()) {
        return;
    }

    CueList* cueList = m_app->show()->cueList();
    if (cueIndex < 0 || cueIndex >= cueList->count()) {
        clearCueSettings();
        m_activeCueId.clear();
        // disable label editing when no cue is active
        for (DCAWidget* dca : m_dcaWidgets) {
            dca->setLabelEditEnabled(false);
        }
        return;
    }

    const Cue& cue = cueList->at(cueIndex);
    m_activeCueId = cue.id();
    loadCueSettings(cue.id());

    // enable label editing when a cue is active
    for (DCAWidget* dca : m_dcaWidgets) {
        dca->setLabelEditEnabled(true);
    }
}

void MixerFeedbackPanel::onDCALabelEdited(int dcaNumber, const QString& newLabel) {
    if (!m_app || m_activeCueId.isEmpty()) {
        return;
    }

    CueList* cueList = m_app->show()->cueList();
    if (!cueList) {
        return;
    }

    Cue* cue = cueList->findById(m_activeCueId);
    if (!cue) {
        return;
    }

    // save label directly to cue parameters
    QString path = QString("/dca/%1/label").arg(dcaNumber);
    cue->setParameter(path, newLabel);

    // update cue in list
    int cueIndex = cueList->indexOf(m_activeCueId);
    if (cueIndex >= 0) {
        cueList->updateCue(cueIndex, *cue);
    }

    // update widget immediately
    if (dcaNumber >= 1 && dcaNumber <= m_dcaWidgets.size()) {
        m_dcaWidgets[dcaNumber - 1]->setCueLabel(newLabel);
    }
}

void MixerFeedbackPanel::loadCueSettings(const QString& cueId) {
    if (!m_app || !m_app->show() || !m_app->show()->cueList()) {
        return;
    }

    CueList* cueList = m_app->show()->cueList();
    int index = cueList->indexOf(cueId);
    if (index < 0) {
        clearCueSettings();
        return;
    }

    const Cue& cue = cueList->at(index);

    // load /dca/N/label values from cue
    for (int i = 1; i <= m_dcaWidgets.size(); ++i) {
        QString path = QString("/dca/%1/label").arg(i);
        QVariant labelValue = cue.parameter(path);
        QString label = labelValue.toString();
        m_dcaWidgets[i - 1]->setCueLabel(label);
    }
}

void MixerFeedbackPanel::clearCueSettings() {
    for (DCAWidget* dca : m_dcaWidgets) {
        dca->setCueLabel(QString());
    }
}

} // namespace OpenMix
