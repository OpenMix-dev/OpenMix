#include "Application.h"
#include "core/AppLogger.h"
#include "core/ConnectionLogBridge.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/CueValidator.h"
#include "core/DCAMapping.h"
#include "core/DryRunEngine.h"
#include "core/OperationMode.h"
#include "core/PlaybackEngine.h"
#include "core/PlaybackGuard.h"
#include "core/PlaybackLogger.h"
#include "core/ShortcutManager.h"
#include "core/Show.h"
#include "io/AutosaveManager.h"
#include "io/CrashRecovery.h"
#include "midi/MidiInputManager.h"
#include "protocol/MixerProtocol.h"
#include "protocol/ProtocolFactory.h"
#include "protocol/discovery/ConsoleDiscoveryService.h"
#include "protocol/discovery/DiscoveredConsole.h"
#include "protocol/discovery/probes/BehringerWingProbeStrategy.h"
#include "protocol/discovery/probes/BehringerX32ProbeStrategy.h"
#include "protocol/discovery/probes/YamahaOscProbeStrategy.h"
#include <QDir>
#include <QStandardPaths>

namespace OpenMix {

Application* Application::s_instance = nullptr;

Application::Application(QObject* parent) : QObject(parent) {
    s_instance = this;

    m_show = new Show(this);
    m_playbackEngine = new PlaybackEngine(this);
    m_undoStack = new QUndoStack(this);
    m_autosaveManager = new AutosaveManager(this, this);

    m_cueValidator = new CueValidator(this);
    m_playbackGuard = new PlaybackGuard(m_playbackEngine, this);
    m_playbackLogger = new PlaybackLogger(this);
    m_dryRunEngine = new DryRunEngine(this);

    m_shortcutManager = new ShortcutManager(this);
    m_operationModeManager = new OperationModeManager(this);

    m_crashRecovery = new CrashRecovery(this);

    // MIDI input
    m_midiInputManager = new MidiInputManager(this);

    // console discovery
    m_discoveryService = new ConsoleDiscoveryService(this);
    m_discoveryService->registerStrategy(std::make_shared<BehringerX32ProbeStrategy>());
    m_discoveryService->registerStrategy(std::make_shared<BehringerWingProbeStrategy>());
    m_discoveryService->registerStrategy(std::make_shared<YamahaOscProbeStrategy>());

    // application logging
    m_appLogger = new AppLogger(this);
    m_connectionLogBridge = new ConnectionLogBridge(m_appLogger, this);

    // setup log file
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    m_appLogger->setLogFile(logDir + "/application.log");
}

Application::~Application() {
    if (m_crashRecovery) {
        m_crashRecovery->markCleanExit();
    }

    disconnectFromMixer();

    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void Application::initialize() {
    m_playbackEngine->setCueList(m_show->cueList());
    m_playbackEngine->setDCAMapping(m_show->dcaMapping());

    if (m_mixer) {
        m_playbackEngine->setMixer(m_mixer);
    }

    m_playbackEngine->setValidator(m_cueValidator);
    m_playbackEngine->setGuard(m_playbackGuard);
    m_playbackEngine->setLogger(m_playbackLogger);

    m_dryRunEngine->setCueList(m_show->cueList());
    m_dryRunEngine->setValidator(m_cueValidator);

    m_crashRecovery->setShow(m_show);
    m_crashRecovery->setPlaybackEngine(m_playbackEngine);
    m_crashRecovery->setAutoSaveEnabled(true);

    connect(m_show, &Show::modifiedChanged, m_autosaveManager, [this](bool modified) {
        if (modified) {
            m_autosaveManager->scheduleSave();
        }
    });

    connect(m_show, &Show::modifiedChanged, [this](bool modified) {
        if (!modified) {
            m_undoStack->setClean();
        }
    });

    connect(m_playbackEngine, &PlaybackEngine::cueExecuted, [this](int index) {
        if (m_playbackLogger && m_show->cueList() && index >= 0) {
            const Cue& cue = m_show->cueList()->at(index);
            m_playbackLogger->logCueExecuted(cue.id(), cue.name(), cue.number());
        }
    });

    connect(m_playbackEngine, &PlaybackEngine::goLockout, [this](const QString& reason) {
        if (m_playbackLogger) {
            m_playbackLogger->logGoLockout(reason);
        }
    });

    m_shortcutManager->loadFromSettings();
    m_operationModeManager->loadFromSettings();

    // MIDI input setup
    m_midiInputManager->setPlaybackEngine(m_playbackEngine);
    m_midiInputManager->setPlaybackGuard(m_playbackGuard);
    m_midiInputManager->loadFromSettings();
}

void Application::connectToMixer(const QString& type, const QString& host, int port) {
    disconnectFromMixer();

    m_mixer = ProtocolFactory::create(type, this);
    if (!m_mixer) {
        return;
    }

    m_playbackEngine->setMixer(m_mixer);

    // setup connection logging
    m_connectionLogBridge->setConnectionContext(type, host, port);
    m_connectionLogBridge->attachToMixer(m_mixer);
    m_appLogger->logConnectionAttempt(type, host, port);

    connect(m_mixer, &MixerProtocol::connected, this, [this]() { emit mixerConnected(); });
    connect(m_mixer, &MixerProtocol::disconnected, this, [this]() { emit mixerDisconnected(); });

    m_mixer->connect(host, port);
}

void Application::connectToDiscoveredConsole(const DiscoveredConsole& console) {
    if (!console.isValid()) {
        return;
    }

    disconnectFromMixer();

    m_mixer = ProtocolFactory::create(console, this);
    if (!m_mixer) {
        return;
    }

    m_playbackEngine->setMixer(m_mixer);

    // setup connection logging
    QString host = console.address.toString();
    QString protocol = console.modelName;
    m_connectionLogBridge->setConnectionContext(protocol, host, console.port);
    m_connectionLogBridge->attachToMixer(m_mixer);
    m_appLogger->logConnectionAttempt(protocol, host, console.port);

    connect(m_mixer, &MixerProtocol::connected, this, [this]() { emit mixerConnected(); });
    connect(m_mixer, &MixerProtocol::disconnected, this, [this]() { emit mixerDisconnected(); });

    m_mixer->connect(host, console.port);
}

void Application::disconnectFromMixer() {
    if (m_mixer) {
        m_connectionLogBridge->detachFromMixer();
        m_mixer->disconnect();
        m_playbackEngine->setMixer(nullptr);
        delete m_mixer;
        m_mixer = nullptr;
    }
}

} // namespace OpenMix
