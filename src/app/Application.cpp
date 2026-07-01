#include "Application.h"
#include "OscRemoteServer.h"
#include "QLabClient.h"
#include "ui/MainWindow.h"
#include "core/AppLogger.h"
#include "core/ChannelMonitor.h"
#include "core/ConnectionLogBridge.h"
#include "core/Cue.h"
#include "core/CueList.h"
#include "core/CueZero.h"
#include "core/Position.h"
#include "core/TimecodeTrigger.h"
#include "core/CueValidator.h"
#include "core/DCAMapping.h"
#include "core/DryRunEngine.h"
#include "core/OperationMode.h"
#include "core/PlaybackEngine.h"
#include "core/PlaybackGuard.h"
#include "core/PlaybackLogger.h"
#include "core/ScribbleController.h"
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
#include <QSettings>
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

    // inbound OSC remote control (QLab / stage-manager)
    m_oscRemoteServer = new OscRemoteServer(this);

    // outbound QLab / DAW remote
    m_qLabClient = new QLabClient(this);

    // timecode triggers + channel monitor
    m_timecodeTriggers = new TimecodeTriggerList(this);
    m_channelMonitor = new ChannelMonitor(this);

    // scribble-strip driver
    m_scribbleController = new ScribbleController(this);

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
    m_playbackEngine->setActorLibrary(m_show->actorProfileLibrary());
    m_playbackEngine->setPositionLibrary(m_show->positionLibrary());
    m_playbackEngine->setChannelGangs(m_show->channelGangs());
    m_playbackEngine->setDimDcaFaders(m_show->dimDcaFaders());
    m_playbackEngine->setMuteDcaUnassign(m_show->muteDcaUnassign());

    // re-seed gangs + console-behavior toggles whenever a project is loaded
    // (fromJson re-emits nameChanged)
    connect(m_show, &Show::nameChanged, this, [this]() {
        m_playbackEngine->setChannelGangs(m_show->channelGangs());
        m_playbackEngine->setDimDcaFaders(m_show->dimDcaFaders());
        m_playbackEngine->setMuteDcaUnassign(m_show->muteDcaUnassign());
    });

    if (m_mixer) {
        m_playbackEngine->setMixer(m_mixer);
    }

    m_playbackEngine->setValidator(m_cueValidator);
    m_playbackEngine->setGuard(m_playbackGuard);
    m_playbackEngine->setLogger(m_playbackLogger);
    m_playbackEngine->setVerifyCues(true);

    // Cue Zero base levels double as the panic/safe values
    m_playbackGuard->setDefaultSafeValues(m_show->cueZero()->levels());

    connect(m_playbackEngine, &PlaybackEngine::cueDrifted, this,
            [this](int index, const QStringList& paths) {
                if (m_appLogger) {
                    m_appLogger->log(LogLevel::Warning, LogSource::Playback,
                                     QString("Cue %1 drift on %2 parameter(s): %3")
                                         .arg(index)
                                         .arg(paths.size())
                                         .arg(paths.join(", ")));
                }
            });

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

    // inbound OSC remote control: /cue/go, /ctrl/fadeall, next/prev/goto
    m_oscRemoteServer->setPlaybackEngine(m_playbackEngine);
    connect(m_oscRemoteServer, &OscRemoteServer::fadeAllRequested, this, [this]() {
        if (m_playbackGuard)
            m_playbackGuard->panic();
    });
    m_oscRemoteServer->loadFromSettings();
    if (m_oscRemoteServer->start(m_oscRemoteServer->port())) {
        m_appLogger->log(LogLevel::Info, LogSource::System,
                         QString("OSC remote control listening on port %1")
                             .arg(m_oscRemoteServer->port()));
    } else {
        m_appLogger->log(LogLevel::Warning, LogSource::System,
                         QString("OSC remote control could not bind port %1")
                             .arg(m_oscRemoteServer->port()));
    }

    // outbound QLab/DAW remote: fire a linked QLab cue when a cue executes
    m_qLabClient->loadFromSettings();
    connect(m_playbackEngine, &PlaybackEngine::cueExecuted, this, [this](int index) {
        if (!m_qLabClient->isEnabled() || index < 0 || !m_show->cueList() ||
            index >= m_show->cueList()->count())
            return;
        const Cue& cue = m_show->cueList()->at(index);
        if (!cue.qLabCue().isEmpty())
            m_qLabClient->triggerCue(cue.qLabCue());
    });

    // timecode-triggered cues: incoming MTC -> trigger list -> fire cue by number
    connect(m_midiInputManager, &MidiInputManager::timecodeChanged, m_timecodeTriggers,
            &TimecodeTriggerList::onTimecode);
    connect(m_timecodeTriggers, &TimecodeTriggerList::triggerFired, this,
            [this](double cueNumber, const QString&) {
                m_playbackEngine->goToNumber(cueNumber);
                m_playbackEngine->go();
            });

    // scribble strips: actor names + cue number + silence/clip coloring +
    // active-channel highlight (DCA mapping resolves a cue's touched channels)
    m_scribbleController->setActorLibrary(m_show->actorProfileLibrary());
    m_scribbleController->setCueList(m_show->cueList());
    m_scribbleController->setDCAMapping(m_show->dcaMapping());
    connect(m_show->actorProfileLibrary(), &ActorProfileLibrary::changed, m_scribbleController,
            &ScribbleController::onActorLibraryChanged);
    connect(m_channelMonitor, &ChannelMonitor::channelStateChanged, m_scribbleController,
            &ScribbleController::onChannelStateChanged);
    connect(m_playbackEngine, &PlaybackEngine::currentCueChanged, m_scribbleController,
            &ScribbleController::onCurrentCueChanged);

    // seed scribble-highlight + channel-monitor tunables persisted by the
    // Settings dialog
    {
        QSettings settings;
        settings.beginGroup("Scribble");
        m_scribbleController->setHighlightColor(
            settings.value("highlightColor", m_scribbleController->highlightColor()).toInt());
        m_scribbleController->setHighlightEnabled(
            settings.value("highlightEnabled", false).toBool());
        settings.endGroup();

        settings.beginGroup("Monitor");
        m_channelMonitor->setSilenceThreshold(
            settings.value("silenceThreshold", m_channelMonitor->silenceThreshold()).toDouble());
        m_channelMonitor->setClipThreshold(
            settings.value("clipThreshold", m_channelMonitor->clipThreshold()).toDouble());
        m_channelMonitor->setSilenceTimeoutMs(
            settings.value("silenceTimeoutMs", m_channelMonitor->silenceTimeoutMs()).toInt());
        m_channelMonitor->setClipHoldMs(
            settings.value("clipHoldMs", m_channelMonitor->clipHoldMs()).toInt());
        settings.endGroup();
    }
}

