#include "ReaperClient.h"

#include <QByteArray>
#include <QSettings>

namespace OpenMix {

ReaperClient::ReaperClient(QObject* parent) : QObject(parent) { rebuildAddress(); }

ReaperClient::~ReaperClient() {
    if (m_address)
        lo_address_free(m_address);
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
    m_markers.insert(cueNumber, m_nextMarker);
    ++m_nextMarker;
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
    } else if (m_markers.contains(cueNumber)) {
        jumpToMarker(m_markers.value(cueNumber));
    }
}

void ReaperClient::resetMarkers() {
    m_markers.clear();
    m_nextMarker = 1;
}

void ReaperClient::loadFromSettings() {
    QSettings settings;
    settings.beginGroup("Reaper");
    m_host = settings.value("host", m_host).toString();
    m_port = settings.value("port", m_port).toInt();
    m_enabled = settings.value("enabled", false).toBool();
    m_recordMode = settings.value("recordMode", false).toBool();
    m_preRollSeconds = settings.value("preRollSeconds", 0).toInt();
    settings.endGroup();
    rebuildAddress();
}

void ReaperClient::saveToSettings() {
    QSettings settings;
    settings.beginGroup("Reaper");
    settings.setValue("host", m_host);
    settings.setValue("port", m_port);
    settings.setValue("enabled", m_enabled);
    settings.setValue("recordMode", m_recordMode);
    settings.setValue("preRollSeconds", m_preRollSeconds);
    settings.endGroup();
}

} // namespace OpenMix
