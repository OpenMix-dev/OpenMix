#include "MixerFeedbackPanel.h"
#include "CueConfidenceIndicator.h"
#include "DCAWidget.h"
#include "app/Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/LevelDb.h"
#include "core/PlaybackEngine.h"
#include "core/Show.h"
#include "protocol/MixerProtocol.h"
#include "theme/Icons.h"
#include "theme/Theme.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace OpenMix {

MixerFeedbackPanel::MixerFeedbackPanel(Application* app, QWidget* parent)
    : QWidget(parent), m_app(app) {
    setupUi();

    if (m_app) {
        connect(m_app, &Application::mixerConnected, this, &MixerFeedbackPanel::onMixerConnected);
        connect(m_app, &Application::mixerDisconnected, this,
                &MixerFeedbackPanel::onMixerDisconnected);
        connect(m_app, &Application::dcaCountChanged, this, &MixerFeedbackPanel::setDCACount);
        connect(m_app, &Application::activeDcasChanged, this,
                &MixerFeedbackPanel::applyActiveDcaVisibility);
        setDCACount(m_app->effectiveDcaCount());

        if (m_app->mixer() && m_app->mixer()->isConnected()) {
            onMixerConnected();
        }
    }
}

void MixerFeedbackPanel::setupUi() {
    setMinimumSize(450, 200);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    setFocusPolicy(Qt::StrongFocus);

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    // post-fire confidence row: did the last GO actually land on the console?
    QHBoxLayout* statusRow = new QHBoxLayout();
    statusRow->setSpacing(6);
    QLabel* caption = new QLabel(tr("Last fire:"), this);
    m_fireIndicator = new CueConfidenceIndicator(this);
    m_fireIndicator->setFixedSize(14, 14);
    m_fireIndicator->setConfidence(ConfidenceLevel::Unknown, tr("Awaiting GO"));
    m_fireStatusLabel = new QLabel(tr("Awaiting GO"), this);
    m_fireStatusLabel->setStyleSheet(QString("color: %1;").arg(Theme::Colors::TextSecondary));
    statusRow->addWidget(caption);
    statusRow->addWidget(m_fireIndicator);
    statusRow->addWidget(m_fireStatusLabel, 1);
    statusRow->addStretch();

    // standby back/next, same semantics as the main toolbar's transport.
    // NoFocus is load-bearing: keyPressEvent implements Tab into DCA label
    // editing, which focusable buttons would capture.
    m_prevCueButton = new QToolButton(this);
    m_prevCueButton->setIcon(Icons::mediaPrevious());
    m_prevCueButton->setAutoRaise(true);
    m_prevCueButton->setFixedSize(28, 28);
    m_prevCueButton->setIconSize(QSize(16, 16));
    m_prevCueButton->setFocusPolicy(Qt::NoFocus);
    m_prevCueButton->setToolTip(tr("Back cue (Up)"));
    connect(m_prevCueButton, &QToolButton::clicked, this, [this]() {
        if (m_app && m_app->playbackEngine())
            m_app->playbackEngine()->previous();
    });
    statusRow->addWidget(m_prevCueButton);

    m_nextCueButton = new QToolButton(this);
    m_nextCueButton->setIcon(Icons::mediaNext());
    m_nextCueButton->setAutoRaise(true);
    m_nextCueButton->setFixedSize(28, 28);
    m_nextCueButton->setIconSize(QSize(16, 16));
    m_nextCueButton->setFocusPolicy(Qt::NoFocus);
    m_nextCueButton->setToolTip(tr("Next cue (Down)"));
    connect(m_nextCueButton, &QToolButton::clicked, this, [this]() {
        if (m_app && m_app->playbackEngine())
            m_app->playbackEngine()->next();
    });
    statusRow->addWidget(m_nextCueButton);

    outer->addLayout(statusRow);

    m_dcaLayout = new QHBoxLayout();
    m_dcaLayout->setSpacing(4);

    // create default 8 DCA widgets (will be adjusted by setDCACount)
    for (int i = 1; i <= 8; ++i) {
        DCAWidget* dca = new DCAWidget(i, this);
        connectDCASignals(dca);
        m_dcaWidgets.append(dca);
        m_dcaLayout->addWidget(dca);
    }

    m_dcaLayout->addStretch();
    outer->addLayout(m_dcaLayout);
}

