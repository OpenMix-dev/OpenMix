#include "ReaperClient.h"

#include <QByteArray>
#include <QMetaObject>
#include <QSettings>

namespace OpenMix {

namespace {
// liblo callback (runs on the OSC server thread): extract a float and forward.
int hTransport(const char* path, const char* types, lo_arg** argv, int argc, lo_message,
               void* user) {
    float value = 1.0f;
    if (argc > 0 && types) {
        if (types[0] == 'f')
            value = argv[0]->f;
        else if (types[0] == 'i')
            value = static_cast<float>(argv[0]->i32);
    }
    static_cast<ReaperClient*>(user)->deliverTransport(QString::fromUtf8(path), value);
    return 0;
}
} // namespace

ReaperClient::ReaperClient(QObject* parent) : QObject(parent) { rebuildAddress(); }

ReaperClient::~ReaperClient() {
    stopListening();
    if (m_address)
        lo_address_free(m_address);
}

void ReaperClient::setAutoDetect(bool on) {
    m_autoDetect = on;
    if (on)
        startListening();
    else
        stopListening();
}

void ReaperClient::setListenPort(int port) {
    if (port > 0)
        m_listenPort = port;
    if (m_autoDetect)
        startListening(); // rebind
}

void ReaperClient::startListening() {
    stopListening();
    const QByteArray portStr = QByteArray::number(m_listenPort);
    m_listener = lo_server_thread_new(portStr.constData(), nullptr);
    if (!m_listener)
        return;
    lo_server_thread_add_method(m_listener, "/record", nullptr, hTransport, this);
    lo_server_thread_add_method(m_listener, "/play", nullptr, hTransport, this);
    lo_server_thread_add_method(m_listener, "/stop", nullptr, hTransport, this);
    lo_server_thread_add_method(m_listener, "/pause", nullptr, hTransport, this);
    lo_server_thread_start(m_listener);
}

void ReaperClient::stopListening() {
    if (m_listener) {
        lo_server_thread_stop(m_listener);
        lo_server_thread_free(m_listener);
        m_listener = nullptr;
    }
}

void ReaperClient::deliverTransport(const QString& address, float value) {
    // marshal from the OSC server thread onto this object's thread
    QMetaObject::invokeMethod(
        this, [this, address, value]() { onTransport(address, value); }, Qt::QueuedConnection);
}

void ReaperClient::onTransport(const QString& address, float value) {
    bool recording = m_recordMode;
    if (address == QLatin1String("/record"))
        recording = value != 0.0f;
    else if (address == QLatin1String("/stop"))
        recording = false;
    // /play and /pause do not change the record state on their own

    if (recording != m_recordMode) {
        m_recordMode = recording;
        emit recordModeChanged(m_recordMode);
    }
}

void ReaperClient::setTarget(const QString& host, int port) {
    m_host = host;
    m_port = port > 0 ? port : REAPER_DEFAULT_PORT;
    rebuildAddress();
}

void ReaperClient::rebuildAddress() {
    if (m_address) {
        lo_address_free(m_address);
        m_address = nullptr;
    }
    m_address =
        lo_address_new(m_host.toUtf8().constData(), QByteArray::number(m_port).constData());
}

void ReaperClient::send(const QString& address) {
    if (!m_enabled || !m_address)
        return;
    lo_send(m_address, address.toUtf8().constData(), "f", 1.0f); // bang / trigger
    emit sent(address);
}

void ReaperClient::sendString(const QString& address, const QString& value) {
    if (!m_enabled || !m_address)
        return;
    lo_send(m_address, address.toUtf8().constData(), "s", value.toUtf8().constData());
    emit sent(address);
}

void ReaperClient::placeMarker(double cueNumber, const QString& name) {
    // insert a marker at the play cursor, then name it after the cue
    send(QStringLiteral("/action/%1").arg(ACTION_INSERT_MARKER));
    const QString label = name.isEmpty() ? QString::number(cueNumber, 'f', 2) : name;
    sendString(QStringLiteral("/lastmarker/name"), label);
    m_markers.append({cueNumber, label, m_nextMarker, QString()});
    ++m_nextMarker;
    emit markersChanged();
}

void ReaperClient::jumpToMarker(int index) {
    // go to the marker; pre-roll (m_preRollSeconds) is applied once REAPER marker
    // times are available via feedback -- see header note.
    send(QStringLiteral("/marker/%1").arg(index));
}

void ReaperClient::onCueFired(double cueNumber, const QString& name) {
    if (!m_enabled)
        return;
    if (m_recordMode) {
        placeMarker(cueNumber, name);
        return;
    }
    for (const MarkerEntry& marker : m_markers) {
        if (marker.cueNumber == cueNumber) {
            jumpToMarker(marker.index);
            return;
        }
    }
}

void ReaperClient::setMarkerNoteAt(int row, const QString& note) {
    if (row < 0 || row >= m_markers.size())
        return;
    m_markers[row].note = note;
    emit markersChanged();
}

void ReaperClient::resetMarkers() {
    m_markers.clear();
    m_nextMarker = 1;
    emit markersChanged();
}

void ReaperClient::loadFromSettings() {
    QSettings settings;
    settings.beginGroup("Reaper");
    m_host = settings.value("host", m_host).toString();
    m_port = settings.value("port", m_port).toInt();
    m_enabled = settings.value("enabled", false).toBool();
    m_recordMode = settings.value("recordMode", false).toBool();
    m_preRollSeconds = settings.value("preRollSeconds", 0).toInt();
    m_listenPort = settings.value("listenPort", DEFAULT_LISTEN_PORT).toInt();
    m_autoDetect = settings.value("autoDetect", false).toBool();
    settings.endGroup();
    rebuildAddress();
    if (m_autoDetect)
        startListening();
}

void ReaperClient::saveToSettings() {
    QSettings settings;
    settings.beginGroup("Reaper");
    settings.setValue("host", m_host);
    settings.setValue("port", m_port);
    settings.setValue("enabled", m_enabled);
    settings.setValue("recordMode", m_recordMode);
    settings.setValue("preRollSeconds", m_preRollSeconds);
    settings.setValue("listenPort", m_listenPort);
    settings.setValue("autoDetect", m_autoDetect);
    settings.endGroup();
}

} // namespace OpenMix