void Application::setupMixerConnection(const QString& type, const QString& host, int port) {
    m_playbackEngine->setMixer(m_mixer);

    m_connectionLogBridge->setConnectionContext(type, host, port);
    m_connectionLogBridge->attachToMixer(m_mixer);
    m_appLogger->logConnectionAttempt(type, host, port);

    connect(m_mixer, &MixerProtocol::connected, this, [this]() { emit mixerConnected(); });
    connect(m_mixer, &MixerProtocol::disconnected, this, [this]() { emit mixerDisconnected(); });

    // live console metering -> channel silence/clip monitor
    connect(m_mixer, &MixerProtocol::channelMeter, m_channelMonitor,
            [this](int channel, float level) { m_channelMonitor->onLevel(channel, level); });

    // scribble strips push actor names/colors to this console
    m_scribbleController->setMixer(m_mixer);

    m_mixer->connect(host, port);

    QSettings settings;
    settings.beginGroup("LastMixer");
    settings.setValue("host", host);
    settings.setValue("type", type);
    settings.setValue("port", port);
    settings.endGroup();
}

void Application::connectToMixer(const QString& type, const QString& host, int port) {
    disconnectFromMixer();

    m_mixer = ProtocolFactory::create(type, this);
    if (!m_mixer)
        return;

    setupMixerConnection(type, host, port);
}

void Application::connectToDiscoveredConsole(const DiscoveredConsole& console) {
    if (!console.isValid())
        return;

    disconnectFromMixer();

    m_mixer = ProtocolFactory::create(console, this);
    if (!m_mixer)
        return;

    setupMixerConnection(console.toCapabilities().protocolId,
                         console.address.toString(),
                         console.port);
}

void Application::disconnectFromMixer() {
    if (m_mixer) {
        m_connectionLogBridge->detachFromMixer();
        m_scribbleController->setMixer(nullptr);
        m_mixer->disconnect();
        m_playbackEngine->setMixer(nullptr);
        delete m_mixer;
        m_mixer = nullptr;
    }
}

void Application::startupScan() {
    QSettings settings;
    settings.beginGroup("LastMixer");
    QString savedHost = settings.value("host").toString();
    settings.endGroup();

    if (!savedHost.isEmpty()) {
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(m_discoveryService, &ConsoleDiscoveryService::consoleDiscovered,
                        this, [this, savedHost, conn](const DiscoveredConsole& console) {
                            if (m_mixer == nullptr && console.address.toString() == savedHost) {
                                QObject::disconnect(*conn);
                                connectToDiscoveredConsole(console);
                            }
                        });

        connect(m_discoveryService, &ConsoleDiscoveryService::scanFinished,
                this, [conn]() {
                    QObject::disconnect(*conn);
                }, Qt::SingleShotConnection);
    }

    m_discoveryService->startScan(3000);
}

void Application::setMainWindow(MainWindow* window) { m_mainWindow = window; }

MainWindow* Application::mainWindow() { return m_mainWindow; }

} // namespace OpenMix
