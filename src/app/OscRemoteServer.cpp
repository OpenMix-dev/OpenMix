#include "OscRemoteServer.h"
#include "core/PlaybackEngine.h"

#include <QByteArray>
#include <QSettings>

namespace OpenMix {

namespace {
void onLoError(int num, const char* msg, const char* where) {
    Q_UNUSED(num);
    Q_UNUSED(msg);
    Q_UNUSED(where);
}

OscRemoteServer* server(void* user) { return static_cast<OscRemoteServer*>(user); }

int hGo(const char*, const char*, lo_arg**, int, lo_message, void* u) {
    server(u)->deliver(OscRemoteServer::Command::Go);
    return 0;
}
int hStop(const char*, const char*, lo_arg**, int, lo_message, void* u) {
    server(u)->deliver(OscRemoteServer::Command::Stop);
    return 0;
}
int hNext(const char*, const char*, lo_arg**, int, lo_message, void* u) {
    server(u)->deliver(OscRemoteServer::Command::Next);
    return 0;
}
int hPrev(const char*, const char*, lo_arg**, int, lo_message, void* u) {
    server(u)->deliver(OscRemoteServer::Command::Prev);
    return 0;
}
int hFadeAll(const char*, const char*, lo_arg**, int, lo_message, void* u) {
    server(u)->deliver(OscRemoteServer::Command::FadeAll);
    return 0;
}
int hGoto(const char*, const char* types, lo_arg** argv, int argc, lo_message, void* u) {
    if (argc < 1 || !types)
        return 0;
    double value = 0.0;
    switch (types[0]) {
    case 'i':
        value = argv[0]->i;
        break;
    case 'f':
        value = argv[0]->f;
        break;
    case 'h':
        value = static_cast<double>(argv[0]->h);
        break;
    case 'd':
        value = argv[0]->d;
        break;
    default:
        return 0;
    }
    server(u)->deliver(OscRemoteServer::Command::Goto, value);
    return 0;
}
} // namespace

OscRemoteServer::OscRemoteServer(QObject* parent) : QObject(parent) {}

OscRemoteServer::~OscRemoteServer() { stop(); }

bool OscRemoteServer::start(int port) {
    if (m_server)
        stop();

    const QByteArray portStr = port > 0 ? QByteArray::number(port) : QByteArray();
    m_server = lo_server_thread_new(port > 0 ? portStr.constData() : nullptr, onLoError);
    if (!m_server)
        return false;

    registerMethods();

    if (lo_server_thread_start(m_server) < 0) {
        lo_server_thread_free(m_server);
        m_server = nullptr;
        return false;
    }

    m_port = lo_server_thread_get_port(m_server);
    emit started(m_port);
    return true;
}

void OscRemoteServer::stop() {
    if (!m_server)
        return;
    lo_server_thread_stop(m_server);
    lo_server_thread_free(m_server);
    m_server = nullptr;
    emit stopped();
}

void OscRemoteServer::registerMethods() {
    lo_server_thread_add_method(m_server, "/go", nullptr, hGo, this);
    lo_server_thread_add_method(m_server, "/cue/go", nullptr, hGo, this);
    lo_server_thread_add_method(m_server, "/ctrl/go", nullptr, hGo, this);
    lo_server_thread_add_method(m_server, "/stop", nullptr, hStop, this);
    lo_server_thread_add_method(m_server, "/cue/stop", nullptr, hStop, this);
    lo_server_thread_add_method(m_server, "/ctrl/stop", nullptr, hStop, this);
    lo_server_thread_add_method(m_server, "/next", nullptr, hNext, this);
    lo_server_thread_add_method(m_server, "/prev", nullptr, hPrev, this);
    lo_server_thread_add_method(m_server, "/previous", nullptr, hPrev, this);
    lo_server_thread_add_method(m_server, "/ctrl/fadeall", nullptr, hFadeAll, this);
    lo_server_thread_add_method(m_server, "/cue/goto", nullptr, hGoto, this);
}

void OscRemoteServer::deliver(Command command, double arg) {
    // hop from the liblo server thread onto this object's (main) thread
    QMetaObject::invokeMethod(
        this,
        [this, command, arg]() {
            switch (command) {
            case Command::Go:
                emit goRequested();
                break;
            case Command::Stop:
                emit stopRequested();
                break;
            case Command::Next:
                emit nextRequested();
                break;
            case Command::Prev:
                emit prevRequested();
                break;
            case Command::FadeAll:
                emit fadeAllRequested();
                break;
            case Command::Goto:
                emit gotoRequested(arg);
                break;
            }
        },
        Qt::QueuedConnection);
}

void OscRemoteServer::setPlaybackEngine(PlaybackEngine* engine) {
    if (!engine)
        return;
    connect(this, &OscRemoteServer::goRequested, engine, &PlaybackEngine::go);
    connect(this, &OscRemoteServer::stopRequested, engine, &PlaybackEngine::stop);
    connect(this, &OscRemoteServer::nextRequested, engine, &PlaybackEngine::next);
    connect(this, &OscRemoteServer::prevRequested, engine, &PlaybackEngine::previous);
    connect(this, &OscRemoteServer::gotoRequested, engine, &PlaybackEngine::goToNumber);
}

void OscRemoteServer::loadFromSettings() {
    QSettings settings;
    settings.beginGroup("OscRemote");
    m_port = settings.value("port", 8000).toInt();
    settings.endGroup();
}

void OscRemoteServer::saveToSettings() {
    QSettings settings;
    settings.beginGroup("OscRemote");
    settings.setValue("port", m_port);
    settings.endGroup();
}

} // namespace OpenMix
