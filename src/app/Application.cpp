#include "Application.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/CueValidator.h"
#include "core/DryRunEngine.h"
#include "core/FadeConflictResolver.h"
#include "core/OperationMode.h"
#include "core/PlaybackEngine.h"
#include "core/PlaybackGuard.h"
#include "core/PlaybackLogger.h"
#include "core/ShortcutManager.h"
#include "core/Show.h"
#include "io/AutosaveManager.h"
#include "io/CrashRecovery.h"
#include "protocol/MixerProtocol.h"

namespace StageBlend {

Application* Application::s_instance = nullptr;

Application::Application(QObject* parent) : QObject(parent) {
    s_instance = this;

    // core components
    m_show = new Show(this);
    m_playbackEngine = new PlaybackEngine(this);
    m_undoStack = new QUndoStack(this);
    m_autosaveManager = new AutosaveManager(this, this);

    // safety & validation
    m_cueValidator = new CueValidator(this);
    m_playbackGuard = new PlaybackGuard(m_playbackEngine, this);
    m_playbackLogger = new PlaybackLogger(this);
    m_fadeConflictResolver = new FadeConflictResolver(this);
    m_dryRunEngine = new DryRunEngine(this);

    // operator experience
    m_shortcutManager = new ShortcutManager(this);
    m_operationModeManager = new OperationModeManager(this);

    // recovery
    m_crashRecovery = new CrashRecovery(this);
}

Application::~Application() {
    // mark clean exit for crash recovery
    if (m_crashRecovery) {
        m_crashRecovery->markCleanExit();
    }

    disconnectFromMixer();

    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void Application::initialize() {
    // connect playback engine to show's cue list
    m_playbackEngine->setCueList(m_show->cueList());

    // connect playback engine to mixer if available
    if (m_mixer) {
        m_playbackEngine->setMixer(m_mixer);
    }

    // set up safety components on playback engine
    m_playbackEngine->setValidator(m_cueValidator);
    m_playbackEngine->setGuard(m_playbackGuard);
    m_playbackEngine->setLogger(m_playbackLogger);
    m_playbackEngine->setConflictResolver(m_fadeConflictResolver);

    // set up cue list references
    m_fadeConflictResolver->setCueList(m_show->cueList());
    m_dryRunEngine->setCueList(m_show->cueList());
    m_dryRunEngine->setValidator(m_cueValidator);

    // set up crash recovery
    m_crashRecovery->setShow(m_show);
    m_crashRecovery->setPlaybackEngine(m_playbackEngine);
    m_crashRecovery->setAutoSaveEnabled(true);

    // connect autosave to show modifications
    connect(m_show, &Show::modifiedChanged, m_autosaveManager, [this](bool modified) {
        if (modified) {
            m_autosaveManager->scheduleSave();
        }
    });

    // mark undo stack as clean when show is saved
    connect(m_show, &Show::modifiedChanged, [this](bool modified) {
        if (!modified) {
            m_undoStack->setClean();
        }
    });

    // connect playback events to logger
    connect(m_playbackEngine, &PlaybackEngine::cueExecuted, [this](int index) {
        if (m_playbackLogger && m_show->cueList() && index >= 0) {
            const Cue& cue = m_show->cueList()->at(index);
            m_playbackLogger->logCueExecuted(cue.id(), cue.name(), cue.number());
        }
    });

    connect(m_playbackEngine, &PlaybackEngine::fadeStarted, [this](const QString& cueId) {
        if (m_playbackLogger && m_show->cueList()) {
            const Cue* cue = m_show->cueList()->findById(cueId);
            if (cue) {
                m_playbackLogger->logFadeStarted(cueId, cue->fadeTime());
            }
        }
    });

    connect(m_playbackEngine, &PlaybackEngine::fadeCompleted, [this](const QString& cueId) {
        if (m_playbackLogger) {
            m_playbackLogger->logFadeCompleted(cueId);
        }
    });

    connect(m_playbackEngine, &PlaybackEngine::goLockout, [this](const QString& reason) {
        if (m_playbackLogger) {
            m_playbackLogger->logGoLockout(reason);
        }
    });

    // load settings
    m_shortcutManager->loadFromSettings();
    m_operationModeManager->loadFromSettings();
}

void Application::connectToMixer(const QString& type, const QString& host, int port) {
    // disconnect existing mixer if any
    disconnectFromMixer();

    // create new mixer protocol
    m_mixer = createMixerProtocol(type, this);
    if (!m_mixer) {
        return;
    }

    // connect to playback engine
    m_playbackEngine->setMixer(m_mixer);

    // connect signals
    connect(m_mixer, &MixerProtocol::connected, this, [this]() { emit mixerConnected(); });

    connect(m_mixer, &MixerProtocol::disconnected, this, [this]() { emit mixerDisconnected(); });

    // attempt connection
    m_mixer->connect(host, port);
}

void Application::disconnectFromMixer() {
    if (m_mixer) {
        m_mixer->disconnect();
        m_playbackEngine->setMixer(nullptr);
        delete m_mixer;
        m_mixer = nullptr;
    }
}

} // namespace StageBlend
