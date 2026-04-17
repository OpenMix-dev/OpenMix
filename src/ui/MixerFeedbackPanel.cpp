#include "MixerFeedbackPanel.h"
#include "DCAWidget.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "protocol/MixerProtocol.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>

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
    setMinimumSize(450, 200);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setFocusPolicy(Qt::StrongFocus);

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
    connect(dca, &DCAWidget::tabToNextRequested, this, &MixerFeedbackPanel::onTabToNext);
    connect(dca, &DCAWidget::tabToPreviousRequested, this, &MixerFeedbackPanel::onTabToPrevious);
}

void MixerFeedbackPanel::setDCACount(int count) {
    count = std::clamp(count, 1, 24); // support up to 24 DCAs (WING max)

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
    if (const auto cueIndex = cueList->indexOf(m_activeCueId)) {
        cueList->updateCue(*cueIndex, *cue);
    }

    // update widget immediately
    if (dcaNumber >= 1 && dcaNumber <= m_dcaWidgets.size()) {
        m_dcaWidgets[dcaNumber - 1]->setCueLabel(newLabel);
    }
}

void MixerFeedbackPanel::onTabToNext(int dcaNumber) {
    if (m_dcaWidgets.isEmpty()) {
        return;
    }

    // find next visible DCA, wrapping around
    int count = m_dcaWidgets.size();
    int startIndex = dcaNumber - 1; // convert to 0-based
    for (int i = 1; i <= count; ++i) {
        int nextIndex = (startIndex + i) % count;
        DCAWidget* next = m_dcaWidgets[nextIndex];
        if (next->isVisible() && next->isLabelEditEnabled()) {
            next->startLabelEdit();
            return;
        }
    }
}

void MixerFeedbackPanel::onTabToPrevious(int dcaNumber) {
    if (m_dcaWidgets.isEmpty()) {
        return;
    }

    // find previous visible DCA, wrapping around
    int count = m_dcaWidgets.size();
    int startIndex = dcaNumber - 1; // convert to 0-based
    for (int i = 1; i <= count; ++i) {
        int prevIndex = (startIndex - i + count) % count;
        DCAWidget* prev = m_dcaWidgets[prevIndex];
        if (prev->isVisible() && prev->isLabelEditEnabled()) {
            prev->startLabelEdit();
            return;
        }
    }
}

void MixerFeedbackPanel::loadCueSettings(const QString& cueId) {
    if (!m_app || !m_app->show() || !m_app->show()->cueList()) {
        return;
    }

    CueList* cueList = m_app->show()->cueList();
    const auto index = cueList->indexOf(cueId);
    if (!index) {
        clearCueSettings();
        return;
    }

    const Cue& cue = cueList->at(*index);

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

void MixerFeedbackPanel::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Tab && !isAnyLabelBeingEdited()) {
        DCAWidget* first = firstEditableDCA();
        if (first) {
            first->startLabelEdit();
            event->accept();
            return;
        }
    } else if (event->key() == Qt::Key_Backtab && !isAnyLabelBeingEdited()) {
        DCAWidget* last = lastEditableDCA();
        if (last) {
            last->startLabelEdit();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

bool MixerFeedbackPanel::isAnyLabelBeingEdited() const {
    for (DCAWidget* dca : m_dcaWidgets) {
        if (dca->findChild<QLineEdit*>() && dca->findChild<QLineEdit*>()->isVisible()) {
            return true;
        }
    }
    return false;
}

DCAWidget* MixerFeedbackPanel::firstEditableDCA() const {
    for (DCAWidget* dca : m_dcaWidgets) {
        if (dca->isVisible() && dca->isLabelEditEnabled()) {
            return dca;
        }
    }
    return nullptr;
}

DCAWidget* MixerFeedbackPanel::lastEditableDCA() const {
    for (int i = m_dcaWidgets.size() - 1; i >= 0; --i) {
        DCAWidget* dca = m_dcaWidgets[i];
        if (dca->isVisible() && dca->isLabelEditEnabled()) {
            return dca;
        }
    }
    return nullptr;
}

} // namespace OpenMix