void MixerFeedbackPanel::connectDCASignals(DCAWidget* dca) {
    connect(dca, &DCAWidget::labelEdited, this, &MixerFeedbackPanel::onDCALabelEdited);
    connect(dca, &DCAWidget::tabToNextRequested, this, &MixerFeedbackPanel::onTabToNext);
    connect(dca, &DCAWidget::tabToPreviousRequested, this, &MixerFeedbackPanel::onTabToPrevious);
}

void MixerFeedbackPanel::setDCACount(int count) {
    count = std::clamp(count, 1, 24); // support up to 24 DCAs (DM7/SD7 max)

    if (!m_dcaLayout)
        return;

    while (m_dcaWidgets.size() > count) {
        DCAWidget* dca = m_dcaWidgets.takeLast();
        m_dcaLayout->removeWidget(dca);
        dca->deleteLater();
    }

    bool added = false;
    while (m_dcaWidgets.size() < count) {
        int dcaNum = m_dcaWidgets.size() + 1;
        DCAWidget* dca = new DCAWidget(dcaNum, this);
        connectDCASignals(dca);
        m_dcaWidgets.append(dca);
        // insert before stretch
        m_dcaLayout->insertWidget(m_dcaLayout->count() - 1, dca);
        added = true;
    }

    // hydrate widgets created after startup with the current cue + mixer state
    if (added) {
        if (!m_activeCueId.isEmpty()) {
            loadCueSettings(m_activeCueId);
            for (DCAWidget* dca : m_dcaWidgets)
                dca->setLabelEditEnabled(true);
        }
        if (m_app && m_app->mixer() && m_app->mixer()->isConnected())
            refresh();
    }

    applyActiveDcaVisibility();
}

void MixerFeedbackPanel::applyActiveDcaVisibility() {
    if (!m_app)
        return;
    // direct visibility, not setVisibleDCAs: its empty-list-means-all
    // convention would show everything when every DCA is inactive
    for (DCAWidget* dca : m_dcaWidgets)
        dca->setVisible(m_app->isDcaActive(dca->dcaNumber()));
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
            dca->setLevelDb(value.toFloat());
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
        dca->setLevelDb(static_cast<float>(NEG_INF_DB));
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

QString MixerFeedbackPanel::cueDescription(int cueIndex) const {
    if (!m_app || !m_app->show() || !m_app->show()->cueList())
        return tr("Cue %1").arg(cueIndex);
    const CueList* cueList = m_app->show()->cueList();
    if (cueIndex < 0 || cueIndex >= cueList->count())
        return tr("Cue %1").arg(cueIndex);
    const Cue& cue = cueList->at(cueIndex);
    QString desc = tr("Cue %1").arg(cue.number(), 0, 'f', 1);
    if (!cue.name().isEmpty())
        desc += QString(" - %1").arg(cue.name());
    return desc;
}

void MixerFeedbackPanel::onCueLanded(int cueIndex) {
    if (!m_fireIndicator)
        return;
    const QString desc = cueDescription(cueIndex);
    m_fireIndicator->setConfidence(ConfidenceLevel::Good,
                                   tr("%1 landed: console matches the fired values.").arg(desc));
    m_fireStatusLabel->setText(tr("%1 landed").arg(desc));
    m_fireStatusLabel->setStyleSheet(QString("color: %1;").arg(Theme::Colors::AccentGreen));
}

void MixerFeedbackPanel::onCueDrifted(int cueIndex, const QStringList& paths) {
    if (!m_fireIndicator)
        return;
    const QString desc = cueDescription(cueIndex);
    QString tooltip = tr("%1 drift on %n parameter(s):", "", paths.size()).arg(desc);
    tooltip += "\n" + paths.join("\n");
    m_fireIndicator->setConfidence(ConfidenceLevel::Warning, tooltip);
    m_fireStatusLabel->setText(tr("%1 drift on %n parameter(s)", "", paths.size()).arg(desc));
    m_fireStatusLabel->setStyleSheet(QString("color: %1;").arg(Theme::Colors::AccentAmber));
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
