#include "LiveEditSession.h"
#include "Cue.h"
#include "CueList.h"
#include "PreviewLayer.h"
#include "protocol/MixerProtocol.h"
#include <QDateTime>

namespace OpenMix {

LiveEditSession::LiveEditSession(QObject* parent) : QObject(parent) {}

void LiveEditSession::setCueList(CueList* cueList) { m_cueList = cueList; }

void LiveEditSession::setMixer(MixerProtocol* mixer) { m_mixer = mixer; }

void LiveEditSession::setPreviewLayer(PreviewLayer* previewLayer) { m_previewLayer = previewLayer; }

int LiveEditSession::activeCueIndex() const {
    if (!m_cueList || m_activeCueId.isEmpty()) {
        return -1;
    }
    return m_cueList->indexOf(m_activeCueId);
}

QJsonObject LiveEditSession::pendingEdits() const {
    QJsonObject edits;
    for (auto it = m_edits.begin(); it != m_edits.end(); ++it) {
        edits[it.key()] = QJsonValue::fromVariant(it.value().currentValue);
    }
    return edits;
}

QStringList LiveEditSession::editedPaths() const { return m_edits.keys(); }

bool LiveEditSession::hasEdits() const { return !m_edits.isEmpty(); }

int LiveEditSession::editCount() const { return m_edits.size(); }

QVariant LiveEditSession::originalValue(const QString& path) const {
    auto it = m_edits.constFind(path);
    if (it != m_edits.constEnd()) {
        return it.value().originalValue;
    }
    return m_originalValues.value(path).toVariant();
}

const ParameterEdit* LiveEditSession::edit(const QString& path) const {
    auto it = m_edits.constFind(path);
    if (it != m_edits.constEnd()) {
        return &it.value();
    }
    return nullptr;
}

void LiveEditSession::startLiveEdit(const QString& cueId) {
    startSession(cueId, LiveEditMode::Live);
}

void LiveEditSession::startLiveEditByIndex(int cueIndex) {
    if (!m_cueList || cueIndex < 0 || cueIndex >= m_cueList->count()) {
        return;
    }
    startLiveEdit(m_cueList->at(cueIndex).id());
}

void LiveEditSession::startPreview(const QString& cueId) {
    startSession(cueId, LiveEditMode::Preview);
}

void LiveEditSession::startPreviewByIndex(int cueIndex) {
    if (!m_cueList || cueIndex < 0 || cueIndex >= m_cueList->count()) {
        return;
    }
    startPreview(m_cueList->at(cueIndex).id());
}

void LiveEditSession::togglePreviewMode() {
    if (m_mode == LiveEditMode::Inactive) {
        return;
    }

    if (m_mode == LiveEditMode::Live) {
        setMode(LiveEditMode::Preview);
        // enable preview layer. edits go to cache
        if (m_previewLayer) {
            m_previewLayer->setEnabled(true);
        }
    } else {
        setMode(LiveEditMode::Live);
        // disable preview layer & flush cached edits to mixer
        if (m_previewLayer) {
            m_previewLayer->flush();
            m_previewLayer->setEnabled(false);
        }
    }
}

void LiveEditSession::cancel() {
    if (m_mode == LiveEditMode::Inactive) {
        return;
    }

    // restore original values to mixer
    restoreOriginalValues();

    // clear preview layer
    if (m_previewLayer) {
        m_previewLayer->discard();
        m_previewLayer->setEnabled(false);
    }

    m_edits.clear();
    m_originalValues = QJsonObject();
    m_activeCueId.clear();
    setMode(LiveEditMode::Inactive);

    emit editsCancelled();
    emit sessionEnded();
}

void LiveEditSession::setParameter(const QString& path, const QVariant& value) {
    if (m_mode == LiveEditMode::Inactive) {
        return;
    }

    QVariant oldValue;
    if (m_edits.contains(path)) {
        oldValue = m_edits[path].currentValue;
    } else {
        // first edit, capture original value
        oldValue = m_originalValues.value(path).toVariant();
        if (!oldValue.isValid() && m_mixer) {
            // if not in original cue values, get from mixer
            oldValue = m_mixer->getParameter(path);
        }
    }

    ParameterEdit edit;
    edit.path = path;
    edit.originalValue = m_edits.contains(path) ? m_edits[path].originalValue : oldValue;
    edit.currentValue = value;
    edit.timestamp = QDateTime::currentMSecsSinceEpoch();

    m_edits[path] = edit;

    // apply edit to mixer (or preview layer)
    applyEditToMixer(path, value);

    emit parameterEdited(path, oldValue, value);
}

void LiveEditSession::revertParameter(const QString& path) {
    if (!m_edits.contains(path)) {
        return;
    }

    QVariant originalValue = m_edits[path].originalValue;
    m_edits.remove(path);

    // restore original value to mixer
    applyEditToMixer(path, originalValue);

    emit parameterReverted(path);
}

void LiveEditSession::revertAll() {
    if (m_edits.isEmpty()) {
        return;
    }

    // restore all original values
    restoreOriginalValues();
    m_edits.clear();

    // clear preview layer
    if (m_previewLayer && m_mode == LiveEditMode::Preview) {
        m_previewLayer->discard();
    }
}

void LiveEditSession::commitToCue(const QString& targetCueId) {
    if (m_mode == LiveEditMode::Inactive || m_edits.isEmpty()) {
        return;
    }

    if (!m_cueList) {
        return;
    }

    Cue* cue = m_cueList->findById(targetCueId);
    if (!cue) {
        return;
    }

    // merge edits into cue parameters
    QJsonObject params = cue->parameters();
    for (auto it = m_edits.begin(); it != m_edits.end(); ++it) {
        params[it.key()] = QJsonValue::fromVariant(it.value().currentValue);
    }
    cue->setParameters(params);

    // update cue in list (triggers undo/redo tracking)
    int cueIndex = m_cueList->indexOf(targetCueId);
    if (cueIndex >= 0) {
        m_cueList->updateCue(cueIndex, *cue);
    }

    // if in preview mode, flush preview layer to apply changes to mixer
    if (m_previewLayer && m_mode == LiveEditMode::Preview) {
        m_previewLayer->flush();
        m_previewLayer->setEnabled(false);
    }

    QString committedCueId = targetCueId;
    m_edits.clear();
    m_originalValues = QJsonObject();
    m_activeCueId.clear();
    setMode(LiveEditMode::Inactive);

    emit editsCommitted(committedCueId);
    emit sessionEnded();
}

void LiveEditSession::commitToCurrentCue() {
    if (m_activeCueId.isEmpty()) {
        return;
    }
    commitToCue(m_activeCueId);
}

void LiveEditSession::commitToNewCue(const QString& cueName) {
    if (m_mode == LiveEditMode::Inactive || m_edits.isEmpty()) {
        return;
    }

    if (!m_cueList) {
        return;
    }

    // find source cue to copy props from
    const Cue* sourceCue = m_cueList->findById(m_activeCueId);
    if (!sourceCue) {
        return;
    }

    // create new cue
    Cue newCue;
    newCue.setNumber(sourceCue->number() + 0.1); // auto-number
    newCue.setName(cueName.isEmpty() ? tr("Live Edit") : cueName);
    newCue.setType(sourceCue->type());
    newCue.setFadeTime(sourceCue->fadeTime());
    newCue.setFadeCurve(sourceCue->fadeCurve());

    // set parameters with edits applied
    QJsonObject params = sourceCue->parameters();
    for (auto it = m_edits.begin(); it != m_edits.end(); ++it) {
        params[it.key()] = QJsonValue::fromVariant(it.value().currentValue);
    }
    newCue.setParameters(params);

    // insert after source cue
    int insertIndex = m_cueList->indexOf(m_activeCueId);
    if (insertIndex >= 0) {
        m_cueList->insertCue(insertIndex + 1, newCue);
    } else {
        m_cueList->addCue(newCue);
    }

    // if in preview mode, flush preview layer
    if (m_previewLayer && m_mode == LiveEditMode::Preview) {
        m_previewLayer->flush();
        m_previewLayer->setEnabled(false);
    }

    QString newCueId = newCue.id();
    m_edits.clear();
    m_originalValues = QJsonObject();
    m_activeCueId.clear();
    setMode(LiveEditMode::Inactive);

    emit editsCommitted(newCueId);
    emit sessionEnded();
}

void LiveEditSession::setMode(LiveEditMode mode) {
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(mode);
    }
}

