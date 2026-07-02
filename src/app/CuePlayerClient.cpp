#include "CuePlayerClient.h"

#include <QByteArray>
#include <QSettings>

namespace OpenMix {

CuePlayerClient::CuePlayerClient(QObject* parent) : QObject(parent) { rebuildAddress(); }

CuePlayerClient::~CuePlayerClient() {
    if (m_address)
        lo_address_free(m_address);
}

void CuePlayerClient::setTarget(const QString& host, int port) {
    m_host = host;
    m_port = port > 0 ? port : CUEPLAYER_DEFAULT_PORT;
    rebuildAddress();
}

void CuePlayerClient::rebuildAddress() {
    if (m_address) {
        lo_address_free(m_address);
        m_address = nullptr;
    }
    m_address =
        lo_address_new(m_host.toUtf8().constData(), QByteArray::number(m_port).constData());
}

void CuePlayerClient::send(const QString& address) {
    if (!m_enabled || !m_address)
        return;
    lo_send(m_address, address.toUtf8().constData(), "");
    emit sent(address);
}

void CuePlayerClient::play() { send(QStringLiteral("/cpsound/play")); }

void CuePlayerClient::stop() { send(QStringLiteral("/cpsound/stop")); }

void CuePlayerClient::loadFromSettings() {
    QSettings settings;
    settings.beginGroup("CuePlayer");
    m_host = settings.value("host", m_host).toString();
    m_port = settings.value("port", m_port).toInt();
    m_enabled = settings.value("enabled", false).toBool();
    settings.endGroup();
    rebuildAddress();
}

void CuePlayerClient::saveToSettings() {
    QSettings settings;
    settings.beginGroup("CuePlayer");
    settings.setValue("host", m_host);
    settings.setValue("port", m_port);
    settings.setValue("enabled", m_enabled);
    settings.endGroup();
}

} // namespace OpenMix