void LiveEditSession::startSession(const QString& cueId, LiveEditMode mode) {
    // cancel any existing session
    if (m_mode != LiveEditMode::Inactive) {
        cancel();
    }

    if (!m_cueList) {
        return;
    }

    const Cue* cue = m_cueList->findById(cueId);
    if (!cue) {
        return;
    }

    m_activeCueId = cueId;
    m_edits.clear();

    // capture original values from cue
    captureOriginalState();

    // enable preview layer if preview mode
    if (m_previewLayer && mode == LiveEditMode::Preview) {
        m_previewLayer->setEnabled(true);
    }

    setMode(mode);
    emit sessionStarted(cueId);
}

void LiveEditSession::captureOriginalState() {
    if (!m_cueList || m_activeCueId.isEmpty()) {
        return;
    }

    const Cue* cue = m_cueList->findById(m_activeCueId);
    if (cue) {
        m_originalValues = cue->parameters();
    }

    // also capture current mixer state for parameters not in cue
    if (m_mixer && m_mixer->isConnected()) {
        QJsonObject mixerState = m_mixer->captureCurrentState();
        // merge mixer state, cue values take precedence
        for (auto it = mixerState.begin(); it != mixerState.end(); ++it) {
            if (!m_originalValues.contains(it.key())) {
                m_originalValues[it.key()] = it.value();
            }
        }
    }
}

void LiveEditSession::applyEditToMixer(const QString& path, const QVariant& value) {
    if (m_mode == LiveEditMode::Preview && m_previewLayer) {
        // in preview mode, edits go to preview layer
        m_previewLayer->sendParameter(path, value);
    } else if (m_mode == LiveEditMode::Live && m_mixer) {
        // in live mode, edits go directly to mixer
        m_mixer->sendParameter(path, value);
    }
}

void LiveEditSession::restoreOriginalValues() {
    if (!m_mixer || m_edits.isEmpty()) {
        return;
    }

    // restore each edited param to its original value
    for (auto it = m_edits.begin(); it != m_edits.end(); ++it) {
        m_mixer->sendParameter(it.key(), it.value().originalValue);
    }
}

} // namespace OpenMix
